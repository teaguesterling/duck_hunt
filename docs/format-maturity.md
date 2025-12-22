# Format Maturity Levels

This document describes the maturity levels for each supported format in Duck Hunt. Maturity is determined by test coverage and the presence of sample files.

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
| Production ⭐⭐⭐⭐⭐ | 9 | 11% |
| Stable ⭐⭐⭐⭐ | 28 | 35% |
| Beta ⭐⭐⭐ | 10 | 13% |
| Alpha ⭐⭐ | 15 | 19% |
| Experimental ⭐ | 18 | 22% |

---

## Production Ready ⭐⭐⭐⭐⭐

These formats have extensive test coverage and are recommended for production use.

| Format | Tests | Sample | Description |
|--------|-------|--------|-------------|
| `logfmt` | 12 | ✓ | logfmt structured logs |
| `logrus` | 16 | ✓ | Logrus (Go) logging |
| `nlog` | 14 | ✓ | NLog (.NET) logging |
| `rails_log` | 14 | ✓ | Rails application logs |
| `serilog` | 11 | ✓ | Serilog (.NET) logging |
| `strace` | 11 | ✓ | strace system call tracing |
| `syslog` | 10 | ✓ | Syslog format |

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

## Beta ⭐⭐⭐

These formats have moderate test coverage and work for common scenarios.

| Format | Tests | Sample | Description |
|--------|-------|--------|-------------|
| `apache_access` | 10 | - | Apache access logs |
| `auditd` | 7 | - | Linux auditd logs |
| `cisco_asa` | 4 | - | Cisco ASA firewall logs |
| `generic_lint` | 15 | - | Generic lint format |
| `iptables` | 5 | - | iptables/netfilter logs |
| `mypy_text` | 3 | ✓ | MyPy type checker |
| `nginx_access` | 6 | - | Nginx access logs |
| `pytest_json` | 2 | ✓ | pytest JSON output |
| `pytest_text` | 5 | ✓ | pytest text output |
| `windows_event` | 4 | - | Windows Event Log |
| `winston` | 3 | ✓ | Winston (Node.js) logging |

## Alpha ⭐⭐

These formats have limited test coverage. Use with caution.

| Format | Tests | Sample | Description |
|--------|-------|--------|-------------|
| `aws_cloudtrail` | 1 | ✓ | AWS CloudTrail |
| `azure_activity` | 1 | ✓ | Azure Activity Log |
| `bunyan` | 1 | ✓ | Bunyan (Node.js) logging |
| `gcp_cloud_logging` | 1 | ✓ | GCP Cloud Logging |
| `gotest_json` | 1 | ✓ | Go test JSON output |
| `jsonl` | 1 | ✓ | JSON Lines |

## Experimental ⭐

These formats have sample files but no dedicated tests. They may work but are not validated.

| Format | Sample | Description |
|--------|--------|-------------|
| `bazel_build` | ✓ | Bazel build output |
| `black_text` | ✓ | Black Python formatter |
| `cargo_build` | ✓ | Cargo build (Rust) |
| `clang_tidy_text` | ✓ | clang-tidy C++ linter |
| `flake8_text` | ✓ | Flake8 Python linter |
| `gdb_lldb` | ✓ | GDB/LLDB debugger |
| `github_actions_text` | ✓ | GitHub Actions logs |
| `gitlab_ci_text` | ✓ | GitLab CI logs |
| `gradle_build` | ✓ | Gradle build output |
| `gtest_text` | ✓ | Google Test (C++) |
| `isort_text` | ✓ | isort Python import sorter |
| `jenkins_text` | ✓ | Jenkins console output |
| `maven_build` | ✓ | Maven build output |
| `mocha_chai_text` | ✓ | Mocha/Chai (JavaScript) |
| `msbuild` | ✓ | MSBuild (.NET) |
| `node_build` | ✓ | npm/yarn build errors |
| `pylint_text` | ✓ | Pylint Python linter |
| `python_build` | ✓ | pip/setuptools errors |
| `rspec_text` | ✓ | RSpec (Ruby) |
| `s3_access` | ✓ | S3 Access Logs |
| `tfsec_json` | ✓ | tfsec Terraform security |
| `trivy_json` | ✓ | Trivy vulnerability scanner |
| `valgrind` | ✓ | Valgrind memory analyzer |

---

## How to Improve Coverage

To move a format to a higher maturity level:

1. **Add sample files** - Create realistic sample output in `test/samples/`
2. **Add file-based tests** - Test parsing with the sample files
3. **Test edge cases** - Empty input, malformed data, large files
4. **Test all fields** - Verify all schema fields are populated correctly

See [CONTRIBUTING.md](../CONTRIBUTING.md) for test guidelines.

## Known Parser Issues

Some parsers have known issues tracked in GitHub:

- **Issue #25**: Parser bugs for shellcheck (integer codes), rubocop (correctable field), bandit (CWE extraction)

These issues have corresponding "BLOCKED" tests in the test suite that document the expected behavior.
