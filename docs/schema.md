# Schema Reference

All Duck Hunt parsers output a standardized `ValidationEvent` schema with 38 fields organized into logical groups.

## Core Fields

Essential fields present in most events.

| Field | Type | Description |
|-------|------|-------------|
| `event_id` | BIGINT | Unique identifier for each event |
| `tool_name` | VARCHAR | Name of the tool (pytest, eslint, make, etc.) |
| `event_type` | VARCHAR | Category: TEST_RESULT, BUILD_ERROR, LINT_ISSUE, MEMORY_ERROR, etc. |
| `file_path` | VARCHAR | Path to the source file |
| `line_number` | INTEGER | Line number (-1 if not applicable) |
| `column_number` | INTEGER | Column number (-1 if not applicable) |
| `function_name` | VARCHAR | Function/method/test name |
| `status` | VARCHAR | PASS, FAIL, ERROR, WARNING, INFO, SKIP |
| `severity` | VARCHAR | error, warning, info, critical |
| `category` | VARCHAR | Specific category (compilation, test_failure, lint_error) |
| `message` | VARCHAR | Detailed error/warning message |
| `suggestion` | VARCHAR | Suggested fix or improvement |
| `error_code` | VARCHAR | Tool-specific error code or rule ID |
| `test_name` | VARCHAR | Full test identifier |
| `execution_time` | DOUBLE | Execution time in seconds |
| `raw_output` | VARCHAR | Original raw output line |
| `structured_data` | VARCHAR | Additional JSON metadata |

## Error Analysis Fields

Advanced fields for error pattern detection and root cause analysis.

| Field | Type | Description |
|-------|------|-------------|
| `error_fingerprint` | VARCHAR | Normalized error signature for pattern detection |
| `similarity_score` | DOUBLE | Similarity to pattern cluster centroid (0.0-1.0) |
| `pattern_id` | BIGINT | Assigned error pattern group ID (-1 if unassigned) |
| `root_cause_category` | VARCHAR | Detected root cause type |

### Root Cause Categories

- `network` - Connection failures, timeouts, DNS errors
- `permission` - Access denied, authentication failures
- `config` - Configuration errors, missing settings
- `syntax` - Syntax errors, parse failures
- `build` - Compilation errors, linking failures
- `resource` - Out of memory, disk full, resource limits
- `dependency` - Missing dependencies, version conflicts

## Multi-file Metadata Fields

Fields for processing multiple log files with glob patterns.

| Field | Type | Description |
|-------|------|-------------|
| `source_file` | VARCHAR | Original log file path |
| `build_id` | VARCHAR | Build ID extracted from file path |
| `environment` | VARCHAR | Environment (prod, staging, dev) from path |
| `file_index` | INTEGER | Processing order index (0-based) |

## Workflow CI/CD Fields

Fields for hierarchical CI/CD workflow parsing (GitHub Actions, GitLab CI, Jenkins, Docker).

| Field | Type | Description |
|-------|------|-------------|
| `workflow_name` | VARCHAR | Name of the workflow/pipeline |
| `job_name` | VARCHAR | Name of the current job/stage |
| `step_name` | VARCHAR | Name of the current step |
| `workflow_run_id` | VARCHAR | Unique workflow run identifier |
| `job_id` | VARCHAR | Unique job identifier |
| `step_id` | VARCHAR | Unique step identifier |
| `workflow_status` | VARCHAR | running, success, failure, cancelled |
| `job_status` | VARCHAR | pending, running, success, failure, skipped |
| `step_status` | VARCHAR | pending, running, success, failure, skipped |
| `started_at` | VARCHAR | Start timestamp (ISO format) |
| `completed_at` | VARCHAR | Completion timestamp (ISO format) |
| `duration` | DOUBLE | Duration in seconds |
| `workflow_type` | VARCHAR | github_actions, gitlab_ci, jenkins, docker_build |
| `hierarchy_level` | INTEGER | 0=workflow, 1=job, 2=step, 3=tool_output |
| `parent_id` | VARCHAR | Parent element ID in hierarchy |

## Event Types

| Event Type | Description |
|------------|-------------|
| `TEST_RESULT` | Test pass/fail/skip |
| `LINT_ISSUE` | Linting warning or error |
| `TYPE_ERROR` | Type checking error |
| `BUILD_ERROR` | Compilation/build error |
| `SECURITY_FINDING` | Security vulnerability |
| `MEMORY_ERROR` | Memory access error |
| `MEMORY_LEAK` | Memory leak detected |
| `THREAD_ERROR` | Threading/concurrency error |
| `PERFORMANCE_ISSUE` | Performance problem |
| `CRASH_SIGNAL` | Program crash/signal |
| `DEBUG_EVENT` | Debugger event |
| `SUMMARY` | Summary/statistics |

## Status Values

| Status | Description |
|--------|-------------|
| `PASS` | Test passed, check succeeded |
| `FAIL` | Test failed, check failed |
| `ERROR` | Error occurred |
| `WARNING` | Warning issued |
| `INFO` | Informational message |
| `SKIP` | Test/check skipped |

## Example Queries

**Get all fields for an event:**
```sql
SELECT * FROM read_duck_hunt_log('build.log', 'auto') LIMIT 1;
```

**Filter by event type:**
```sql
SELECT file_path, line_number, message
FROM read_duck_hunt_log('build.log', 'auto')
WHERE event_type = 'BUILD_ERROR';
```

**Aggregate by root cause:**
```sql
SELECT root_cause_category, COUNT(*) as count
FROM read_duck_hunt_log('logs/*.log', 'auto')
WHERE root_cause_category IS NOT NULL
GROUP BY root_cause_category;
```

**Workflow hierarchy traversal:**
```sql
SELECT hierarchy_level, workflow_name, job_name, step_name, step_status
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
ORDER BY hierarchy_level, event_id;
```
