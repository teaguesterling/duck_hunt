# Schema Reference

All Duck Hunt parsers output a standardized `ValidationEvent` schema with 39 fields organized into logical groups.

## Field Summary

| Group | Fields |
|-------|--------|
| Core | `event_id`, `tool_name`, `event_type` |
| Reference Location | `ref_file`, `ref_line`, `ref_column`, `function_name` |
| Classification | `status`, `severity`, `category`, `error_code` |
| Content | `message`, `suggestion`, `log_content`, `structured_data` |
| Log Tracking | `log_file`, `log_line_start`, `log_line_end` |
| Test | `test_name`, `execution_time` |
| Identity | `principal`, `origin`, `target`, `actor_type` |
| Temporal | `started_at` |
| Correlation | `external_id` |
| Hierarchy | `hierarchy_level`, `scope`, `scope_id`, `scope_status`, `group`, `group_id`, `group_status`, `unit`, `unit_id`, `unit_status`, `subunit`, `subunit_id` |
| Pattern Analysis | `fingerprint`, `similarity_score`, `pattern_id` |

---

## Core Fields

| Field | Type | Description |
|-------|------|-------------|
| `event_id` | BIGINT | Unique identifier for each event |
| `tool_name` | VARCHAR | Name of the tool (pytest, eslint, make, etc.) |
| `event_type` | VARCHAR | Category: TEST_RESULT, BUILD_ERROR, LINT_ISSUE, etc. |

## Reference Location Fields

For code locations referenced in events (lint issues, test failures, stack traces). These refer to positions in source code files mentioned in the log output.

| Field | Type | Description |
|-------|------|-------------|
| `ref_file` | VARCHAR | Path to referenced source code file |
| `ref_line` | INTEGER | Line number in source file (-1 if not applicable) |
| `ref_column` | INTEGER | Column number in source file (-1 if not applicable) |
| `function_name` | VARCHAR | Function/method name in source code |

## Classification Fields

| Field | Type | Description |
|-------|------|-------------|
| `status` | VARCHAR | PASS, FAIL, ERROR, WARNING, INFO, SKIP |
| `severity` | VARCHAR | debug, info, warning, error, critical |
| `category` | VARCHAR | Domain-specific classifier |
| `error_code` | VARCHAR | Tool-specific error code or rule ID |

### Severity Levels

Severity indicates the importance/urgency of an event, ordered from lowest to highest:

| Level | Description | Examples |
|-------|-------------|----------|
| `debug` | Trace/debug information | Build command output, directory changes |
| `info` | Informational events | Passing tests, commands executed, summaries |
| `warning` | Non-fatal issues | Skipped tests, deprecations, compiler warnings |
| `error` | Errors and failures | Test failures, build errors, lint errors |
| `critical` | Fatal/severe errors | Crashes, security critical, internal errors |

### Severity Threshold Parameter

Control which events are emitted using `severity_threshold`:

```sql
-- Default (threshold = 'all') - includes everything, backwards compatible
SELECT * FROM read_duck_hunt_log('build.log', 'make_error');

-- Only warnings and above
SELECT * FROM read_duck_hunt_log('test.json', 'pytest_json', severity_threshold := 'warning');

-- Only errors and critical
SELECT * FROM read_duck_hunt_log('build.log', 'make_error', severity_threshold := 'error');

-- Explicit 'all' for clarity
SELECT * FROM read_duck_hunt_log('build.log', 'make_error', severity_threshold := 'all');
```

Events are emitted when `event.severity >= threshold`. The default is `'all'` (alias for `'debug'`) for backwards compatibility.

### Status vs Severity

These fields serve different purposes:

| Field | Purpose | Example |
|-------|---------|---------|
| `status` | Semantic outcome | PASS, FAIL, SKIP |
| `severity` | Importance level | info, warning, error |

A passing test has `status='PASS'` but `severity='info'`. A skipped test has `status='SKIP'` but `severity='warning'`.

## Content Fields

