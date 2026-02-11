# Supported Formats

Duck Hunt supports 107 format strings for parsing development tool outputs. Use these with `read_duck_hunt_log()` or `parse_duck_hunt_log()`.

> **See also:** [Format Maturity Levels](format-maturity.md) for test coverage and stability ratings.

## Format Groups

Instead of specifying an exact format, you can use a **format group** to try all parsers for a language or ecosystem. When a group is specified, parsers are tried in priority order until one matches.

```sql
-- Use 'python' group to auto-detect pytest, mypy, pylint, flake8, etc.
SELECT * FROM parse_duck_hunt_log(content, 'python');

-- Use 'ci' group for CI/CD outputs
SELECT * FROM parse_duck_hunt_log(content, 'ci');
```

### Language Groups

| Group | Description | Count |
|-------|-------------|-------|
| `python` | Python tools (pytest, pylint, mypy, flake8, ruff, bandit, etc.) | 19 |
| `java` | Java/JVM tools (junit, maven, gradle, spotbugs, etc.) | 11 |
| `c_cpp` | C/C++ tools (gtest, make, cmake, valgrind, etc.) | 10 |
| `javascript` | JavaScript/Node.js tools (eslint, mocha, playwright, winston, pino, etc.) | 10 |
| `ruby` | Ruby tools (rspec, rubocop, ruby_logger, rails) | 5 |
| `dotnet` | .NET tools (nunit, msbuild, serilog, nlog) | 4 |
| `rust` | Rust tools (cargo_build, clippy, cargo_test) | 3 |
| `go` | Go tools (gotest_json, gotest_text, logrus) | 3 |
| `shell` | Shell tools (shellcheck_json, shellcheck_text, strace) | 3 |
| `docker` | Docker/container tools (hadolint, docker_build) | 2 |
| `swift` | Swift tools (swiftlint) | 1 |
| `php` | PHP tools (phpstan) | 1 |
| `mobile` | Mobile platforms (android) | 1 |

### Tool-Type Groups

| Group | Description | Count |
|-------|-------------|-------|
| `lint` | Linting & static analysis tools | 33 |
| `infrastructure` | Infrastructure & DevOps tools | 17 |
| `test` | Test frameworks & runners | 15 |
| `logging` | Logging frameworks & formats | 13 |
| `security` | Security scanning & audit tools | 11 |
| `build` | Build systems & compilers | 10 |
| `cloud` | Cloud platform logs & services | 7 |
| `ci` | CI/CD systems | 7 |
| `distributed` | Distributed systems logs | 6 |
| `web` | Web server & access logs | 4 |
| `coverage` | Code coverage tools | 3 |
| `debug` | Debugging tools | 3 |

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

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `auto` | Auto-detect | - | - |
| `regexp:<pattern>` | Dynamic regex | - | - |

### Test Frameworks

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `pytest_json` | pytest (JSON) | python, test | [pytest_json_failures.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/pytest_json_failures.json) |
| `pytest_text` | pytest (text) | python, test | [pytest_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/pytest_failures.txt) |
| `gotest_json` | Go test (JSON) | go, test | [gotest_failures.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/gotest_failures.json) |
| `gotest_text` | Go test (text) | go, test | - |
| `cargo_test_json` | Cargo test (Rust) | rust, test | [cargo_test_output.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/cargo_test_output.jsonl) |
| `junit_text` | JUnit/TestNG/Surefire | java, test | - |
| `junit_xml` | JUnit XML | java, test | [junit_xml_failures.xml](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/junit_xml_failures.xml) |
| `rspec_text` | RSpec (Ruby) | ruby, test | [rspec_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/rspec_failures.txt) |
| `mocha_chai_text` | Mocha/Chai (JS) | javascript, test | [mocha_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/mocha_failures.txt) |
| `gtest_text` | Google Test (C++) | c_cpp, test | [gtest_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/gtest_failures.txt) |
| `nunit_xunit_text` | NUnit/xUnit (.NET) | dotnet, test | [nunit_xml_failures.xml](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/test_frameworks/nunit_xml_failures.xml) |
| `duckdb_test` | DuckDB test runner | c_cpp, test | - |
| `pytest_cov_text` | pytest-cov | python, coverage | - |

