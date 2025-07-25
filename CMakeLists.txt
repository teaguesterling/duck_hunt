cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME duck_hunt)

# DuckDB's extension distribution supports vcpkg. As such, dependencies can be added in ./vcpkg.json and then
# used in cmake with find_package. Feel free to remove or replace with other dependencies.
# Note that it should also be removed from vcpkg.json to prevent needlessly installing it..
find_package(OpenSSL REQUIRED)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES 
    src/duck_hunt_extension.cpp
    src/validation_event_types.cpp
    src/read_test_results_function.cpp
    
    # Core modular architecture
    src/core/parser_registry.cpp
    src/core/format_detector.cpp
    
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
    
    # Phase 6: Text Parser Extraction - Batch 1
    src/parsers/test_frameworks/junit_text_parser.cpp
    
    # Phase 6: Text Parser Extraction - Batch 2
    src/parsers/test_frameworks/rspec_text_parser.cpp
    
    # Phase 6: Text Parser Extraction - Batch 3
    src/parsers/test_frameworks/mocha_chai_text_parser.cpp
    
    # Phase 6: Text Parser Extraction - Batch 4
    src/parsers/test_frameworks/gtest_text_parser.cpp
    src/parsers/test_frameworks/nunit_xunit_text_parser.cpp
    
    # Phase 7: Specialized Parser Extraction - Batch 1
    src/parsers/specialized/valgrind_parser.cpp
    
    # Phase 7: Specialized Parser Extraction - Batch 2
    src/parsers/specialized/gdb_lldb_parser.cpp
    
    # Phase 7: Specialized Parser Extraction - Batch 3
    src/parsers/specialized/coverage_parser.cpp
    
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
    
    # New parsers - CI Systems and improved linting tools
    src/parsers/ci_systems/github_cli_parser.cpp
    src/parsers/ci_systems/drone_ci_text_parser.cpp
    src/parsers/ci_systems/terraform_text_parser.cpp
    src/parsers/linting_tools/clang_tidy_parser.cpp
    src/parsers/linting_tools/yapf_text_parser.cpp
    
    # Phase 2 Final: Infrastructure Tools
    src/parsers/infrastructure_tools/ansible_text_parser.cpp
)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Link OpenSSL in both the static library as the loadable extension
target_link_libraries(${EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(${LOADABLE_EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
