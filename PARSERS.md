# Duck Hunt Parsers

This document lists all available log parsers in the Duck Hunt extension.

## Parser Categories

### Test Frameworks (10 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| pytest_json | `pytest_json` | test/samples/pytest.json | Yes |
| pytest | `pytest_text` | workspace/ci-logs/staging/pytest-results.txt | Yes |
| pytest_cov_text | `pytest_cov` | - | Inline |
| junit_text | `junit_text` | - | Inline |
| junit_xml | `junit_xml` | - | Yes |
| gtest_text | `gtest_text` | - | Inline |
| rspec_text | `rspec_text` | - | Inline |
| mocha_chai_text | `mocha_chai` | - | Inline |
| nunit_xunit_text | `nunit_xunit` | - | Inline |
| duckdb_test | `duckdb_test` | - | Inline |

### Linting Tools (7 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| mypy | `mypy_text` | test/samples/mypy.txt | Yes |
| flake8 | `flake8` | - | Inline |
| pylint | `pylint` | - | Inline |
| black | `black` | - | Inline |
| autopep8_text | `autopep8` | - | Inline |
| yapf_text | `yapf` | - | Inline |
| clang_tidy | `clang_tidy` | - | Inline |

### Tool Outputs - JSON Linters (20 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| eslint_json | `eslint_json` | test/samples/eslint.json | Yes |
| gotest_json | `gotest_json` | test/samples/gotest.json | Yes |
| rubocop_json | `rubocop_json` | workspace/rubocop_sample.json | Yes |
| cargo_test_json | `cargo_test_json` | workspace/cargo_test_sample.json | Yes |
| swiftlint_json | `swiftlint_json` | workspace/swiftlint_sample.json | Yes |
| phpstan_json | `phpstan_json` | workspace/phpstan_sample.json | Yes |
| shellcheck_json | `shellcheck_json` | workspace/shellcheck_sample.json | Yes |
| stylelint_json | `stylelint_json` | workspace/stylelint_sample.json | Yes |
| clippy_json | `clippy_json` | workspace/clippy_sample.json | Yes |
| markdownlint_json | `markdownlint_json` | workspace/markdownlint_sample.json | Yes |
| yamllint_json | `yamllint_json` | workspace/yamllint_sample.json | Yes |
| bandit_json | `bandit_json` | workspace/bandit_sample.json | Yes |
| spotbugs_json | `spotbugs_json` | workspace/spotbugs_sample.json | Yes |
| ktlint_json | `ktlint_json` | workspace/ktlint_sample.json | Yes |
| hadolint_json | `hadolint_json` | workspace/hadolint_sample.json | Yes |
| lintr_json | `lintr_json` | workspace/lintr_sample.json | Yes |
| sqlfluff_json | `sqlfluff_json` | workspace/sqlfluff_sample.json | Yes |
| tflint_json | `tflint_json` | workspace/tflint_sample.json | Yes |
| kubescore_json | `kube_score_json` | workspace/kube_score_sample.json | Yes |
| generic_lint | `generic_lint` | test/samples/large_build.out | Yes |
| regexp | `regexp:PATTERN` | - | Inline |

### Build Systems (9 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| make | `make_error` | test/samples/make.out | Yes |
| cmake | `cmake_build` | workspace/cmake_sample.txt | Yes |
| maven | `maven` | - | Inline |
| gradle | `gradle` | - | Inline |
| msbuild | `msbuild` | - | Inline |
| node | `node` | - | Inline |
| python | `python` | - | Inline |
| cargo | `cargo` | - | Inline |
| bazel | `bazel` | - | Inline |

### Specialized / Debugging (4 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| valgrind | `valgrind` | - | Inline |
| gdb_lldb | `gdb_lldb` | - | Inline |
| strace | `strace` | - | Yes |
| coverage | `coverage` | - | Inline |

### App Logging (10 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| python_logging | `python_log` | - | Yes |
| log4j | `log4j` | - | Yes |
| logrus | `logrus` | - | Yes |
| winston | `winston` | - | Yes |
| pino | `pino` | - | Yes |
| bunyan | `bunyan` | - | Yes |
| serilog | `serilog` | - | Yes |
| nlog | `nlog` | - | Yes |
| ruby_logger | `ruby_log` | - | Yes |
| rails_log | `rails` | - | Yes |

### Structured Logs (2 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| jsonl | `jsonl` | - | Yes |
| logfmt | `logfmt` | - | Yes |

### Web Access Logs (3 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| syslog | `syslog` | - | Yes |
| apache_access | `apache_access` | - | Yes |
| nginx_access | `nginx_access` | - | Yes |

### Cloud Logs (3 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| aws_cloudtrail | `aws_cloudtrail` | - | Yes |
| gcp_cloud_logging | `gcp_cloud_logging` | - | Yes |
| azure_activity | `azure_activity` | - | Yes |

### Infrastructure (8 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| kubernetes | `kubernetes` | - | Yes |
| iptables | `iptables` | - | Yes |
| pf | `pf` | - | Yes |
| cisco_asa | `cisco_asa` | - | Yes |
| vpc_flow | `vpc_flow` | - | Yes |
| windows_event | `windows_event` | - | Yes |
| auditd | `auditd` | - | Yes |
| s3_access | `s3_access` | - | Yes |

### Infrastructure Tools (1 parser)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| ansible_text | `ansible` | workspace/ansible_sample.txt | Yes |

### CI Systems (3 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| github_cli | `github_cli` | - | Inline |
| drone_ci_text | `drone_ci` | - | Inline |
| terraform_text | `terraform` | - | Inline |

### Workflow Engines (4 parsers)

| Parser | Format Name | Sample File | Tests |
|--------|-------------|-------------|-------|
| github_actions | `github_actions` | test/samples/github_actions.log | Yes |
| gitlab_ci | `gitlab_ci` | - | Inline |
| jenkins | `jenkins` | - | Inline |
| docker | `docker` | - | Inline |

## Total: 85 parsers

## Usage

```sql
-- Auto-detect format
SELECT * FROM read_duck_hunt_log('path/to/logfile.txt', 'auto');

-- Explicit format
SELECT * FROM read_duck_hunt_log('path/to/logfile.txt', 'pytest_json');

-- Parse inline content
SELECT * FROM parse_duck_hunt_log('{"Action":"run",...}', 'gotest_json');

-- Regexp parser with custom pattern
SELECT * FROM read_duck_hunt_log('path/to/logfile.txt', 'regexp:ERROR: (.*)');
```

## Adding New Parsers

Parsers are implemented in `src/parsers/<category>/<name>_parser.cpp` and registered
in the corresponding `init.cpp` file. See existing parsers for examples.