### Linting & Static Analysis

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `eslint_json` | ESLint (JSON) | javascript, lint | [eslint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/eslint_output.json) |
| `eslint_text` | ESLint (text) | javascript, lint | - |
| `pylint_text` | Pylint | python, lint | [pylint_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/pylint_output.txt) |
| `flake8_text` | Flake8 | python, lint | [flake8_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/flake8_output.txt) |
| `mypy_text` | MyPy | python, lint | [mypy_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/mypy_output.txt) |
| `black_text` | Black | python, lint | [black_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/python_tools/black_output.txt) |
| `bandit_json` | Bandit (security) | python, security, lint | [bandit_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/bandit_output.json) |
| `rubocop_json` | RuboCop (JSON) | ruby, lint | [rubocop_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/rubocop_output.json) |
| `rubocop_text` | RuboCop (text) | ruby, lint | - |
| `swiftlint_json` | SwiftLint | swift, lint | [swiftlint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/swiftlint_output.json) |
| `phpstan_json` | PHPStan | php, lint | [phpstan_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/phpstan_output.json) |
| `shellcheck_json` | ShellCheck (JSON) | shell, lint | [shellcheck_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/shellcheck_output.json) |
| `shellcheck_text` | ShellCheck (text) | shell, lint | - |
| `stylelint_json` | Stylelint | javascript, lint | [stylelint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/stylelint_output.json) |
| `clippy_json` | Clippy (Rust) | rust, lint | [clippy_output.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/clippy_output.jsonl) |
| `markdownlint_json` | Markdownlint | lint | [markdownlint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/markdownlint_output.json) |
| `yamllint_json` | yamllint | lint | [yamllint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/yamllint_output.json) |
| `spotbugs_json` | SpotBugs | java, lint, security | [spotbugs_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/spotbugs_output.json) |
| `ktlint_json` | ktlint | java, lint | [ktlint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/ktlint_output.json) |
| `hadolint_json` | Hadolint (JSON) | docker, infrastructure, lint | [hadolint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/hadolint_output.json) |
| `hadolint_text` | Hadolint (text) | docker, infrastructure, lint | - |
| `lintr_json` | lintr (R) | lint | [lintr_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/lintr_output.json) |
| `sqlfluff_json` | sqlfluff | lint | [sqlfluff_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/sqlfluff_output.json) |
| `clang_tidy_text` | clang-tidy | c_cpp, lint | [clang_tidy_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/linting_tools/clang_tidy_output.txt) |
| `isort_text` | isort | python, lint | [isort_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/python_tools/isort_output.txt) |
| `autopep8_text` | autopep8 | python, lint | - |
| `yapf_text` | YAPF | python, lint | - |
| `ruff_text` | Ruff | python, lint | - |
| `ruff_json` | Ruff (JSON) | python, lint | - |
| `bandit_text` | Bandit (text) | python, security, lint | - |
| `generic_lint` | Generic format | lint | - |

### Build Systems

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `make_error` | GNU Make | c_cpp, build | [make_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/make_errors.txt) |
| `cmake_build` | CMake | c_cpp, build | [cmake_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/cmake_build_errors.txt) |
| `python_build` | pip/setuptools | python, build | [pip_install_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/pip_install_errors.txt) |
| `node_build` | npm/yarn/webpack | javascript, build | [npm_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/npm_build_errors.txt) |
| `cargo_build` | Cargo (Rust) | rust, build | [cargo_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/cargo_build_errors.txt) |
| `maven_build` | Maven | java, build | [maven_build_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/maven_build_failures.txt) |
| `gradle_build` | Gradle | java, build | [gradle_build_failures.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/gradle_build_failures.txt) |
| `msbuild` | MSBuild (.NET) | dotnet, build | [msbuild_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/msbuild_errors.txt) |
| `bazel_build` | Bazel | c_cpp, java, build | [bazel_build_errors.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/build_systems/bazel_build_errors.txt) |
| `docker_build` | Docker | infrastructure, build | [docker_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/docker_logs.txt) |

### Infrastructure & Security

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `tflint_json` | tflint | infrastructure, lint | [tflint_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/tflint_output.json) |
| `kube_score_json` | kube-score | infrastructure, lint | [kube_score_output.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/kube_score_output.json) |
| `trivy_json` | Trivy (security) | infrastructure, security | [trivy_scan.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/security_tools/trivy_scan.json) |
| `tfsec_json` | tfsec (security) | infrastructure, security | [tfsec_scan.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/security_tools/tfsec_scan.json) |
| `kubernetes` | Kubernetes events | infrastructure, cloud | [kubernetes_events.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/kubernetes_events.json) |
| `vpc_flow` | VPC Flow Logs | infrastructure, cloud | [vpc_flow_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/vpc_flow_logs.txt) |
| `s3_access` | S3 Access Logs | infrastructure, cloud | [s3_access_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/s3_access_logs.txt) |
| `pf` | BSD Packet Filter | infrastructure, security | [pf_firewall.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/pf_firewall.txt) |
| `iptables` | iptables/netfilter | infrastructure, security | - |
| `cisco_asa` | Cisco ASA | infrastructure, security | - |
| `windows_event` | Windows Event Log | infrastructure, security | - |
| `auditd` | Linux auditd | infrastructure, security | - |

