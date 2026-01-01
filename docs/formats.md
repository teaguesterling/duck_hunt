# Supported Formats

Duck Hunt supports 90+ format strings for parsing development tool outputs. Use these with `read_duck_hunt_log()` or `parse_duck_hunt_log()`.

> **See also:** [Format Maturity Levels](format-maturity.md) for test coverage and stability ratings.

## Notes

### XML Format Requirements

Some XML-based formats require the optional `webbed` DuckDB extension for XML parsing:
- `junit_xml` - JUnit/NUnit/xUnit XML test reports
- `cobertura_xml` - Cobertura coverage XML reports

To use these formats, first install the webbed extension:
```sql
INSTALL webbed;
LOAD webbed;
```

If the extension is not available, use the text-based alternatives (`junit_text`, `lcov`) or convert XML to JSON before parsing.

### Format Detection

Use `duck_hunt_detect_format()` to automatically identify the format of log content:

```sql
-- Detect format from file content
SELECT duck_hunt_detect_format(content) AS detected_format
FROM read_text('build.log');

-- Returns: 'make_error', 'pytest_json', etc.
```

The function analyzes content patterns and returns the best-matching format string. Returns `NULL` if no format matches confidently. This is the same detection logic used by `format := 'auto'`.

## Quick Reference

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `auto` | Auto-detect | - |
| `regexp:<pattern>` | Dynamic regex | - |

### Test Frameworks

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `pytest_json` | pytest (JSON) | [pytest_json_failures.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/pytest_json_failures.json) |
| `pytest_text` | pytest (text) | [pytest_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/pytest_failures.txt) |
| `gotest_json` | Go test | [gotest_failures.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/gotest_failures.json) |
| `cargo_test_json` | Cargo test (Rust) | [cargo_test_output.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/cargo_test_output.jsonl) |
| `junit_text` | JUnit/TestNG/Surefire | - |
| `junit_xml` | JUnit XML | [junit_xml_failures.xml](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/junit_xml_failures.xml) |
| `rspec_text` | RSpec (Ruby) | [rspec_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/rspec_failures.txt) |
| `mocha_chai_text` | Mocha/Chai (JS) | [mocha_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/mocha_failures.txt) |
| `gtest_text` | Google Test (C++) | [gtest_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/gtest_failures.txt) |
| `nunit_xunit_text` | NUnit/xUnit (.NET) | [nunit_xml_failures.xml](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/nunit_xml_failures.xml) |
| `duckdb_test` | DuckDB test runner | - |
| `pytest_cov_text` | pytest-cov | - |

### Linting & Static Analysis

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `eslint_json` | ESLint | [eslint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/eslint_output.json) |
| `pylint_text` | Pylint | [pylint_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/pylint_output.txt) |
| `flake8_text` | Flake8 | [flake8_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/flake8_output.txt) |
| `mypy_text` | MyPy | [mypy_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/mypy_output.txt) |
| `black_text` | Black | [black_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/python_tools/black_output.txt) |
| `bandit_json` | Bandit (security) | [bandit_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/bandit_output.json) |
| `rubocop_json` | RuboCop | [rubocop_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/rubocop_output.json) |
| `swiftlint_json` | SwiftLint | [swiftlint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/swiftlint_output.json) |
| `phpstan_json` | PHPStan | [phpstan_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/phpstan_output.json) |
| `shellcheck_json` | Shellcheck | [shellcheck_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/shellcheck_output.json) |
| `stylelint_json` | Stylelint | [stylelint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/stylelint_output.json) |
| `clippy_json` | Clippy (Rust) | [clippy_output.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/clippy_output.jsonl) |
| `markdownlint_json` | Markdownlint | [markdownlint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/markdownlint_output.json) |
| `yamllint_json` | yamllint | [yamllint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/yamllint_output.json) |
| `spotbugs_json` | SpotBugs | [spotbugs_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/spotbugs_output.json) |
| `ktlint_json` | ktlint | [ktlint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/ktlint_output.json) |
| `hadolint_json` | Hadolint | [hadolint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/hadolint_output.json) |
| `lintr_json` | lintr (R) | [lintr_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/lintr_output.json) |
| `sqlfluff_json` | sqlfluff | [sqlfluff_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/sqlfluff_output.json) |
| `clang_tidy_text` | clang-tidy | [clang_tidy_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/clang_tidy_output.txt) |
| `isort_text` | isort | [isort_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/python_tools/isort_output.txt) |
| `autopep8_text` | autopep8 | - |
| `yapf_text` | YAPF | - |
| `generic_lint` | Generic format | - |

### Build Systems

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `make_error` | GNU Make | [make_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/make_errors.txt) |
| `cmake_build` | CMake | [cmake_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/cmake_build_errors.txt) |
| `python_build` | pip/setuptools | [pip_install_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/pip_install_errors.txt) |
| `node_build` | npm/yarn/webpack | [npm_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/npm_build_errors.txt) |
| `cargo_build` | Cargo (Rust) | [cargo_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/cargo_build_errors.txt) |
| `maven_build` | Maven | [maven_build_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/maven_build_failures.txt) |
| `gradle_build` | Gradle | [gradle_build_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/gradle_build_failures.txt) |
| `msbuild` | MSBuild (.NET) | [msbuild_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/msbuild_errors.txt) |
| `bazel_build` | Bazel | [bazel_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/bazel_build_errors.txt) |
| `docker_build` | Docker | [docker_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/docker_logs.txt) |

