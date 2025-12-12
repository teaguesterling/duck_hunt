# Usage Examples

Comprehensive examples for using Duck Hunt in various scenarios.

## Pipeline Integration

### Real-time Build Analysis

```bash
# Capture make output and analyze in real-time
make 2>&1 | tee build.log | duckdb -s "
  SELECT status_badge(status) as badge, file_path, message
  FROM read_duck_hunt_log('/dev/stdin', 'auto')
  WHERE status IN ('ERROR', 'WARNING')
"

# Save build results to parquet for historical analysis
make 2>&1 | duckdb -s "
  COPY (
    SELECT * EXCLUDE (raw_output)
    FROM read_duck_hunt_log('/dev/stdin', 'auto')
  ) TO 'builds/$(date +%Y%m%d).parquet'
"

# JSON output for CI integration
./build.sh 2>&1 | duckdb -json -s "
  SELECT tool_name, COUNT(*) as errors
  FROM read_duck_hunt_log('/dev/stdin', 'auto')
  WHERE status = 'ERROR'
  GROUP BY tool_name
"
```

### Quality Gates

```sql
-- Fail if more than 5 errors
SELECT CASE WHEN error_count > 5 THEN 'FAIL' ELSE 'PASS' END as gate_status
FROM (
  SELECT COUNT(*) as error_count
  FROM read_duck_hunt_log('build.log', 'auto')
  WHERE status = 'ERROR'
);

-- Fail if any security issues
SELECT CASE WHEN COUNT(*) > 0 THEN 'FAIL' ELSE 'PASS' END as security_gate
FROM read_duck_hunt_log('bandit_output.json', 'bandit_json')
WHERE severity = 'error';
```

## Test Analysis

### Cross-Framework Comparison

```sql
-- Compare test results across frameworks
SELECT
  tool_name,
  COUNT(*) as total,
  SUM(CASE WHEN status = 'PASS' THEN 1 ELSE 0 END) as passed,
  SUM(CASE WHEN status = 'FAIL' THEN 1 ELSE 0 END) as failed,
  ROUND(AVG(execution_time), 3) as avg_time_sec
FROM read_duck_hunt_log('test_outputs/*', 'auto')
WHERE event_type = 'TEST_RESULT'
GROUP BY tool_name
ORDER BY failed DESC;
```

### Slow Test Detection

```sql
-- Find slowest tests
SELECT test_name, execution_time, tool_name
FROM read_duck_hunt_log('pytest_output.json', 'pytest_json')
WHERE status = 'PASS'
ORDER BY execution_time DESC
LIMIT 10;

-- Tests exceeding threshold
SELECT test_name, execution_time
FROM read_duck_hunt_log('test_results.json', 'auto')
WHERE execution_time > 5.0
ORDER BY execution_time DESC;
```

### Flaky Test Detection

```sql
-- Find tests that both passed and failed across runs
SELECT test_name,
       COUNT(CASE WHEN status = 'PASS' THEN 1 END) as passes,
       COUNT(CASE WHEN status = 'FAIL' THEN 1 END) as failures
FROM read_duck_hunt_log('ci_logs/**/*.json', 'auto')
WHERE event_type = 'TEST_RESULT'
GROUP BY test_name
HAVING passes > 0 AND failures > 0
ORDER BY failures DESC;
```

## Build Error Analysis

### Error Pattern Clustering

```sql
-- Group similar errors by fingerprint
SELECT
  error_fingerprint,
  COUNT(*) as occurrences,
  ANY_VALUE(message) as example_message,
  GROUP_CONCAT(DISTINCT file_path) as affected_files
FROM read_duck_hunt_log('build_logs/*.log', 'auto')
WHERE status = 'ERROR'
GROUP BY error_fingerprint
ORDER BY occurrences DESC;
```

### Root Cause Analysis

```sql
-- Categorize errors by root cause
SELECT
  root_cause_category,
  COUNT(*) as count,
  GROUP_CONCAT(DISTINCT tool_name) as tools
FROM read_duck_hunt_log('logs/**/*.log', 'auto')
WHERE root_cause_category IS NOT NULL
GROUP BY root_cause_category
ORDER BY count DESC;
```

### Compilation Error Hotspots

```sql
-- Find files with most errors
SELECT file_path, COUNT(*) as error_count
FROM read_duck_hunt_log('build.log', 'cmake_build')
WHERE status = 'ERROR' AND category = 'compilation'
GROUP BY file_path
ORDER BY error_count DESC
LIMIT 10;
```

## Linting Analysis

