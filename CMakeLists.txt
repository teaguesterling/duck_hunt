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
    src/parsers/build_systems/cmake_parser.cpp
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
