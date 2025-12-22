# Workflow Format Reference

Duck Hunt provides specialized parsing for CI/CD workflow logs through the `read_duck_hunt_workflow_log()` function. This document covers the supported workflow formats and how to use them.

## Overview

Workflow parsing differs from regular log parsing (`read_duck_hunt_log()`) in several key ways:

| Aspect | Log Parsing | Workflow Parsing |
|--------|-------------|------------------|
| **Function** | `read_duck_hunt_log()` | `read_duck_hunt_workflow_log()` |
| **Purpose** | Extract validation events (errors, warnings) | Parse hierarchical workflow structure |
| **Output** | Individual events | Workflow → Job → Step hierarchy |
| **Primary fields** | `status`, `message`, `file_path` | `scope`, `group`, `unit`, `*_status` |

### When to Use Workflow Parsing

Use `read_duck_hunt_workflow_log()` when you need to:
- Understand workflow/pipeline structure
- Find which jobs or steps failed
- Track step execution times
- Analyze workflow patterns across runs

Use `read_duck_hunt_log()` when you need to:
- Extract individual error messages
- Parse compiler/linter output within CI logs
- Count specific error types

## Quick Start

```sql
-- Load the extension
LOAD duck_hunt;

-- Parse GitHub Actions workflow log
SELECT
    scope as workflow,
    "group" as job,
    unit as step,
    unit_status,
    message
FROM read_duck_hunt_workflow_log('workflow.log', 'github_actions')
WHERE unit_status = 'failure';
```

## Hierarchical Structure

Workflow logs are parsed into a 4-level hierarchy:

```
scope (workflow/pipeline)
  └── group (job)
        └── unit (step)
              └── subunit (sub-step, optional)
```

Each level has associated fields:

| Level | Name Field | ID Field | Status Field |
|-------|------------|----------|--------------|
| 1 | `scope` | `scope_id` | `scope_status` |
| 2 | `group` | `group_id` | `group_status` |
| 3 | `unit` | `unit_id` | `unit_status` |
| 4 | `subunit` | `subunit_id` | - |

---

## Supported Formats

### github_actions

Parses GitHub Actions workflow run logs.

**Sample Input:**
```
2024-01-15T10:00:00.0000000Z ##[group]Run actions/checkout@v4
2024-01-15T10:00:00.1234567Z with:
2024-01-15T10:00:00.1234567Z   repository: myorg/myrepo
2024-01-15T10:00:01.0000000Z ##[endgroup]
2024-01-15T10:00:05.0000000Z ##[group]Run npm test
2024-01-15T10:01:10.0000000Z FAIL src/auth.test.js
2024-01-15T10:01:15.2000000Z ##[error]Process completed with exit code 1.
2024-01-15T10:01:16.0000000Z ##[endgroup]
```

**Key Patterns Recognized:**
- `##[group]Step Name` / `##[endgroup]` - Step boundaries
- `##[error]Message` - Error annotations
- `##[warning]Message` - Warning annotations
- Timestamps in ISO 8601 format

**Field Mappings:**

| Field | GitHub Actions Meaning |
|-------|------------------------|
| `scope` | Workflow name (from context) |
| `group` | Job name |
| `unit` | Step name (from `##[group]`) |
| `unit_status` | Step result |
| `message` | Log line content |
| `started_at` | Timestamp |

**Example Queries:**

```sql
-- Find all failed steps
SELECT unit as step, message
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE status = 'ERROR';

-- Count events by step
SELECT unit as step, COUNT(*) as lines
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE unit IS NOT NULL
GROUP BY unit
ORDER BY lines DESC;

-- Find error annotations
SELECT unit as step, message
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE message LIKE '##[error]%';
```

---

### gitlab_ci

Parses GitLab CI/CD job logs.

**Sample Input:**
```
Running with gitlab-runner 16.6.0 (bef5fcef)
  on runner-abc123-project-456-concurrent-0 abc123XY
Preparing the "docker" executor
Using Docker executor with image ruby:3.2.2 ...
Executing "step_script" stage of the job script
$ bundle install --jobs 4 --retry 3
$ bundle exec rspec spec/
.......F..E....

Failures:
  1) UserController#create creates a new user
     Failure/Error: expect(response).to have_http_status(:created)
```

**Key Patterns Recognized:**
- `Running with gitlab-runner` - Job start
- `Executing "stage_name" stage` - Stage execution
- `$ command` - Shell commands
- RSpec/test framework output

**Field Mappings:**

| Field | GitLab CI Meaning |
|-------|-------------------|
| `scope` | Pipeline (from context) |
| `group` | Job name |
| `unit` | Stage name |
| `origin` | Runner identifier |
| `message` | Log line content |
| `category` | Log category (executor, git, script) |

**Example Queries:**

```sql
-- Find all shell commands executed
SELECT message
FROM read_duck_hunt_workflow_log('gitlab.log', 'gitlab_ci')
WHERE message LIKE '$ %';

-- Get runner information
SELECT DISTINCT origin as runner
FROM read_duck_hunt_workflow_log('gitlab.log', 'gitlab_ci')
WHERE origin IS NOT NULL;
```

---

### jenkins

Parses Jenkins console output logs.

