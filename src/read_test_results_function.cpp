#include "include/read_test_results_function.hpp"
#include "include/validation_event_types.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "yyjson.hpp"
#include <fstream>
#include <regex>
#include <sstream>
#include <map>

namespace duckdb {

using namespace duckdb_yyjson;

TestResultFormat DetectTestResultFormat(const std::string& content) {
    // First check if it's valid JSON
    if (IsValidJSON(content)) {
        // Check for specific JSON formats
        if (content.find("\"tests\":") != std::string::npos) {
            return TestResultFormat::PYTEST_JSON;
        }
        if (content.find("\"Action\":") != std::string::npos && content.find("\"Package\":") != std::string::npos) {
            return TestResultFormat::GOTEST_JSON;
        }
        if (content.find("\"messages\":") != std::string::npos && content.find("\"filePath\":") != std::string::npos) {
            return TestResultFormat::ESLINT_JSON;
        }
        if (content.find("\"files\":") != std::string::npos && content.find("\"offenses\":") != std::string::npos && content.find("\"cop_name\":") != std::string::npos) {
            return TestResultFormat::RUBOCOP_JSON;
        }
        if (content.find("\"type\":") != std::string::npos && content.find("\"event\":") != std::string::npos && 
            (content.find("\"suite\"") != std::string::npos || content.find("\"test\"") != std::string::npos)) {
            return TestResultFormat::CARGO_TEST_JSON;
        }
        if (content.find("\"rule_id\":") != std::string::npos && content.find("\"severity\":") != std::string::npos && 
            content.find("\"file\":") != std::string::npos) {
            return TestResultFormat::SWIFTLINT_JSON;
        }
        if (content.find("\"totals\":") != std::string::npos && content.find("\"files\":") != std::string::npos && 
            content.find("\"errors\":") != std::string::npos) {
            return TestResultFormat::PHPSTAN_JSON;
        }
        if (content.find("\"file\":") != std::string::npos && content.find("\"level\":") != std::string::npos && 
            content.find("\"code\":") != std::string::npos && content.find("\"message\":") != std::string::npos && content.find("\"line\":") != std::string::npos &&
            content.find("\"DL") != std::string::npos) {
            return TestResultFormat::HADOLINT_JSON;
        }
        if (content.find("\"code\":") != std::string::npos && content.find("\"level\":") != std::string::npos && 
            content.find("\"line\":") != std::string::npos && content.find("\"column\":") != std::string::npos) {
            return TestResultFormat::SHELLCHECK_JSON;
        }
        if (content.find("\"source\":") != std::string::npos && content.find("\"warnings\":") != std::string::npos && 
            content.find("\"rule\":") != std::string::npos && content.find("\"severity\":") != std::string::npos) {
            return TestResultFormat::STYLELINT_JSON;
        }
        if (content.find("\"message\":") != std::string::npos && content.find("\"spans\":") != std::string::npos && 
            content.find("\"level\":") != std::string::npos && content.find("\"file_name\":") != std::string::npos) {
            return TestResultFormat::CLIPPY_JSON;
        }
        if (content.find("\"fileName\":") != std::string::npos && content.find("\"lineNumber\":") != std::string::npos && 
            content.find("\"ruleNames\":") != std::string::npos && content.find("\"ruleDescription\":") != std::string::npos) {
            return TestResultFormat::MARKDOWNLINT_JSON;
        }
        if (content.find("\"file\":") != std::string::npos && content.find("\"line\":") != std::string::npos && 
            content.find("\"column\":") != std::string::npos && content.find("\"rule\":") != std::string::npos && content.find("\"level\":") != std::string::npos) {
            return TestResultFormat::YAMLLINT_JSON;
        }
        if (content.find("\"results\":") != std::string::npos && content.find("\"test_id\":") != std::string::npos && 
            content.find("\"issue_severity\":") != std::string::npos && content.find("\"issue_confidence\":") != std::string::npos) {
            return TestResultFormat::BANDIT_JSON;
        }
        if (content.find("\"BugCollection\":") != std::string::npos && content.find("\"BugInstance\":") != std::string::npos && 
            content.find("\"type\":") != std::string::npos && content.find("\"priority\":") != std::string::npos) {
            return TestResultFormat::SPOTBUGS_JSON;
        }
        if (content.find("\"file\":") != std::string::npos && content.find("\"errors\":") != std::string::npos && 
            content.find("\"rule\":") != std::string::npos && content.find("\"line\":") != std::string::npos && content.find("\"column\":") != std::string::npos) {
            return TestResultFormat::KTLINT_JSON;
        }
        if (content.find("\"filename\":") != std::string::npos && content.find("\"line_number\":") != std::string::npos && 
            content.find("\"column_number\":") != std::string::npos && content.find("\"linter\":") != std::string::npos && content.find("\"type\":") != std::string::npos) {
            return TestResultFormat::LINTR_JSON;
        }
        if (content.find("\"filepath\":") != std::string::npos && content.find("\"violations\":") != std::string::npos && 
            content.find("\"line_no\":") != std::string::npos && content.find("\"code\":") != std::string::npos && content.find("\"rule\":") != std::string::npos) {
            return TestResultFormat::SQLFLUFF_JSON;
        }
        if (content.find("\"issues\":") != std::string::npos && content.find("\"rule\":") != std::string::npos && 
            content.find("\"range\":") != std::string::npos && content.find("\"filename\":") != std::string::npos && content.find("\"severity\":") != std::string::npos) {
            return TestResultFormat::TFLINT_JSON;
        }
        if (content.find("\"object_name\":") != std::string::npos && content.find("\"type_meta\":") != std::string::npos && 
            content.find("\"checks\":") != std::string::npos && content.find("\"grade\":") != std::string::npos && content.find("\"file_name\":") != std::string::npos) {
            return TestResultFormat::KUBE_SCORE_JSON;
        }
    }
    
    // Check text patterns (DuckDB test should be checked before make error since it may contain both)
    if (content.find("[0/") != std::string::npos && content.find("] (0%):") != std::string::npos && 
        content.find("test cases:") != std::string::npos) {
        return TestResultFormat::DUCKDB_TEST;
    }
    
    // Check for Valgrind patterns (should be checked early due to unique format)
    if ((content.find("==") != std::string::npos && content.find("Memcheck") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("Helgrind") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("Cachegrind") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("Massif") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("DRD") != std::string::npos) ||
        (content.find("Invalid read") != std::string::npos || content.find("Invalid write") != std::string::npos) ||
        (content.find("definitely lost") != std::string::npos && content.find("bytes") != std::string::npos) ||
        (content.find("Possible data race") != std::string::npos && content.find("thread") != std::string::npos)) {
        return TestResultFormat::VALGRIND;
    }
    
    // Check for GDB/LLDB patterns (should be checked early due to unique format)
    if ((content.find("GNU gdb") != std::string::npos || content.find("(gdb)") != std::string::npos) ||
        (content.find("lldb") != std::string::npos && content.find("target create") != std::string::npos) ||
        (content.find("Program received signal") != std::string::npos && content.find("Segmentation fault") != std::string::npos) ||
        (content.find("Process") != std::string::npos && content.find("stopped") != std::string::npos && content.find("EXC_BAD_ACCESS") != std::string::npos) ||
        (content.find("frame #") != std::string::npos && content.find("0x") != std::string::npos) ||
        (content.find("breakpoint") != std::string::npos && content.find("hit count") != std::string::npos) ||
        (content.find("(lldb)") != std::string::npos) ||
        (content.find("Reading symbols from") != std::string::npos && content.find("Starting program:") != std::string::npos)) {
        return TestResultFormat::GDB_LLDB;
    }
    
    // Check for Mocha/Chai text patterns (should be checked before RSpec since they can share similar symbols)
    if ((content.find("passing") != std::string::npos && content.find("failing") != std::string::npos) ||
        (content.find("Error:") != std::string::npos && content.find("at Context.<anonymous>") != std::string::npos) ||
        (content.find("AssertionError:") != std::string::npos && content.find("at Context.<anonymous>") != std::string::npos) ||
        (content.find("at Test.Runnable.run") != std::string::npos && content.find("node_modules/mocha") != std::string::npos) ||
        (content.find("✓") != std::string::npos && content.find("✗") != std::string::npos && content.find("(") != std::string::npos && content.find("ms)") != std::string::npos)) {
        return TestResultFormat::MOCHA_CHAI_TEXT;
    }
    
    // Check for Google Test patterns (should be checked before other C++ test frameworks)
    if ((content.find("[==========]") != std::string::npos && content.find("Running") != std::string::npos && content.find("tests from") != std::string::npos) ||
        (content.find("[ RUN      ]") != std::string::npos && content.find("[       OK ]") != std::string::npos) ||
        (content.find("[  FAILED  ]") != std::string::npos && content.find("ms total") != std::string::npos) ||
        (content.find("[  PASSED  ]") != std::string::npos && content.find("tests.") != std::string::npos) ||
        (content.find("[----------]") != std::string::npos && content.find("Global test environment") != std::string::npos)) {
        return TestResultFormat::GTEST_TEXT;
    }
    
    // Check for NUnit/xUnit patterns (should be checked before other .NET test frameworks)
    if ((content.find("NUnit") != std::string::npos && content.find("Test Count:") != std::string::npos && content.find("Passed:") != std::string::npos) ||
        (content.find("Test Run Summary") != std::string::npos && content.find("Overall result:") != std::string::npos) ||
        (content.find("xUnit.net") != std::string::npos && content.find("VSTest Adapter") != std::string::npos) ||
        (content.find("[PASS]") != std::string::npos && content.find("[FAIL]") != std::string::npos && content.find(".Tests.") != std::string::npos) ||
        (content.find("Starting:") != std::string::npos && content.find("Finished:") != std::string::npos && content.find("==>") != std::string::npos) ||
        (content.find("Total tests:") != std::string::npos && content.find("Failed:") != std::string::npos && content.find("Skipped:") != std::string::npos)) {
        return TestResultFormat::NUNIT_XUNIT_TEXT;
    }
    
    // Check for RSpec text patterns (should be checked after Mocha/Chai since they can contain similar keywords)
    if ((content.find("Finished in") != std::string::npos && content.find("examples") != std::string::npos) ||
        (content.find("Randomized with seed") != std::string::npos && content.find("failures") != std::string::npos) ||
        (content.find("Failed examples:") != std::string::npos && content.find("rspec") != std::string::npos) ||
        (content.find("✓") != std::string::npos && content.find("✗") != std::string::npos) ||
        (content.find("pending:") != std::string::npos && content.find("PENDING:") != std::string::npos) ||
        (content.find("Failure/Error:") != std::string::npos && content.find("expected") != std::string::npos)) {
        return TestResultFormat::RSPEC_TEXT;
    }
    
    // Check for JUnit text patterns (should be checked before pytest since they can contain similar keywords)
    if ((content.find("T E S T S") != std::string::npos && content.find("Tests run:") != std::string::npos) ||
        (content.find("JUnit Jupiter") != std::string::npos && content.find("tests found") != std::string::npos) ||
        (content.find("Running TestSuite") != std::string::npos && content.find("Total tests run:") != std::string::npos) ||
        (content.find("Time elapsed:") != std::string::npos && content.find("PASSED!") != std::string::npos) ||
        (content.find("Time elapsed:") != std::string::npos && content.find("FAILURE!") != std::string::npos) ||
        (content.find(" > ") != std::string::npos && (content.find(" PASSED") != std::string::npos || content.find(" FAILED") != std::string::npos))) {
        return TestResultFormat::JUNIT_TEXT;
    }
    
    if (content.find("PASSED") != std::string::npos && content.find("::") != std::string::npos) {
        return TestResultFormat::PYTEST_TEXT;
    }
    
    if ((content.find("CMake Error") != std::string::npos || content.find("CMake Warning") != std::string::npos || 
         content.find("gmake[") != std::string::npos) && 
        (content.find("Building C") != std::string::npos || content.find("Building CXX") != std::string::npos || 
         content.find("Linking") != std::string::npos || content.find("CMakeLists.txt") != std::string::npos)) {
        return TestResultFormat::CMAKE_BUILD;
    }
    
    // Check for Python build patterns
    if ((content.find("Building wheel") != std::string::npos && content.find("setup.py") != std::string::npos) ||
        (content.find("running build") != std::string::npos && content.find("python setup.py") != std::string::npos) ||
        (content.find("pip install") != std::string::npos && content.find("ERROR:") != std::string::npos) ||
        (content.find("FAILED") != std::string::npos && content.find("AssertionError") != std::string::npos)) {
        return TestResultFormat::PYTHON_BUILD;
    }
    
    // Check for Node.js build patterns
    if ((content.find("npm ERR!") != std::string::npos || content.find("yarn install") != std::string::npos) ||
        (content.find("webpack") != std::string::npos && (content.find("ERROR") != std::string::npos || content.find("WARNING") != std::string::npos)) ||
        (content.find("jest") != std::string::npos && content.find("FAIL") != std::string::npos) ||
        (content.find("eslint") != std::string::npos && content.find("error") != std::string::npos)) {
        return TestResultFormat::NODE_BUILD;
    }
    
    // Check for Cargo build patterns
    if ((content.find("Compiling") != std::string::npos && content.find("cargo") != std::string::npos) ||
        (content.find("error[E") != std::string::npos && content.find("-->") != std::string::npos) ||
        (content.find("cargo test") != std::string::npos && content.find("FAILED") != std::string::npos) ||
        (content.find("cargo clippy") != std::string::npos && content.find("warning:") != std::string::npos) ||
        (content.find("rustc --explain") != std::string::npos)) {
        return TestResultFormat::CARGO_BUILD;
    }
    
    // Check for Maven build patterns
    if ((content.find("[INFO]") != std::string::npos && content.find("maven") != std::string::npos) ||
        (content.find("[ERROR]") != std::string::npos && content.find("COMPILATION ERROR") != std::string::npos) ||
        (content.find("maven-compiler-plugin") != std::string::npos) ||
        (content.find("maven-surefire-plugin") != std::string::npos && content.find("Tests run:") != std::string::npos) ||
        (content.find("BUILD FAILURE") != std::string::npos && content.find("Total time:") != std::string::npos)) {
        return TestResultFormat::MAVEN_BUILD;
    }
    
    // Check for Gradle build patterns
    if ((content.find("> Task :") != std::string::npos) ||
        (content.find("BUILD SUCCESSFUL") != std::string::npos && content.find("actionable task") != std::string::npos) ||
        (content.find("BUILD FAILED") != std::string::npos && content.find("actionable task") != std::string::npos) ||
        (content.find("Gradle") != std::string::npos && content.find("build") != std::string::npos) ||
        (content.find("[ant:checkstyle]") != std::string::npos) ||
        (content.find("Execution failed for task") != std::string::npos)) {
        return TestResultFormat::GRADLE_BUILD;
    }
    
    // Check for MSBuild patterns
    if ((content.find("Microsoft (R) Build Engine") != std::string::npos) ||
        (content.find("Build started") != std::string::npos && content.find("Time Elapsed") != std::string::npos) ||
        (content.find("Build FAILED") != std::string::npos && content.find("Error(s)") != std::string::npos) ||
        (content.find("Build succeeded") != std::string::npos && content.find("Warning(s)") != std::string::npos) ||
        (content.find("error CS") != std::string::npos && content.find(".csproj") != std::string::npos) ||
        (content.find("xUnit.net") != std::string::npos && content.find("[FAIL]") != std::string::npos)) {
        return TestResultFormat::MSBUILD;
    }
    
    
    if (content.find("make: ***") != std::string::npos && content.find("Error") != std::string::npos) {
        return TestResultFormat::MAKE_ERROR;
    }
    
    if (content.find(": error:") != std::string::npos || content.find(": warning:") != std::string::npos) {
        return TestResultFormat::GENERIC_LINT;
    }
    
    return TestResultFormat::UNKNOWN;
}

std::string TestResultFormatToString(TestResultFormat format) {
    switch (format) {
        case TestResultFormat::UNKNOWN: return "unknown";
        case TestResultFormat::AUTO: return "auto";
        case TestResultFormat::PYTEST_JSON: return "pytest_json";
        case TestResultFormat::GOTEST_JSON: return "gotest_json";
        case TestResultFormat::ESLINT_JSON: return "eslint_json";
        case TestResultFormat::PYTEST_TEXT: return "pytest_text";
        case TestResultFormat::MAKE_ERROR: return "make_error";
        case TestResultFormat::GENERIC_LINT: return "generic_lint";
        case TestResultFormat::DUCKDB_TEST: return "duckdb_test";
        case TestResultFormat::RUBOCOP_JSON: return "rubocop_json";
        case TestResultFormat::CARGO_TEST_JSON: return "cargo_test_json";
        case TestResultFormat::SWIFTLINT_JSON: return "swiftlint_json";
        case TestResultFormat::PHPSTAN_JSON: return "phpstan_json";
        case TestResultFormat::SHELLCHECK_JSON: return "shellcheck_json";
        case TestResultFormat::STYLELINT_JSON: return "stylelint_json";
        case TestResultFormat::CLIPPY_JSON: return "clippy_json";
        case TestResultFormat::MARKDOWNLINT_JSON: return "markdownlint_json";
        case TestResultFormat::YAMLLINT_JSON: return "yamllint_json";
        case TestResultFormat::BANDIT_JSON: return "bandit_json";
        case TestResultFormat::SPOTBUGS_JSON: return "spotbugs_json";
        case TestResultFormat::KTLINT_JSON: return "ktlint_json";
        case TestResultFormat::HADOLINT_JSON: return "hadolint_json";
        case TestResultFormat::LINTR_JSON: return "lintr_json";
        case TestResultFormat::SQLFLUFF_JSON: return "sqlfluff_json";
        case TestResultFormat::TFLINT_JSON: return "tflint_json";
        case TestResultFormat::KUBE_SCORE_JSON: return "kube_score_json";
        case TestResultFormat::CMAKE_BUILD: return "cmake_build";
        case TestResultFormat::PYTHON_BUILD: return "python_build";
        case TestResultFormat::NODE_BUILD: return "node_build";
        case TestResultFormat::CARGO_BUILD: return "cargo_build";
        case TestResultFormat::MAVEN_BUILD: return "maven_build";
        case TestResultFormat::GRADLE_BUILD: return "gradle_build";
        case TestResultFormat::MSBUILD: return "msbuild";
        case TestResultFormat::JUNIT_TEXT: return "junit_text";
        case TestResultFormat::VALGRIND: return "valgrind";
        case TestResultFormat::GDB_LLDB: return "gdb_lldb";
        case TestResultFormat::RSPEC_TEXT: return "rspec_text";
        case TestResultFormat::MOCHA_CHAI_TEXT: return "mocha_chai_text";
        case TestResultFormat::GTEST_TEXT: return "gtest_text";
        case TestResultFormat::NUNIT_XUNIT_TEXT: return "nunit_xunit_text";
        default: return "unknown";
    }
}

TestResultFormat StringToTestResultFormat(const std::string& str) {
    if (str == "auto") return TestResultFormat::AUTO;
    if (str == "pytest_json") return TestResultFormat::PYTEST_JSON;
    if (str == "gotest_json") return TestResultFormat::GOTEST_JSON;
    if (str == "eslint_json") return TestResultFormat::ESLINT_JSON;
    if (str == "pytest_text") return TestResultFormat::PYTEST_TEXT;
    if (str == "make_error") return TestResultFormat::MAKE_ERROR;
    if (str == "generic_lint") return TestResultFormat::GENERIC_LINT;
    if (str == "duckdb_test") return TestResultFormat::DUCKDB_TEST;
    if (str == "rubocop_json") return TestResultFormat::RUBOCOP_JSON;
    if (str == "cargo_test_json") return TestResultFormat::CARGO_TEST_JSON;
    if (str == "swiftlint_json") return TestResultFormat::SWIFTLINT_JSON;
    if (str == "phpstan_json") return TestResultFormat::PHPSTAN_JSON;
    if (str == "shellcheck_json") return TestResultFormat::SHELLCHECK_JSON;
    if (str == "stylelint_json") return TestResultFormat::STYLELINT_JSON;
    if (str == "clippy_json") return TestResultFormat::CLIPPY_JSON;
    if (str == "markdownlint_json") return TestResultFormat::MARKDOWNLINT_JSON;
    if (str == "yamllint_json") return TestResultFormat::YAMLLINT_JSON;
    if (str == "bandit_json") return TestResultFormat::BANDIT_JSON;
    if (str == "spotbugs_json") return TestResultFormat::SPOTBUGS_JSON;
    if (str == "ktlint_json") return TestResultFormat::KTLINT_JSON;
    if (str == "hadolint_json") return TestResultFormat::HADOLINT_JSON;
    if (str == "lintr_json") return TestResultFormat::LINTR_JSON;
    if (str == "sqlfluff_json") return TestResultFormat::SQLFLUFF_JSON;
    if (str == "tflint_json") return TestResultFormat::TFLINT_JSON;
    if (str == "kube_score_json") return TestResultFormat::KUBE_SCORE_JSON;
    if (str == "cmake_build") return TestResultFormat::CMAKE_BUILD;
    if (str == "python_build") return TestResultFormat::PYTHON_BUILD;
    if (str == "node_build") return TestResultFormat::NODE_BUILD;
    if (str == "cargo_build") return TestResultFormat::CARGO_BUILD;
    if (str == "maven_build") return TestResultFormat::MAVEN_BUILD;
    if (str == "gradle_build") return TestResultFormat::GRADLE_BUILD;
    if (str == "msbuild") return TestResultFormat::MSBUILD;
    if (str == "junit_text") return TestResultFormat::JUNIT_TEXT;
    if (str == "valgrind") return TestResultFormat::VALGRIND;
    if (str == "gdb_lldb") return TestResultFormat::GDB_LLDB;
    if (str == "rspec_text") return TestResultFormat::RSPEC_TEXT;
    if (str == "mocha_chai_text") return TestResultFormat::MOCHA_CHAI_TEXT;
    if (str == "gtest_text") return TestResultFormat::GTEST_TEXT;
    if (str == "nunit_xunit_text") return TestResultFormat::NUNIT_XUNIT_TEXT;
    if (str == "unknown") return TestResultFormat::UNKNOWN;
    return TestResultFormat::AUTO;  // Default to auto-detection
}

std::string ReadContentFromSource(const std::string& source) {
    // For now, assume source is a file path
    // Later we can add support for direct content strings
    std::ifstream file(source);
    if (!file.is_open()) {
        throw IOException("Could not open file: " + source);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    return content;
}

bool IsValidJSON(const std::string& content) {
    // Simple heuristic - starts with { or [
    std::string trimmed = content;
    StringUtil::Trim(trimmed);
    return !trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '[');
}

unique_ptr<FunctionData> ReadTestResultsBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names) {
    auto bind_data = make_uniq<ReadTestResultsBindData>();
    
    // Get source parameter (required)
    if (input.inputs.empty()) {
        throw BinderException("read_test_results requires at least one parameter (source)");
    }
    bind_data->source = input.inputs[0].ToString();
    
    // Get format parameter (optional, defaults to auto)
    if (input.inputs.size() > 1) {
        bind_data->format = StringToTestResultFormat(input.inputs[1].ToString());
    } else {
        bind_data->format = TestResultFormat::AUTO;
    }
    
    // Define return schema
    return_types = {
        LogicalType::BIGINT,   // event_id
        LogicalType::VARCHAR,  // tool_name
        LogicalType::VARCHAR,  // event_type
        LogicalType::VARCHAR,  // file_path
        LogicalType::INTEGER,  // line_number
        LogicalType::INTEGER,  // column_number
        LogicalType::VARCHAR,  // function_name
        LogicalType::VARCHAR,  // status
        LogicalType::VARCHAR,  // severity
        LogicalType::VARCHAR,  // category
        LogicalType::VARCHAR,  // message
        LogicalType::VARCHAR,  // suggestion
        LogicalType::VARCHAR,  // error_code
        LogicalType::VARCHAR,  // test_name
        LogicalType::DOUBLE,   // execution_time
        LogicalType::VARCHAR,  // raw_output
        LogicalType::VARCHAR   // structured_data
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number",
        "column_number", "function_name", "status", "severity", "category",
        "message", "suggestion", "error_code", "test_name", "execution_time",
        "raw_output", "structured_data"
    };
    
    return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ReadTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<ReadTestResultsBindData>();
    auto global_state = make_uniq<ReadTestResultsGlobalState>();
    
    // Read content from source
    std::string content;
    try {
        content = ReadContentFromSource(bind_data.source);
    } catch (const IOException&) {
        // If file reading fails, treat source as direct content
        content = bind_data.source;
    }
    
    // Auto-detect format if needed
    TestResultFormat format = bind_data.format;
    if (format == TestResultFormat::AUTO) {
        format = DetectTestResultFormat(content);
    }
    
    // Parse content based on detected format
    switch (format) {
        case TestResultFormat::PYTEST_JSON:
            ParsePytestJSON(content, global_state->events);
            break;
        case TestResultFormat::DUCKDB_TEST:
            ParseDuckDBTestOutput(content, global_state->events);
            break;
        case TestResultFormat::ESLINT_JSON:
            ParseESLintJSON(content, global_state->events);
            break;
        case TestResultFormat::GOTEST_JSON:
            ParseGoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::MAKE_ERROR:
            ParseMakeErrors(content, global_state->events);
            break;
        case TestResultFormat::PYTEST_TEXT:
            ParsePytestText(content, global_state->events);
            break;
        case TestResultFormat::GENERIC_LINT:
            ParseGenericLint(content, global_state->events);
            break;
        case TestResultFormat::RUBOCOP_JSON:
            ParseRuboCopJSON(content, global_state->events);
            break;
        case TestResultFormat::CARGO_TEST_JSON:
            ParseCargoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::SWIFTLINT_JSON:
            ParseSwiftLintJSON(content, global_state->events);
            break;
        case TestResultFormat::PHPSTAN_JSON:
            ParsePHPStanJSON(content, global_state->events);
            break;
        case TestResultFormat::SHELLCHECK_JSON:
            ParseShellCheckJSON(content, global_state->events);
            break;
        case TestResultFormat::STYLELINT_JSON:
            ParseStylelintJSON(content, global_state->events);
            break;
        case TestResultFormat::CLIPPY_JSON:
            ParseClippyJSON(content, global_state->events);
            break;
        case TestResultFormat::MARKDOWNLINT_JSON:
            ParseMarkdownlintJSON(content, global_state->events);
            break;
        case TestResultFormat::YAMLLINT_JSON:
            ParseYamllintJSON(content, global_state->events);
            break;
        case TestResultFormat::BANDIT_JSON:
            ParseBanditJSON(content, global_state->events);
            break;
        case TestResultFormat::SPOTBUGS_JSON:
            ParseSpotBugsJSON(content, global_state->events);
            break;
        case TestResultFormat::KTLINT_JSON:
            ParseKtlintJSON(content, global_state->events);
            break;
        case TestResultFormat::HADOLINT_JSON:
            ParseHadolintJSON(content, global_state->events);
            break;
        case TestResultFormat::LINTR_JSON:
            ParseLintrJSON(content, global_state->events);
            break;
        case TestResultFormat::SQLFLUFF_JSON:
            ParseSqlfluffJSON(content, global_state->events);
            break;
        case TestResultFormat::TFLINT_JSON:
            ParseTflintJSON(content, global_state->events);
            break;
        case TestResultFormat::KUBE_SCORE_JSON:
            ParseKubeScoreJSON(content, global_state->events);
            break;
        case TestResultFormat::CMAKE_BUILD:
            ParseCMakeBuild(content, global_state->events);
            break;
        case TestResultFormat::PYTHON_BUILD:
            ParsePythonBuild(content, global_state->events);
            break;
        case TestResultFormat::NODE_BUILD:
            ParseNodeBuild(content, global_state->events);
            break;
        case TestResultFormat::CARGO_BUILD:
            ParseCargoBuild(content, global_state->events);
            break;
        case TestResultFormat::MAVEN_BUILD:
            ParseMavenBuild(content, global_state->events);
            break;
        case TestResultFormat::GRADLE_BUILD:
            ParseGradleBuild(content, global_state->events);
            break;
        case TestResultFormat::MSBUILD:
            ParseMSBuild(content, global_state->events);
            break;
        case TestResultFormat::JUNIT_TEXT:
            ParseJUnitText(content, global_state->events);
            break;
        case TestResultFormat::VALGRIND:
            ParseValgrind(content, global_state->events);
            break;
        case TestResultFormat::GDB_LLDB:
            ParseGdbLldb(content, global_state->events);
            break;
        case TestResultFormat::RSPEC_TEXT:
            ParseRSpecText(content, global_state->events);
            break;
        case TestResultFormat::MOCHA_CHAI_TEXT:
            ParseMochaChai(content, global_state->events);
            break;
        case TestResultFormat::GTEST_TEXT:
            ParseGoogleTest(content, global_state->events);
            break;
        case TestResultFormat::NUNIT_XUNIT_TEXT:
            ParseNUnitXUnit(content, global_state->events);
            break;
        default:
            // For unknown formats, don't create any events
            break;
    }
    
    return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ReadTestResultsInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                            GlobalTableFunctionState *global_state) {
    return make_uniq<ReadTestResultsLocalState>();
}

void ReadTestResultsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &global_state = data_p.global_state->Cast<ReadTestResultsGlobalState>();
    auto &local_state = data_p.local_state->Cast<ReadTestResultsLocalState>();
    
    // Populate output chunk
    PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE);
    
    // Update offset for next chunk
    local_state.chunk_offset += output.size();
}

void PopulateDataChunkFromEvents(DataChunk &output, const std::vector<ValidationEvent> &events, 
                                idx_t start_offset, idx_t chunk_size) {
    idx_t events_remaining = events.size() > start_offset ? events.size() - start_offset : 0;
    idx_t output_size = std::min(chunk_size, events_remaining);
    
    if (output_size == 0) {
        output.SetCardinality(0);
        return;
    }
    
    output.SetCardinality(output_size);
    
    for (idx_t i = 0; i < output_size; i++) {
        const auto &event = events[start_offset + i];
        idx_t col = 0;
        
        output.SetValue(col++, i, Value::BIGINT(event.event_id));
        output.SetValue(col++, i, Value(event.tool_name));
        output.SetValue(col++, i, Value(ValidationEventTypeToString(event.event_type)));
        output.SetValue(col++, i, Value(event.file_path));
        output.SetValue(col++, i, event.line_number == -1 ? Value() : Value::INTEGER(event.line_number));
        output.SetValue(col++, i, event.column_number == -1 ? Value() : Value::INTEGER(event.column_number));
        output.SetValue(col++, i, Value(event.function_name));
        output.SetValue(col++, i, Value(ValidationEventStatusToString(event.status)));
        output.SetValue(col++, i, Value(event.severity));
        output.SetValue(col++, i, Value(event.category));
        output.SetValue(col++, i, Value(event.message));
        output.SetValue(col++, i, Value(event.suggestion));
        output.SetValue(col++, i, Value(event.error_code));
        output.SetValue(col++, i, Value(event.test_name));
        output.SetValue(col++, i, Value::DOUBLE(event.execution_time));
        output.SetValue(col++, i, Value(event.raw_output));
        output.SetValue(col++, i, Value(event.structured_data));
    }
}

void ParsePytestJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse pytest JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid pytest JSON: root is not an object");
    }
    
    // Get tests array
    yyjson_val *tests = yyjson_obj_get(root, "tests");
    if (!tests || !yyjson_is_arr(tests)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid pytest JSON: no tests array found");
    }
    
    // Parse each test
    size_t idx, max;
    yyjson_val *test;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(tests, idx, max, test) {
        if (!yyjson_is_obj(test)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "pytest";
        event.event_type = ValidationEventType::TEST_RESULT;
        event.line_number = -1;
        event.column_number = -1;
        event.execution_time = 0.0;
        
        // Extract nodeid (test name with file path)
        yyjson_val *nodeid = yyjson_obj_get(test, "nodeid");
        if (nodeid && yyjson_is_str(nodeid)) {
            std::string nodeid_str = yyjson_get_str(nodeid);
            
            // Parse nodeid format: "file.py::test_function"
            size_t separator = nodeid_str.find("::");
            if (separator != std::string::npos) {
                event.file_path = nodeid_str.substr(0, separator);
                event.test_name = nodeid_str.substr(separator + 2);
                event.function_name = event.test_name;
            } else {
                event.test_name = nodeid_str;
                event.function_name = nodeid_str;
            }
        }
        
        // Extract outcome
        yyjson_val *outcome = yyjson_obj_get(test, "outcome");
        if (outcome && yyjson_is_str(outcome)) {
            std::string outcome_str = yyjson_get_str(outcome);
            event.status = StringToValidationEventStatus(outcome_str);
        } else {
            event.status = ValidationEventStatus::ERROR;
        }
        
        // Extract call details
        yyjson_val *call = yyjson_obj_get(test, "call");
        if (call && yyjson_is_obj(call)) {
            // Extract duration
            yyjson_val *duration = yyjson_obj_get(call, "duration");
            if (duration && yyjson_is_num(duration)) {
                event.execution_time = yyjson_get_real(duration);
            }
            
            // Extract longrepr (error message)
            yyjson_val *longrepr = yyjson_obj_get(call, "longrepr");
            if (longrepr && yyjson_is_str(longrepr)) {
                event.message = yyjson_get_str(longrepr);
            }
        }
        
        // Set category based on status
        switch (event.status) {
            case ValidationEventStatus::PASS:
                event.category = "test_success";
                if (event.message.empty()) event.message = "Test passed";
                break;
            case ValidationEventStatus::FAIL:
                event.category = "test_failure";
                if (event.message.empty()) event.message = "Test failed";
                break;
            case ValidationEventStatus::SKIP:
                event.category = "test_skipped";
                if (event.message.empty()) event.message = "Test skipped";
                break;
            default:
                event.category = "test_error";
                if (event.message.empty()) event.message = "Test error";
                break;
        }
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseDuckDBTestOutput(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track current test being processed
    std::string current_test_file;
    bool in_failure_section = false;
    std::string failure_message;
    std::string failure_query;
    int failure_line = -1;
    
    while (std::getline(stream, line)) {
        // Parse test progress lines: [X/Y] (Z%): /path/to/test.test
        if (line.find("[") == 0 && line.find("]:") != std::string::npos) {
            size_t path_start = line.find("): ");
            if (path_start != std::string::npos) {
                current_test_file = line.substr(path_start + 3);
                // Trim any trailing whitespace/dots
                while (!current_test_file.empty() && 
                       (current_test_file.back() == '.' || current_test_file.back() == ' ')) {
                    current_test_file.pop_back();
                }
            }
        }
        
        // Detect failure start
        else if (line.find("Wrong result in query!") != std::string::npos ||
                 line.find("Query unexpectedly failed") != std::string::npos) {
            in_failure_section = true;
            failure_message = line;
            
            // Extract line number from failure message
            size_t line_start = line.find(".test:");
            if (line_start != std::string::npos) {
                line_start += 6; // Length of ".test:"
                size_t line_end = line.find(")", line_start);
                if (line_end != std::string::npos) {
                    try {
                        failure_line = std::stoi(line.substr(line_start, line_end - line_start));
                    } catch (...) {
                        failure_line = -1;
                    }
                }
            }
        }
        
        // Capture SQL query in failure section
        else if (in_failure_section && !line.empty() && 
                 line.find("================================================================================") == std::string::npos &&
                 line.find("SELECT") == 0) {
            failure_query = line;
        }
        
        // End of failure section - create failure event
        else if (in_failure_section && line.find("FAILED:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "duckdb_test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = current_test_file;
            event.line_number = failure_line;
            event.column_number = -1;
            event.function_name = failure_query.empty() ? "unknown" : failure_query.substr(0, 50);
            event.status = ValidationEventStatus::FAIL;
            event.category = "test_failure";
            event.message = failure_message;
            event.raw_output = failure_query;
            event.execution_time = 0.0;
            
            events.push_back(event);
            
            // Reset failure tracking
            in_failure_section = false;
            failure_message.clear();
            failure_query.clear();
            failure_line = -1;
        }
        
        // Parse summary line: test cases: X | Y passed | Z failed
        else if (line.find("test cases:") != std::string::npos) {
            // Extract summary statistics and create summary events
            std::string summary_line = line;
            
            // Extract passed count
            size_t passed_pos = summary_line.find(" passed");
            if (passed_pos != std::string::npos) {
                size_t passed_start = summary_line.rfind(" ", passed_pos - 1);
                if (passed_start != std::string::npos) {
                    try {
                        int passed_count = std::stoi(summary_line.substr(passed_start + 1, passed_pos - passed_start - 1));
                        
                        // Create passed test events (summary)
                        ValidationEvent summary_event;
                        summary_event.event_id = event_id++;
                        summary_event.tool_name = "duckdb_test";
                        summary_event.event_type = ValidationEventType::TEST_RESULT;
                        summary_event.status = ValidationEventStatus::INFO;
                        summary_event.category = "test_summary";
                        summary_event.message = "Test summary: " + std::to_string(passed_count) + " tests passed";
                        summary_event.line_number = -1;
                        summary_event.column_number = -1;
                        summary_event.execution_time = 0.0;
                        
                        events.push_back(summary_event);
                    } catch (...) {
                        // Ignore parsing errors
                    }
                }
            }
        }
    }
    
    // If no events were created, add a basic summary
    if (events.empty()) {
        ValidationEvent summary_event;
        summary_event.event_id = 1;
        summary_event.tool_name = "duckdb_test";
        summary_event.event_type = ValidationEventType::TEST_RESULT;
        summary_event.status = ValidationEventStatus::INFO;
        summary_event.category = "test_summary";
        summary_event.message = "DuckDB test output parsed (no specific test results found)";
        summary_event.line_number = -1;
        summary_event.column_number = -1;
        summary_event.execution_time = 0.0;
        
        events.push_back(summary_event);
    }
}

void ParseESLintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse ESLint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid ESLint JSON: root is not an array");
    }
    
    // Parse each file result
    size_t idx, max;
    yyjson_val *file_result;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, file_result) {
        if (!yyjson_is_obj(file_result)) continue;
        
        // Get file path
        yyjson_val *file_path = yyjson_obj_get(file_result, "filePath");
        std::string file_path_str;
        if (file_path && yyjson_is_str(file_path)) {
            file_path_str = yyjson_get_str(file_path);
        }
        
        // Get messages array
        yyjson_val *messages = yyjson_obj_get(file_result, "messages");
        if (!messages || !yyjson_is_arr(messages)) continue;
        
        // Parse each message
        size_t msg_idx, msg_max;
        yyjson_val *message;
        yyjson_arr_foreach(messages, msg_idx, msg_max, message) {
            if (!yyjson_is_obj(message)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "eslint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path_str;
            event.execution_time = 0.0;
            
            // Extract line and column
            yyjson_val *line = yyjson_obj_get(message, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            yyjson_val *column = yyjson_obj_get(message, "column");
            if (column && yyjson_is_num(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Extract message
            yyjson_val *msg_text = yyjson_obj_get(message, "message");
            if (msg_text && yyjson_is_str(msg_text)) {
                event.message = yyjson_get_str(msg_text);
            }
            
            // Extract rule ID
            yyjson_val *rule_id = yyjson_obj_get(message, "ruleId");
            if (rule_id && yyjson_is_str(rule_id)) {
                event.error_code = yyjson_get_str(rule_id);
                event.function_name = yyjson_get_str(rule_id);
            }
            
            // Map severity to status
            yyjson_val *severity = yyjson_obj_get(message, "severity");
            if (severity && yyjson_is_num(severity)) {
                int sev = yyjson_get_int(severity);
                switch (sev) {
                    case 2:
                        event.status = ValidationEventStatus::ERROR;
                        event.category = "lint_error";
                        event.severity = "error";
                        break;
                    case 1:
                        event.status = ValidationEventStatus::WARNING;
                        event.category = "lint_warning"; 
                        event.severity = "warning";
                        break;
                    default:
                        event.status = ValidationEventStatus::INFO;
                        event.category = "lint_info";
                        event.severity = "info";
                        break;
                }
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.category = "lint_warning";
                event.severity = "warning";
            }
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseGoTestJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track test results
    std::map<std::string, ValidationEvent> test_events;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse each JSON line
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) continue;
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Extract fields
        yyjson_val *action = yyjson_obj_get(root, "Action");
        yyjson_val *package = yyjson_obj_get(root, "Package");
        yyjson_val *test = yyjson_obj_get(root, "Test");
        yyjson_val *elapsed = yyjson_obj_get(root, "Elapsed");
        yyjson_val *output = yyjson_obj_get(root, "Output");
        
        if (!action || !yyjson_is_str(action)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        std::string action_str = yyjson_get_str(action);
        std::string package_str = package && yyjson_is_str(package) ? yyjson_get_str(package) : "";
        std::string test_str = test && yyjson_is_str(test) ? yyjson_get_str(test) : "";
        
        // Create unique test key
        std::string test_key = package_str + "::" + test_str;
        
        if (action_str == "run" && !test_str.empty()) {
            // Initialize test event
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "go_test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = package_str;
            event.test_name = test_str;
            event.function_name = test_str;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            
            test_events[test_key] = event;
        } else if ((action_str == "pass" || action_str == "fail" || action_str == "skip") && !test_str.empty()) {
            // Finalize test event
            if (test_events.find(test_key) != test_events.end()) {
                ValidationEvent &event = test_events[test_key];
                
                if (elapsed && yyjson_is_num(elapsed)) {
                    event.execution_time = yyjson_get_real(elapsed);
                }
                
                if (action_str == "pass") {
                    event.status = ValidationEventStatus::PASS;
                    event.category = "test_success";
                    event.message = "Test passed";
                } else if (action_str == "fail") {
                    event.status = ValidationEventStatus::FAIL;
                    event.category = "test_failure";
                    event.message = "Test failed";
                } else if (action_str == "skip") {
                    event.status = ValidationEventStatus::SKIP;
                    event.category = "test_skipped";
                    event.message = "Test skipped";
                }
                
                events.push_back(event);
                test_events.erase(test_key);
            }
        }
        
        yyjson_doc_free(doc);
    }
}

void ParseMakeErrors(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_function;
    
    while (std::getline(stream, line)) {
        // Parse function context: "file.c: In function 'function_name':"
        std::regex function_pattern(R"(([^:]+):\s*In function\s+'([^']+)':)");
        std::smatch func_match;
        if (std::regex_match(line, func_match, function_pattern)) {
            current_function = func_match[2].str();
            continue;
        }
        
        // Parse GCC/Clang error format: file:line:column: severity: message
        std::regex error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = current_function;
            event.message = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            
            std::string severity = match[4].str();
            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "compilation"; 
                event.severity = "warning";
            } else if (severity == "note") {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }
            
            events.push_back(event);
        }
        // Parse make failure line with target extraction
        else if (line.find("make: ***") != std::string::npos && line.find("Error") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_failure";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            
            // Extract makefile target from pattern like "[Makefile:23: build/main]"
            std::regex target_pattern(R"(\[([^:]+):(\d+):\s*([^\]]+)\])");
            std::smatch target_match;
            if (std::regex_search(line, target_match, target_pattern)) {
                event.file_path = target_match[1].str();  // Makefile
                event.line_number = std::stoi(target_match[2].str());  // Line in Makefile
                event.test_name = target_match[3].str();  // Target name (e.g., "build/main")
            }
            
            events.push_back(event);
        }
        // Parse linker errors for make builds
        else if (line.find("undefined reference") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            
            // Extract symbol name from undefined reference
            std::regex symbol_pattern(R"(undefined reference to `([^']+)')");
            std::smatch symbol_match;
            if (std::regex_search(line, symbol_match, symbol_pattern)) {
                event.function_name = symbol_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }
            
            events.push_back(event);
        }
    }
}

void ParsePytestText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Look for pytest text output patterns: "file.py::test_name STATUS"
        if (line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "test";
            
            // Parse the line format: "file.py::test_name STATUS"
            size_t separator = line.find("::");
            if (separator != std::string::npos) {
                event.file_path = line.substr(0, separator);
                
                std::string rest = line.substr(separator + 2);
                
                // Find the status at the end
                if (rest.find(" PASSED") != std::string::npos) {
                    event.status = ValidationEventStatus::PASS;
                    event.message = "Test passed";
                    event.test_name = rest.substr(0, rest.find(" PASSED"));
                } else if (rest.find(" FAILED") != std::string::npos) {
                    event.status = ValidationEventStatus::FAIL;
                    event.message = "Test failed";
                    event.test_name = rest.substr(0, rest.find(" FAILED"));
                } else if (rest.find(" SKIPPED") != std::string::npos) {
                    event.status = ValidationEventStatus::SKIP;
                    event.message = "Test skipped";
                    event.test_name = rest.substr(0, rest.find(" SKIPPED"));
                } else {
                    // Default case
                    event.status = ValidationEventStatus::INFO;
                    event.message = "Test result";
                    event.test_name = rest;
                }
            }
            
            events.push_back(event);
        }
    }
}

void ParseRuboCopJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse RuboCop JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid RuboCop JSON: root is not an object");
    }
    
    // Get files array
    yyjson_val *files = yyjson_obj_get(root, "files");
    if (!files || !yyjson_is_arr(files)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid RuboCop JSON: no files array found");
    }
    
    // Parse each file
    size_t idx, max;
    yyjson_val *file;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(files, idx, max, file) {
        if (!yyjson_is_obj(file)) continue;
        
        // Get file path
        yyjson_val *path = yyjson_obj_get(file, "path");
        if (!path || !yyjson_is_str(path)) continue;
        
        std::string file_path = yyjson_get_str(path);
        
        // Get offenses array
        yyjson_val *offenses = yyjson_obj_get(file, "offenses");
        if (!offenses || !yyjson_is_arr(offenses)) continue;
        
        // Parse each offense
        size_t offense_idx, offense_max;
        yyjson_val *offense;
        
        yyjson_arr_foreach(offenses, offense_idx, offense_max, offense) {
            if (!yyjson_is_obj(offense)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rubocop";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "code_quality";
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(offense, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else if (severity_str == "convention") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "convention";
                } else {
                    event.status = ValidationEventStatus::INFO;
                    event.severity = severity_str;
                }
            }
            
            // Get message
            yyjson_val *message = yyjson_obj_get(offense, "message");
            if (message && yyjson_is_str(message)) {
                event.message = yyjson_get_str(message);
            }
            
            // Get cop name (rule ID)
            yyjson_val *cop_name = yyjson_obj_get(offense, "cop_name");
            if (cop_name && yyjson_is_str(cop_name)) {
                event.error_code = yyjson_get_str(cop_name);
            }
            
            // Get location
            yyjson_val *location = yyjson_obj_get(offense, "location");
            if (location && yyjson_is_obj(location)) {
                yyjson_val *start_line = yyjson_obj_get(location, "start_line");
                yyjson_val *start_column = yyjson_obj_get(location, "start_column");
                
                if (start_line && yyjson_is_num(start_line)) {
                    event.line_number = yyjson_get_int(start_line);
                }
                if (start_column && yyjson_is_num(start_column)) {
                    event.column_number = yyjson_get_int(start_column);
                }
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "rubocop_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseCargoTestJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track test events
    std::map<std::string, ValidationEvent> test_events;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse each JSON line
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) continue;
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get type and event
        yyjson_val *type = yyjson_obj_get(root, "type");
        yyjson_val *event_val = yyjson_obj_get(root, "event");
        
        if (!type || !yyjson_is_str(type) || !event_val || !yyjson_is_str(event_val)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        std::string type_str = yyjson_get_str(type);
        std::string event_str = yyjson_get_str(event_val);
        
        // Handle test events
        if (type_str == "test") {
            yyjson_val *name = yyjson_obj_get(root, "name");
            if (!name || !yyjson_is_str(name)) {
                yyjson_doc_free(doc);
                continue;
            }
            
            std::string test_name = yyjson_get_str(name);
            
            if (event_str == "started") {
                // Initialize test event
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo_test";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = test_name;
                event.function_name = test_name;
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.category = "test";
                
                test_events[test_name] = event;
            } else if (event_str == "ok" || event_str == "failed" || event_str == "ignored") {
                // Finalize test event
                if (test_events.find(test_name) != test_events.end()) {
                    ValidationEvent &event = test_events[test_name];
                    
                    // Get execution time
                    yyjson_val *exec_time = yyjson_obj_get(root, "exec_time");
                    if (exec_time && yyjson_is_num(exec_time)) {
                        event.execution_time = yyjson_get_real(exec_time);
                    }
                    
                    // Set status based on event
                    if (event_str == "ok") {
                        event.status = ValidationEventStatus::PASS;
                        event.message = "Test passed";
                        event.severity = "success";
                    } else if (event_str == "failed") {
                        event.status = ValidationEventStatus::FAIL;
                        event.message = "Test failed";
                        event.severity = "error";
                        
                        // Get failure details from stdout
                        yyjson_val *stdout_val = yyjson_obj_get(root, "stdout");
                        if (stdout_val && yyjson_is_str(stdout_val)) {
                            std::string stdout_str = yyjson_get_str(stdout_val);
                            if (!stdout_str.empty()) {
                                event.message = "Test failed: " + stdout_str;
                            }
                        }
                    } else if (event_str == "ignored") {
                        event.status = ValidationEventStatus::SKIP;
                        event.message = "Test ignored";
                        event.severity = "info";
                    }
                    
                    // Set raw output and structured data
                    event.raw_output = content;
                    event.structured_data = "cargo_test_json";
                    
                    events.push_back(event);
                    test_events.erase(test_name);
                }
            }
        }
        
        yyjson_doc_free(doc);
    }
}

void ParseSwiftLintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse SwiftLint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid SwiftLint JSON: root is not an array");
    }
    
    // Parse each violation
    size_t idx, max;
    yyjson_val *violation;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, violation) {
        if (!yyjson_is_obj(violation)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "swiftlint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.line_number = -1;
        event.column_number = -1;
        event.execution_time = 0.0;
        event.category = "code_quality";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(violation, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line and column
        yyjson_val *line = yyjson_obj_get(violation, "line");
        if (line && yyjson_is_num(line)) {
            event.line_number = yyjson_get_int(line);
        }
        
        yyjson_val *column = yyjson_obj_get(violation, "column");
        if (column && yyjson_is_num(column)) {
            event.column_number = yyjson_get_int(column);
        }
        
        // Get severity
        yyjson_val *severity = yyjson_obj_get(violation, "severity");
        if (severity && yyjson_is_str(severity)) {
            std::string severity_str = yyjson_get_str(severity);
            if (severity_str == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else if (severity_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.severity = severity_str;
            }
        }
        
        // Get reason (message)
        yyjson_val *reason = yyjson_obj_get(violation, "reason");
        if (reason && yyjson_is_str(reason)) {
            event.message = yyjson_get_str(reason);
        }
        
        // Get rule ID
        yyjson_val *rule_id = yyjson_obj_get(violation, "rule_id");
        if (rule_id && yyjson_is_str(rule_id)) {
            event.error_code = yyjson_get_str(rule_id);
        }
        
        // Get type (rule type)
        yyjson_val *type = yyjson_obj_get(violation, "type");
        if (type && yyjson_is_str(type)) {
            event.suggestion = yyjson_get_str(type);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "swiftlint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParsePHPStanJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse PHPStan JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid PHPStan JSON: root is not an object");
    }
    
    // Get files object
    yyjson_val *files = yyjson_obj_get(root, "files");
    if (!files || !yyjson_is_obj(files)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid PHPStan JSON: no files object found");
    }
    
    // Parse each file
    size_t idx, max;
    yyjson_val *file_key, *file_data;
    int64_t event_id = 1;
    
    yyjson_obj_foreach(files, idx, max, file_key, file_data) {
        if (!yyjson_is_str(file_key) || !yyjson_is_obj(file_data)) continue;
        
        std::string file_path = yyjson_get_str(file_key);
        
        // Get messages array
        yyjson_val *messages = yyjson_obj_get(file_data, "messages");
        if (!messages || !yyjson_is_arr(messages)) continue;
        
        // Parse each message
        size_t msg_idx, msg_max;
        yyjson_val *message;
        
        yyjson_arr_foreach(messages, msg_idx, msg_max, message) {
            if (!yyjson_is_obj(message)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "phpstan";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "static_analysis";
            
            // Get message text
            yyjson_val *msg_text = yyjson_obj_get(message, "message");
            if (msg_text && yyjson_is_str(msg_text)) {
                event.message = yyjson_get_str(msg_text);
            }
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(message, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            }
            
            // Get ignorable status (use as severity indicator)
            yyjson_val *ignorable = yyjson_obj_get(message, "ignorable");
            if (ignorable && yyjson_is_bool(ignorable)) {
                if (yyjson_get_bool(ignorable)) {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                }
            } else {
                // Default to error
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "phpstan_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseShellCheckJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse ShellCheck JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid ShellCheck JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "shellcheck";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "shell_script";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(issue, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line number
        yyjson_val *line = yyjson_obj_get(issue, "line");
        if (line && yyjson_is_int(line)) {
            event.line_number = yyjson_get_int(line);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column = yyjson_obj_get(issue, "column");
        if (column && yyjson_is_int(column)) {
            event.column_number = yyjson_get_int(column);
        } else {
            event.column_number = -1;
        }
        
        // Get severity/level
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map ShellCheck levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else if (level_str == "style") {
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get error code (SC#### codes)
        yyjson_val *code = yyjson_obj_get(issue, "code");
        if (code && yyjson_is_str(code)) {
            event.error_code = yyjson_get_str(code);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get fix suggestions if available
        yyjson_val *fix = yyjson_obj_get(issue, "fix");
        if (fix && yyjson_is_obj(fix)) {
            yyjson_val *replacements = yyjson_obj_get(fix, "replacements");
            if (replacements && yyjson_is_arr(replacements)) {
                event.suggestion = "Fix available";
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "shellcheck_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseStylelintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse stylelint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid stylelint JSON: root is not an array");
    }
    
    // Parse each file result
    size_t idx, max;
    yyjson_val *file_result;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, file_result) {
        if (!yyjson_is_obj(file_result)) continue;
        
        // Get source file path
        yyjson_val *source = yyjson_obj_get(file_result, "source");
        if (!source || !yyjson_is_str(source)) continue;
        
        std::string file_path = yyjson_get_str(source);
        
        // Get warnings array
        yyjson_val *warnings = yyjson_obj_get(file_result, "warnings");
        if (!warnings || !yyjson_is_arr(warnings)) continue;
        
        // Parse each warning
        size_t warn_idx, warn_max;
        yyjson_val *warning;
        
        yyjson_arr_foreach(warnings, warn_idx, warn_max, warning) {
            if (!yyjson_is_obj(warning)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "stylelint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "css_style";
            event.file_path = file_path;
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(warning, "line");
            if (line && yyjson_is_int(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            // Get column number
            yyjson_val *column = yyjson_obj_get(warning, "column");
            if (column && yyjson_is_int(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(warning, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                event.severity = severity_str;
                
                // Map stylelint severity to ValidationEventStatus
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                } else {
                    event.status = ValidationEventStatus::WARNING;
                }
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            // Get rule name
            yyjson_val *rule = yyjson_obj_get(warning, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.error_code = yyjson_get_str(rule);
            }
            
            // Get message text
            yyjson_val *text = yyjson_obj_get(warning, "text");
            if (text && yyjson_is_str(text)) {
                event.message = yyjson_get_str(text);
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "stylelint_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseClippyJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSONL format (JSON Lines) - each line is a separate JSON object
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse JSON using yyjson
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) {
            continue; // Skip invalid JSON lines
        }
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get message object
        yyjson_val *message = yyjson_obj_get(root, "message");
        if (!message || !yyjson_is_obj(message)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get spans array
        yyjson_val *spans = yyjson_obj_get(message, "spans");
        if (!spans || !yyjson_is_arr(spans)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get primary span (first span with is_primary = true)
        yyjson_val *primary_span = nullptr;
        size_t idx, max;
        yyjson_val *span;
        
        yyjson_arr_foreach(spans, idx, max, span) {
            if (!yyjson_is_obj(span)) continue;
            
            yyjson_val *is_primary = yyjson_obj_get(span, "is_primary");
            if (is_primary && yyjson_is_bool(is_primary) && yyjson_get_bool(is_primary)) {
                primary_span = span;
                break;
            }
        }
        
        if (!primary_span) {
            // If no primary span found, use the first span
            primary_span = yyjson_arr_get_first(spans);
        }
        
        if (!primary_span) {
            yyjson_doc_free(doc);
            continue;
        }
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "clippy";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "code_quality";
        
        // Get file name from primary span
        yyjson_val *file_name = yyjson_obj_get(primary_span, "file_name");
        if (file_name && yyjson_is_str(file_name)) {
            event.file_path = yyjson_get_str(file_name);
        }
        
        // Get line number from primary span
        yyjson_val *line_start = yyjson_obj_get(primary_span, "line_start");
        if (line_start && yyjson_is_int(line_start)) {
            event.line_number = yyjson_get_int(line_start);
        } else {
            event.line_number = -1;
        }
        
        // Get column number from primary span
        yyjson_val *column_start = yyjson_obj_get(primary_span, "column_start");
        if (column_start && yyjson_is_int(column_start)) {
            event.column_number = yyjson_get_int(column_start);
        } else {
            event.column_number = -1;
        }
        
        // Get severity level
        yyjson_val *level = yyjson_obj_get(message, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map clippy levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warn" || level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "note" || level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get code object for error code
        yyjson_val *code = yyjson_obj_get(message, "code");
        if (code && yyjson_is_obj(code)) {
            yyjson_val *code_str = yyjson_obj_get(code, "code");
            if (code_str && yyjson_is_str(code_str)) {
                event.error_code = yyjson_get_str(code_str);
            }
        }
        
        // Get message text
        yyjson_val *message_text = yyjson_obj_get(message, "message");
        if (message_text && yyjson_is_str(message_text)) {
            event.message = yyjson_get_str(message_text);
        }
        
        // Get suggestion from primary span
        yyjson_val *suggested_replacement = yyjson_obj_get(primary_span, "suggested_replacement");
        if (suggested_replacement && yyjson_is_str(suggested_replacement)) {
            event.suggestion = yyjson_get_str(suggested_replacement);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "clippy_json";
        
        events.push_back(event);
        
        yyjson_doc_free(doc);
    }
}

void ParseMarkdownlintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse markdownlint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid markdownlint JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "markdownlint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "documentation";
        
        // Get file name
        yyjson_val *file_name = yyjson_obj_get(issue, "fileName");
        if (file_name && yyjson_is_str(file_name)) {
            event.file_path = yyjson_get_str(file_name);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "lineNumber");
        if (line_number && yyjson_is_int(line_number)) {
            event.line_number = yyjson_get_int(line_number);
        } else {
            event.line_number = -1;
        }
        
        // Markdownlint doesn't provide column in the same way - use errorRange if available
        yyjson_val *error_range = yyjson_obj_get(issue, "errorRange");
        if (error_range && yyjson_is_arr(error_range)) {
            yyjson_val *first_elem = yyjson_arr_get_first(error_range);
            if (first_elem && yyjson_is_int(first_elem)) {
                event.column_number = yyjson_get_int(first_elem);
            } else {
                event.column_number = -1;
            }
        } else {
            event.column_number = -1;
        }
        
        // Markdownlint issues are typically warnings (unless they're really severe)
        event.severity = "warning";
        event.status = ValidationEventStatus::WARNING;
        
        // Get rule names (first rule name as error code)
        yyjson_val *rule_names = yyjson_obj_get(issue, "ruleNames");
        if (rule_names && yyjson_is_arr(rule_names)) {
            yyjson_val *first_rule = yyjson_arr_get_first(rule_names);
            if (first_rule && yyjson_is_str(first_rule)) {
                event.error_code = yyjson_get_str(first_rule);
            }
        }
        
        // Get rule description as message
        yyjson_val *rule_description = yyjson_obj_get(issue, "ruleDescription");
        if (rule_description && yyjson_is_str(rule_description)) {
            event.message = yyjson_get_str(rule_description);
        }
        
        // Get error detail as suggestion if available
        yyjson_val *error_detail = yyjson_obj_get(issue, "errorDetail");
        if (error_detail && yyjson_is_str(error_detail)) {
            event.suggestion = yyjson_get_str(error_detail);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "markdownlint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseYamllintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse yamllint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid yamllint JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "yamllint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "configuration";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(issue, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line number
        yyjson_val *line = yyjson_obj_get(issue, "line");
        if (line && yyjson_is_int(line)) {
            event.line_number = yyjson_get_int(line);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column = yyjson_obj_get(issue, "column");
        if (column && yyjson_is_int(column)) {
            event.column_number = yyjson_get_int(column);
        } else {
            event.column_number = -1;
        }
        
        // Get severity level
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map yamllint levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get rule name as error code
        yyjson_val *rule = yyjson_obj_get(issue, "rule");
        if (rule && yyjson_is_str(rule)) {
            event.error_code = yyjson_get_str(rule);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "yamllint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseBanditJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse Bandit JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid Bandit JSON: root is not an object");
    }
    
    // Get results array
    yyjson_val *results = yyjson_obj_get(root, "results");
    if (!results || !yyjson_is_arr(results)) {
        yyjson_doc_free(doc);
        return; // No results to process
    }
    
    // Parse each security issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(results, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "bandit";
        event.event_type = ValidationEventType::SECURITY_FINDING;
        event.category = "security";
        
        // Get file path
        yyjson_val *filename = yyjson_obj_get(issue, "filename");
        if (filename && yyjson_is_str(filename)) {
            event.file_path = yyjson_get_str(filename);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "line_number");
        if (line_number && yyjson_is_int(line_number)) {
            event.line_number = yyjson_get_int(line_number);
        } else {
            event.line_number = -1;
        }
        
        // Get column offset (Bandit uses col_offset)
        yyjson_val *col_offset = yyjson_obj_get(issue, "col_offset");
        if (col_offset && yyjson_is_int(col_offset)) {
            event.column_number = yyjson_get_int(col_offset);
        } else {
            event.column_number = -1;
        }
        
        // Get test ID as error code
        yyjson_val *test_id = yyjson_obj_get(issue, "test_id");
        if (test_id && yyjson_is_str(test_id)) {
            event.error_code = yyjson_get_str(test_id);
        }
        
        // Get issue severity and map to status
        yyjson_val *issue_severity = yyjson_obj_get(issue, "issue_severity");
        if (issue_severity && yyjson_is_str(issue_severity)) {
            std::string severity_str = yyjson_get_str(issue_severity);
            event.severity = severity_str;
            
            // Map Bandit severity to ValidationEventStatus
            if (severity_str == "HIGH") {
                event.status = ValidationEventStatus::ERROR;
            } else if (severity_str == "MEDIUM") {
                event.status = ValidationEventStatus::WARNING;
            } else { // LOW
                event.status = ValidationEventStatus::INFO;
            }
        } else {
            event.severity = "medium";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get issue text as message
        yyjson_val *issue_text = yyjson_obj_get(issue, "issue_text");
        if (issue_text && yyjson_is_str(issue_text)) {
            event.message = yyjson_get_str(issue_text);
        }
        
        // Get test name as function context
        yyjson_val *test_name = yyjson_obj_get(issue, "test_name");
        if (test_name && yyjson_is_str(test_name)) {
            event.function_name = yyjson_get_str(test_name);
        }
        
        // Get CWE information for suggestion
        yyjson_val *issue_cwe = yyjson_obj_get(issue, "issue_cwe");
        if (issue_cwe && yyjson_is_obj(issue_cwe)) {
            yyjson_val *cwe_id = yyjson_obj_get(issue_cwe, "id");
            yyjson_val *cwe_link = yyjson_obj_get(issue_cwe, "link");
            
            if (cwe_id && yyjson_is_int(cwe_id)) {
                std::string suggestion = "CWE-" + std::to_string(yyjson_get_int(cwe_id));
                if (cwe_link && yyjson_is_str(cwe_link)) {
                    suggestion += ": " + std::string(yyjson_get_str(cwe_link));
                }
                event.suggestion = suggestion;
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "bandit_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseSpotBugsJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse SpotBugs JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid SpotBugs JSON: root is not an object");
    }
    
    // Get BugCollection object
    yyjson_val *bug_collection = yyjson_obj_get(root, "BugCollection");
    if (!bug_collection || !yyjson_is_obj(bug_collection)) {
        yyjson_doc_free(doc);
        return; // No bug collection to process
    }
    
    // Get BugInstance array
    yyjson_val *bug_instances = yyjson_obj_get(bug_collection, "BugInstance");
    if (!bug_instances || !yyjson_is_arr(bug_instances)) {
        yyjson_doc_free(doc);
        return; // No bug instances to process
    }
    
    // Parse each bug instance
    size_t idx, max;
    yyjson_val *bug;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(bug_instances, idx, max, bug) {
        if (!yyjson_is_obj(bug)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "spotbugs";
        event.event_type = ValidationEventType::LINT_ISSUE;
        
        // Get bug type as error code
        yyjson_val *type = yyjson_obj_get(bug, "type");
        if (type && yyjson_is_str(type)) {
            event.error_code = yyjson_get_str(type);
        }
        
        // Get category and map event type
        yyjson_val *category = yyjson_obj_get(bug, "category");
        if (category && yyjson_is_str(category)) {
            std::string category_str = yyjson_get_str(category);
            event.category = category_str;
            
            // Map SpotBugs categories to event types
            if (category_str == "SECURITY") {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (category_str == "PERFORMANCE") {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            } else if (category_str == "CORRECTNESS") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "correctness";
            } else if (category_str == "BAD_PRACTICE") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "code_quality";
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "static_analysis";
            }
        } else {
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "static_analysis";
        }
        
        // Get priority and map to status
        yyjson_val *priority = yyjson_obj_get(bug, "priority");
        if (priority && yyjson_is_str(priority)) {
            std::string priority_str = yyjson_get_str(priority);
            event.severity = priority_str;
            
            // Map SpotBugs priority to ValidationEventStatus (1=highest, 3=lowest)
            if (priority_str == "1") {
                event.status = ValidationEventStatus::ERROR;
            } else if (priority_str == "2") {
                event.status = ValidationEventStatus::WARNING;
            } else { // priority 3
                event.status = ValidationEventStatus::INFO;
            }
        } else {
            event.severity = "medium";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get short message as main message
        yyjson_val *short_message = yyjson_obj_get(bug, "ShortMessage");
        if (short_message && yyjson_is_str(short_message)) {
            event.message = yyjson_get_str(short_message);
        }
        
        // Get long message as suggestion
        yyjson_val *long_message = yyjson_obj_get(bug, "LongMessage");
        if (long_message && yyjson_is_str(long_message)) {
            event.suggestion = yyjson_get_str(long_message);
        }
        
        // Get source line information
        yyjson_val *source_line = yyjson_obj_get(bug, "SourceLine");
        if (source_line && yyjson_is_obj(source_line)) {
            // Check if this is the primary source line
            yyjson_val *primary = yyjson_obj_get(source_line, "primary");
            if (primary && yyjson_is_bool(primary) && yyjson_get_bool(primary)) {
                // Get source path
                yyjson_val *sourcepath = yyjson_obj_get(source_line, "sourcepath");
                if (sourcepath && yyjson_is_str(sourcepath)) {
                    event.file_path = yyjson_get_str(sourcepath);
                }
                
                // Get line number (use start line)
                yyjson_val *start_line = yyjson_obj_get(source_line, "start");
                if (start_line && yyjson_is_str(start_line)) {
                    event.line_number = std::stoll(yyjson_get_str(start_line));
                } else {
                    event.line_number = -1;
                }
                
                // SpotBugs doesn't provide column information
                event.column_number = -1;
            }
        }
        
        // Get method information for function context
        yyjson_val *method = yyjson_obj_get(bug, "Method");
        if (method && yyjson_is_obj(method)) {
            yyjson_val *primary = yyjson_obj_get(method, "primary");
            if (primary && yyjson_is_bool(primary) && yyjson_get_bool(primary)) {
                yyjson_val *method_name = yyjson_obj_get(method, "name");
                yyjson_val *classname = yyjson_obj_get(method, "classname");
                
                if (method_name && yyjson_is_str(method_name) && classname && yyjson_is_str(classname)) {
                    event.function_name = std::string(yyjson_get_str(classname)) + "." + std::string(yyjson_get_str(method_name));
                }
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "spotbugs_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseKtlintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse ktlint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid ktlint JSON: root is not an array");
    }
    
    // Parse each file entry
    size_t file_idx, file_max;
    yyjson_val *file_entry;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, file_idx, file_max, file_entry) {
        if (!yyjson_is_obj(file_entry)) continue;
        
        // Get file path
        std::string file_path;
        yyjson_val *file = yyjson_obj_get(file_entry, "file");
        if (file && yyjson_is_str(file)) {
            file_path = yyjson_get_str(file);
        }
        
        // Get errors array
        yyjson_val *errors = yyjson_obj_get(file_entry, "errors");
        if (!errors || !yyjson_is_arr(errors)) continue;
        
        // Parse each error
        size_t error_idx, error_max;
        yyjson_val *error;
        
        yyjson_arr_foreach(errors, error_idx, error_max, error) {
            if (!yyjson_is_obj(error)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ktlint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "code_style";
            event.file_path = file_path;
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(error, "line");
            if (line && yyjson_is_int(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            // Get column number
            yyjson_val *column = yyjson_obj_get(error, "column");
            if (column && yyjson_is_int(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Get rule name as error code
            yyjson_val *rule = yyjson_obj_get(error, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.error_code = yyjson_get_str(rule);
            }
            
            // Get message
            yyjson_val *message = yyjson_obj_get(error, "message");
            if (message && yyjson_is_str(message)) {
                event.message = yyjson_get_str(message);
            }
            
            // ktlint doesn't provide explicit severity, so we infer from rule types
            std::string rule_str = event.error_code;
            if (rule_str.find("max-line-length") != std::string::npos || 
                rule_str.find("no-wildcard-imports") != std::string::npos) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else if (rule_str.find("indent") != std::string::npos || 
                       rule_str.find("final-newline") != std::string::npos) {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else {
                // Default to warning for style issues
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "ktlint_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseHadolintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse hadolint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid hadolint JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "hadolint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "dockerfile";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(issue, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line number
        yyjson_val *line = yyjson_obj_get(issue, "line");
        if (line && yyjson_is_int(line)) {
            event.line_number = yyjson_get_int(line);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column = yyjson_obj_get(issue, "column");
        if (column && yyjson_is_int(column)) {
            event.column_number = yyjson_get_int(column);
        } else {
            event.column_number = -1;
        }
        
        // Get code as error code
        yyjson_val *code = yyjson_obj_get(issue, "code");
        if (code && yyjson_is_str(code)) {
            event.error_code = yyjson_get_str(code);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get level and map to status
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map hadolint levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else if (level_str == "style") {
                event.status = ValidationEventStatus::WARNING; // Treat style as warning
            } else {
                event.status = ValidationEventStatus::WARNING; // Default to warning
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "hadolint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseLintrJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse lintr JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid lintr JSON: root is not an array");
    }
    
    // Parse each lint issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "lintr";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "r_code_style";
        
        // Get filename
        yyjson_val *filename = yyjson_obj_get(issue, "filename");
        if (filename && yyjson_is_str(filename)) {
            event.file_path = yyjson_get_str(filename);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "line_number");
        if (line_number && yyjson_is_int(line_number)) {
            event.line_number = yyjson_get_int(line_number);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column_number = yyjson_obj_get(issue, "column_number");
        if (column_number && yyjson_is_int(column_number)) {
            event.column_number = yyjson_get_int(column_number);
        } else {
            event.column_number = -1;
        }
        
        // Get linter name as error code
        yyjson_val *linter = yyjson_obj_get(issue, "linter");
        if (linter && yyjson_is_str(linter)) {
            event.error_code = yyjson_get_str(linter);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get type and map to status
        yyjson_val *type = yyjson_obj_get(issue, "type");
        if (type && yyjson_is_str(type)) {
            std::string type_str = yyjson_get_str(type);
            event.severity = type_str;
            
            // Map lintr types to ValidationEventStatus
            if (type_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (type_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (type_str == "style") {
                event.status = ValidationEventStatus::WARNING; // Treat style as warning
            } else {
                event.status = ValidationEventStatus::INFO; // Default for other types
            }
        } else {
            event.severity = "style";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get line content as suggestion (if available)
        yyjson_val *line_content = yyjson_obj_get(issue, "line");
        if (line_content && yyjson_is_str(line_content)) {
            event.suggestion = "Code: " + std::string(yyjson_get_str(line_content));
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "lintr_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseSqlfluffJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse sqlfluff JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid sqlfluff JSON: root is not an array");
    }
    
    // Parse each file entry
    size_t file_idx, file_max;
    yyjson_val *file_entry;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, file_idx, file_max, file_entry) {
        if (!yyjson_is_obj(file_entry)) continue;
        
        // Get filepath
        yyjson_val *filepath = yyjson_obj_get(file_entry, "filepath");
        if (!filepath || !yyjson_is_str(filepath)) continue;
        
        std::string file_path = yyjson_get_str(filepath);
        
        // Get violations array
        yyjson_val *violations = yyjson_obj_get(file_entry, "violations");
        if (!violations || !yyjson_is_arr(violations)) continue;
        
        // Parse each violation
        size_t viol_idx, viol_max;
        yyjson_val *violation;
        
        yyjson_arr_foreach(violations, viol_idx, viol_max, violation) {
            if (!yyjson_is_obj(violation)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "sqlfluff";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "sql_style";
            event.file_path = file_path;
            
            // Get line number
            yyjson_val *line_no = yyjson_obj_get(violation, "line_no");
            if (line_no && yyjson_is_int(line_no)) {
                event.line_number = yyjson_get_int(line_no);
            } else {
                event.line_number = -1;
            }
            
            // Get column position  
            yyjson_val *line_pos = yyjson_obj_get(violation, "line_pos");
            if (line_pos && yyjson_is_int(line_pos)) {
                event.column_number = yyjson_get_int(line_pos);
            } else {
                event.column_number = -1;
            }
            
            // Get rule code as error_code
            yyjson_val *code = yyjson_obj_get(violation, "code");
            if (code && yyjson_is_str(code)) {
                event.error_code = yyjson_get_str(code);
            }
            
            // Get rule name for context
            yyjson_val *rule = yyjson_obj_get(violation, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.function_name = yyjson_get_str(rule);
            }
            
            // Get description as message
            yyjson_val *description = yyjson_obj_get(violation, "description");
            if (description && yyjson_is_str(description)) {
                event.message = yyjson_get_str(description);
            }
            
            // All sqlfluff violations are warnings by default
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            
            // Create suggestion from rule name
            if (!event.function_name.empty()) {
                event.suggestion = "Rule: " + event.function_name;
            }
            
            event.raw_output = content;
            event.structured_data = "sqlfluff_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseTflintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse tflint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid tflint JSON: root is not an object");
    }
    
    // Get issues array
    yyjson_val *issues = yyjson_obj_get(root, "issues");
    if (!issues || !yyjson_is_arr(issues)) {
        yyjson_doc_free(doc);
        return; // No issues to process
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(issues, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "tflint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "infrastructure";
        
        // Get rule information
        yyjson_val *rule = yyjson_obj_get(issue, "rule");
        if (rule && yyjson_is_obj(rule)) {
            // Get rule name as error code
            yyjson_val *rule_name = yyjson_obj_get(rule, "name");
            if (rule_name && yyjson_is_str(rule_name)) {
                event.error_code = yyjson_get_str(rule_name);
                event.function_name = yyjson_get_str(rule_name);
            }
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(rule, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                event.severity = severity_str;
                
                // Map tflint severities to ValidationEventStatus
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                } else if (severity_str == "notice") {
                    event.status = ValidationEventStatus::INFO;
                } else {
                    event.status = ValidationEventStatus::WARNING; // Default
                }
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get range information
        yyjson_val *range = yyjson_obj_get(issue, "range");
        if (range && yyjson_is_obj(range)) {
            // Get filename
            yyjson_val *filename = yyjson_obj_get(range, "filename");
            if (filename && yyjson_is_str(filename)) {
                event.file_path = yyjson_get_str(filename);
            }
            
            // Get start position
            yyjson_val *start = yyjson_obj_get(range, "start");
            if (start && yyjson_is_obj(start)) {
                yyjson_val *line = yyjson_obj_get(start, "line");
                if (line && yyjson_is_int(line)) {
                    event.line_number = yyjson_get_int(line);
                } else {
                    event.line_number = -1;
                }
                
                yyjson_val *column = yyjson_obj_get(start, "column");
                if (column && yyjson_is_int(column)) {
                    event.column_number = yyjson_get_int(column);
                } else {
                    event.column_number = -1;
                }
            }
        }
        
        // Create suggestion from rule name
        if (!event.function_name.empty()) {
            event.suggestion = "Rule: " + event.function_name;
        }
        
        event.raw_output = content;
        event.structured_data = "tflint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseKubeScoreJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse kube-score JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid kube-score JSON: root is not an array");
    }
    
    // Parse each Kubernetes object
    size_t obj_idx, obj_max;
    yyjson_val *k8s_object;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, obj_idx, obj_max, k8s_object) {
        if (!yyjson_is_obj(k8s_object)) continue;
        
        // Get object metadata
        std::string object_name;
        std::string file_name;
        std::string resource_kind;
        std::string namespace_name = "default";
        int line_number = -1;
        
        yyjson_val *obj_name = yyjson_obj_get(k8s_object, "object_name");
        if (obj_name && yyjson_is_str(obj_name)) {
            object_name = yyjson_get_str(obj_name);
        }
        
        yyjson_val *file_name_val = yyjson_obj_get(k8s_object, "file_name");
        if (file_name_val && yyjson_is_str(file_name_val)) {
            file_name = yyjson_get_str(file_name_val);
        }
        
        yyjson_val *file_row = yyjson_obj_get(k8s_object, "file_row");
        if (file_row && yyjson_is_int(file_row)) {
            line_number = yyjson_get_int(file_row);
        }
        
        // Get resource kind from type_meta
        yyjson_val *type_meta = yyjson_obj_get(k8s_object, "type_meta");
        if (type_meta && yyjson_is_obj(type_meta)) {
            yyjson_val *kind = yyjson_obj_get(type_meta, "kind");
            if (kind && yyjson_is_str(kind)) {
                resource_kind = yyjson_get_str(kind);
            }
        }
        
        // Get namespace from object_meta
        yyjson_val *object_meta = yyjson_obj_get(k8s_object, "object_meta");
        if (object_meta && yyjson_is_obj(object_meta)) {
            yyjson_val *ns = yyjson_obj_get(object_meta, "namespace");
            if (ns && yyjson_is_str(ns)) {
                namespace_name = yyjson_get_str(ns);
            }
        }
        
        // Get checks array
        yyjson_val *checks = yyjson_obj_get(k8s_object, "checks");
        if (!checks || !yyjson_is_arr(checks)) continue;
        
        // Parse each check
        size_t check_idx, check_max;
        yyjson_val *check;
        
        yyjson_arr_foreach(checks, check_idx, check_max, check) {
            if (!yyjson_is_obj(check)) continue;
            
            // Get grade to determine if this is an issue
            yyjson_val *grade = yyjson_obj_get(check, "grade");
            if (!grade || !yyjson_is_str(grade)) continue;
            
            std::string grade_str = yyjson_get_str(grade);
            
            // Skip OK checks unless they have comments
            yyjson_val *comments = yyjson_obj_get(check, "comments");
            bool has_comments = comments && yyjson_is_arr(comments) && yyjson_arr_size(comments) > 0;
            
            if (grade_str == "OK" && !has_comments) continue;
            
            // Get check information
            yyjson_val *check_info = yyjson_obj_get(check, "check");
            std::string check_id;
            std::string check_name;
            std::string check_comment;
            
            if (check_info && yyjson_is_obj(check_info)) {
                yyjson_val *id = yyjson_obj_get(check_info, "id");
                if (id && yyjson_is_str(id)) {
                    check_id = yyjson_get_str(id);
                }
                
                yyjson_val *name = yyjson_obj_get(check_info, "name");
                if (name && yyjson_is_str(name)) {
                    check_name = yyjson_get_str(name);
                }
                
                yyjson_val *comment = yyjson_obj_get(check_info, "comment");
                if (comment && yyjson_is_str(comment)) {
                    check_comment = yyjson_get_str(comment);
                }
            }
            
            // Create validation event for each comment or one general event
            if (has_comments) {
                size_t comment_idx, comment_max;
                yyjson_val *comment_obj;
                
                yyjson_arr_foreach(comments, comment_idx, comment_max, comment_obj) {
                    if (!yyjson_is_obj(comment_obj)) continue;
                    
                    ValidationEvent event;
                    event.event_id = event_id++;
                    event.tool_name = "kube-score";
                    event.event_type = ValidationEventType::LINT_ISSUE;
                    event.category = "kubernetes";
                    event.file_path = file_name;
                    event.line_number = line_number;
                    event.column_number = -1;
                    event.error_code = check_id;
                    event.function_name = object_name + " (" + resource_kind + ")";
                    
                    // Map grade to status
                    if (grade_str == "CRITICAL") {
                        event.status = ValidationEventStatus::ERROR;
                        event.severity = "critical";
                    } else if (grade_str == "WARNING") {
                        event.status = ValidationEventStatus::WARNING;
                        event.severity = "warning";
                    } else {
                        event.status = ValidationEventStatus::INFO;
                        event.severity = "info";
                    }
                    
                    // Get comment details
                    yyjson_val *summary = yyjson_obj_get(comment_obj, "summary");
                    yyjson_val *description = yyjson_obj_get(comment_obj, "description");
                    yyjson_val *path = yyjson_obj_get(comment_obj, "path");
                    
                    if (summary && yyjson_is_str(summary)) {
                        event.message = yyjson_get_str(summary);
                    } else {
                        event.message = check_name;
                    }
                    
                    if (description && yyjson_is_str(description)) {
                        event.suggestion = yyjson_get_str(description);
                    }
                    
                    // Add path context if available
                    if (path && yyjson_is_str(path) && strlen(yyjson_get_str(path)) > 0) {
                        event.test_name = yyjson_get_str(path);
                    }
                    
                    event.raw_output = content;
                    event.structured_data = "kube_score_json";
                    
                    events.push_back(event);
                }
            } else {
                // Create general event for non-OK checks without specific comments
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "kube-score";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "kubernetes";
                event.file_path = file_name;
                event.line_number = line_number;
                event.column_number = -1;
                event.error_code = check_id;
                event.function_name = object_name + " (" + resource_kind + ")";
                event.message = check_name;
                event.suggestion = check_comment;
                
                // Map grade to status
                if (grade_str == "CRITICAL") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "critical";
                } else if (grade_str == "WARNING") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else {
                    event.status = ValidationEventStatus::INFO;
                    event.severity = "info";
                }
                
                event.raw_output = content;
                event.structured_data = "kube_score_json";
                
                events.push_back(event);
            }
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseCMakeBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse GCC/Clang error format: file:line:column: severity: message
        std::regex cpp_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, cpp_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;
            
            std::string severity = match[4].str();
            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "compilation";
                event.severity = "warning";
            } else if (severity == "note") {
                event.status = ValidationEventStatus::ERROR;  // Treat notes as errors for CMake builds
                event.category = "compilation";
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }
            
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake configuration errors
        else if (line.find("CMake Error") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "configuration";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract file info from CMake errors like "CMake Error at CMakeLists.txt:25"
            std::regex cmake_error_pattern(R"(CMake Error at ([^:]+):(\d+))");
            std::smatch cmake_match;
            if (std::regex_search(line, cmake_match, cmake_error_pattern)) {
                event.file_path = cmake_match[1].str();
                event.line_number = std::stoi(cmake_match[2].str());
            }
            
            event.message = content;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake warnings
        else if (line.find("CMake Warning") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.category = "configuration";
            event.severity = "warning";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract file info from CMake warnings
            std::regex cmake_warning_pattern(R"(CMake Warning at ([^:]+):(\d+))");
            std::smatch cmake_match;
            if (std::regex_search(line, cmake_match, cmake_warning_pattern)) {
                event.file_path = cmake_match[1].str();
                event.line_number = std::stoi(cmake_match[2].str());
            }
            
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse linker errors (both with and without /usr/bin/ld: prefix)
        else if (line.find("undefined reference") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract symbol name from linker error
            std::regex linker_pattern(R"(undefined reference to `([^']+)')");
            std::smatch linker_match;
            if (std::regex_search(line, linker_match, linker_pattern)) {
                event.function_name = linker_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }
            
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse collect2 linker errors
        else if (line.find("collect2: error:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse gmake errors
        else if (line.find("gmake[") != std::string::npos && line.find("***") != std::string::npos && line.find("Error") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_failure";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake configuration summary errors  
        else if (line.find("-- Configuring incomplete, errors occurred!") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "configuration";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
    }
}

void ParseGenericLint(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse generic lint format: file:line:column: level: message
        std::regex lint_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|info|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, lint_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "lint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;
            
            std::string level = match[4].str();
            if (level == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "lint_error";
                event.severity = "error";
            } else if (level == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "lint_warning";
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "lint_info";
                event.severity = "info";
            }
            
            events.push_back(event);
        }
    }
    
    // If no events were created, add a basic summary
    if (events.empty()) {
        ValidationEvent summary_event;
        summary_event.event_id = 1;
        summary_event.tool_name = "lint";
        summary_event.event_type = ValidationEventType::LINT_ISSUE;
        summary_event.status = ValidationEventStatus::INFO;
        summary_event.category = "lint_summary";
        summary_event.message = "Generic lint output parsed (no issues found)";
        summary_event.line_number = -1;
        summary_event.column_number = -1;
        summary_event.execution_time = 0.0;
        
        events.push_back(summary_event);
    }
}

// Parse test results implementation for string input
unique_ptr<FunctionData> ParseTestResultsBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
    auto bind_data = make_uniq<ReadTestResultsBindData>();
    
    // Get content parameter (required)
    if (input.inputs.empty()) {
        throw BinderException("parse_test_results requires at least one parameter (content)");
    }
    bind_data->source = input.inputs[0].ToString();
    
    // Get format parameter (optional, defaults to auto)
    if (input.inputs.size() > 1) {
        bind_data->format = StringToTestResultFormat(input.inputs[1].ToString());
    } else {
        bind_data->format = TestResultFormat::AUTO;
    }
    
    // Define return schema (same as read_test_results)
    return_types = {
        LogicalType::BIGINT,   // event_id
        LogicalType::VARCHAR,  // tool_name
        LogicalType::VARCHAR,  // event_type
        LogicalType::VARCHAR,  // file_path
        LogicalType::INTEGER,  // line_number
        LogicalType::INTEGER,  // column_number
        LogicalType::VARCHAR,  // function_name
        LogicalType::VARCHAR,  // status
        LogicalType::VARCHAR,  // severity
        LogicalType::VARCHAR,  // category
        LogicalType::VARCHAR,  // message
        LogicalType::VARCHAR,  // suggestion
        LogicalType::VARCHAR,  // error_code
        LogicalType::VARCHAR,  // test_name
        LogicalType::DOUBLE,   // execution_time
        LogicalType::VARCHAR,  // raw_output
        LogicalType::VARCHAR   // structured_data
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number",
        "column_number", "function_name", "status", "severity", "category",
        "message", "suggestion", "error_code", "test_name", "execution_time",
        "raw_output", "structured_data"
    };
    
    return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ParseTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<ReadTestResultsBindData>();
    auto global_state = make_uniq<ReadTestResultsGlobalState>();
    
    // Use source directly as content (no file reading)
    std::string content = bind_data.source;
    
    // Auto-detect format if needed
    TestResultFormat format = bind_data.format;
    if (format == TestResultFormat::AUTO) {
        format = DetectTestResultFormat(content);
    }
    
    // Parse content based on detected format (same logic as read_test_results)
    switch (format) {
        case TestResultFormat::PYTEST_JSON:
            ParsePytestJSON(content, global_state->events);
            break;
        case TestResultFormat::DUCKDB_TEST:
            ParseDuckDBTestOutput(content, global_state->events);
            break;
        case TestResultFormat::ESLINT_JSON:
            ParseESLintJSON(content, global_state->events);
            break;
        case TestResultFormat::GOTEST_JSON:
            ParseGoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::MAKE_ERROR:
            ParseMakeErrors(content, global_state->events);
            break;
        case TestResultFormat::PYTEST_TEXT:
            ParsePytestText(content, global_state->events);
            break;
        case TestResultFormat::GENERIC_LINT:
            ParseGenericLint(content, global_state->events);
            break;
        case TestResultFormat::RUBOCOP_JSON:
            ParseRuboCopJSON(content, global_state->events);
            break;
        case TestResultFormat::CARGO_TEST_JSON:
            ParseCargoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::SWIFTLINT_JSON:
            ParseSwiftLintJSON(content, global_state->events);
            break;
        case TestResultFormat::PHPSTAN_JSON:
            ParsePHPStanJSON(content, global_state->events);
            break;
        case TestResultFormat::SHELLCHECK_JSON:
            ParseShellCheckJSON(content, global_state->events);
            break;
        case TestResultFormat::STYLELINT_JSON:
            ParseStylelintJSON(content, global_state->events);
            break;
        case TestResultFormat::CLIPPY_JSON:
            ParseClippyJSON(content, global_state->events);
            break;
        case TestResultFormat::MARKDOWNLINT_JSON:
            ParseMarkdownlintJSON(content, global_state->events);
            break;
        case TestResultFormat::YAMLLINT_JSON:
            ParseYamllintJSON(content, global_state->events);
            break;
        case TestResultFormat::BANDIT_JSON:
            ParseBanditJSON(content, global_state->events);
            break;
        case TestResultFormat::SPOTBUGS_JSON:
            ParseSpotBugsJSON(content, global_state->events);
            break;
        case TestResultFormat::KTLINT_JSON:
            ParseKtlintJSON(content, global_state->events);
            break;
        case TestResultFormat::HADOLINT_JSON:
            ParseHadolintJSON(content, global_state->events);
            break;
        case TestResultFormat::LINTR_JSON:
            ParseLintrJSON(content, global_state->events);
            break;
        case TestResultFormat::SQLFLUFF_JSON:
            ParseSqlfluffJSON(content, global_state->events);
            break;
        case TestResultFormat::TFLINT_JSON:
            ParseTflintJSON(content, global_state->events);
            break;
        case TestResultFormat::KUBE_SCORE_JSON:
            ParseKubeScoreJSON(content, global_state->events);
            break;
        case TestResultFormat::CMAKE_BUILD:
            ParseCMakeBuild(content, global_state->events);
            break;
        case TestResultFormat::PYTHON_BUILD:
            ParsePythonBuild(content, global_state->events);
            break;
        case TestResultFormat::NODE_BUILD:
            ParseNodeBuild(content, global_state->events);
            break;
        case TestResultFormat::CARGO_BUILD:
            ParseCargoBuild(content, global_state->events);
            break;
        case TestResultFormat::MAVEN_BUILD:
            ParseMavenBuild(content, global_state->events);
            break;
        case TestResultFormat::GRADLE_BUILD:
            ParseGradleBuild(content, global_state->events);
            break;
        case TestResultFormat::MSBUILD:
            ParseMSBuild(content, global_state->events);
            break;
        case TestResultFormat::JUNIT_TEXT:
            ParseJUnitText(content, global_state->events);
            break;
        case TestResultFormat::VALGRIND:
            ParseValgrind(content, global_state->events);
            break;
        case TestResultFormat::GDB_LLDB:
            ParseGdbLldb(content, global_state->events);
            break;
        case TestResultFormat::RSPEC_TEXT:
            ParseRSpecText(content, global_state->events);
            break;
        case TestResultFormat::MOCHA_CHAI_TEXT:
            ParseMochaChai(content, global_state->events);
            break;
        case TestResultFormat::GTEST_TEXT:
            ParseGoogleTest(content, global_state->events);
            break;
        case TestResultFormat::NUNIT_XUNIT_TEXT:
            ParseNUnitXUnit(content, global_state->events);
            break;
        default:
            // For unknown formats, don't create any events
            break;
    }
    
    return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ParseTestResultsInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                              GlobalTableFunctionState *global_state) {
    return make_uniq<ReadTestResultsLocalState>();
}

void ParseTestResultsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &global_state = data_p.global_state->Cast<ReadTestResultsGlobalState>();
    auto &local_state = data_p.local_state->Cast<ReadTestResultsLocalState>();
    
    // Populate output chunk (same logic as read_test_results)
    PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE);
    
    // Update offset for next chunk
    local_state.chunk_offset += output.size();
}

TableFunction GetReadTestResultsFunction() {
    TableFunction function("read_test_results", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                          ReadTestResultsFunction, ReadTestResultsBind, ReadTestResultsInitGlobal, ReadTestResultsInitLocal);
    
    return function;
}

TableFunction GetParseTestResultsFunction() {
    TableFunction function("parse_test_results", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                          ParseTestResultsFunction, ParseTestResultsBind, ParseTestResultsInitGlobal, ParseTestResultsInitLocal);
    
    return function;
}

void ParsePythonBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_test;
    std::string current_package;
    
    while (std::getline(stream, line)) {
        // Parse pip wheel building errors
        if (line.find("ERROR: Failed building wheel for") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pip";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "package_build";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract package name
            std::regex package_pattern(R"(ERROR: Failed building wheel for ([^\s,]+))");
            std::smatch package_match;
            if (std::regex_search(line, package_match, package_pattern)) {
                event.test_name = package_match[1].str();
            }
            
            events.push_back(event);
        }
        // Parse setuptools/distutils compilation errors (C extension errors)
        else if (line.find("error:") != std::string::npos && 
                (line.find(".c:") != std::string::npos || line.find(".cpp:") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "compilation";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract file and line info from C compilation errors
            std::regex c_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*error:\s*(.+))");
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_error_pattern)) {
                event.file_path = c_match[1].str();
                event.line_number = std::stoi(c_match[2].str());
                event.column_number = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            
            events.push_back(event);
        }
        // Parse Python test failures (pytest format)
        else if (line.find("FAILED ") != std::string::npos && line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract test name
            std::regex test_pattern(R"(FAILED\s+([^:]+::[\w_]+))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.file_path = event.test_name.substr(0, sep_pos);
                }
            }
            
            events.push_back(event);
        }
        // Parse Python test errors
        else if (line.find("ERROR ") != std::string::npos && line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::ERROR;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract test name
            std::regex test_pattern(R"(ERROR\s+([^:]+::[\w_]+))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.file_path = event.test_name.substr(0, sep_pos);
                }
            }
            
            events.push_back(event);
        }
        // Parse assertion errors with file:line info
        else if (line.find("AssertionError:") != std::string::npos || line.find("TypeError:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "assertion";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // These are usually part of a test failure context
            if (!current_test.empty()) {
                event.test_name = current_test;
            }
            
            events.push_back(event);
        }
        // Parse file:line test location info
        else if (std::regex_match(line, std::regex(R"(\s*([^:]+):(\d+):\s+in\s+\w+)"))) {
            std::regex location_pattern(R"(\s*([^:]+):(\d+):\s+in\s+(\w+))");
            std::smatch loc_match;
            if (std::regex_search(line, loc_match, location_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "pytest";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = ValidationEventStatus::INFO;
                event.category = "traceback";
                event.severity = "info";
                event.file_path = loc_match[1].str();
                event.line_number = std::stoi(loc_match[2].str());
                event.function_name = loc_match[3].str();
                event.message = line;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "python_build";
                
                events.push_back(event);
            }
        }
        // Parse setup.py command failures
        else if (line.find("error: command") != std::string::npos && line.find("failed with exit status") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_command";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract command name
            std::regex cmd_pattern(R"(error: command '([^']+)')");
            std::smatch cmd_match;
            if (std::regex_search(line, cmd_match, cmd_pattern)) {
                event.function_name = cmd_match[1].str();
            }
            
            events.push_back(event);
        }
        // Parse C extension warnings
        else if (line.find("warning:") != std::string::npos && 
                (line.find(".c:") != std::string::npos || line.find(".cpp:") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.category = "compilation";
            event.severity = "warning";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract file and line info
            std::regex c_warn_pattern(R"(([^:]+):(\d+):(\d*):?\s*warning:\s*(.+))");
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_warn_pattern)) {
                event.file_path = c_match[1].str();
                event.line_number = std::stoi(c_match[2].str());
                event.column_number = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            
            events.push_back(event);
        }
    }
}

void ParseNodeBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_test_file;
    
    while (std::getline(stream, line)) {
        // Parse npm/yarn errors
        if (line.find("npm ERR!") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "npm";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "package_manager";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract error codes
            if (line.find("npm ERR! code") != std::string::npos) {
                std::regex code_pattern(R"(npm ERR! code ([A-Z_]+))");
                std::smatch code_match;
                if (std::regex_search(line, code_match, code_pattern)) {
                    event.error_code = code_match[1].str();
                }
            }
            
            events.push_back(event);
        }
        // Parse yarn errors  
        else if (line.find("error ") != std::string::npos && line.find("yarn") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yarn";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "package_manager";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Jest test failures
        else if (line.find("FAIL ") != std::string::npos && line.find(".test.js") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract test file
            std::regex test_file_pattern(R"(FAIL\s+([^\s]+\.test\.js))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_file_pattern)) {
                event.file_path = test_match[1].str();
                current_test_file = event.file_path;
            }
            
            events.push_back(event);
        }
        // Parse Jest test suite failures
        else if (line.find("● Test suite failed to run") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::ERROR;
            event.category = "test_suite";
            event.severity = "error";
            event.message = line;
            event.file_path = current_test_file;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Jest individual test failures
        else if (line.find("●") != std::string::npos && line.find("›") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "test_case";
            event.severity = "error";
            event.message = line;
            event.file_path = current_test_file;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract test name
            std::regex test_name_pattern(R"(●\s+([^›]+)\s+›\s+(.+))");
            std::smatch name_match;
            if (std::regex_search(line, name_match, test_name_pattern)) {
                event.test_name = name_match[1].str() + " › " + name_match[2].str();
            }
            
            events.push_back(event);
        }
        // Parse ESLint errors and warnings
        else if (std::regex_match(line, std::regex(R"(\s*\d+:\d+\s+(error|warning)\s+.+)"))) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "eslint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Parse ESLint format: "  15:5   error    'console' is not defined    no-undef"
            std::regex eslint_pattern(R"(\s*(\d+):(\d+)\s+(error|warning)\s+(.+?)\s+([^\s]+)$)");
            std::smatch eslint_match;
            if (std::regex_search(line, eslint_match, eslint_pattern)) {
                event.line_number = std::stoi(eslint_match[1].str());
                event.column_number = std::stoi(eslint_match[2].str());
                std::string severity = eslint_match[3].str();
                event.message = eslint_match[4].str();
                event.error_code = eslint_match[5].str();
                
                if (severity == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.category = "lint_error";
                    event.severity = "error";
                } else {
                    event.status = ValidationEventStatus::WARNING;
                    event.category = "lint_warning";
                    event.severity = "warning";
                }
            }
            
            events.push_back(event);
        }
        // Parse file paths for ESLint
        else if (line.find("/") != std::string::npos && line.find(".js") != std::string::npos && 
                line.find("  ") != 0 && line.find("error") == std::string::npos) {
            // This is likely a file path line like "/home/user/myproject/src/index.js"
            // Store it for the next ESLint errors
            if (!events.empty() && events.back().tool_name == "eslint" && events.back().file_path.empty()) {
                events.back().file_path = line;
            }
        }
        // Parse Webpack errors
        else if (line.find("ERROR in") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "webpack";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "bundling";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file and line info: "ERROR in ./src/components/Header.js"
            std::regex webpack_error_pattern(R"(ERROR in (.+?)(?:\s+(\d+):(\d+))?$)");
            std::smatch webpack_match;
            if (std::regex_search(line, webpack_match, webpack_error_pattern)) {
                event.file_path = webpack_match[1].str();
                if (webpack_match[2].matched) {
                    event.line_number = std::stoi(webpack_match[2].str());
                    event.column_number = std::stoi(webpack_match[3].str());
                }
            }
            
            events.push_back(event);
        }
        // Parse Webpack warnings
        else if (line.find("WARNING in") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "webpack";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.category = "bundling";
            event.severity = "warning";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file info
            std::regex webpack_warn_pattern(R"(WARNING in (.+))");
            std::smatch webpack_match;
            if (std::regex_search(line, webpack_match, webpack_warn_pattern)) {
                event.file_path = webpack_match[1].str();
            }
            
            events.push_back(event);
        }
        // Parse syntax errors in compilation
        else if (line.find("Syntax error:") != std::string::npos || line.find("Parsing error:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "javascript";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "syntax";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Node.js runtime errors with line numbers
        else if (line.find("at Object.<anonymous>") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "node";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "runtime";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file and line info: "at Object.<anonymous> (src/index.test.js:5:23)"
            std::regex runtime_pattern(R"(at Object\.<anonymous> \(([^:]+):(\d+):(\d+)\))");
            std::smatch runtime_match;
            if (std::regex_search(line, runtime_match, runtime_pattern)) {
                event.file_path = runtime_match[1].str();
                event.line_number = std::stoi(runtime_match[2].str());
                event.column_number = std::stoi(runtime_match[3].str());
            }
            
            events.push_back(event);
        }
        // Parse dependency resolution errors
        else if (line.find("Could not resolve dependency:") != std::string::npos || 
                line.find("Module not found:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "npm";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "dependency";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
    }
}

void ParseCargoBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse Rust compiler errors: error[E0XXX]: message --> file:line:column
        std::regex rust_error_pattern(R"(error\[E(\d+)\]:\s*(.+))");
        std::smatch match;
        
        if (std::regex_search(line, match, rust_error_pattern)) {
            std::string error_code = "E" + match[1].str();
            std::string message = match[2].str();
            
            // Look ahead for the file location line
            std::string location_line;
            if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                std::regex location_pattern(R"(-->\s*([^:]+):(\d+):(\d+))");
                std::smatch loc_match;
                
                if (std::regex_search(location_line, loc_match, location_pattern)) {
                    ValidationEvent event;
                    event.event_id = event_id++;
                    event.tool_name = "rustc";
                    event.event_type = ValidationEventType::BUILD_ERROR;
                    event.file_path = loc_match[1].str();
                    event.line_number = std::stoi(loc_match[2].str());
                    event.column_number = std::stoi(loc_match[3].str());
                    event.function_name = "";
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "compilation";
                    event.message = message;
                    event.error_code = error_code;
                    event.execution_time = 0.0;
                    event.raw_output = content;
                    event.structured_data = "cargo_build";
                    
                    events.push_back(event);
                }
            }
        }
        // Parse warning patterns: warning: message --> file:line:column
        else if (line.find("warning:") != std::string::npos) {
            std::regex warning_pattern(R"(warning:\s*(.+))");
            std::smatch warn_match;
            
            if (std::regex_search(line, warn_match, warning_pattern)) {
                std::string message = warn_match[1].str();
                
                // Look ahead for the file location line
                std::string location_line;
                if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                    std::regex location_pattern(R"(-->\s*([^:]+):(\d+):(\d+))");
                    std::smatch loc_match;
                    
                    if (std::regex_search(location_line, loc_match, location_pattern)) {
                        ValidationEvent event;
                        event.event_id = event_id++;
                        event.tool_name = "rustc";
                        event.event_type = ValidationEventType::LINT_ISSUE;
                        event.file_path = loc_match[1].str();
                        event.line_number = std::stoi(loc_match[2].str());
                        event.column_number = std::stoi(loc_match[3].str());
                        event.function_name = "";
                        event.status = ValidationEventStatus::WARNING;
                        event.severity = "warning";
                        event.category = "compilation";
                        event.message = message;
                        event.execution_time = 0.0;
                        event.raw_output = content;
                        event.structured_data = "cargo_build";
                        
                        events.push_back(event);
                    }
                }
            }
        }
        // Parse cargo test failures: test tests::test_name ... FAILED
        else if (line.find("test ") != std::string::npos && line.find("FAILED") != std::string::npos) {
            std::regex test_pattern(R"(test\s+([^\s]+)\s+\.\.\.\s+FAILED)");
            std::smatch test_match;
            
            if (std::regex_search(line, test_match, test_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = test_match[1].str();
                event.function_name = test_match[1].str();
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo test panic details: thread 'test_name' panicked at 'message', file:line:column
        else if (line.find("thread '") != std::string::npos && line.find("panicked at") != std::string::npos) {
            std::regex panic_pattern(R"(thread '([^']+)' panicked at '([^']+)',\s*([^:]+):(\d+):(\d+))");
            std::smatch panic_match;
            
            if (std::regex_search(line, panic_match, panic_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = panic_match[1].str();
                event.function_name = panic_match[1].str();
                event.file_path = panic_match[3].str();
                event.line_number = std::stoi(panic_match[4].str());
                event.column_number = std::stoi(panic_match[5].str());
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_panic";
                event.message = panic_match[2].str();
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo clippy warnings and errors
        else if ((line.find("clippy::") != std::string::npos || line.find("warning:") != std::string::npos) &&
                 (line.find("-->") != std::string::npos || line.find("src/") != std::string::npos)) {
            std::regex clippy_pattern(R"(([^:]+):(\d+):(\d+):\s*(warning|error):\s*(.+))");
            std::smatch clippy_match;
            
            if (std::regex_search(line, clippy_match, clippy_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "clippy";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.file_path = clippy_match[1].str();
                event.line_number = std::stoi(clippy_match[2].str());
                event.column_number = std::stoi(clippy_match[3].str());
                event.function_name = "";
                
                std::string severity = clippy_match[4].str();
                if (severity == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "lint_error";
                } else {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                    event.category = "lint_warning";
                }
                
                event.message = clippy_match[5].str();
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo build/compilation failures
        else if (line.find("error: could not compile") != std::string::npos) {
            std::regex compile_error_pattern(R"(error: could not compile `([^`]+)`)");
            std::smatch compile_match;
            
            if (std::regex_search(line, compile_match, compile_error_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = compile_match[1].str();
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "compilation";
                event.message = "Could not compile package: " + compile_match[1].str();
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo test result summary: test result: FAILED. X passed; Y failed; Z ignored
        else if (line.find("test result: FAILED") != std::string::npos) {
            std::regex summary_pattern(R"(test result: FAILED\.\s*(\d+) passed;\s*(\d+) failed)");
            std::smatch summary_match;
            
            if (std::regex_search(line, summary_match, summary_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_summary";
                event.message = "Test suite failed: " + summary_match[2].str() + " failed, " + 
                               summary_match[1].str() + " passed";
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo fmt check differences
        else if (line.find("Diff in") != std::string::npos && line.find("at line") != std::string::npos) {
            std::regex fmt_pattern(R"(Diff in ([^\s]+) at line (\d+):)");
            std::smatch fmt_match;
            
            if (std::regex_search(line, fmt_match, fmt_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "rustfmt";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.file_path = fmt_match[1].str();
                event.line_number = std::stoi(fmt_match[2].str());
                event.column_number = -1;
                event.function_name = "";
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
                event.category = "formatting";
                event.message = "Code formatting difference detected";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
    }
}

void ParseMavenBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // Maven patterns
    std::regex compile_error_pattern(R"(\[ERROR\]\s+(.+?):(\[(\d+),(\d+)\])\s+(.+))");
    std::regex compile_warning_pattern(R"(\[WARNING\]\s+(.+?):(\[(\d+),(\d+)\])\s+(.+))");
    std::regex test_failure_pattern(R"(\[ERROR\]\s+(.+?)\(\s*(.+?)\s*\)\s+Time elapsed:\s+([\d.]+)\s+s\s+<<<\s+(FAILURE|ERROR)!)");
    std::regex test_result_pattern(R"(Tests run:\s+(\d+),\s+Failures:\s+(\d+),\s+Errors:\s+(\d+),\s+Skipped:\s+(\d+))");
    std::regex checkstyle_pattern(R"(\[(ERROR|WARN)\]\s+(.+?):(\d+):\s+(.+?)\s+\[(.+?)\])");
    std::regex spotbugs_pattern(R"(\[(ERROR|WARN)\]\s+(High|Medium|Low):\s+(.+?)\s+in\s+(.+?)\s+\[(.+?)\])");
    std::regex pmd_pattern(R"(\[(ERROR|WARN)\]\s+(.+?):(\d+):\s+(.+?)\s+\[(.+?)\])");
    std::regex dependency_pattern(R"(\[WARNING\]\s+(Used undeclared dependencies|Unused declared dependencies) found:)");
    std::regex build_failure_pattern(R"(BUILD FAILURE)");
    std::regex compilation_failure_pattern(R"(COMPILATION ERROR)");
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse Java compilation errors (Maven compiler plugin format)
        if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-compiler";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = std::stoi(match[4].str());
            event.function_name = "";
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse Java compilation warnings
        else if (std::regex_search(line, match, compile_warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-compiler";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = std::stoi(match[4].str());
            event.function_name = "";
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "compilation";
            event.message = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse JUnit test failures
        else if (std::regex_search(line, match, test_failure_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = match[1].str();
            event.test_name = match[2].str() + "." + match[1].str();
            event.execution_time = std::stod(match[3].str());
            event.status = (match[4].str() == "FAILURE") ? ValidationEventStatus::FAIL : ValidationEventStatus::ERROR;
            event.severity = (match[4].str() == "FAILURE") ? "error" : "critical";
            event.category = (match[4].str() == "FAILURE") ? "test_failure" : "test_error";
            std::string failure_type = match[4].str();
            std::transform(failure_type.begin(), failure_type.end(), failure_type.begin(), ::tolower);
            event.message = "Test " + failure_type;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse Checkstyle violations (when preceded by checkstyle plugin info)
        else if (std::regex_search(line, match, checkstyle_pattern) && 
                 (content.find("maven-checkstyle-plugin") != std::string::npos || 
                  content.find("checkstyle") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "checkstyle";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[2].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = -1;
            event.function_name = "";
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = (match[1].str() == "ERROR") ? "error" : "warning";
            event.category = "style";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse SpotBugs findings
        else if (std::regex_search(line, match, spotbugs_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "spotbugs";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.function_name = match[4].str();
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = match[2].str();
            std::transform(event.severity.begin(), event.severity.end(), event.severity.begin(), ::tolower);
            event.category = "static_analysis";
            event.message = match[3].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            // Map SpotBugs category
            if (event.error_code.find("SQL") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("PERFORMANCE") != std::string::npos || 
                      event.error_code.find("DLS_") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            }
            
            events.push_back(event);
        }
        // Parse PMD violations (when preceded by PMD plugin info)
        else if (std::regex_search(line, match, pmd_pattern) && 
                 (content.find("maven-pmd-plugin") != std::string::npos || 
                  content.find("PMD version") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pmd";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[2].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = -1;
            event.function_name = "";
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = (match[1].str() == "ERROR") ? "error" : "warning";
            event.category = "code_quality";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse Maven dependency analysis warnings
        else if (std::regex_search(line, match, dependency_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-dependency";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "dependency";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse build failure summary
        else if (std::regex_search(line, match, build_failure_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "build_failure";
            event.message = "Maven build failed";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse test result summaries
        else if (std::regex_search(line, match, test_result_pattern)) {
            int total_tests = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            
            if (total_tests > 0) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "maven-surefire";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
                event.severity = (failures > 0 || errors > 0) ? "error" : "info";
                event.category = "test_summary";
                event.message = "Tests: " + std::to_string(total_tests) + 
                               " total, " + std::to_string(failures) + " failures, " + 
                               std::to_string(errors) + " errors, " + 
                               std::to_string(skipped) + " skipped";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "maven_build";
                
                events.push_back(event);
            }
        }
    }
}

void ParseGradleBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // Gradle patterns
    std::regex task_pattern(R"(> Task :([^\s]+)\s+(FAILED|UP-TO-DATE|SKIPPED))");
    std::regex compile_error_pattern(R"((.+?):(\d+): error: (.+))");
    std::regex test_failure_pattern(R"((\w+) > (\w+) (FAILED|PASSED|SKIPPED))");
    std::regex test_summary_pattern(R"((\d+) tests completed(?:, (\d+) failed)?(?:, (\d+) skipped)?)");
    std::regex checkstyle_pattern(R"(\[ant:checkstyle\] (.+?):(\d+): (.+?) \[(.+?)\])");
    std::regex spotbugs_pattern(R"(Bug: (High|Medium|Low): (.+?) \[(.+?)\])");
    std::regex android_lint_pattern(R"((.+?):(\d+): (Error|Warning): (.+?) \[(.+?)\])");
    std::regex build_result_pattern(R"(BUILD (SUCCESSFUL|FAILED) in (\d+)s)");
    std::regex execution_failed_pattern(R"(Execution failed for task '([^']+)')");
    std::regex gradle_error_pattern(R"(\* What went wrong:)");
    
    std::string current_task = "";
    bool in_error_block = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse task execution results
        if (std::regex_search(line, match, task_pattern)) {
            std::string task_name = match[1].str();
            std::string task_result = match[2].str();
            current_task = task_name;
            
            if (task_result == "FAILED") {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "gradle";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = task_name;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "task_failure";
                event.message = "Task " + task_name + " failed";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "gradle_build";
                
                events.push_back(event);
            }
        }
        // Parse Java compilation errors
        else if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-javac";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse test results
        else if (std::regex_search(line, match, test_failure_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string test_result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            
            if (test_result == "FAILED") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (test_result == "PASSED") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (test_result == "SKIPPED") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse test summaries
        else if (std::regex_search(line, match, test_summary_pattern)) {
            int total_tests = std::stoi(match[1].str());
            int failed_tests = match[2].matched ? std::stoi(match[2].str()) : 0;
            int skipped_tests = match[3].matched ? std::stoi(match[3].str()) : 0;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed_tests > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed_tests > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total_tests) + 
                           " completed, " + std::to_string(failed_tests) + " failed, " + 
                           std::to_string(skipped_tests) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse Checkstyle violations (Gradle format)
        else if (std::regex_search(line, match, checkstyle_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-checkstyle";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "style";
            event.message = match[3].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse SpotBugs findings (Gradle format)
        else if (std::regex_search(line, match, spotbugs_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-spotbugs";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.function_name = current_task;
            event.severity = match[1].str();
            std::transform(event.severity.begin(), event.severity.end(), event.severity.begin(), ::tolower);
            event.status = (event.severity == "high") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.category = "static_analysis";
            event.message = match[2].str();
            event.error_code = match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            // Map SpotBugs categories
            if (event.error_code.find("SQL") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("PERFORMANCE") != std::string::npos || 
                      event.error_code.find("DLS_") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
            }
            
            events.push_back(event);
        }
        // Parse Android Lint issues
        else if (std::regex_search(line, match, android_lint_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-android-lint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            
            std::string level = match[3].str();
            if (level == "Error") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            event.category = "android_lint";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            // Map Android-specific categories
            if (event.error_code.find("Security") != std::string::npos || 
                event.error_code.find("SQLInjection") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("Performance") != std::string::npos ||
                      event.error_code.find("ThreadSleep") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            }
            
            events.push_back(event);
        }
        // Parse build results
        else if (std::regex_search(line, match, build_result_pattern)) {
            std::string result = match[1].str();
            int duration = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = (result == "SUCCESSFUL") ? ValidationEventStatus::PASS : ValidationEventStatus::ERROR;
            event.severity = (result == "SUCCESSFUL") ? "info" : "error";
            event.category = "build_result";
            event.message = "Build " + result;
            event.execution_time = static_cast<double>(duration);
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse execution failure messages
        else if (std::regex_search(line, match, execution_failed_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.function_name = match[1].str();
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "execution_failure";
            event.message = "Execution failed for task '" + match[1].str() + "'";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Track error block context
        else if (std::regex_search(line, match, gradle_error_pattern)) {
            in_error_block = true;
        }
    }
}

void ParseMSBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // MSBuild patterns
    std::regex compile_error_pattern(R"((.+?)\((\d+),(\d+)\): error (CS\d+): (.+?) \[(.+?\.csproj)\])");
    std::regex compile_warning_pattern(R"((.+?)\((\d+),(\d+)\): warning (CS\d+|CA\d+): (.+?) \[(.+?\.csproj)\])");
    std::regex build_result_pattern(R"(Build (FAILED|succeeded)\.)");
    std::regex error_summary_pattern(R"(\s+(\d+) Error\(s\))");
    std::regex warning_summary_pattern(R"(\s+(\d+) Warning\(s\))");
    std::regex time_elapsed_pattern(R"(Time Elapsed (\d+):(\d+):(\d+)\.(\d+))");
    std::regex test_result_pattern(R"((Failed|Passed)!\s+-\s+Failed:\s+(\d+),\s+Passed:\s+(\d+),\s+Skipped:\s+(\d+),\s+Total:\s+(\d+),\s+Duration:\s+(\d+)\s*ms)");
    std::regex xunit_test_pattern(R"(\[xUnit\.net\s+[\d:\.]+\]\s+(.+?)\.(.+?)\s+\[(PASS|FAIL|SKIP)\])");
    std::regex project_pattern("Project \"(.+?\\.csproj)\" on node (\\d+) \\((.+?) targets\\)\\.");
    std::regex analyzer_warning_pattern("(.+?)\\((\\d+),(\\d+)\\): warning (CA\\d+): (.+?) \\[(.+?\\.csproj)\\]");
    
    std::string current_project = "";
    bool in_build_summary = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse C# compilation errors
        if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild-csc";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.function_name = current_project;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse C# compilation warnings
        else if (std::regex_search(line, match, compile_warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild-csc";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.function_name = current_project;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "compilation";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            // Map analyzer warnings to appropriate categories
            std::string error_code = match[4].str();
            if (error_code.find("CA") == 0) {
                event.tool_name = "msbuild-analyzer";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "code_analysis";
                
                // Map specific CA codes to categories
                if (error_code == "CA2100" || error_code.find("Security") != std::string::npos) {
                    event.event_type = ValidationEventType::SECURITY_FINDING;
                    event.category = "security";
                } else if (error_code == "CA1031" || error_code.find("Performance") != std::string::npos) {
                    event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                    event.category = "performance";
                }
            }
            
            events.push_back(event);
        }
        // Parse .NET test results summary
        else if (std::regex_search(line, match, test_result_pattern)) {
            std::string result = match[1].str();
            int failed = std::stoi(match[2].str());
            int passed = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            int total = std::stoi(match[5].str());
            int duration = std::stoi(match[6].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "dotnet-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " + 
                           std::to_string(failed) + " failed, " + 
                           std::to_string(skipped) + " skipped";
            event.execution_time = static_cast<double>(duration) / 1000.0; // Convert ms to seconds
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse xUnit test results
        else if (std::regex_search(line, match, xunit_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string test_result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "xunit";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            
            if (test_result == "FAIL") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (test_result == "PASS") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (test_result == "SKIP") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse build results
        else if (std::regex_search(line, match, build_result_pattern)) {
            std::string result = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.function_name = current_project;
            event.status = (result == "succeeded") ? ValidationEventStatus::PASS : ValidationEventStatus::ERROR;
            event.severity = (result == "succeeded") ? "info" : "error";
            event.category = "build_result";
            event.message = "Build " + result;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse project context
        else if (std::regex_search(line, match, project_pattern)) {
            current_project = match[1].str();
        }
        // Parse build timing
        else if (std::regex_search(line, match, time_elapsed_pattern)) {
            int hours = std::stoi(match[1].str());
            int minutes = std::stoi(match[2].str());
            int seconds = std::stoi(match[3].str());
            int milliseconds = std::stoi(match[4].str());
            
            double total_seconds = hours * 3600 + minutes * 60 + seconds + milliseconds / 1000.0;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.function_name = current_project;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "build_timing";
            event.message = "Build completed";
            event.execution_time = total_seconds;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse error/warning summaries
        else if (std::regex_search(line, match, error_summary_pattern)) {
            int error_count = std::stoi(match[1].str());
            
            if (error_count > 0) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "msbuild";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = current_project;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "error_summary";
                event.message = std::to_string(error_count) + " compilation error(s)";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "msbuild";
                
                events.push_back(event);
            }
        }
        else if (std::regex_search(line, match, warning_summary_pattern)) {
            int warning_count = std::stoi(match[1].str());
            
            if (warning_count > 0) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "msbuild";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = current_project;
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
                event.category = "warning_summary";
                event.message = std::to_string(warning_count) + " compilation warning(s)";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "msbuild";
                
                events.push_back(event);
            }
        }
    }
}

void ParseJUnitText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // JUnit text patterns for different output formats
    std::regex junit4_class_pattern(R"(Running (.+))");
    std::regex junit4_summary_pattern(R"(Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+), Time elapsed: ([\d.]+) sec.*?)");
    std::regex junit4_test_pattern(R"((.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) sec\s+<<< (PASSED!|FAILURE!|ERROR!|SKIPPED!))");
    std::regex junit4_exception_pattern(R"((.+?): (.+)$)");
    std::regex junit4_stack_trace_pattern(R"(\s+at (.+?)\.(.+?)\((.+?):(\d+)\))");
    
    // JUnit 5 patterns
    std::regex junit5_header_pattern(R"(JUnit Jupiter ([\d.]+))");
    std::regex junit5_class_pattern(R"([├└]─ (.+?) [✓✗↷])");
    std::regex junit5_test_pattern(R"([│\s]+[├└]─ (.+?)\(\) ([✓✗↷]) \((\d+)ms\))");
    std::regex junit5_summary_pattern(R"(\[\s+(\d+) tests (found|successful|failed|skipped)\s+\])");
    
    // Maven Surefire patterns
    std::regex surefire_class_pattern(R"(\[INFO\] Running (.+))");
    std::regex surefire_test_pattern(R"(\[ERROR\] (.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) s\s+<<< (FAILURE!|ERROR!))");
    std::regex surefire_summary_pattern(R"(\[INFO\] Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+))");
    std::regex surefire_results_pattern(R"(\[ERROR\] (.+):(\d+) (.+))");
    
    // Gradle patterns
    std::regex gradle_test_pattern(R"((.+?) > (.+?) (PASSED|FAILED|SKIPPED))");
    std::regex gradle_summary_pattern(R"((\d+) tests completed, (\d+) failed, (\d+) skipped)");
    
    // TestNG patterns
    std::regex testng_test_pattern(R"((.+?)\.(.+?): (PASS|FAIL|SKIP))");
    std::regex testng_summary_pattern(R"(Total tests run: (\d+), Failures: (\d+), Skips: (\d+))");
    
    std::string current_class = "";
    std::string current_exception = "";
    std::string current_test = "";
    bool in_stack_trace = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse JUnit 4 class execution
        if (std::regex_search(line, match, junit4_class_pattern)) {
            current_class = match[1].str();
            in_stack_trace = false;
        }
        // Parse JUnit 4 class summary
        else if (std::regex_search(line, match, junit4_summary_pattern)) {
            int tests_run = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            double time_elapsed = std::stod(match[5].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit4";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = current_class;
            event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0 || errors > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(tests_run) + 
                           " total, " + std::to_string(tests_run - failures - errors - skipped) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(errors) + " errors, " +
                           std::to_string(skipped) + " skipped";
            event.execution_time = time_elapsed;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse JUnit 4 individual test results
        else if (std::regex_search(line, match, junit4_test_pattern)) {
            std::string test_method = match[1].str();
            std::string test_class = match[2].str();
            double time_elapsed = std::stod(match[3].str());
            std::string result = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit4";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = time_elapsed;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "PASSED!") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAILURE!") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
                current_test = event.test_name;
                in_stack_trace = true;
            } else if (result == "ERROR!") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_error";
                event.message = "Test error";
                current_test = event.test_name;
                in_stack_trace = true;
            } else if (result == "SKIPPED!") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse JUnit 5 header
        else if (std::regex_search(line, match, junit5_header_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit5";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_framework";
            event.message = "JUnit Jupiter " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse JUnit 5 class results
        else if (std::regex_search(line, match, junit5_class_pattern)) {
            current_class = match[1].str();
        }
        // Parse JUnit 5 test results
        else if (std::regex_search(line, match, junit5_test_pattern)) {
            std::string test_method = match[1].str();
            std::string result_symbol = match[2].str();
            int time_ms = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit5";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = current_class + "." + test_method;
            event.execution_time = static_cast<double>(time_ms) / 1000.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result_symbol == "✓") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result_symbol == "✗") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result_symbol == "↷") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse Maven Surefire class execution
        else if (std::regex_search(line, match, surefire_class_pattern)) {
            current_class = match[1].str();
        }
        // Parse Maven Surefire test failures
        else if (std::regex_search(line, match, surefire_test_pattern)) {
            std::string test_method = match[1].str();
            std::string test_class = match[2].str();
            double time_elapsed = std::stod(match[3].str());
            std::string result = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = time_elapsed;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "FAILURE!") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "ERROR!") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_error";
                event.message = "Test error";
            }
            
            events.push_back(event);
        }
        // Parse Maven Surefire summary
        else if (std::regex_search(line, match, surefire_summary_pattern)) {
            int tests_run = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0 || errors > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(tests_run) + 
                           " total, " + std::to_string(tests_run - failures - errors - skipped) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(errors) + " errors, " +
                           std::to_string(skipped) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse Gradle test results
        else if (std::regex_search(line, match, gradle_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "PASSED") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAILED") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "SKIPPED") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse Gradle test summary
        else if (std::regex_search(line, match, gradle_summary_pattern)) {
            int total = std::stoi(match[1].str());
            int failed = std::stoi(match[2].str());
            int skipped = std::stoi(match[3].str());
            int passed = total - failed - skipped;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " +
                           std::to_string(failed) + " failed, " + 
                           std::to_string(skipped) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse TestNG test results
        else if (std::regex_search(line, match, testng_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "testng";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "PASS") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAIL") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "SKIP") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse TestNG summary
        else if (std::regex_search(line, match, testng_summary_pattern)) {
            int total = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int skips = std::stoi(match[3].str());
            int passed = total - failures - skips;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "testng";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failures > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(skips) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse exception messages and stack traces
        else if (in_stack_trace && std::regex_search(line, match, junit4_exception_pattern)) {
            std::string exception_type = match[1].str();
            std::string exception_message = match[2].str();
            
            // Update the last test event with exception details
            if (!events.empty() && events.back().test_name == current_test) {
                events.back().message = exception_type + ": " + exception_message;
                events.back().error_code = exception_type;
            }
        }
        // Parse stack trace lines for file/line information
        else if (in_stack_trace && std::regex_search(line, match, junit4_stack_trace_pattern)) {
            std::string class_name = match[1].str();
            std::string method_name = match[2].str();
            std::string file_name = match[3].str();
            int line_number = std::stoi(match[4].str());
            
            // Update the last test event with file/line details
            if (!events.empty() && events.back().test_name == current_test && events.back().file_path.empty()) {
                events.back().file_path = file_name;
                events.back().line_number = line_number;
                events.back().function_name = method_name;
            }
        }
        // Reset stack trace mode on blank lines or new test patterns
        else if (line.empty() || line.find("Running") != std::string::npos) {
            in_stack_trace = false;
            current_test = "";
        }
    }
}

void ParseValgrind(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    uint64_t event_id = 1;
    
    // Track current context
    std::string current_tool = "Valgrind";
    std::string current_pid;
    std::string current_error_type;
    std::string current_message;
    std::vector<std::string> stack_trace;
    bool in_error_block = false;
    bool in_summary = false;
    
    // Regular expressions for different Valgrind patterns
    std::regex pid_regex(R"(==(\d+)==)");
    std::regex memcheck_header(R"(==\d+== Memcheck, a memory error detector)");
    std::regex helgrind_header(R"(==\d+== Helgrind, a thread error detector)");
    std::regex cachegrind_header(R"(==\d+== Cachegrind, a cache and branch-prediction profiler)");
    std::regex massif_header(R"(==\d+== Massif, a heap profiler)");
    std::regex drd_header(R"(==\d+== DRD, a thread error detector)");
    
    // Error patterns
    std::regex invalid_access(R"(==\d+== (Invalid (read|write) of size \d+))");
    std::regex memory_leak(R"(==\d+== (\d+ bytes .* (definitely|indirectly|possibly) lost))");
    std::regex uninitialized(R"(==\d+== (Conditional jump .* uninitialised|Use of uninitialised))");
    std::regex invalid_free(R"(==\d+== (Invalid free\(\)|delete|realloc))");
    std::regex data_race(R"(==\d+== (Possible data race))");
    std::regex lock_order(R"(==\d+== (Lock order .* violated))");
    
    // Stack trace patterns
    std::regex stack_frame(R"(==\d+==\s+at 0x[0-9A-F]+: (.+) \(([^:]+):(\d+)\))");
    std::regex stack_frame_no_line(R"(==\d+==\s+at 0x[0-9A-F]+: (.+))");
    std::regex error_summary(R"(==\d+== ERROR SUMMARY: (\d+) errors?)");
    
    // Cache statistics patterns
    std::regex cache_stat(R"(==\d+== ([DL1L]+)\s+(refs|misses|miss rate):\s*([0-9,]+|[\d.]+%))");
    std::regex branch_stat(R"(==\d+== (Branches|Mispredicts|Mispred rate):\s*([0-9,]+|[\d.]+%))");
    
    // Heap statistics
    std::regex heap_usage(R"(==\d+== Total heap usage: (\d+) allocs, (\d+) frees, ([0-9,]+) bytes allocated)");
    std::regex peak_memory(R"(==\d+== Peak memory usage: ([0-9,]+) bytes)");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Extract PID from Valgrind output
        if (std::regex_search(line, match, pid_regex)) {
            current_pid = match[1].str();
        }
        
        // Detect Valgrind tool type
        if (std::regex_search(line, memcheck_header)) {
            current_tool = "Memcheck";
        } else if (std::regex_search(line, helgrind_header)) {
            current_tool = "Helgrind";
        } else if (std::regex_search(line, cachegrind_header)) {
            current_tool = "Cachegrind";
        } else if (std::regex_search(line, massif_header)) {
            current_tool = "Massif";
        } else if (std::regex_search(line, drd_header)) {
            current_tool = "DRD";
        }
        
        // Memory errors (Memcheck)
        if (std::regex_search(line, match, invalid_access)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = match[1].str();
            event.error_code = "INVALID_ACCESS";
            event.raw_output = line;
            
            current_error_type = "invalid_access";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Memory leaks
        else if (std::regex_search(line, match, memory_leak)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_LEAK;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "memory_leak";
            event.message = match[1].str();
            event.error_code = "MEMORY_LEAK";
            event.raw_output = line;
            
            current_error_type = "memory_leak";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Uninitialized value errors
        else if (std::regex_search(line, match, uninitialized)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "uninitialized";
            event.message = match[1].str();
            event.error_code = "UNINITIALIZED";
            event.raw_output = line;
            
            current_error_type = "uninitialized";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Invalid free/delete
        else if (std::regex_search(line, match, invalid_free)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "invalid_free";
            event.message = match[1].str();
            event.error_code = "INVALID_FREE";
            event.raw_output = line;
            
            current_error_type = "invalid_free";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Thread errors (Helgrind/DRD)
        else if (std::regex_search(line, match, data_race)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::THREAD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "data_race";
            event.message = match[1].str();
            event.error_code = "DATA_RACE";
            event.raw_output = line;
            
            current_error_type = "data_race";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Lock order violations
        else if (std::regex_search(line, match, lock_order)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::THREAD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "lock_order";
            event.message = match[1].str();
            event.error_code = "LOCK_ORDER_VIOLATION";
            event.raw_output = line;
            
            current_error_type = "lock_order";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Stack trace with file and line
        else if (std::regex_search(line, match, stack_frame)) {
            if (in_error_block && !events.empty()) {
                auto& last_event = events.back();
                if (last_event.file_path.empty()) {
                    // Use first stack frame for file location
                    last_event.function_name = match[1].str();
                    last_event.file_path = match[2].str();
                    try {
                        last_event.line_number = std::stoi(match[3].str());
                    } catch (...) {
                        last_event.line_number = -1;
                    }
                }
                stack_trace.push_back(line);
            }
        }
        
        // Stack trace without line info
        else if (std::regex_search(line, match, stack_frame_no_line)) {
            if (in_error_block && !events.empty()) {
                auto& last_event = events.back();
                if (last_event.function_name.empty()) {
                    last_event.function_name = match[1].str();
                }
                stack_trace.push_back(line);
            }
        }
        
        // Cache statistics (Cachegrind)
        else if (std::regex_search(line, match, cache_stat)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "cache_analysis";
            event.message = match[1].str() + " " + match[2].str() + ": " + match[3].str();
            event.error_code = "CACHE_STAT";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Branch statistics (Cachegrind)
        else if (std::regex_search(line, match, branch_stat)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "branch_analysis";
            event.message = match[1].str() + ": " + match[2].str();
            event.error_code = "BRANCH_STAT";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Heap usage summary (Memcheck/Massif)
        else if (std::regex_search(line, match, heap_usage)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "heap_analysis";
            event.message = "Total heap usage: " + match[1].str() + " allocs, " + 
                           match[2].str() + " frees, " + match[3].str() + " bytes allocated";
            event.error_code = "HEAP_SUMMARY";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Peak memory (Massif)
        else if (std::regex_search(line, match, peak_memory)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "memory_usage";
            event.message = "Peak memory usage: " + match[1].str() + " bytes";
            event.error_code = "PEAK_MEMORY";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Error summary
        else if (std::regex_search(line, match, error_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::SUMMARY;
            event.status = std::stoi(match[1].str()) > 0 ? ValidationEventStatus::ERROR : ValidationEventStatus::PASS;
            event.severity = std::stoi(match[1].str()) > 0 ? "error" : "info";
            event.category = "summary";
            event.message = "Total errors: " + match[1].str();
            event.error_code = "ERROR_SUMMARY";
            event.raw_output = line;
            in_summary = true;
            events.push_back(event);
        }
        
        // End of error block detection
        if (line.find("==") != std::string::npos && line.find("== ") != std::string::npos && 
            line.length() > 10 && in_error_block) {
            // Check if this starts a new error or ends current one
            if (line.find("Invalid") == std::string::npos && 
                line.find("bytes") == std::string::npos && 
                line.find("Conditional") == std::string::npos &&
                line.find("Use of") == std::string::npos &&
                line.find("Possible data race") == std::string::npos &&
                line.find("Lock order") == std::string::npos &&
                !stack_trace.empty()) {
                // Update last event with complete stack trace
                if (!events.empty()) {
                    std::string complete_trace;
                    for (const auto& trace_line : stack_trace) {
                        if (!complete_trace.empty()) complete_trace += "\\n";
                        complete_trace += trace_line;
                    }
                    events.back().structured_data = complete_trace;
                }
                in_error_block = false;
                stack_trace.clear();
            }
        }
    }
}

void ParseGdbLldb(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    uint64_t event_id = 1;
    
    // Track current context
    std::string current_debugger = "GDB";
    std::string current_program;
    std::string current_frame;
    std::vector<std::string> stack_trace;
    bool in_backtrace = false;
    bool in_register_dump = false;
    
    // Regular expressions for GDB/LLDB patterns
    std::regex gdb_header(R"(GNU gdb \(.*\) ([\d.]+))");
    std::regex lldb_header(R"(lldb.*version ([\d.]+))");
    std::regex program_start(R"(Starting program: (.+))");
    std::regex target_create(R"(target create \"(.+)\")");
    
    // Signal/crash patterns
    std::regex signal_received(R"(Program received signal (\w+), (.+))");
    std::regex exc_bad_access(R"(stop reason = EXC_BAD_ACCESS \(code=(\d+), address=(0x[0-9a-fA-F]+)\))");
    std::regex segfault_location(R"(0x([0-9a-fA-F]+) in (.+) \(.*\) at (.+):(\d+))");
    std::regex lldb_crash_frame(R"(frame #\d+: (0x[0-9a-fA-F]+) .+`(.+) at (.+):(\d+):(\d+))");
    
    // Backtrace patterns
    std::regex gdb_frame(R"(#(\d+)\s+(0x[0-9a-fA-F]+) in (.+) \(.*\) at (.+):(\d+))");
    std::regex gdb_frame_no_file(R"(#(\d+)\s+(0x[0-9a-fA-F]+) in (.+))");
    std::regex lldb_frame(R"(\* frame #(\d+): (0x[0-9a-fA-F]+) .+`(.+) at (.+):(\d+):(\d+))");
    std::regex lldb_frame_simple(R"(frame #(\d+): (0x[0-9a-fA-F]+) .+`(.+))");
    
    // Breakpoint patterns
    std::regex breakpoint_hit(R"(Breakpoint (\d+), (.+) \(.*\) at (.+):(\d+))");
    std::regex lldb_breakpoint_hit(R"(stop reason = breakpoint (\d+)\.(\d+))");
    std::regex breakpoint_set(R"(Breakpoint (\d+):.*where = .+`(.+) \+ \d+ at (.+):(\d+))");
    
    // Register and memory patterns
    std::regex register_line(R"((\w+)\s+(0x[0-9a-fA-F]+))");
    std::regex memory_access(R"(Cannot access memory at address (0x[0-9a-fA-F]+))");
    
    // Thread patterns
    std::regex thread_info(R"(\* thread #(\d+).*tid = (0x[0-9a-fA-F]+))");
    std::regex gdb_thread_info(R"(\* (\d+)\s+Thread (0x[0-9a-fA-F]+) \(LWP (\d+)\))");
    
    // Watchpoint patterns
    std::regex watchpoint_hit(R"(Hardware watchpoint (\d+): (.+))");
    std::regex watchpoint_set(R"(Watchpoint (\d+): addr = (0x[0-9a-fA-F]+))");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Detect debugger type
        if (std::regex_search(line, match, gdb_header)) {
            current_debugger = "GDB";
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "debugger_startup";
            event.message = "GDB version " + match[1].str() + " started";
            event.error_code = "DEBUGGER_START";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, lldb_header)) {
            current_debugger = "LLDB";
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "debugger_startup";
            event.message = "LLDB version " + match[1].str() + " started";
            event.error_code = "DEBUGGER_START";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Program startup
        else if (std::regex_search(line, match, program_start)) {
            current_program = match[1].str();
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "program_start";
            event.message = "Started program: " + current_program;
            event.error_code = "PROGRAM_START";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, target_create)) {
            current_program = match[1].str();
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "target_create";
            event.message = "Target created: " + current_program;
            event.error_code = "TARGET_CREATE";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Signal/crash detection
        else if (std::regex_search(line, match, signal_received)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::CRASH_SIGNAL;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "signal_crash";
            event.message = "Signal " + match[1].str() + ": " + match[2].str();
            event.error_code = match[1].str();
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, exc_bad_access)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::CRASH_SIGNAL;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = "EXC_BAD_ACCESS at address " + match[2].str();
            event.error_code = "EXC_BAD_ACCESS";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Crash location detection
        else if (std::regex_search(line, match, segfault_location)) {
            if (!events.empty() && events.back().event_type == ValidationEventType::CRASH_SIGNAL) {
                auto& last_event = events.back();
                last_event.function_name = match[2].str();
                last_event.file_path = match[3].str();
                try {
                    last_event.line_number = std::stoi(match[4].str());
                } catch (...) {
                    last_event.line_number = -1;
                }
            }
        } else if (std::regex_search(line, match, lldb_crash_frame)) {
            if (!events.empty() && events.back().event_type == ValidationEventType::CRASH_SIGNAL) {
                auto& last_event = events.back();
                last_event.function_name = match[2].str();
                last_event.file_path = match[3].str();
                try {
                    last_event.line_number = std::stoi(match[4].str());
                    last_event.column_number = std::stoi(match[5].str());
                } catch (...) {
                    last_event.line_number = -1;
                    last_event.column_number = -1;
                }
            }
        }
        
        // Backtrace parsing
        else if (line.find("(gdb) bt") != std::string::npos || line.find("(lldb) bt") != std::string::npos) {
            in_backtrace = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, gdb_frame)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
                if (stack_trace.size() == 1 && !events.empty()) {
                    // First frame - update crash event with location details
                    auto& last_event = events.back();
                    if (last_event.file_path.empty()) {
                        last_event.function_name = match[3].str();
                        last_event.file_path = match[4].str();
                        try {
                            last_event.line_number = std::stoi(match[5].str());
                        } catch (...) {
                            last_event.line_number = -1;
                        }
                    }
                }
            }
        } else if (std::regex_search(line, match, gdb_frame_no_file)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
            }
        } else if (std::regex_search(line, match, lldb_frame)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
                if (stack_trace.size() == 1 && !events.empty()) {
                    auto& last_event = events.back();
                    if (last_event.file_path.empty()) {
                        last_event.function_name = match[3].str();
                        last_event.file_path = match[4].str();
                        try {
                            last_event.line_number = std::stoi(match[5].str());
                            last_event.column_number = std::stoi(match[6].str());
                        } catch (...) {
                            last_event.line_number = -1;
                            last_event.column_number = -1;
                        }
                    }
                }
            }
        } else if (std::regex_search(line, match, lldb_frame_simple)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
            }
        }
        
        // Breakpoint events
        else if (std::regex_search(line, match, breakpoint_hit)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_hit";
            event.function_name = match[2].str();
            event.file_path = match[3].str();
            try {
                event.line_number = std::stoi(match[4].str());
            } catch (...) {
                event.line_number = -1;
            }
            event.message = "Breakpoint " + match[1].str() + " hit at " + match[2].str();
            event.error_code = "BREAKPOINT_HIT";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, lldb_breakpoint_hit)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_hit";
            event.message = "Breakpoint " + match[1].str() + "." + match[2].str() + " hit";
            event.error_code = "BREAKPOINT_HIT";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, breakpoint_set)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_set";
            event.function_name = match[2].str();
            event.file_path = match[3].str();
            try {
                event.line_number = std::stoi(match[4].str());
            } catch (...) {
                event.line_number = -1;
            }
            event.message = "Breakpoint " + match[1].str() + " set at " + match[2].str();
            event.error_code = "BREAKPOINT_SET";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Watchpoint events
        else if (std::regex_search(line, match, watchpoint_hit)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "watchpoint_hit";
            event.message = "Watchpoint " + match[1].str() + " hit: " + match[2].str();
            event.error_code = "WATCHPOINT_HIT";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, watchpoint_set)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "watchpoint_set";
            event.message = "Watchpoint " + match[1].str() + " set at address " + match[2].str();
            event.error_code = "WATCHPOINT_SET";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Memory access errors
        else if (std::regex_search(line, match, memory_access)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = "Cannot access memory at address " + match[1].str();
            event.error_code = "MEMORY_ACCESS_ERROR";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Thread information
        else if (std::regex_search(line, match, thread_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "thread_info";
            event.message = "Thread #" + match[1].str() + " (TID: " + match[2].str() + ")";
            event.error_code = "THREAD_INFO";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, gdb_thread_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "thread_info";
            event.message = "Thread " + match[1].str() + " (LWP: " + match[3].str() + ")";
            event.error_code = "THREAD_INFO";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // End backtrace when we see a new command prompt
        if ((line.find("(gdb)") != std::string::npos || line.find("(lldb)") != std::string::npos) && 
            line.find("bt") == std::string::npos) {
            if (in_backtrace && !stack_trace.empty()) {
                // Add stack trace to the last crash event
                if (!events.empty()) {
                    std::string complete_trace;
                    for (const auto& trace_line : stack_trace) {
                        if (!complete_trace.empty()) complete_trace += "\\n";
                        complete_trace += trace_line;
                    }
                    // Find the most recent crash or debug event to attach the stack trace
                    for (auto it = events.rbegin(); it != events.rend(); ++it) {
                        if (it->event_type == ValidationEventType::CRASH_SIGNAL ||
                            it->event_type == ValidationEventType::DEBUG_EVENT) {
                            it->structured_data = complete_trace;
                            break;
                        }
                    }
                }
                in_backtrace = false;
                stack_trace.clear();
            }
        }
    }
}

void ParseRSpecText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for RSpec output
    std::regex test_passed(R"(\s*✓\s*(.+))");
    std::regex test_failed(R"(\s*✗\s*(.+))");
    std::regex test_pending(R"(\s*pending:\s*(.+)\s*\(PENDING:\s*(.+)\))");
    std::regex context_start(R"(^([A-Z][A-Za-z0-9_:]+)\s*$)");
    std::regex nested_context(R"(^\s+(#\w+|.+)\s*$)");
    std::regex failure_error(R"(Failure/Error:\s*(.+))");
    std::regex expected_pattern(R"(\s*expected\s*(.+))");
    std::regex got_pattern(R"(\s*got:\s*(.+))");
    std::regex file_line_pattern(R"(# (.+):(\d+):in)");
    std::regex summary_pattern(R"(Finished in (.+) seconds .* (\d+) examples?, (\d+) failures?(, (\d+) pending)?)");;
    std::regex failed_example(R"(rspec (.+):(\d+) # (.+))");
    
    std::string current_context;
    std::string current_method;
    std::string current_failure_file;
    int current_failure_line = -1;
    std::string current_failure_message;
    bool in_failure_section = false;
    bool in_failed_examples = false;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Skip empty lines and dividers
        if (line.empty() || line.find("Failures:") != std::string::npos ||
            line.find("Failed examples:") != std::string::npos) {
            if (line.find("Failures:") != std::string::npos) {
                in_failure_section = true;
            }
            if (line.find("Failed examples:") != std::string::npos) {
                in_failed_examples = true;
            }
            continue;
        }
        
        // Parse failed example references
        if (in_failed_examples && std::regex_search(line, match, failed_example)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.test_name = match[3].str();
            event.message = "Test failed: " + match[3].str();
            event.raw_output = line;
            events.push_back(event);
            continue;
        }
        
        // Parse test context (class/module names)
        if (std::regex_match(line, match, context_start)) {
            current_context = match[1].str();
            continue;
        }
        
        // Parse nested context (method names or descriptions)
        if (!current_context.empty() && std::regex_match(line, match, nested_context)) {
            current_method = match[1].str();
            // Remove leading # if present
            if (current_method.substr(0, 1) == "#") {
                current_method = current_method.substr(1);
            }
            continue;
        }
        
        // Parse passed tests
        if (std::regex_search(line, match, test_passed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "test_success";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test passed: " + match[1].str();
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Parse failed tests
        else if (std::regex_search(line, match, test_failed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test failed: " + match[1].str();
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Parse pending tests
        else if (std::regex_search(line, match, test_pending)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::SKIP;
            event.severity = "warning";
            event.category = "test_pending";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test pending: " + match[2].str();
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Parse failure details
        else if (std::regex_search(line, match, failure_error)) {
            current_failure_message = match[1].str();
        }
        
        // Parse expected/got patterns for better error details
        else if (std::regex_search(line, match, expected_pattern)) {
            if (!current_failure_message.empty()) {
                current_failure_message += " | Expected: " + match[1].str();
            }
        }
        else if (std::regex_search(line, match, got_pattern)) {
            if (!current_failure_message.empty()) {
                current_failure_message += " | Got: " + match[1].str();
            }
        }
        
        // Parse file and line information
        else if (std::regex_search(line, match, file_line_pattern)) {
            current_failure_file = match[1].str();
            current_failure_line = std::stoi(match[2].str());
            
            // Update the most recent failed test event with file/line info
            for (auto it = events.rbegin(); it != events.rend(); ++it) {
                if (it->tool_name == "RSpec" && it->status == ValidationEventStatus::FAIL && 
                    it->file_path.empty()) {
                    it->file_path = current_failure_file;
                    it->line_number = current_failure_line;
                    if (!current_failure_message.empty()) {
                        it->message = current_failure_message;
                    }
                    break;
                }
            }
        }
        
        // Parse summary
        else if (std::regex_search(line, match, summary_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::SUMMARY;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_summary";
            
            std::string execution_time = match[1].str();
            int total_examples = std::stoi(match[2].str());
            int failures = std::stoi(match[3].str());
            int pending = 0;
            if (match[5].matched) {
                pending = std::stoi(match[5].str());
            }
            
            event.message = "Test run completed: " + std::to_string(total_examples) + 
                          " examples, " + std::to_string(failures) + " failures, " + 
                          std::to_string(pending) + " pending";
            event.execution_time = std::stod(execution_time);
            event.raw_output = line;
            events.push_back(event);
        }
    }
}

void ParseMochaChai(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Mocha/Chai output
    std::regex test_passed(R"(\s*✓\s*(.+)\s*\((\d+)ms\))");
    std::regex test_failed(R"(\s*✗\s*(.+))");
    std::regex test_pending(R"(\s*-\s*(.+)\s*\(pending\))");
    std::regex context_start(R"(^\s*([A-Z][A-Za-z0-9\s]+)\s*$)");
    std::regex nested_context(R"(^\s{2,}([a-z][A-Za-z0-9\s]+)\s*$)");
    std::regex error_line(R"((Error|AssertionError):\s*(.+))");
    std::regex file_line(R"(\s*at\s+Context\.<anonymous>\s+\((.+):(\d+):(\d+)\))");
    std::regex test_stack(R"(\s*at\s+Test\.Runnable\.run\s+\((.+):(\d+):(\d+)\))");
    std::regex summary_line(R"(\s*(\d+)\s+passing\s*\(([0-9.]+s)\))");
    std::regex failing_line(R"(\s*(\d+)\s+failing)");
    std::regex pending_line(R"(\s*(\d+)\s+pending)");
    std::regex failed_example_start(R"(\s*(\d+)\)\s+(.+))");
    std::regex expected_got_line(R"(\s*\+(.+))");
    std::regex actual_line(R"(\s*-(.+))");
    
    std::string current_context;
    std::string current_nested_context;
    std::string current_test_name;
    std::string current_error_message;
    std::string current_file_path;
    int current_line_number = 0;
    int current_column = 0;
    int64_t current_execution_time = 0;
    std::vector<std::string> stack_trace;
    bool in_failure_details = false;
    int failure_number = 0;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for test passed
        if (std::regex_match(line, match, test_passed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            current_execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.message = "Test passed: " + test_name;
            event.test_name = current_context + " " + current_nested_context + " " + test_name;
            event.status = ValidationEventStatus::PASS;
            event.file_path = current_file_path;
            event.line_number = current_line_number;
            event.column_number = current_column;
            event.execution_time = current_execution_time;
            event.tool_name = "mocha";
            event.category = "mocha_chai_text";
            event.raw_output = line;
            event.function_name = current_context;
            event.structured_data = "{}";
            
            events.push_back(event);
            
            // Reset for next test
            current_file_path = "";
            current_line_number = 0;
            current_column = 0;
            current_execution_time = 0;
        }
        // Check for test failed
        else if (std::regex_match(line, match, test_failed)) {
            current_test_name = match[1].str();
        }
        // Check for test pending
        else if (std::regex_match(line, match, test_pending)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.message = "Test pending: " + test_name;
            event.test_name = current_context + " " + current_nested_context + " " + test_name;
            event.status = ValidationEventStatus::SKIP;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "mocha";
            event.category = "mocha_chai_text";
            event.raw_output = line;
            event.function_name = current_context;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for context start
        else if (std::regex_match(line, match, context_start)) {
            current_context = match[1].str();
            current_nested_context = "";
        }
        // Check for nested context
        else if (std::regex_match(line, match, nested_context)) {
            current_nested_context = match[1].str();
        }
        // Check for error messages
        else if (std::regex_match(line, match, error_line)) {
            current_error_message = match[1].str() + ": " + match[2].str();
        }
        // Check for file and line information
        else if (std::regex_match(line, match, file_line)) {
            current_file_path = match[1].str();
            current_line_number = std::stoi(match[2].str());
            current_column = std::stoi(match[3].str());
            
            // If we have a failed test, create the event now
            if (!current_test_name.empty() && !current_error_message.empty()) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.event_type = ValidationEventType::TEST_RESULT;
                event.severity = "error";
                event.message = current_error_message;
                event.test_name = current_context + " " + current_nested_context + " " + current_test_name;
                event.status = ValidationEventStatus::FAIL;
                event.file_path = current_file_path;
                event.line_number = current_line_number;
                event.column_number = current_column;
                event.execution_time = 0;
                event.tool_name = "mocha";
                event.category = "mocha_chai_text";
                event.raw_output = line;
                event.function_name = current_context;
                event.structured_data = "{}";
                
                events.push_back(event);
                
                // Reset for next test
                current_test_name = "";
                current_error_message = "";
                current_file_path = "";
                current_line_number = 0;
                current_column = 0;
            }
        }
        // Check for failed example start (in failure summary)
        else if (std::regex_match(line, match, failed_example_start)) {
            failure_number = std::stoi(match[1].str());
            std::string full_test_name = match[2].str();
            in_failure_details = true;
            
            // Extract context and test name from full name
            size_t last_space = full_test_name.rfind(' ');
            if (last_space != std::string::npos) {
                current_context = full_test_name.substr(0, last_space);
                current_test_name = full_test_name.substr(last_space + 1);
            } else {
                current_test_name = full_test_name;
            }
        }
        // Check for summary lines
        else if (std::regex_match(line, match, summary_line)) {
            int passing_count = std::stoi(match[1].str());
            std::string total_time = match[2].str();
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "info";
            summary_event.message = "Test execution completed with " + std::to_string(passing_count) + " passing tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::INFO;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"passing_tests\": " + std::to_string(passing_count) + ", \"total_time\": \"" + total_time + "\"}";
            
            events.push_back(summary_event);
        }
        else if (std::regex_match(line, match, failing_line)) {
            int failing_count = std::stoi(match[1].str());
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "error";
            summary_event.message = "Test execution completed with " + std::to_string(failing_count) + " failing tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::FAIL;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"failing_tests\": " + std::to_string(failing_count) + "}";
            
            events.push_back(summary_event);
        }
        else if (std::regex_match(line, match, pending_line)) {
            int pending_count = std::stoi(match[1].str());
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "warning";
            summary_event.message = "Test execution completed with " + std::to_string(pending_count) + " pending tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::WARNING;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"pending_tests\": " + std::to_string(pending_count) + "}";
            
            events.push_back(summary_event);
        }
        
        // Always add stack trace lines when we encounter them
        if (std::regex_match(line, match, test_stack) || std::regex_match(line, match, file_line)) {
            stack_trace.push_back(line);
        }
        
        // Clear stack trace after processing failure details
        if (in_failure_details && line.empty()) {
            in_failure_details = false;
            stack_trace.clear();
        }
    }
}

void ParseGoogleTest(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Google Test output
    std::regex test_run_start(R"(\[\s*RUN\s*\]\s*(.+))");
    std::regex test_passed(R"(\[\s*OK\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_failed(R"(\[\s*FAILED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_skipped(R"(\[\s*SKIPPED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_suite_start(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+))");
    std::regex test_suite_end(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+)\s*\((\d+)\s*ms total\))");
    std::regex test_summary_start(R"(\[==========\]\s*(\d+)\s*tests from\s*(\d+)\s*test suites ran\.\s*\((\d+)\s*ms total\))");
    std::regex tests_passed_summary(R"(\[\s*PASSED\s*\]\s*(\d+)\s*tests\.)");
    std::regex tests_failed_summary(R"(\[\s*FAILED\s*\]\s*(\d+)\s*tests,\s*listed below:)");
    std::regex failed_test_list(R"(\[\s*FAILED\s*\]\s*(.+))");
    std::regex failure_detail(R"((.+):\s*(.+):(\d+):\s*Failure)");
    std::regex global_env_setup(R"(\[----------\]\s*Global test environment set-up)");
    std::regex global_env_teardown(R"(\[----------\]\s*Global test environment tear-down)");
    
    std::string current_test_suite;
    std::string current_test_name;
    bool in_failure_details = false;
    std::vector<std::string> failure_lines;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for test run start
        if (std::regex_match(line, match, test_run_start)) {
            current_test_name = match[1].str();
        }
        // Check for test passed
        else if (std::regex_match(line, match, test_passed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.message = "Test passed: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::PASS;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for test failed
        else if (std::regex_match(line, match, test_failed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.message = "Test failed: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::FAIL;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for test skipped
        else if (std::regex_match(line, match, test_skipped)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.message = "Test skipped: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::SKIP;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for test suite start
        else if (std::regex_match(line, match, test_suite_start)) {
            current_test_suite = match[2].str();
        }
        // Check for test suite end
        else if (std::regex_match(line, match, test_suite_end)) {
            std::string suite_name = match[2].str();
            std::string total_time = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Test suite completed: " + suite_name + " (" + total_time + " ms total)";
            event.test_name = "";
            event.status = ValidationEventStatus::INFO;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = std::stoll(total_time);
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = suite_name;
            event.structured_data = "{\"suite_name\": \"" + suite_name + "\", \"total_time_ms\": " + total_time + "}";
            
            events.push_back(event);
        }
        // Check for overall test summary
        else if (std::regex_match(line, match, test_summary_start)) {
            std::string total_tests = match[1].str();
            std::string total_suites = match[2].str();
            std::string total_time = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Test execution completed: " + total_tests + " tests from " + total_suites + " test suites";
            event.test_name = "";
            event.status = ValidationEventStatus::INFO;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = std::stoll(total_time);
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"total_tests\": " + total_tests + ", \"total_suites\": " + total_suites + ", \"total_time_ms\": " + total_time + "}";
            
            events.push_back(event);
        }
        // Check for passed tests summary
        else if (std::regex_match(line, match, tests_passed_summary)) {
            std::string passed_count = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Tests passed: " + passed_count + " tests";
            event.test_name = "";
            event.status = ValidationEventStatus::PASS;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"passed_tests\": " + passed_count + "}";
            
            events.push_back(event);
        }
        // Check for failed tests summary
        else if (std::regex_match(line, match, tests_failed_summary)) {
            std::string failed_count = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "error";
            event.message = "Tests failed: " + failed_count + " tests";
            event.test_name = "";
            event.status = ValidationEventStatus::FAIL;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"failed_tests\": " + failed_count + "}";
            
            events.push_back(event);
        }
        // Check for failure details (file paths and line numbers)
        else if (std::regex_match(line, match, failure_detail)) {
            std::string test_name = match[1].str();
            std::string file_path = match[2].str();
            std::string line_str = match[3].str();
            int line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing line number fails, keep it as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.message = "Test failure details: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::FAIL;
            event.file_path = file_path;
            event.line_number = line_number;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{\"file_path\": \"" + file_path + "\", \"line_number\": " + std::to_string(line_number) + "}";
            
            events.push_back(event);
        }
    }
}

void ParseNUnitXUnit(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for NUnit/xUnit output
    std::regex nunit_header(R"(NUnit\s+([\d\.]+))");
    std::regex nunit_summary(R"(Test Count:\s*(\d+),\s*Passed:\s*(\d+),\s*Failed:\s*(\d+),\s*Warnings:\s*(\d+),\s*Inconclusive:\s*(\d+),\s*Skipped:\s*(\d+))");
    std::regex nunit_overall_result(R"(Overall result:\s*(\w+))");
    std::regex nunit_duration(R"(Duration:\s*([\d\.]+)\s*seconds)");
    std::regex nunit_failed_test(R"(\d+\)\s*(.+))");
    std::regex nunit_test_source(R"(Source:\s*(.+):line\s*(\d+))");
    std::regex nunit_test_assertion(R"(Expected:\s*(.+)\s*But was:\s*(.+))");
    
    std::regex xunit_header(R"(xUnit\.net VSTest Adapter\s+v([\d\.]+))");
    std::regex xunit_test_start(R"(Starting:\s*(.+))");
    std::regex xunit_test_finish(R"(Finished:\s*(.+))");
    std::regex xunit_test_pass(R"(\s*(.+)\s*\[PASS\])");
    std::regex xunit_test_fail(R"(\s*(.+)\s*\[FAIL\])");
    std::regex xunit_test_skip(R"(\s*(.+)\s*\[SKIP\])");
    std::regex xunit_assertion_failure(R"(Assert\.(\w+)\(\)\s*Failure)");
    std::regex xunit_stack_trace(R"(at\s+(.+)\s+in\s+(.+):line\s+(\d+))");
    std::regex xunit_total_summary(R"(Total tests:\s*(\d+))");
    std::regex xunit_passed_summary(R"(Passed:\s*(\d+))");
    std::regex xunit_failed_summary(R"(Failed:\s*(\d+))");
    std::regex xunit_skipped_summary(R"(Skipped:\s*(\d+))");
    std::regex xunit_time_summary(R"(Time:\s*([\d\.]+)s)");
    
    std::string current_test_suite;
    std::string current_framework = "unknown";
    bool in_failed_tests_section = false;
    bool in_skipped_tests_section = false;
    bool in_xunit_test_failure = false;
    std::vector<std::string> failure_details;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Detect NUnit vs xUnit framework
        if (std::regex_search(line, match, nunit_header)) {
            current_framework = "nunit";
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "NUnit version " + match[1].str();
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        else if (std::regex_search(line, match, xunit_header)) {
            current_framework = "xunit";
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "xUnit.net VSTest Adapter version " + match[1].str();
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // NUnit Test Summary
        else if (std::regex_search(line, match, nunit_summary)) {
            int total_tests = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int failed = std::stoi(match[3].str());
            int warnings = std::stoi(match[4].str());
            int inconclusive = std::stoi(match[5].str());
            int skipped = std::stoi(match[6].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = failed > 0 ? "error" : "info";
            event.status = failed > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.message = "Test summary: " + std::to_string(total_tests) + " total, " + 
                          std::to_string(passed) + " passed, " + std::to_string(failed) + " failed, " +
                          std::to_string(skipped) + " skipped";
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // NUnit Overall Result
        else if (std::regex_search(line, match, nunit_overall_result)) {
            std::string result = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = (result == "Failed") ? "error" : "info";
            event.status = (result == "Failed") ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.message = "Overall test result: " + result;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // NUnit Duration
        else if (std::regex_search(line, match, nunit_duration)) {
            double duration_seconds = std::stod(match[1].str());
            int64_t duration_ms = static_cast<int64_t>(duration_seconds * 1000);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Test execution time: " + match[1].str() + " seconds";
            event.execution_time = duration_ms;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            
            events.push_back(event);
        }
        // xUnit Test Suite Start
        else if (std::regex_search(line, match, xunit_test_start)) {
            current_test_suite = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Starting test suite: " + current_test_suite;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Test Suite Finish
        else if (std::regex_search(line, match, xunit_test_finish)) {
            std::string test_suite = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Finished test suite: " + test_suite;
            event.function_name = test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Test Pass
        else if (std::regex_search(line, match, xunit_test_pass)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Test passed: " + test_name;
            event.test_name = test_name;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Test Fail
        else if (std::regex_search(line, match, xunit_test_fail)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Test failed: " + test_name;
            event.test_name = test_name;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
            in_xunit_test_failure = true;
        }
        // xUnit Test Skip
        else if (std::regex_search(line, match, xunit_test_skip)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.status = ValidationEventStatus::SKIP;
            event.message = "Test skipped: " + test_name;
            event.test_name = test_name;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Stack Trace
        else if (std::regex_search(line, match, xunit_stack_trace)) {
            std::string method = match[1].str();
            std::string file_path = match[2].str();
            std::string line_number = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Stack trace: " + method;
            event.file_path = file_path;
            event.line_number = std::stoll(line_number);
            event.function_name = method;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.structured_data = "{\"file_path\": \"" + file_path + "\", \"line_number\": " + line_number + "}";
            
            events.push_back(event);
        }
        // NUnit Test Source (file and line)
        else if (std::regex_search(line, match, nunit_test_source)) {
            std::string source_path = match[1].str();
            std::string line_number = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Test failure location";
            event.file_path = source_path;
            event.line_number = std::stoll(line_number);
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.structured_data = "{\"file_path\": \"" + source_path + "\", \"line_number\": " + line_number + "}";
            
            events.push_back(event);
        }
        // xUnit Total Summary
        else if (std::regex_search(line, match, xunit_total_summary)) {
            int total_tests = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "xUnit test summary: " + std::to_string(total_tests) + " total tests";
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // Check for section markers
        else if (line.find("Failed Tests - Failures:") != std::string::npos) {
            in_failed_tests_section = true;
        }
        else if (line.find("Skipped Tests:") != std::string::npos) {
            in_skipped_tests_section = true;
            in_failed_tests_section = false;
        }
        // Handle assertion failures in xUnit
        else if (in_xunit_test_failure && std::regex_search(line, match, xunit_assertion_failure)) {
            std::string assertion_type = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Assertion failure: " + assertion_type;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // Handle NUnit assertion details
        else if (in_failed_tests_section && std::regex_search(line, match, nunit_test_assertion)) {
            std::string expected = match[1].str();
            std::string actual = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Assertion failure - Expected: " + expected + ", But was: " + actual;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        
        // Reset failure context when we encounter an empty line
        if (line.empty()) {
            in_xunit_test_failure = false;
        }
    }
}

} // namespace duckdb