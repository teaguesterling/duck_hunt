cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME duck_hunt)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include src)

set(EXTENSION_SOURCES 
    src/duck_hunt_extension.cpp
    src/validation_event_types.cpp
    src/read_duck_hunt_log_function.cpp
    
    # Core modular architecture
    src/core/parser_registry.cpp
    src/core/parse_content.cpp
    src/core/format_utils.cpp
    src/core/error_patterns.cpp
    src/core/webbed_integration.cpp
    src/core/zipfs_integration.cpp

    # New parser category registrations
    src/parsers/debugging/init.cpp
    src/parsers/test_frameworks/init.cpp
    src/parsers/build_systems/init.cpp
    src/parsers/linting_tools/init.cpp
    src/parsers/tool_outputs/init.cpp
    src/parsers/ci_systems/init.cpp
    src/parsers/structured_logs/init.cpp
    src/parsers/web_access/init.cpp
    src/parsers/cloud_logs/init.cpp
    src/parsers/app_logging/init.cpp
    src/parsers/infrastructure/init.cpp
    src/parsers/infrastructure_tools/init.cpp
    src/parsers/coverage/init.cpp
    src/parsers/distributed_systems/init.cpp

    # Parser implementations
    src/parsers/test_frameworks/pytest_parser.cpp
    src/parsers/test_frameworks/pytest_json_parser.cpp
    src/parsers/tool_outputs/eslint_json_parser.cpp
    src/parsers/tool_outputs/gotest_json_parser.cpp
    src/parsers/build_systems/make_parser.cpp
    src/parsers/linting_tools/mypy_parser.cpp
    src/parsers/linting_tools/flake8_parser.cpp
    
    # Phase 4: Additional Tool Parsers
    src/parsers/linting_tools/black_parser.cpp
    src/parsers/linting_tools/pylint_parser.cpp
    
    # Phase 5: JSON Parser Extraction - Batch 1
    src/parsers/tool_outputs/rubocop_json_parser.cpp
    src/parsers/tool_outputs/cargo_test_json_parser.cpp
    src/parsers/tool_outputs/swiftlint_json_parser.cpp
    
    # Phase 5: JSON Parser Extraction - Batch 2
    src/parsers/tool_outputs/phpstan_json_parser.cpp
    src/parsers/tool_outputs/shellcheck_json_parser.cpp
    src/parsers/tool_outputs/stylelint_json_parser.cpp
    
    # Phase 5: JSON Parser Extraction - Batch 3
    src/parsers/tool_outputs/clippy_json_parser.cpp
    src/parsers/tool_outputs/markdownlint_json_parser.cpp
    src/parsers/tool_outputs/yamllint_json_parser.cpp
    
    # Phase 5: JSON Parser Extraction - Batch 4 (Final JSON Batch)
    src/parsers/tool_outputs/bandit_json_parser.cpp
    src/parsers/tool_outputs/spotbugs_json_parser.cpp
    src/parsers/tool_outputs/ktlint_json_parser.cpp
    src/parsers/tool_outputs/kubescore_json_parser.cpp

    # Extracted inline JSON parsers
    src/parsers/tool_outputs/hadolint_json_parser.cpp
    src/parsers/tool_outputs/lintr_json_parser.cpp
    src/parsers/tool_outputs/sqlfluff_json_parser.cpp
    src/parsers/tool_outputs/tflint_json_parser.cpp
    src/parsers/tool_outputs/trivy_json_parser.cpp
    src/parsers/tool_outputs/tfsec_json_parser.cpp

    # Phase 6: Text Parser Extraction - Batch 1
    src/parsers/test_frameworks/junit_text_parser.cpp
    
    # Phase 6: Text Parser Extraction - Batch 2
    src/parsers/test_frameworks/rspec_text_parser.cpp
    
    # Phase 6: Text Parser Extraction - Batch 3
    src/parsers/test_frameworks/mocha_chai_text_parser.cpp
    
    # Phase 6: Text Parser Extraction - Batch 4
    src/parsers/test_frameworks/gtest_text_parser.cpp
    src/parsers/test_frameworks/nunit_xunit_text_parser.cpp

    # Phase 8: Additional parser extractions
    src/parsers/test_frameworks/duckdb_test_parser.cpp
    src/parsers/test_frameworks/pytest_cov_text_parser.cpp
    src/parsers/tool_outputs/generic_lint_parser.cpp
    src/parsers/tool_outputs/regexp_parser.cpp

    # Phase 7: Specialized Parser Extraction - Batch 1
    src/parsers/specialized/valgrind_parser.cpp
    
    # Phase 7: Specialized Parser Extraction - Batch 2
    src/parsers/specialized/gdb_lldb_parser.cpp
    
    # Phase 7: Specialized Parser Extraction - Batch 3
    src/parsers/specialized/coverage_parser.cpp

    # System tracing parsers
    src/parsers/specialized/strace_parser.cpp
    
    # Phase 8: Build System Parser Extraction - Batch 1
    src/parsers/build_systems/maven_parser.cpp
    src/parsers/build_systems/gradle_parser.cpp
    
    # Phase 8: Build System Parser Extraction - Batch 2
    src/parsers/build_systems/msbuild_parser.cpp
    src/parsers/build_systems/node_parser.cpp
    src/parsers/build_systems/python_parser.cpp
    
    # Phase 8: Build System Parser Extraction - Batch 3
    src/parsers/build_systems/cargo_parser.cpp
    src/parsers/build_systems/cmake_parser.cpp

    # Extracted inline text parsers
    src/parsers/build_systems/bazel_parser.cpp
    src/parsers/linting_tools/autopep8_text_parser.cpp

    # New parsers - CI Systems and improved linting tools
    src/parsers/ci_systems/github_cli_parser.cpp
    src/parsers/ci_systems/drone_ci_text_parser.cpp
    src/parsers/ci_systems/terraform_text_parser.cpp
    src/parsers/ci_systems/github_actions_text_parser.cpp
    src/parsers/ci_systems/gitlab_ci_text_parser.cpp
    src/parsers/ci_systems/jenkins_text_parser.cpp
    src/parsers/linting_tools/clang_tidy_parser.cpp
    src/parsers/linting_tools/yapf_text_parser.cpp
    src/parsers/linting_tools/isort_parser.cpp
    src/parsers/linting_tools/bandit_text_parser.cpp
    src/parsers/linting_tools/ruff_parser.cpp
    src/parsers/linting_tools/eslint_text_parser.cpp
    src/parsers/linting_tools/rubocop_text_parser.cpp
    src/parsers/linting_tools/shellcheck_text_parser.cpp
    src/parsers/linting_tools/hadolint_text_parser.cpp
    src/parsers/build_systems/docker_build_parser.cpp

    # Phase 2 Final: Infrastructure Tools
    src/parsers/infrastructure_tools/ansible_text_parser.cpp
    
    # Phase 3: Workflow Engine Implementation
    src/read_duck_hunt_workflow_log_function.cpp
    src/parse_duck_hunt_workflow_log_function.cpp
    src/workflow_engine_interface.cpp
    src/parsers/workflow_engines/github_actions_parser.cpp
    src/parsers/workflow_engines/github_actions_zip_parser.cpp
    src/parsers/workflow_engines/gitlab_ci_parser.cpp
    src/parsers/workflow_engines/jenkins_parser.cpp
    src/parsers/workflow_engines/docker_parser.cpp
    src/parsers/workflow_engines/spack_parser.cpp

    # Scalar utility functions
    src/status_badge_function.cpp

    # Format discovery function
    src/duck_hunt_formats_function.cpp

    # Diagnostic functions
    src/duck_hunt_diagnose_function.cpp

    # Format detection scalar function
    src/duck_hunt_detect_format_function.cpp

    # Table macros
    src/duck_hunt_macros.cpp

    # Cross-language structured log parsers
    src/parsers/structured_logs/jsonl_parser.cpp
    src/parsers/structured_logs/logfmt_parser.cpp

    # Web access and system log parsers
    src/parsers/web_access/syslog_parser.cpp
    src/parsers/web_access/apache_access_parser.cpp
    src/parsers/web_access/nginx_access_parser.cpp

    # Cloud provider log parsers
    src/parsers/cloud_logs/aws_cloudtrail_parser.cpp
    src/parsers/cloud_logs/gcp_cloud_logging_parser.cpp
    src/parsers/cloud_logs/azure_activity_parser.cpp

    # Application logging parsers
    src/parsers/app_logging/python_logging_parser.cpp
    src/parsers/app_logging/log4j_parser.cpp
    src/parsers/app_logging/logrus_parser.cpp

    # Application logging parsers - Phase 2 (Node.js, .NET, Ruby)
    src/parsers/app_logging/winston_parser.cpp
    src/parsers/app_logging/pino_parser.cpp
    src/parsers/app_logging/bunyan_parser.cpp
    src/parsers/app_logging/serilog_parser.cpp
    src/parsers/app_logging/nlog_parser.cpp
    src/parsers/app_logging/ruby_logger_parser.cpp
    src/parsers/app_logging/rails_log_parser.cpp

    # Infrastructure log parsers
    src/parsers/infrastructure/iptables_parser.cpp
    src/parsers/infrastructure/pf_parser.cpp
    src/parsers/infrastructure/cisco_asa_parser.cpp
    src/parsers/infrastructure/vpc_flow_parser.cpp
    src/parsers/infrastructure/kubernetes_parser.cpp
    src/parsers/infrastructure/windows_event_parser.cpp
    src/parsers/infrastructure/auditd_parser.cpp
    src/parsers/infrastructure/s3_access_parser.cpp

    # Coverage parsers
    src/parsers/coverage/lcov_parser.cpp

    # Distributed systems parsers (loghub formats)
    src/parsers/distributed_systems/hdfs_parser.cpp
    src/parsers/distributed_systems/spark_parser.cpp
    src/parsers/distributed_systems/android_parser.cpp
    src/parsers/distributed_systems/zookeeper_parser.cpp
    src/parsers/distributed_systems/openstack_parser.cpp
    src/parsers/distributed_systems/bgl_parser.cpp

    # XML-based parsers (require webbed extension)
    src/parsers/test_frameworks/junit_xml_parser.cpp

    # Playwright test framework parsers
    src/parsers/test_frameworks/playwright_text_parser.cpp
    src/parsers/test_frameworks/playwright_json_parser.cpp

    # Go test text parser
    src/parsers/test_frameworks/gotest_text_parser.cpp

    # Generic error fallback parser
    src/parsers/tool_outputs/generic_error_parser.cpp
)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