### Debugging & Coverage

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `valgrind` | Valgrind | c_cpp, debug | [valgrind_memcheck.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/debugging_tools/valgrind_memcheck.txt) |
| `gdb_lldb` | GDB/LLDB | c_cpp, debug | [gdb_session.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/debugging_tools/gdb_session.txt) |
| `strace` | strace | c_cpp, shell, debug | [strace_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/debugging_tools/strace_output.txt) |
| `lcov` | LCOV/gcov | c_cpp, coverage | [lcov_coverage.info](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/coverage/lcov_coverage.info) |
| `coverage_text` | Coverage.py | python, coverage | [coverage_xml.xml](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/coverage/coverage_xml.xml) |
| `pytest_cov_text` | pytest-cov | python, coverage | - |

### CI/CD Systems

> **Note:** CI/CD workflow formats use `read_duck_hunt_workflow_log()` for hierarchical parsing.
> See **[Workflow Formats](workflow-formats.md)** for complete documentation.

| Format String | Tool | Groups | Function | Sample File |
|---------------|------|--------|----------|-------------|
| `github_actions` | GitHub Actions | ci | workflow | [github_actions_workflow.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/github_actions_workflow.txt) |
| `gitlab_ci` | GitLab CI | ci | workflow | [gitlab_ci_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/gitlab_ci_output.txt) |
| `jenkins` | Jenkins | ci | workflow | [jenkins_console_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/jenkins_console_output.txt) |
| `docker_build` | Docker | infrastructure, build | workflow | [docker_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/infrastructure/docker_logs.txt) |
| `github_cli` | GitHub CLI | ci | log | - |
| `drone_ci_text` | Drone CI | ci | log | - |
| `terraform_text` | Terraform | ci, infrastructure | log | - |
| `ansible_text` | Ansible | infrastructure, ci, python | log | - |
| `spack` | Spack | build | workflow | [spack_build.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/ci_systems/spack_build.txt) |

### Cloud Logging

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `aws_cloudtrail` | AWS CloudTrail | cloud, security | [aws_cloudtrail_events.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/cloud_audit/aws_cloudtrail_events.json) |
| `gcp_cloud_logging` | GCP Cloud Logging | cloud | [gcp_cloud_logging.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/cloud_audit/gcp_cloud_logging.json) |
| `azure_activity` | Azure Activity Log | cloud | [azure_activity_log.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/cloud_audit/azure_activity_log.json) |

### Application Logging

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `python_logging` | Python logging | python, logging | [python_logging_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/python_logging_output.txt) |
| `log4j` | Log4j/Log4j2 | java, logging | [log4j_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/log4j_output.txt) |
| `logrus` | Logrus (Go) | go, logging | [logrus_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/logrus_output.txt) |
| `winston` | Winston (Node.js) | javascript, logging | [winston_logs.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/winston_logs.json) |
| `pino` | Pino (Node.js) | javascript, logging | [pino_output.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/pino_output.jsonl) |
| `bunyan` | Bunyan (Node.js) | javascript, logging | [bunyan_logs.json](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/bunyan_logs.json) |
| `serilog` | Serilog (.NET) | dotnet, logging | [serilog_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/serilog_output.txt) |
| `nlog` | NLog (.NET) | dotnet, logging | [nlog_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/nlog_output.txt) |
| `ruby_logger` | Ruby Logger | ruby, logging | [ruby_logger_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/ruby_logger_output.txt) |
| `rails_log` | Rails Log | ruby, logging, web | [rails_log_output.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/app_logging/rails_log_output.txt) |

### Web Access Logs

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `syslog` | Syslog | web, logging | [syslog_messages.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/structured_logs/syslog_messages.txt) |
| `apache_access` | Apache Access Log | web | - |
| `nginx_access` | Nginx Access Log | web | - |

### Distributed Systems

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `hdfs` | Hadoop HDFS | distributed, java | [HDFS_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/HDFS/HDFS_2k.log) |
| `spark` | Apache Spark | distributed, java | [Spark_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/Spark/Spark_2k.log) |
| `android` | Android logcat | distributed, mobile | [Android_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/Android/Android_2k.log) |
| `zookeeper` | Apache Zookeeper | distributed, java | [Zookeeper_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/Zookeeper/Zookeeper_2k.log) |
| `openstack` | OpenStack services | distributed, cloud, python | [OpenStack_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/OpenStack/OpenStack_2k.log) |
| `bgl` | Blue Gene/L | distributed | [BGL_2k.log](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/loghub/BGL/BGL_2k.log) |

### Structured Logs

| Format String | Tool | Groups | Sample File |
|---------------|------|--------|-------------|
| `jsonl` | JSON Lines | logging | [jsonl_logs.jsonl](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/structured_logs/jsonl_logs.jsonl) |
| `logfmt` | logfmt | logging | [logfmt_logs.txt](https://github.com/teaguesterling/duck_hunt/blob/main/test/samples/structured_logs/logfmt_logs.txt) |

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
SELECT ref_file, ref_line, error_code, message
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
SELECT ref_file, ref_line, severity, message
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
SELECT ref_file, ref_line, error_code, message
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
| `file`, `file_path`, `path` | ref_file | File path |
| `line`, `line_number`, `lineno` | ref_line | Line number |
| `column`, `col` | ref_column | Column number |
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
