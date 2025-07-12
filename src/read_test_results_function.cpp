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
    
    if (content.find("PASSED") != std::string::npos && content.find("::") != std::string::npos) {
        return TestResultFormat::PYTEST_TEXT;
    }
    
    if ((content.find("CMake Error") != std::string::npos || content.find("CMake Warning") != std::string::npos || 
         content.find("gmake[") != std::string::npos) && 
        (content.find("Building C") != std::string::npos || content.find("Building CXX") != std::string::npos || 
         content.find("Linking") != std::string::npos || content.find("CMakeLists.txt") != std::string::npos)) {
        return TestResultFormat::CMAKE_BUILD;
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
    
    while (std::getline(stream, line)) {
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
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;
            
            std::string severity = match[4].str();
            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "build_error";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "build_warning";
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "build_info";
                event.severity = "info";
            }
            
            events.push_back(event);
        }
        // Check for make failure line
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

} // namespace duckdb