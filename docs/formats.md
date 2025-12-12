# Supported Formats

Duck Hunt supports 60+ format strings for parsing development tool outputs. Use these with `read_duck_hunt_log()` or `parse_duck_hunt_log()`.

## Quick Reference

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `auto` | Auto-detect | - |
| `regexp:<pattern>` | Dynamic regex | - |

### Test Frameworks

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `pytest_json` | pytest (JSON) | [pytest.json](../test/samples/pytest.json) |
| `pytest_text` | pytest (text) | - |
| `gotest_json` | Go test | [gotest.json](../test/samples/gotest.json) |
| `cargo_test_json` | Cargo test (Rust) | - |
| `junit_text` | JUnit/TestNG/Surefire | - |
| `rspec_text` | RSpec (Ruby) | - |
| `mocha_chai_text` | Mocha/Chai (JS) | - |
| `gtest_text` | Google Test (C++) | - |
| `nunit_xunit_text` | NUnit/xUnit (.NET) | - |
| `duckdb_test` | DuckDB test runner | - |

### Linting & Static Analysis

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `eslint_json` | ESLint | [eslint.json](../test/samples/eslint.json) |
| `pylint_text` | Pylint | - |
| `flake8_text` | Flake8 | - |
| `mypy_text` | MyPy | [mypy.txt](../test/samples/mypy.txt) |
| `black_text` | Black | - |
| `bandit_json` | Bandit (JSON) | - |
| `bandit_text` | Bandit (text) | - |
| `rubocop_json` | RuboCop | - |
| `swiftlint_json` | SwiftLint | - |
| `phpstan_json` | PHPStan | - |
| `shellcheck_json` | Shellcheck | - |
| `stylelint_json` | Stylelint | - |
| `clippy_json` | Clippy (Rust) | - |
| `markdownlint_json` | Markdownlint | - |
| `yamllint_json` | yamllint | - |
| `spotbugs_json` | SpotBugs | - |
| `ktlint_json` | ktlint | - |
| `hadolint_json` | Hadolint | - |
| `lintr_json` | lintr (R) | - |
| `sqlfluff_json` | sqlfluff | - |
| `tflint_json` | tflint | - |
| `isort_text` | isort | - |
| `autopep8_text` | autopep8 | - |
| `yapf_text` | YAPF | - |
| `clang_tidy_text` | clang-tidy | - |
| `kube_score_json` | kube-score | - |
| `generic_lint` | Generic format | - |

### Build Systems

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `make_error` | GNU Make | [make.out](../test/samples/make.out) |
| `cmake_build` | CMake | - |
| `python_build` | pip/setuptools | - |
| `node_build` | npm/yarn/webpack | - |
| `cargo_build` | Cargo (Rust) | - |
| `maven_build` | Maven | - |
| `gradle_build` | Gradle | - |
| `msbuild` | MSBuild (.NET) | - |
| `bazel_build` | Bazel | - |
| `docker_build` | Docker | - |

### Debugging & Coverage

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `valgrind` | Valgrind | - |
| `gdb_lldb` | GDB/LLDB | - |
| `coverage_text` | Coverage.py | - |
| `pytest_cov_text` | pytest-cov | - |

### CI/CD & Infrastructure

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `github_actions_text` | GitHub Actions | [github_actions.log](../test/samples/github_actions.log) |
| `github_cli` | GitHub CLI | - |
| `gitlab_ci_text` | GitLab CI | - |
| `jenkins_text` | Jenkins | - |
| `drone_ci_text` | Drone CI | - |
| `terraform_text` | Terraform | - |
| `ansible_text` | Ansible | - |

---

## Format Details & Examples

### pytest_json

Python pytest framework JSON output (use `pytest --json-report`).

**Sample Input:**
```json
{
  "tests": [
    {"nodeid": "test_auth.py::test_login", "outcome": "passed", "duration": 0.123},
    {"nodeid": "test_auth.py::test_logout", "outcome": "failed", "duration": 0.456}
  ]
}
```

**Query:**
```sql
SELECT test_name, status, execution_time
FROM read_duck_hunt_log('test/samples/pytest.json', 'pytest_json')
WHERE status = 'FAIL';
```

