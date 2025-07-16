#include "msbuild_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool MSBuildParser::CanParse(const std::string& content) const {
    // Check for MSBuild patterns
    return (content.find("Microsoft (R) Build Engine") != std::string::npos || content.find("MSBuild") != std::string::npos) ||
           (content.find("Build FAILED.") != std::string::npos || content.find("Build succeeded.") != std::string::npos) ||
           (content.find("): error CS") != std::string::npos || content.find("): warning CS") != std::string::npos) ||
           (content.find("[xUnit.net") != std::string::npos && content.find(".csproj") != std::string::npos) ||
           (content.find("Time Elapsed") != std::string::npos && content.find("Error(s)") != std::string::npos);
}

void MSBuildParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseMSBuild(content, events);
}

void MSBuildParser::ParseMSBuild(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild-csc";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.function_name = current_project;
            event.status = duckdb::ValidationEventStatus::ERROR;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild-csc";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.function_name = current_project;
            event.status = duckdb::ValidationEventStatus::WARNING;
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
                event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
                event.category = "code_analysis";
                
                // Map specific CA codes to categories
                if (error_code == "CA2100" || error_code.find("Security") != std::string::npos) {
                    event.event_type = duckdb::ValidationEventType::SECURITY_FINDING;
                    event.category = "security";
                } else if (error_code == "CA1031" || error_code.find("Performance") != std::string::npos) {
                    event.event_type = duckdb::ValidationEventType::PERFORMANCE_ISSUE;
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
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "dotnet-test";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = (failed > 0) ? duckdb::ValidationEventStatus::FAIL : duckdb::ValidationEventStatus::PASS;
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
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "xunit";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            
            if (test_result == "FAIL") {
                event.status = duckdb::ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (test_result == "PASS") {
                event.status = duckdb::ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (test_result == "SKIP") {
                event.status = duckdb::ValidationEventStatus::SKIP;
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
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.function_name = current_project;
            event.status = (result == "succeeded") ? duckdb::ValidationEventStatus::PASS : duckdb::ValidationEventStatus::ERROR;
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
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.function_name = current_project;
            event.status = duckdb::ValidationEventStatus::INFO;
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
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "msbuild";
                event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
                event.function_name = current_project;
                event.status = duckdb::ValidationEventStatus::ERROR;
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
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "msbuild";
                event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
                event.function_name = current_project;
                event.status = duckdb::ValidationEventStatus::WARNING;
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

} // namespace duck_hunt