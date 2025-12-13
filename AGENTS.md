# Duck Hunt - AI Agent Guide

This guide helps AI assistants use duck_hunt to analyze build logs, test results, and CI/CD output for users.

## When to Use Duck Hunt

Use duck_hunt when a user shares or needs to analyze:
- Build output (make, cmake, cargo, npm, maven, gradle, msbuild)
- Test results (pytest, jest, go test, JUnit, RSpec, Google Test)
- Linter output (eslint, pylint, flake8, mypy, clippy, shellcheck)
- CI/CD logs (GitHub Actions, GitLab CI, Jenkins)
- Code quality tools (coverage reports, security scanners)

## Quick Start Patterns

### Stream from stdin (recommended for piped output)
```bash
make 2>&1 | duckdb -s "FROM read_duck_hunt_log('/dev/stdin', 'auto')"
```

### Parse string content directly
```sql
SELECT * FROM parse_duck_hunt_log('...paste content here...', 'auto');
```

### Read from file
```sql
SELECT * FROM read_duck_hunt_log('build.log', 'auto');
```

## Common Workflows

### 1. Analyze Local Build Failures
```bash
# Stream make output and show errors
make 2>&1 | duckdb -markdown -s "
  FROM read_duck_hunt_log('/dev/stdin', 'auto')
  WHERE status IN ('FAIL', 'ERROR')
"

# Or capture to parquet for repeated analysis
make 2>&1 | duckdb -s "
  COPY (FROM read_duck_hunt_log('/dev/stdin', 'auto'))
  TO '.duck_hunt/build.parquet';
"
# Then query the parquet file
duckdb -s "FROM '.duck_hunt/build.parquet' WHERE severity = 'error'"
```

### 2. Analyze Remote CI Job (GitHub Actions)
```bash
# Get failed workflow run logs
gh run view <run-id> --log-failed | duckdb -markdown -s "
  FROM read_duck_hunt_log('/dev/stdin', 'github_cli')
"

# List recent failed runs, then analyze one
gh run list --status=failure --limit=5
gh run view <run-id> --log | duckdb -s "
  FROM read_duck_hunt_log('/dev/stdin', 'auto')
  WHERE status = 'FAIL'
"
```

### 3. Analyze Test Results
```bash
# pytest with JSON output (most detailed)
pytest --tb=short -q --json-report --json-report-file=- 2>/dev/null | \
  duckdb -markdown -s "FROM read_duck_hunt_log('/dev/stdin', 'pytest_json')"

# pytest text output
pytest 2>&1 | duckdb -markdown -s "
  FROM read_duck_hunt_log('/dev/stdin', 'pytest_text')
  WHERE status = 'FAIL'
"

# go test JSON output
go test -json ./... 2>&1 | duckdb -markdown -s "
  FROM read_duck_hunt_log('/dev/stdin', 'gotest_json')
  WHERE status = 'FAIL'
"

# JUnit XML (common CI artifact)
duckdb -s "FROM read_duck_hunt_log('test-results.xml', 'junit_xml')"
```

### 4. Analyze Linter Output
```bash
# ESLint JSON
eslint . -f json | duckdb -markdown -s "
  FROM read_duck_hunt_log('/dev/stdin', 'eslint_json')
  ORDER BY severity DESC, file_path
"

# pylint/flake8/mypy text output
pylint src/ 2>&1 | duckdb -markdown -s "
  FROM read_duck_hunt_log('/dev/stdin', 'pylint_text')
"
```

## Useful Queries

### Summary by status
```sql
SELECT status, COUNT(*) as count
FROM read_duck_hunt_log('/dev/stdin', 'auto')
GROUP BY status ORDER BY count DESC;
```

### Errors grouped by file
```sql
SELECT file_path, COUNT(*) as errors
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE status IN ('FAIL', 'ERROR')
GROUP BY file_path ORDER BY errors DESC;
```

### Find similar errors (using fingerprinting)
```sql
SELECT error_fingerprint, COUNT(*) as occurrences,
       FIRST(message) as example_message
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE status = 'FAIL'
GROUP BY error_fingerprint
ORDER BY occurrences DESC;
```

### Test execution time analysis
```sql
SELECT test_name, execution_time
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE execution_time IS NOT NULL
ORDER BY execution_time DESC
LIMIT 10;
```

## Supported Formats

Use `'auto'` for automatic detection, or specify explicitly:

| Format | Use For |
|--------|---------|
| `auto` | Automatic detection (recommended) |
| `pytest_json` | pytest --json-report output |
| `pytest_text` | pytest terminal output |
| `gotest_json` | go test -json output |
| `eslint_json` | ESLint JSON format |
| `junit_xml` | JUnit XML test results (requires webbed extension) |
| `make_error` | make/gcc build output |
| `cmake_build` | CMake build output |
| `cargo_build` | Rust cargo build output |
| `maven_build` | Maven build output |
| `gradle_build` | Gradle build output |
| `github_cli` | gh run view output |
| `github_actions_text` | GitHub Actions log format |
| `pylint_text` | pylint output |
| `flake8_text` | flake8 output |
| `mypy_text` | mypy output |

See full list: `SELECT DISTINCT structured_data FROM parse_duck_hunt_log('', 'auto')`

## Output Schema

Key columns returned by duck_hunt:

| Column | Description |
|--------|-------------|
| `event_id` | Unique identifier for each event |
| `tool_name` | Tool that produced the output (pytest, eslint, make, etc.) |
| `status` | PASS, FAIL, ERROR, SKIP, WARNING, INFO |
| `severity` | error, warning, info |
| `file_path` | Source file path |
| `line_number` | Line number in source file |
| `message` | Error/warning message |
| `test_name` | Name of test (for test frameworks) |
| `function_name` | Function or test identifier |
| `execution_time` | Test execution time in seconds |
| `error_fingerprint` | Hash for grouping similar errors |
| `log_line_start` | Starting line in original log |
| `log_line_end` | Ending line in original log |

## Tips for AI Agents

1. **Always use format parameter**: `read_duck_hunt_log(source, 'auto')` - the second parameter is required

2. **Prefer `/dev/stdin`** for piped content instead of temp files

3. **Use `-markdown` flag** with duckdb CLI for readable output: `duckdb -markdown -s "..."`

4. **Capture to parquet** for large logs or repeated analysis - avoids re-parsing

5. **Check for webbed extension** when parsing XML formats:
   ```sql
   INSTALL webbed FROM community; LOAD webbed;
   ```

6. **Use workflow parsers** for CI/CD logs:
   - `read_duck_hunt_workflow_log()` - for workflow-level analysis
   - `parse_duck_hunt_workflow_log()` - for string content

7. **Filter early** - add WHERE clauses to focus on failures/errors:
   ```sql
   WHERE status IN ('FAIL', 'ERROR') AND severity = 'error'
   ```
