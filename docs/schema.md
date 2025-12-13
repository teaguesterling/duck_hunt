# Schema Reference

All Duck Hunt parsers output a standardized `ValidationEvent` schema with 40 fields organized into logical groups.

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

## Log Line Tracking Fields

Fields for tracking where each event originated within the source log file. Useful for context retrieval and drill-down workflows.

| Field | Type | Description |
|-------|------|-------------|
| `log_line_start` | INTEGER | 1-indexed line number where event starts in log file (NULL for structured formats) |
| `log_line_end` | INTEGER | 1-indexed line number where event ends in log file (NULL for structured formats) |

**Note:** `line_number` refers to the source code line (e.g., line 15 in `main.c`), while `log_line_start`/`log_line_end` track the position within the log file itself.

**Example:**
```sql
-- Get context around an error
SELECT log_line_start, log_line_end, file_path, line_number, message
FROM read_duck_hunt_log('build.log', 'make_error')
WHERE status = 'ERROR';
```

| log_line_start | log_line_end | file_path | line_number | message |
|----------------|--------------|-----------|-------------|---------|
| 2 | 2 | src/main.c | 15 | 'undefined_var' undeclared |
| 5 | 5 | src/main.c | 28 | unused variable 'temp' |

## Error Analysis Fields

Advanced fields for error pattern detection and root cause analysis. These fields enable clustering of similar errors across large log files or multiple CI runs.

| Field | Type | Description |
|-------|------|-------------|
| `error_fingerprint` | VARCHAR | Normalized error signature for pattern detection |
| `similarity_score` | DOUBLE | Similarity to pattern cluster centroid (0.0-1.0) |
| `pattern_id` | BIGINT | Assigned error pattern group ID (-1 if unassigned) |
| `root_cause_category` | VARCHAR | Detected root cause type |

### How Error Fingerprinting Works

The `error_fingerprint` is a hash of the normalized error message combined with tool and category context. Normalization removes variable content so structurally identical errors cluster together:

**Normalization removes:**
- Numbers (`line 15` → `line <N>`)
- Quoted strings (`'myVar'` → `<STR>`)
- File paths (`/home/user/src/main.c` → `<PATH>`)
- Hex values (`0x7fff5fbff8c0` → `<HEX>`)
- UUIDs (`550e8400-e29b-41d4-a716-446655440000` → `<UUID>`)

**Example:** These three errors get the same fingerprint:
```
src/main.c:10: error: 'ptr' undeclared (first use in this function)
src/utils.c:25: error: 'counter' undeclared (first use in this function)
src/api.c:42: error: 'obj' undeclared (first use in this function)
```

All normalize to: `error: <STR> undeclared (first use in this function)`

### Pattern Clustering

Events with identical fingerprints are assigned the same `pattern_id`. This enables aggregation:

```sql
-- Find the most common error types in a build log
SELECT
    pattern_id,
    COUNT(*) as occurrences,
    ANY_VALUE(message) as example
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR'
GROUP BY pattern_id
ORDER BY occurrences DESC;
```

The `similarity_score` (0.0-1.0) measures how similar each message is to the first occurrence of that pattern, using Jaccard similarity on word tokens.

### Root Cause Categories

Automatic categorization based on keyword detection in error messages:

| Category | Keywords | Example |
|----------|----------|---------|
| `network` | connection, timeout, dns, unreachable | `Connection refused` |
| `permission` | access denied, unauthorized, forbidden | `Permission denied` |
| `configuration` | config, not found, missing, does not exist | `File not found` |
| `resource` | memory, disk, quota, limit, space | `Out of memory` |
| `syntax` | syntax, parse, invalid, format | `Syntax error` |
| `build` | compile, link, undefined reference | `Undefined symbol` |
| `dependency` | import, module, require, package | `Module not found` |
| `test_logic` | assert, expect, should, test failures | `Assertion failed` |

**Example:**
```sql
-- Aggregate errors by root cause
SELECT root_cause_category, COUNT(*) as count
FROM read_duck_hunt_log('ci-output.log', 'auto')
WHERE status = 'ERROR'
GROUP BY root_cause_category;
```

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
