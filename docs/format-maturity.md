# Format Maturity Levels

This document describes the maturity levels for formats used with `read_duck_hunt_log()` and `parse_duck_hunt_log()`. Maturity is determined by test coverage and the presence of sample files.

> **Note:** CI/CD workflow formats (`github_actions`, `gitlab_ci`, `jenkins`, `docker_build`, `spack`) use a separate function `read_duck_hunt_workflow_log()` with hierarchical parsing. See [Field Mappings - CI/CD Workflows](field_mappings.md#cicd-workflows) for workflow format documentation.

## Maturity Ratings

| Rating | Level | Criteria |
|--------|-------|----------|
| ⭐⭐⭐⭐⭐ | **Production** | 10+ tests with sample files |
| ⭐⭐⭐⭐ | **Stable** | 6+ tests with sample files |
| ⭐⭐⭐ | **Beta** | 4+ tests, or 2+ tests with samples |
| ⭐⭐ | **Alpha** | 1-3 tests, limited coverage |
| ⭐ | **Experimental** | No tests, sample files only |

## Summary

| Maturity | Count | Percentage |
|----------|-------|------------|
| Production ⭐⭐⭐⭐⭐ | 22 | 24% |
| Stable ⭐⭐⭐⭐ | 28 | 31% |
| Beta ⭐⭐⭐ | 43 | 43% |
| Alpha ⭐⭐ | 1 | 1% |
| Experimental ⭐ | 1 | 1% |

**Total: 100 log formats** (plus 5 workflow formats = 105 total)

---

## Production Ready ⭐⭐⭐⭐⭐

These formats have extensive test coverage and are recommended for production use.

| Format | Tests | Sample | Description |
|--------|-------|--------|-------------|
| `autopep8_text` | 12 | - | autopep8 Python formatter |
| `aws_cloudtrail` | 13 | ✓ | AWS CloudTrail |
| `azure_activity` | 13 | ✓ | Azure Activity Log |
| `bunyan` | 10 | ✓ | Bunyan (Node.js) logging |
| `coverage_text` | 15 | ✓ | Coverage.py text output |
| `gcp_cloud_logging` | 13 | ✓ | GCP Cloud Logging |
| `gotest_json` | 10 | ✓ | Go test JSON output |
| `jsonl` | 15 | ✓ | JSON Lines |
| `junit_text` | 12 | - | JUnit text output |
| `lcov` | 16 | ✓ | LCOV/gcov code coverage |
| `log4j` | 12 | ✓ | Log4j/Log4j2 (Java) logging |
| `logfmt` | 12 | ✓ | logfmt structured logs |
| `logrus` | 16 | ✓ | Logrus (Go) logging |
| `nlog` | 14 | ✓ | NLog (.NET) logging |
| `nunit_xunit_text` | 18 | ✓ | NUnit/xUnit text output |
| `pytest_cov_text` | 13 | - | pytest-cov coverage output |
| `rails_log` | 14 | ✓ | Rails application logs |
| `s3_access` | 10 | ✓ | S3 Access Logs |
| `serilog` | 11 | ✓ | Serilog (.NET) logging |
| `strace` | 11 | ✓ | strace system call tracing |
| `syslog` | 10 | ✓ | Syslog format |
| `yapf_text` | 14 | - | YAPF Python formatter |

## Stable ⭐⭐⭐⭐

These formats have good test coverage and are reliable for most use cases.

| Format | Tests | Sample | Description |
|--------|-------|--------|-------------|
| `bandit_json` | 8 | ✓ | Bandit Python security linter |
| `cargo_test_json` | 7 | ✓ | Cargo test (Rust) JSON output |
| `clippy_json` | 7 | ✓ | Clippy (Rust) linter |
| `cmake_build` | 9 | ✓ | CMake build output |
| `eslint_json` | 7 | ✓ | ESLint JavaScript linter |
| `hadolint_json` | 7 | ✓ | Hadolint Dockerfile linter |
| `junit_xml` | 9 | ✓ | JUnit XML test results |
| `ktlint_json` | 7 | ✓ | ktlint Kotlin linter |
| `kube_score_json` | 9 | ✓ | kube-score Kubernetes analyzer |
| `kubernetes` | 7 | ✓ | Kubernetes event logs |
| `lintr_json` | 7 | ✓ | lintr R linter |
| `make_error` | 9 | ✓ | GNU Make build errors |
| `markdownlint_json` | 7 | ✓ | Markdownlint linter |
| `pf` | 9 | ✓ | BSD Packet Filter logs |
| `phpstan_json` | 7 | ✓ | PHPStan PHP analyzer |
| `pino` | 7 | ✓ | Pino (Node.js) logging |
| `python_logging` | 8 | ✓ | Python logging module |
| `rubocop_json` | 8 | ✓ | RuboCop Ruby linter |
| `ruby_logger` | 7 | ✓ | Ruby Logger |
| `shellcheck_json` | 8 | ✓ | ShellCheck shell linter |
| `spotbugs_json` | 7 | ✓ | SpotBugs Java analyzer |
| `sqlfluff_json` | 7 | ✓ | sqlfluff SQL linter |
| `stylelint_json` | 7 | ✓ | Stylelint CSS linter |
| `swiftlint_json` | 7 | ✓ | SwiftLint Swift linter |
| `tflint_json` | 8 | ✓ | tflint Terraform linter |
| `vpc_flow` | 6 | ✓ | AWS VPC Flow Logs |
| `yamllint_json` | 7 | ✓ | yamllint YAML linter |
| `isort_text` | 9 | ✓ | isort Python import sorter |

## Beta ⭐⭐⭐

These formats have moderate test coverage and work for common scenarios.

| Format | Tests | Sample | Description |
|--------|-------|--------|-------------|
| `apache_access` | 10 | - | Apache access logs |
| `auditd` | 7 | - | Linux auditd logs |
| `bazel_build` | 4 | ✓ | Bazel build output |
| `black_text` | 4 | ✓ | Black Python formatter |
| `cargo_build` | 5 | ✓ | Cargo build (Rust) |
| `cisco_asa` | 4 | - | Cisco ASA firewall logs |
| `clang_tidy_text` | 4 | ✓ | clang-tidy C++ linter |
| `flake8_text` | 4 | ✓ | Flake8 Python linter |
| `gdb_lldb` | 4 | ✓ | GDB/LLDB debugger |
| `generic_lint` | 15 | - | Generic lint format |
| `gradle_build` | 4 | ✓ | Gradle build output |
| `gtest_text` | 4 | ✓ | Google Test (C++) |
| `iptables` | 5 | - | iptables/netfilter logs |
| `maven_build` | 4 | ✓ | Maven build output |
| `mocha_chai_text` | 4 | ✓ | Mocha/Chai (JavaScript) |
| `msbuild` | 4 | ✓ | MSBuild (.NET) |
| `mypy_text` | 3 | ✓ | MyPy type checker |
| `nginx_access` | 6 | - | Nginx access logs |
| `node_build` | 4 | ✓ | npm/yarn build errors |
| `pylint_text` | 4 | ✓ | Pylint Python linter |
| `pytest_json` | 2 | ✓ | pytest JSON output |
| `pytest_text` | 5 | ✓ | pytest text output |
| `rspec_text` | 4 | ✓ | RSpec (Ruby) |
| `tfsec_json` | 4 | ✓ | tfsec Terraform security |
| `trivy_json` | 4 | ✓ | Trivy vulnerability scanner |
| `valgrind` | 4 | ✓ | Valgrind memory analyzer |
| `windows_event` | 4 | - | Windows Event Log |
| `winston` | 3 | ✓ | Winston (Node.js) logging |
| `hdfs` | 2 | ✓ | Hadoop HDFS logs |
| `spark` | 2 | ✓ | Apache Spark logs |
| `android` | 2 | ✓ | Android logcat |
| `zookeeper` | 2 | ✓ | Apache Zookeeper logs |
| `openstack` | 2 | ✓ | OpenStack service logs |
| `bgl` | 2 | ✓ | Blue Gene/L supercomputer logs |
| `ansible_text` | 7 | - | Ansible playbook output |
| `drone_ci_text` | 6 | - | Drone CI logs |
| `github_cli` | 4 | - | GitHub CLI output |
| `terraform_text` | 7 | - | Terraform output |
| `eslint_text` | - | - | ESLint JavaScript/TypeScript linter (text) |
| `rubocop_text` | - | - | RuboCop Ruby linter (text) |
| `shellcheck_text` | - | - | ShellCheck shell script linter (text) |
| `hadolint_text` | - | - | Hadolint Dockerfile linter (text) |
| `gotest_text` | - | - | Go test (text) |

## Alpha ⭐⭐

These formats have limited test coverage. Use with caution.

| Format | Tests | Sample | Description |
|--------|-------|--------|-------------|
| `python_build` | 3 | ✓ | pip/setuptools errors |

## Experimental ⭐

These formats have sample files but no dedicated tests. They may work but are not validated.

| Format | Sample | Description |
|--------|--------|-------------|
| `duckdb_test` | - | DuckDB test runner |

---

## Workflow Formats

The following formats use `read_duck_hunt_workflow_log()` for hierarchical CI/CD parsing:

| Format | Description |
|--------|-------------|
| `github_actions` | GitHub Actions workflow logs |
| `gitlab_ci` | GitLab CI pipeline logs |
| `jenkins` | Jenkins build console output |
| `docker_build` | Docker build output |
| `spack` | Spack package manager build logs |

These formats parse workflow structure (scope → group → unit → subunit) rather than individual validation events. See [Field Mappings - CI/CD Workflows](field_mappings.md#cicd-workflows) for details.

---

## How to Improve Coverage

To move a format to a higher maturity level:

1. **Add sample files** - Create realistic sample output in `test/samples/`
2. **Add file-based tests** - Test parsing with the sample files
3. **Test edge cases** - Empty input, malformed data, large files
4. **Test all fields** - Verify all schema fields are populated correctly

See [CONTRIBUTING.md](../CONTRIBUTING.md) for test guidelines.

## Known Parser Issues

No known parser issues at this time. Previously tracked issues have been resolved:

- ~~**Issue #25**: Parser bugs for shellcheck (integer codes), rubocop (correctable field), bandit (CWE extraction)~~ - Fixed