### Rule Violation Summary

```sql
-- ESLint rule violations
SELECT error_code as rule, COUNT(*) as violations, severity
FROM read_duck_hunt_log('eslint.json', 'eslint_json')
GROUP BY error_code, severity
ORDER BY violations DESC;
```

### Cross-Linter Report

```sql
-- Aggregate issues from multiple linters
SELECT
  tool_name,
  severity,
  COUNT(*) as issues
FROM read_duck_hunt_log('lint_outputs/*', 'auto')
WHERE event_type = 'LINT_ISSUE'
GROUP BY tool_name, severity
ORDER BY tool_name, severity;
```

## CI/CD Workflow Analysis

### GitHub Actions Failures

```sql
-- Find failed steps in workflows
SELECT
  workflow_name,
  job_name,
  step_name,
  message
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE step_status = 'failure'
ORDER BY event_id;
```

### Workflow Duration Analysis

```sql
-- Job duration statistics
SELECT
  job_name,
  COUNT(*) as runs,
  ROUND(AVG(duration), 2) as avg_duration,
  ROUND(MAX(duration), 2) as max_duration
FROM read_duck_hunt_workflow_log('ci_logs/*.log', 'auto')
WHERE hierarchy_level = 1 AND job_status IS NOT NULL
GROUP BY job_name
ORDER BY avg_duration DESC;
```

### Cross-CI Comparison

```sql
-- Compare workflow systems
SELECT
  workflow_type,
  COUNT(DISTINCT workflow_run_id) as runs,
  SUM(CASE WHEN workflow_status = 'failure' THEN 1 ELSE 0 END) as failures,
  ROUND(AVG(duration), 2) as avg_duration
FROM read_duck_hunt_workflow_log('**/*.log', 'auto')
WHERE hierarchy_level = 0
GROUP BY workflow_type;
```

## Memory & Debugging

### Valgrind Analysis

```sql
-- Memory leak summary
SELECT
  category,
  COUNT(*) as occurrences,
  GROUP_CONCAT(DISTINCT file_path) as files
FROM read_duck_hunt_log('valgrind.txt', 'valgrind')
WHERE event_type = 'MEMORY_LEAK'
GROUP BY category;
```

### Crash Analysis

```sql
-- Extract crash information
SELECT
  file_path,
  function_name,
  error_code as signal,
  message
FROM read_duck_hunt_log('gdb_output.txt', 'gdb_lldb')
WHERE event_type = 'CRASH_SIGNAL';
```

## Status Badges

### Build Summary Dashboard

```sql
-- Generate status badges for each tool
SELECT
  tool_name,
  status_badge(
    COUNT(CASE WHEN status = 'ERROR' THEN 1 END),
    COUNT(CASE WHEN status = 'WARNING' THEN 1 END)
  ) as badge,
  COUNT(*) as total_events
FROM read_duck_hunt_log('build.log', 'auto')
GROUP BY tool_name;
```

### Overall Build Health

```sql
SELECT status_badge(
  (SELECT COUNT(*) FROM read_duck_hunt_log('build.log', 'auto') WHERE status = 'ERROR'),
  (SELECT COUNT(*) FROM read_duck_hunt_log('build.log', 'auto') WHERE status = 'WARNING')
) as build_health;
```

## Dynamic Regexp Examples

### Custom Log Format

```sql
-- Parse custom application logs
SELECT severity, category, message
FROM parse_duck_hunt_log(
  'myapp.ERROR.database: Connection timeout after 30s
   myapp.WARNING.cache: Cache miss for user_123
   myapp.ERROR.api: Invalid response from service',
  'regexp:myapp\.(?P<severity>ERROR|WARNING)\.(?P<category>\w+):\s+(?P<message>.+)'
);
```

### Custom CI Format

```sql
-- Parse custom CI output
SELECT error_code, message
FROM parse_duck_hunt_log(
  '[BUILD-001] Compilation failed in module core
   [TEST-002] 3 tests failed in suite auth
   [DEPLOY-001] Deployment to staging failed',
  'regexp:\[(?P<code>[A-Z]+-\d+)\]\s+(?P<message>.+)'
);
```

### Extracting Structured Data

```sql
-- Parse logs with file:line:message format
SELECT file_path, line_number, severity, message
FROM parse_duck_hunt_log(
  '/src/main.c:42:error:undefined reference
   /src/utils.c:15:warning:unused variable',
  'regexp:(?P<file>[^:]+):(?P<line>\d+):(?P<severity>\w+):(?P<message>.+)'
);
```