### Infrastructure & Security

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `tflint_json` | tflint | [tflint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/tflint_output.json) |
| `kube_score_json` | kube-score | [kube_score_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/kube_score_output.json) |
| `trivy_json` | Trivy (security) | [trivy_scan.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/security_tools/trivy_scan.json) |
| `tfsec_json` | tfsec (security) | [tfsec_scan.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/security_tools/tfsec_scan.json) |
| `kubernetes` | Kubernetes events | [kubernetes_events.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/kubernetes_events.json) |
| `vpc_flow` | VPC Flow Logs | [vpc_flow_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/vpc_flow_logs.txt) |
| `s3_access` | S3 Access Logs | [s3_access_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/s3_access_logs.txt) |
| `pf` | BSD Packet Filter | [pf_firewall.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/pf_firewall.txt) |
| `iptables` | iptables/netfilter | - |
| `cisco_asa` | Cisco ASA | - |
| `windows_event` | Windows Event Log | - |
| `auditd` | Linux auditd | - |

### Debugging & Coverage

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `valgrind` | Valgrind | [valgrind_memcheck.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/debugging_tools/valgrind_memcheck.txt) |
| `gdb_lldb` | GDB/LLDB | [gdb_session.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/debugging_tools/gdb_session.txt) |
| `strace` | strace | [strace_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/debugging_tools/strace_output.txt) |
| `lcov` | LCOV/gcov | [lcov_coverage.info](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/coverage/lcov_coverage.info) |
| `coverage_text` | Coverage.py | [coverage_xml.xml](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/coverage/coverage_xml.xml) |
| `pytest_cov_text` | pytest-cov | - |

### CI/CD Systems

> **Note:** CI/CD workflow formats use `read_duck_hunt_workflow_log()` for hierarchical parsing.
> See **[Workflow Formats](workflow-formats.md)** for complete documentation.

| Format String | Tool | Function | Sample File |
|---------------|------|----------|-------------|
| `github_actions` | GitHub Actions | workflow | [github_actions_workflow.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/github_actions_workflow.txt) |
| `gitlab_ci` | GitLab CI | workflow | [gitlab_ci_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/gitlab_ci_output.txt) |
| `jenkins` | Jenkins | workflow | [jenkins_console_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/jenkins_console_output.txt) |
| `docker_build` | Docker | workflow | [docker_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/docker_logs.txt) |
| `github_cli` | GitHub CLI | log | - |
| `drone_ci_text` | Drone CI | log | - |
| `terraform_text` | Terraform | log | - |
| `ansible_text` | Ansible | log | - |
| `spack` | Spack | workflow | [spack_build.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/spack_build.txt) |

### Cloud Logging

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `aws_cloudtrail` | AWS CloudTrail | [aws_cloudtrail_events.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/cloud_audit/aws_cloudtrail_events.json) |
| `gcp_cloud_logging` | GCP Cloud Logging | [gcp_cloud_logging.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/cloud_audit/gcp_cloud_logging.json) |
| `azure_activity` | Azure Activity Log | [azure_activity_log.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/cloud_audit/azure_activity_log.json) |

### Application Logging

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `python_logging` | Python logging | [python_logging_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/python_logging_output.txt) |
| `log4j` | Log4j/Log4j2 | [log4j_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/log4j_output.txt) |
| `logrus` | Logrus (Go) | [logrus_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/logrus_output.txt) |
| `winston` | Winston (Node.js) | [winston_logs.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/winston_logs.json) |
| `pino` | Pino (Node.js) | [pino_output.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/pino_output.jsonl) |
| `bunyan` | Bunyan (Node.js) | [bunyan_logs.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/bunyan_logs.json) |
| `serilog` | Serilog (.NET) | [serilog_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/serilog_output.txt) |
| `nlog` | NLog (.NET) | [nlog_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/nlog_output.txt) |
| `ruby_logger` | Ruby Logger | [ruby_logger_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/ruby_logger_output.txt) |
| `rails_log` | Rails Log | [rails_log_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/rails_log_output.txt) |

### Web Access Logs

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `syslog` | Syslog | [syslog_messages.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/structured_logs/syslog_messages.txt) |
| `apache_access` | Apache Access Log | - |
| `nginx_access` | Nginx Access Log | - |

### Distributed Systems

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `hdfs` | Hadoop HDFS | [HDFS_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/HDFS/HDFS_2k.log) |
| `spark` | Apache Spark | [Spark_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/Spark/Spark_2k.log) |
| `android` | Android logcat | [Android_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/Android/Android_2k.log) |
| `zookeeper` | Apache Zookeeper | [Zookeeper_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/Zookeeper/Zookeeper_2k.log) |
| `openstack` | OpenStack services | [OpenStack_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/OpenStack/OpenStack_2k.log) |
| `bgl` | Blue Gene/L | [BGL_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/BGL/BGL_2k.log) |

### Structured Logs

| Format String | Tool | Sample File |
|---------------|------|-------------|
| `jsonl` | JSON Lines | [jsonl_logs.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/structured_logs/jsonl_logs.jsonl) |
| `logfmt` | logfmt | [logfmt_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/structured_logs/logfmt_logs.txt) |

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
SELECT unit as step, message, unit_status
FROM read_duck_hunt_workflow_log('test/samples/github_actions.log', 'github_actions')
WHERE unit_status = 'failure';
```

See [Field Mappings](field_mappings.md#cicd-workflows) for complete CI/CD field documentation.

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