**Sample Input:**
```
Started by user admin
Running as SYSTEM
Building in workspace /var/jenkins_home/workspace/my-pipeline
[Pipeline] Start of Pipeline
[Pipeline] node
[Pipeline] stage
[Pipeline] { (Checkout)
[Pipeline] checkout
Cloning repository https://github.com/myorg/myrepo.git
[Pipeline] }
[Pipeline] // stage
[Pipeline] stage
[Pipeline] { (Build)
[Pipeline] sh
+ mvn clean compile
[INFO] Building myapp 1.0.0-SNAPSHOT
```

**Key Patterns Recognized:**
- `[Pipeline] stage` / `[Pipeline] { (StageName)` - Stage boundaries
- `[Pipeline] sh` - Shell step
- `Started by user` - Build trigger
- Maven/Gradle/npm output patterns

**Field Mappings:**

| Field | Jenkins Meaning |
|-------|-----------------|
| `scope` | Pipeline/Job name |
| `group` | Stage name |
| `unit` | Step type (sh, checkout, etc.) |
| `origin` | Workspace path |
| `principal` | User who triggered build |
| `message` | Log line content |

**Example Queries:**

```sql
-- Find all stages
SELECT DISTINCT "group" as stage
FROM read_duck_hunt_workflow_log('jenkins.log', 'jenkins')
WHERE "group" IS NOT NULL;

-- Get build trigger info
SELECT principal as triggered_by, message
FROM read_duck_hunt_workflow_log('jenkins.log', 'jenkins')
WHERE message LIKE 'Started by%';

-- Find shell commands
SELECT message
FROM read_duck_hunt_workflow_log('jenkins.log', 'jenkins')
WHERE message LIKE '+ %';
```

---

### docker_build

Parses Docker build output and container logs.

**Sample Input:**
```
2024-01-15T10:00:00.000000000Z stdout F Starting application...
2024-01-15T10:00:01.000000000Z stdout F Connecting to database at db.internal:5432
2024-01-15T10:02:00.000000000Z stderr F [ERROR] Database query timeout
2024-01-15T10:03:00.000000000Z stderr F [ERROR] Connection refused: redis.internal:6379
```

**Key Patterns Recognized:**
- Docker log format: `timestamp stream flag message`
- `stdout` / `stderr` stream indicators
- `P` (partial) / `F` (full) log flags
- `[ERROR]`, `[WARN]`, `[INFO]` log levels

**Field Mappings:**

| Field | Docker Meaning |
|-------|----------------|
| `scope` | Container/service name |
| `category` | Stream (stdout/stderr) |
| `severity` | Log level from message |
| `message` | Log content |
| `started_at` | Timestamp |

**Example Queries:**

```sql
-- Find all errors
SELECT started_at, message
FROM read_duck_hunt_workflow_log('docker.log', 'docker_build')
WHERE category = 'stderr' OR message LIKE '%[ERROR]%';

-- Count log lines by stream
SELECT category as stream, COUNT(*) as lines
FROM read_duck_hunt_workflow_log('docker.log', 'docker_build')
GROUP BY category;
```

---

## Common Query Patterns

### Find Failed Steps Across All Formats

```sql
-- Works with any workflow format
SELECT
    scope as workflow,
    "group" as job,
    unit as step,
    unit_status,
    message
FROM read_duck_hunt_workflow_log('workflow.log', 'auto')
WHERE unit_status IN ('failure', 'error', 'failed')
   OR status = 'ERROR';
```

### Calculate Step Durations

```sql
-- For formats with timestamps
WITH step_times AS (
    SELECT
        unit as step,
        MIN(started_at) as start_time,
        MAX(started_at) as end_time
    FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
    WHERE unit IS NOT NULL
    GROUP BY unit
)
SELECT
    step,
    start_time,
    end_time
FROM step_times
ORDER BY start_time;
```

### Aggregate Errors by Job

```sql
SELECT
    "group" as job,
    COUNT(*) FILTER (WHERE status = 'ERROR') as errors,
    COUNT(*) FILTER (WHERE status = 'WARNING') as warnings
FROM read_duck_hunt_workflow_log('workflow.log', 'auto')
GROUP BY "group"
HAVING COUNT(*) FILTER (WHERE status = 'ERROR') > 0;
```

---

## Auto-Detection

Use `'auto'` to automatically detect the workflow format:

```sql
SELECT *
FROM read_duck_hunt_workflow_log('workflow.log', 'auto');
```

The auto-detector recognizes:
- GitHub Actions by `##[group]` and `##[error]` markers
- GitLab CI by `Running with gitlab-runner` header
- Jenkins by `[Pipeline]` markers
- Docker by timestamp + stream format

---

## Combining with Log Parsing

For comprehensive analysis, you can combine workflow parsing with regular log parsing:

```sql
-- Get workflow structure
CREATE TEMP TABLE workflow_structure AS
SELECT DISTINCT "group" as job, unit as step
FROM read_duck_hunt_workflow_log('ci.log', 'github_actions');

-- Extract detailed errors from the same log
CREATE TEMP TABLE build_errors AS
SELECT *
FROM read_duck_hunt_log('ci.log', 'make_error')
WHERE status = 'ERROR';

-- Combine for context
SELECT
    ws.job,
    ws.step,
    be.ref_file as file,
    be.ref_line as line,
    be.message
FROM workflow_structure ws
CROSS JOIN build_errors be;
```

---

## See Also

- [Format Maturity](format-maturity.md) - Stability ratings for all formats
- [Field Mappings](field_mappings.md) - Complete field documentation
- [Schema Reference](schema.md) - Full schema documentation
