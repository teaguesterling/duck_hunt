#include "drone_ci_text_parser.hpp"
#include "core/legacy_parser_registry.hpp"
#include "duckdb/common/exception.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

bool DroneCITextParser::canParse(const std::string& content) const {
    return isValidDroneCIText(content);
}

bool DroneCITextParser::isValidDroneCIText(const std::string& content) const {
    // Look for DroneCI specific markers
    if (content.find("[drone:exec]") != std::string::npos ||
        content.find("starting build step:") != std::string::npos ||
        content.find("pipeline execution") != std::string::npos) {
        return true;
    }
    return false;
}

std::vector<ValidationEvent> DroneCITextParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex patterns for DroneCI output
    std::regex drone_step_start(R"(\[drone:exec\] .* starting build step: (.+))");
    std::regex drone_step_complete(R"(\[drone:exec\] .* completed build step: (.+) \(exit code (\d+)\))");
    std::regex drone_pipeline_complete(R"(\[drone:exec\] .* pipeline execution complete)");
    std::regex drone_pipeline_failed(R"(\[drone:exec\] .* pipeline failed with exit code (\d+))");
    std::regex git_clone(R"(\+ git clone (.+) \.)");
    std::regex git_checkout(R"(\+ git checkout ([a-f0-9]+))");
    std::regex npm_install(R"(added (\d+) packages .* in ([\d.]+)s)");
    std::regex npm_vulnerabilities(R"(found (\d+) vulnerabilit)");
    std::regex jest_test_pass(R"(PASS (.+) \(([\d.]+) s\))");
    std::regex jest_test_fail(R"(FAIL (.+) \(([\d.]+) s\))");
    std::regex jest_test_item(R"(✓ (.+) \((\d+) ms\))");
    std::regex jest_test_fail_item(R"(✗ (.+) \(([\d.]+) s\))");
    std::regex jest_summary(R"(Test Suites: (\d+) failed, (\d+) passed, (\d+) total)");
    std::regex jest_test_summary(R"(Tests: (\d+) failed, (\d+) passed, (\d+) total)");
    std::regex jest_timing(R"(Time: ([\d.]+) s)");
    std::regex webpack_build(R"(Hash: ([a-f0-9]+))");
    std::regex webpack_warning(R"(Module Warning \(from ([^)]+)\):)");
    std::regex eslint_warning(R"((\d+):(\d+)\s+(warning|error)\s+(.+)\s+([a-z-]+))");
    std::regex docker_build_start(R"(Sending build context to Docker daemon\s+([\d.]+[KMG]?B))");
    std::regex docker_step(R"(Step (\d+)/(\d+) : (.+))");
    std::regex docker_success(R"(Successfully built ([a-f0-9]+))");
    std::regex docker_tagged(R"(Successfully tagged (.+))");
    std::regex curl_notification(R"(\+ curl -X POST .* --data '(.+)' )");
    
    std::smatch match;
    std::string current_step = "";
    bool in_jest_failure = false;
    
    while (std::getline(stream, line)) {
        current_line_num++;
        // Parse DroneCI step start
        if (std::regex_search(line, match, drone_step_start)) {
            current_step = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "step_start";
            event.message = "Starting build step: " + current_step;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse DroneCI step completion
        if (std::regex_search(line, match, drone_step_complete)) {
            std::string step_name = match[1].str();
            int exit_code = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = exit_code == 0 ? ValidationEventStatus::PASS : ValidationEventStatus::FAIL;
            event.severity = exit_code == 0 ? "info" : "error";
            event.category = "step_complete";
            event.message = "Completed build step: " + step_name + " (exit code " + std::to_string(exit_code) + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse pipeline completion
        if (std::regex_search(line, match, drone_pipeline_complete)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "pipeline_complete";
            event.message = "Pipeline execution complete";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse pipeline failure
        if (std::regex_search(line, match, drone_pipeline_failed)) {
            int exit_code = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "pipeline_failed";
            event.message = "Pipeline failed with exit code " + std::to_string(exit_code);
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Git operations
        if (std::regex_search(line, match, git_clone)) {
            std::string repo_url = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "git_clone";
            event.message = "Cloning repository: " + repo_url;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, git_checkout)) {
            std::string commit_hash = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "git_checkout";
            event.message = "Checkout commit: " + commit_hash;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse NPM operations
        if (std::regex_search(line, match, npm_install)) {
            int package_count = std::stoi(match[1].str());
            double install_time = std::stod(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "npm_install";
            event.message = "NPM install: " + std::to_string(package_count) + " packages in " + std::to_string(install_time) + "s";
            event.execution_time = install_time;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, npm_vulnerabilities)) {
            int vuln_count = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SECURITY_FINDING;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "npm_vulnerabilities";
            event.message = "Found " + std::to_string(vuln_count) + " npm vulnerabilities";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Jest test results
        if (std::regex_search(line, match, jest_test_pass)) {
            std::string test_file = match[1].str();
            double test_time = std::stod(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = test_file;
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "jest_test";
            event.message = "Test passed: " + test_file;
            event.execution_time = test_time;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, jest_test_fail)) {
            std::string test_file = match[1].str();
            double test_time = std::stod(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = test_file;
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "jest_test";
            event.message = "Test failed: " + test_file;
            event.execution_time = test_time;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            in_jest_failure = true;
            continue;
        }
        
        if (std::regex_search(line, match, jest_test_item)) {
            std::string test_name = match[1].str();
            int test_time_ms = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "jest_test_item";
            event.message = "Test passed: " + test_name;
            event.test_name = test_name;
            event.execution_time = test_time_ms / 1000.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, jest_test_fail_item)) {
            std::string test_name = match[1].str();
            double test_time = std::stod(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "jest_test_item";
            event.message = "Test failed: " + test_name;
            event.test_name = test_name;
            event.execution_time = test_time;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Jest summaries
        if (std::regex_search(line, match, jest_summary)) {
            int failed = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int total = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = failed > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = failed > 0 ? "error" : "info";
            event.category = "jest_suite_summary";
            event.message = "Test Suites: " + std::to_string(failed) + " failed, " + 
                           std::to_string(passed) + " passed, " + std::to_string(total) + " total";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, jest_test_summary)) {
            int failed = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int total = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = failed > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = failed > 0 ? "error" : "info";
            event.category = "jest_test_summary";
            event.message = "Tests: " + std::to_string(failed) + " failed, " + 
                           std::to_string(passed) + " passed, " + std::to_string(total) + " total";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, jest_timing)) {
            double total_time = std::stod(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "jest_timing";
            event.message = "Test execution time: " + std::to_string(total_time) + "s";
            event.execution_time = total_time;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Webpack build
        if (std::regex_search(line, match, webpack_build)) {
            std::string build_hash = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "webpack_build";
            event.message = "Webpack build hash: " + build_hash;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, webpack_warning)) {
            std::string warning_source = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = warning_source;
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "webpack_warning";
            event.message = "Webpack module warning from " + warning_source;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse ESLint warnings/errors
        if (std::regex_search(line, match, eslint_warning)) {
            int line_num = std::stoi(match[1].str());
            int col_num = std::stoi(match[2].str());
            std::string level = match[3].str();
            std::string message = match[4].str();
            std::string rule = match[5].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = line_num;
            event.column_number = col_num;
            event.status = level == "error" ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = level;
            event.category = "eslint";
            event.message = message;
            event.error_code = rule;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Docker operations
        if (std::regex_search(line, match, docker_build_start)) {
            std::string context_size = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "docker_build";
            event.message = "Docker build context: " + context_size;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, docker_step)) {
            int step_num = std::stoi(match[1].str());
            int total_steps = std::stoi(match[2].str());
            std::string step_command = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "docker_step";
            event.message = "Docker step " + std::to_string(step_num) + "/" + std::to_string(total_steps) + ": " + step_command;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, docker_success)) {
            std::string image_id = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "docker_success";
            event.message = "Docker build successful: " + image_id;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, docker_tagged)) {
            std::string tag = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "docker_tagged";
            event.message = "Docker image tagged: " + tag;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse notification curl
        if (std::regex_search(line, match, curl_notification)) {
            std::string notification_data = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "notification";
            event.message = "Sending notification: " + notification_data;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
    }
    
    return events;
}

// Auto-register this parser
REGISTER_PARSER(DroneCITextParser);

} // namespace duckdb