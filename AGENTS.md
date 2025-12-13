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
make 2>&1 | duckdb -s "LOAD duck_hunt; FROM read_duck_hunt_log('/dev/stdin', 'auto')"
```

### Parse string content directly
```sql
SELECT * FROM parse_duck_hunt_log('...paste content here...', 'auto');
```

### Read from file
```sql
SELECT * FROM read_duck_hunt_log('build.log', 'auto');
```

## Quick Diagnosis Recipes

### Instant build status
```bash
make 2>&1 | duckdb -markdown -s "
LOAD duck_hunt;
SELECT status, COUNT(*) as count
FROM read_duck_hunt_log('/dev/stdin', 'auto')
GROUP BY status ORDER BY count DESC;
"
```

### Show only errors with context
```bash
make 2>&1 | duckdb -markdown -s "
LOAD duck_hunt;
SELECT file_path, line_number, message
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE status IN ('FAIL', 'ERROR')
ORDER BY file_path, line_number;
"
```

### Find most problematic files
```bash
cat build.log | duckdb -markdown -s "
LOAD duck_hunt;
SELECT file_path, COUNT(*) as errors
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE severity = 'error'
GROUP BY file_path ORDER BY errors DESC LIMIT 10;
"
```

## Common Workflows

### 1. Analyze Local Build Failures
```bash
# Stream make output and show errors
make 2>&1 | duckdb -markdown -s "
LOAD duck_hunt;
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE status IN ('FAIL', 'ERROR')
"

# Capture to parquet for repeated analysis
make 2>&1 | duckdb -s "
LOAD duck_hunt;
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
LOAD duck_hunt;
FROM read_duck_hunt_log('/dev/stdin', 'github_cli')
"

# List recent failed runs, then analyze one
gh run list --status=failure --limit=5
gh run view <run-id> --log | duckdb -s "
LOAD duck_hunt;
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE status = 'FAIL'
"
```

### 3. Analyze Test Results
```bash
# pytest with JSON output (most detailed)
pytest --tb=short -q --json-report --json-report-file=- 2>/dev/null | \
  duckdb -markdown -s "LOAD duck_hunt; FROM read_duck_hunt_log('/dev/stdin', 'pytest_json')"

# pytest text output
pytest 2>&1 | duckdb -markdown -s "
LOAD duck_hunt;
FROM read_duck_hunt_log('/dev/stdin', 'pytest_text')
WHERE status = 'FAIL'
"

# go test JSON output
go test -json ./... 2>&1 | duckdb -markdown -s "
LOAD duck_hunt;
FROM read_duck_hunt_log('/dev/stdin', 'gotest_json')
WHERE status = 'FAIL'
"

# JUnit XML (common CI artifact) - requires webbed extension
duckdb -s "
INSTALL webbed FROM community; LOAD webbed; LOAD duck_hunt;
FROM read_duck_hunt_log('test-results.xml', 'junit_xml')
"
```

### 4. Analyze Linter Output
```bash
# ESLint JSON
eslint . -f json | duckdb -markdown -s "
LOAD duck_hunt;
FROM read_duck_hunt_log('/dev/stdin', 'eslint_json')
ORDER BY severity DESC, file_path
"

# pylint/flake8/mypy text output
pylint src/ 2>&1 | duckdb -markdown -s "
LOAD duck_hunt;
FROM read_duck_hunt_log('/dev/stdin', 'pylint_text')
"
```

### 5. Save Analysis for Later
```bash
# Save parsed results to parquet
make 2>&1 | duckdb -s "
LOAD duck_hunt;
COPY (
  SELECT *, current_timestamp as analyzed_at
  FROM read_duck_hunt_log('/dev/stdin', 'auto')
) TO 'build_$(date +%Y%m%d_%H%M%S).parquet';
"

# Compare two builds
duckdb -markdown -s "
SELECT 'before' as build, status, COUNT(*) FROM 'build_before.parquet' GROUP BY status
UNION ALL
SELECT 'after' as build, status, COUNT(*) FROM 'build_after.parquet' GROUP BY status
ORDER BY build, status;
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

### Locate error in original log
```sql
SELECT log_line_start, log_line_end, message
FROM read_duck_hunt_log('/dev/stdin', 'auto')
WHERE status = 'ERROR';
```

## All Supported Formats

Use `'auto'` for automatic detection, or specify explicitly for faster parsing.

### Test Frameworks
| Format | Description |
|--------|-------------|
| `pytest_json` | pytest --json-report output |
| `pytest_text` | pytest terminal output |
| `gotest_json` | go test -json output |
| `junit_xml` | JUnit XML (requires webbed extension) |
| `junit_text` | JUnit text output |
| `gtest_text` | Google Test output |
| `rspec_text` | RSpec output |
| `mocha_chai_text` | Mocha/Chai output |
| `nunit_xunit_text` | NUnit/xUnit text output |
| `nunit_xml` | NUnit XML (requires webbed extension) |
| `cargo_test_json` | Rust cargo test JSON |
| `duckdb_test` | DuckDB test runner output |