| Field | Type | Description |
|-------|------|-------------|
| `message` | VARCHAR | Detailed error/warning message |
| `suggestion` | VARCHAR | Suggested fix or improvement |
| `log_content` | VARCHAR | Original raw log content for this event |
| `structured_data` | VARCHAR | Additional metadata (JSON or format name for delegated events) |

### structured_data Usage

The `structured_data` field serves multiple purposes:

- **JSON Metadata**: Additional structured information from the parser (e.g., test configuration)
- **Delegation Format**: For workflow delegation, contains the format name of the delegated parser (e.g., `make_error`, `pytest_text`). This allows filtering delegated events by tool type.

## Log Tracking Fields

Track where each event originated within the source log file.

| Field | Type | Description |
|-------|------|-------------|
| `log_file` | VARCHAR | Path to the log file being parsed |
| `log_line_start` | INTEGER | 1-indexed line where event starts in log |
| `log_line_end` | INTEGER | 1-indexed line where event ends in log |

**Note:** `ref_line` refers to source code line (e.g., line 15 in `main.c`), while `log_line_start`/`log_line_end` track position within the log file itself.

## Test Fields

| Field | Type | Description |
|-------|------|-------------|
| `test_name` | VARCHAR | Full test identifier |
| `execution_time` | DOUBLE | Duration in milliseconds |

## Identity & Network Fields

For tracking actors and network activity.

| Field | Type | Description |
|-------|------|-------------|
| `principal` | VARCHAR | Actor identity (ARN, email, username) |
| `origin` | VARCHAR | Source (IP address, hostname, runner) |
| `target` | VARCHAR | Destination (IP:port, HTTP path, resource ARN) |
| `actor_type` | VARCHAR | Type: user, service, system, anonymous |

## Temporal Fields

| Field | Type | Description |
|-------|------|-------------|
| `started_at` | VARCHAR | Event timestamp (ISO format) |

## Correlation Fields

| Field | Type | Description |
|-------|------|-------------|
| `external_id` | VARCHAR | External correlation ID (request ID, trace ID) |

## Hierarchy Fields

Generic 4-level hierarchy that maps to any domain (CI/CD, K8s, cloud, tests).

| Field | Type | Description |
|-------|------|-------------|
| `hierarchy_level` | INTEGER | Hierarchy depth: 1=scope, 2=group, 3=unit, 4=subunit/delegated |
| `scope` | VARCHAR | Level 1: Broadest context (workflow, cluster, account, suite) |
| `scope_id` | VARCHAR | Unique identifier for scope |
| `scope_status` | VARCHAR | Status at scope level |
| `group` | VARCHAR | Level 2: Middle grouping (job, namespace, region, class) |
| `group_id` | VARCHAR | Unique identifier for group |
| `group_status` | VARCHAR | Status at group level |
| `unit` | VARCHAR | Level 3: Specific unit (step, pod, service, method) |
| `unit_id` | VARCHAR | Unique identifier for unit |
| `unit_status` | VARCHAR | Status at unit level |
| `subunit` | VARCHAR | Level 4: Sub-unit when needed (container, resource) |
| `subunit_id` | VARCHAR | Unique identifier for subunit |

### Hierarchy Level Values

The `hierarchy_level` field indicates the depth of an event in the hierarchy:

| Level | Name | CI/CD Example | Description |
|-------|------|---------------|-------------|
| 1 | Scope | Workflow | Top-level container |
| 2 | Group | Job | Major grouping |
| 3 | Unit | Step | Specific work unit |
| 4 | Subunit/Delegated | Compiler error | Sub-unit or delegated parser output |