---

### eslint_json

ESLint JavaScript/TypeScript linter JSON output (use `eslint --format json`).

**Sample Input:**
```json
[{
  "filePath": "/src/Button.tsx",
  "messages": [
    {"ruleId": "no-unused-vars", "severity": 2, "message": "Unused var", "line": 1, "column": 10}
  ]
}]
```

**Query:**
```sql
SELECT file_path, line_number, error_code, message
FROM read_duck_hunt_log('test/samples/eslint.json', 'eslint_json')
WHERE severity = 'error';
```

---

### make_error

GNU Make build output with GCC/Clang compiler errors and warnings.

**Sample Input:**
```
src/main.c:15:5: error: 'undefined_var' undeclared
src/main.c:28:12: warning: unused variable 'temp'
make: *** [Makefile:23: build] Error 1
```

**Query:**
```sql
SELECT file_path, line_number, severity, message
FROM read_duck_hunt_log('test/samples/make.out', 'make_error')
WHERE status = 'ERROR';
```

---

### mypy_text

MyPy Python type checker text output.

**Sample Input:**
```
src/api.py:23: error: Argument 1 has incompatible type "str"; expected "int"
src/api.py:45: error: "None" has no attribute "items"
Found 2 errors in 1 file
```

**Query:**
```sql
SELECT file_path, line_number, error_code, message
FROM read_duck_hunt_log('test/samples/mypy.txt', 'mypy_text');
```

---

### gotest_json

Go test framework JSON output (use `go test -json`).

**Sample Input:**
```json
{"Time":"2024-01-15T10:30:00Z","Action":"pass","Package":"app","Test":"TestCreate","Elapsed":0.5}
{"Time":"2024-01-15T10:30:01Z","Action":"fail","Package":"app","Test":"TestDelete","Elapsed":0.3}
```

**Query:**
```sql
SELECT test_name, status, execution_time
FROM read_duck_hunt_log('test/samples/gotest.json', 'gotest_json');
```

---

### github_actions_text

GitHub Actions workflow log output.

**Sample Input:**
```
2024-01-15T10:00:00Z ##[group]Run npm test
2024-01-15T10:00:05Z FAIL src/test.js
2024-01-15T10:00:06Z ##[error]Process completed with exit code 1.
2024-01-15T10:00:06Z ##[endgroup]
```

**Query:**
```sql
SELECT step_name, message, step_status
FROM read_duck_hunt_workflow_log('test/samples/github_actions.log', 'github_actions')
WHERE step_status = 'failure';
```

---

## Dynamic Regexp Parser

Parse any log format using custom regex patterns with named capture groups.

### Named Group Mappings

| Named Group | Maps To | Description |
|-------------|---------|-------------|
| `severity`, `level` | status, severity | ERROR, WARNING, INFO |
| `message`, `msg` | message | Error/warning message |
| `file`, `file_path`, `path` | file_path | File path |
| `line`, `line_number`, `lineno` | line_number | Line number |
| `column`, `col` | column_number | Column number |
| `code`, `error_code`, `rule` | error_code | Error code/rule ID |
| `category`, `type` | category | Category |
| `test_name`, `test`, `name` | test_name | Test name |
| `suggestion`, `fix`, `hint` | suggestion | Suggested fix |
| `tool`, `tool_name` | tool_name | Tool name |

### Examples

**Custom application logs:**
```sql
SELECT severity, message
FROM parse_duck_hunt_log(
    'myapp.ERROR.db: Connection failed
     myapp.WARNING.cache: Cache miss',
    'regexp:myapp\.(?P<severity>ERROR|WARNING)\.(?P<category>\w+):\s+(?P<message>.+)'
);
```

**Custom error format:**
```sql
SELECT error_code, message
FROM parse_duck_hunt_log(
    '[E001] Missing dependency
     [W002] Deprecated API',
    'regexp:\[(?P<code>[EW]\d+)\]\s+(?P<message>.+)'
);
```

### Notes

- Supports Python-style `(?P<name>...)` and ECMAScript-style `(?<name>...)`
- Lines not matching the pattern are skipped
- If no matches found, returns a single summary event
- Multi-file glob patterns not supported with regexp (use single files)