### Build Systems
| Format | Description |
|--------|-------------|
| `make_error` | make/gcc build output |
| `cmake_build` | CMake build output |
| `cargo_build` | Rust cargo build output |
| `maven_build` | Maven build output |
| `gradle_build` | Gradle build output |
| `msbuild` | MSBuild output |
| `node_build` | npm/yarn/webpack output |
| `python_build` | Python build/pip output |
| `bazel_build` | Bazel build output |
| `docker_build` | Docker build output |

### Linting Tools
| Format | Description |
|--------|-------------|
| `eslint_json` | ESLint JSON format |
| `pylint_text` | pylint output |
| `flake8_text` | flake8 output |
| `mypy_text` | mypy output |
| `black_text` | black formatter output |
| `clippy_json` | Rust clippy JSON |
| `shellcheck_json` | ShellCheck JSON |
| `rubocop_json` | RuboCop JSON |
| `phpstan_json` | PHPStan JSON |
| `stylelint_json` | Stylelint JSON |
| `markdownlint_json` | Markdownlint JSON |
| `yamllint_json` | yamllint JSON |
| `clang_tidy_text` | clang-tidy output |
| `hadolint_json` | Hadolint (Dockerfile) JSON |
| `ktlint_json` | ktlint (Kotlin) JSON |
| `swiftlint_json` | SwiftLint JSON |
| `lintr_json` | lintr (R) JSON |
| `sqlfluff_json` | SQLFluff JSON |
| `tflint_json` | TFLint JSON |
| `bandit_json` | Bandit security JSON |
| `bandit_text` | Bandit security text |
| `spotbugs_json` | SpotBugs JSON |

### Python Tools
| Format | Description |
|--------|-------------|
| `autopep8_text` | autopep8 output |
| `yapf_text` | yapf formatter output |
| `isort_text` | isort output |
| `coverage_text` | coverage.py output |
| `pytest_cov_text` | pytest-cov output |

### CI/CD Systems
| Format | Description |
|--------|-------------|
| `github_cli` | gh run view output |
| `github_actions_text` | GitHub Actions log format |
| `gitlab_ci_text` | GitLab CI log format |
| `jenkins_text` | Jenkins log format |
| `drone_ci_text` | Drone CI log format |

### Infrastructure Tools
| Format | Description |
|--------|-------------|
| `terraform_text` | Terraform output |
| `ansible_text` | Ansible output |
| `kube_score_json` | kube-score JSON |

### Debugging Tools
| Format | Description |
|--------|-------------|
| `valgrind` | Valgrind output |
| `gdb_lldb` | GDB/LLDB output |

### Other
| Format | Description |
|--------|-------------|
| `auto` | Automatic format detection |
| `generic_lint` | Generic file:line:col: message format |
| `checkstyle_xml` | Checkstyle XML (requires webbed extension) |

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
| `column_number` | Column number in source file |
| `message` | Error/warning message |
| `test_name` | Name of test (for test frameworks) |
| `function_name` | Function or test identifier |
| `execution_time` | Test execution time in seconds |
| `error_fingerprint` | Hash for grouping similar errors |
| `log_line_start` | Starting line in original log |
| `log_line_end` | Ending line in original log |
| `source_file` | Original log file path |

## Workflow Engine Functions

For CI/CD log analysis with job/step structure:

```sql
-- Analyze workflow structure
SELECT * FROM read_duck_hunt_workflow_log('ci.log', 'github_actions_text');

-- Parse workflow content directly
SELECT * FROM parse_duck_hunt_workflow_log('...log content...', 'github_actions_text');
```

## Tips for AI Agents

1. **Always use format parameter**: `read_duck_hunt_log(source, 'auto')` - the second parameter is required

2. **Prefer `/dev/stdin`** for piped content - it works reliably now

3. **Use `-markdown` flag** with duckdb CLI for readable output: `duckdb -markdown -s "..."`

4. **Capture to parquet** for large logs or repeated analysis - avoids re-parsing

5. **`auto` works well** but explicit formats are faster if you know the source

6. **XML formats need webbed**:
   ```sql
   INSTALL webbed FROM community; LOAD webbed;
   ```

7. **Filter early** - add WHERE clauses to focus on failures/errors:
   ```sql
   WHERE status IN ('FAIL', 'ERROR') AND severity = 'error'
   ```

8. **Use error_fingerprint** to group similar errors across files

9. **Check log_line_start/end** to locate errors in original log

## Troubleshooting

### Getting 0 rows?
- Check the format parameter - try `'auto'` or a specific format
- Verify the content matches the expected format
- For XML formats, ensure webbed extension is loaded

### Wrong format detected?
- Specify the format explicitly instead of using `'auto'`
- Check if the output has unusual formatting

### Need to see raw parsing?
```sql
SELECT * FROM read_duck_hunt_log('file.log', 'auto') LIMIT 5;
```

### Extension not loading?
```sql
INSTALL duck_hunt FROM community;
LOAD duck_hunt;
```