**Workflow Delegation**: When workflow parsers delegate to tool parsers (e.g., `make_error`), the delegated events have `hierarchy_level = 4`. See [Workflow Delegation](workflow-formats.md#workflow-delegation) for details.

**See [Field Mappings](field_mappings.md) for how these map to specific domains.**

## Pattern Analysis Fields

For error clustering and root cause analysis.

| Field | Type | Description |
|-------|------|-------------|
| `fingerprint` | VARCHAR | Normalized event signature for pattern detection |
| `similarity_score` | DOUBLE | Similarity to pattern cluster centroid (0.0-1.0) |
| `pattern_id` | BIGINT | Assigned pattern group ID (-1 if unassigned) |

### How Fingerprinting Works

The `fingerprint` is a hash of the normalized message. Normalization removes variable content so structurally identical errors cluster together:

- Numbers: `line 15` → `line <N>`
- Quoted strings: `'myVar'` → `<STR>`
- File paths: `/home/user/src/main.c` → `<PATH>`
- Hex values: `0x7fff5fbff8c0` → `<HEX>`

**Example:** These errors get the same fingerprint:
```
src/main.c:10: error: 'ptr' undeclared
src/utils.c:25: error: 'counter' undeclared
```

Both normalize to: `error: <STR> undeclared`

### Pattern Clustering

```sql
-- Find most common error types
SELECT
    pattern_id,
    COUNT(*) as occurrences,
    ANY_VALUE(message) as example
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR'
GROUP BY pattern_id
ORDER BY occurrences DESC;
```

---

## Workflow-Specific Fields

The `read_duck_hunt_workflow_log()` function outputs additional fields beyond the standard schema:

| Field | Type | Description |
|-------|------|-------------|
| `workflow_type` | VARCHAR | Workflow system: `github_actions`, `gitlab_ci`, `jenkins`, `docker_build`, `spack` |
| `hierarchy_level` | INTEGER | Depth: 1=workflow, 2=job, 3=step, 4=delegated |
| `parent_id` | VARCHAR | ID of parent element in hierarchy |
| `job_order` | INTEGER | Job execution order (ZIP format only, -1 otherwise) |
| `job_name` | VARCHAR | Job name from ZIP filename (ZIP format only) |

The `job_order` and `job_name` fields are populated when using `github_actions_zip` format to parse downloaded workflow run ZIP archives. See [Workflow Formats](workflow-formats.md#github_actions_zip) for details.

---

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

### Summary Events

Parsers emit `SUMMARY` events to confirm tool execution and provide aggregate statistics. This is useful when using `severity_threshold` to filter events—you can still see that a tool ran successfully even when filtering out individual issues.

**Parsers with summary events:**
- **pytest** - `"2 passed, 1 failed in 1.23s"` from text output or JSON summary object
- **ESLint** - `"3 problem(s) in 2 file(s)"` with file/issue counts
- **Flake8** - `"18 issue(s) found"` with total count
- **MyPy** - `"Success: no issues found"` or `"Found 5 errors"`
- **Pylint** - `"Your code has been rated at 8.50/10"`

**Summary event fields:**
- `event_type` = `'summary'`
- `status` = `WARNING` if issues found, `INFO` if clean
- `severity` = `'warning'` or `'info'`
- `message` = Human-readable summary
- `structured_data` = JSON with counts (e.g., `{"passed":5,"failed":1}`)

**Example: Filtering with summaries**
```sql
-- Show only errors, but keep summary events visible
SELECT event_type, status, message
FROM read_duck_hunt_log('pytest.json', 'pytest_json', severity_threshold := 'warning')
WHERE status = 'ERROR' OR event_type = 'summary';
```

## Status Values

| Status | Description |
|--------|-------------|
| `PASS` | Test passed, check succeeded |
| `FAIL` | Test failed, check failed |
| `ERROR` | Error occurred |
| `WARNING` | Warning issued |
| `INFO` | Informational message |
| `SKIP` | Test/check skipped |

---

## Example Queries

**Get all fields for an event:**
```sql
SELECT * FROM read_duck_hunt_log('build.log', 'auto') LIMIT 1;
```

**Filter by status:**
```sql
SELECT ref_file, ref_line, message
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR';
```

**Aggregate by pattern:**
```sql
SELECT fingerprint, COUNT(*) as count
FROM read_duck_hunt_log('logs/*.log', 'auto')
WHERE fingerprint IS NOT NULL
GROUP BY fingerprint;
```

**Workflow hierarchy:**
```sql
SELECT scope, "group", unit, unit_status
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
ORDER BY event_id;
```
