#include "bazel_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

bool BazelParser::canParse(const std::string& content) const {
    // Check for Bazel-specific patterns
    if (content.find("INFO: Analyzed") != std::string::npos ||
        content.find("INFO: Build completed") != std::string::npos ||
        content.find("INFO: Found") != std::string::npos ||
        content.find("bazel build") != std::string::npos ||
        content.find("bazel test") != std::string::npos) {
        return true;
    }

    // Check for Bazel target patterns with PASSED/FAILED
    // Bazel targets look like "//package:target" or "//path/to/package:target"
    // NOT like "hdfs://host:port" or URLs
    // Look for pattern: //word_chars:word_chars followed by test result
    static std::regex bazel_target_pattern(
        R"(//[a-zA-Z0-9_/.-]+:[a-zA-Z0-9_.-]+\s+\((PASSED|FAILED|TIMEOUT|SKIPPED|FLAKY))",
        std::regex::optimize
    );
    if (std::regex_search(content, bazel_target_pattern)) {
        return true;
    }

    return false;
}

std::vector<ValidationEvent> BazelParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int64_t event_id = 1;

    // Regex patterns for Bazel build output (safe patterns with bounded character classes)
    static const std::regex bazel_analyzed(R"(INFO: Analyzed (\d+) targets? \((\d+) packages? loaded, (\d+) targets? configured\)\.)");
    static const std::regex bazel_found(R"(INFO: Found (\d+) targets?\.\.\.)");
    static const std::regex bazel_build_completed(R"(INFO: Build completed successfully, (\d+) total actions)");
    static const std::regex bazel_build_elapsed(R"(INFO: Elapsed time: ([\d\.]+)s, Critical Path: ([\d\.]+)s)");
    static const std::regex bazel_test_passed(R"(PASSED: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(([\d\.]+)s\))");
    static const std::regex bazel_test_failed(R"(FAILED: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(([\d\.]+)s\) \[(\d+)/(\d+) attempts\])");
    static const std::regex bazel_test_timeout(R"(TIMEOUT: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(([\d\.]+)s TIMEOUT\))");
    static const std::regex bazel_test_flaky(R"(FLAKY: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) passed in (\d+) out of (\d+) attempts)");
    static const std::regex bazel_test_skipped(R"(SKIPPED: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(SKIPPED\))");
    static const std::regex bazel_warning(R"(WARNING: (.+))");
    static const std::regex bazel_fail_msg(R"(FAIL (.+):(\d+): (.+))");
    static const std::regex bazel_loading(R"(Loading: (\d+) packages? loaded)");
    static const std::regex bazel_analyzing(R"(Analyzing: (\d+) targets? \((\d+) packages? loaded, (\d+) targets? configured\))");
    static const std::regex bazel_action_info(R"(INFO: From (.+):)");
    static const std::regex bazel_up_to_date(R"(Target (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) up-to-date:)");
    static const std::regex bazel_test_summary(R"(Total: (\d+) tests?, (\d+) passed, (\d+) failed(?:, (\d+) timeout)?(?:, (\d+) flaky)?(?:, (\d+) skipped)?)");

    std::string current_target;
    std::string current_action;

    while (reader.getLine(line)) {
        int32_t current_line_num = reader.lineNumber();
        std::smatch match;

        // Skip test suite headers
        if (line.find("==== Test Suite:") != std::string::npos) {
            continue;
        }

        // Check for analysis phase
        if (SafeParsing::SafeRegexSearch(line, match, bazel_analyzed)) {
            int targets = std::stoi(match[1].str());
            int packages = std::stoi(match[2].str());
            int configured = std::stoi(match[3].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Analyzed " + std::to_string(targets) + " targets (" +
                           std::to_string(packages) + " packages loaded, " +
                           std::to_string(configured) + " targets configured)";
            event.tool_name = "bazel";
            event.category = "analysis";
            event.log_content = line;
            event.structured_data = "{\"targets\": " + std::to_string(targets) +
                                   ", \"packages\": " + std::to_string(packages) +
                                   ", \"configured\": " + std::to_string(configured) + "}";

            events.push_back(event);
        }
        // Check for build completion
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_build_completed)) {
            int actions = std::stoi(match[1].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Build completed successfully with " + std::to_string(actions) + " total actions";
            event.tool_name = "bazel";
            event.category = "build_success";
            event.log_content = line;
            event.structured_data = "{\"total_actions\": " + std::to_string(actions) + "}";

            events.push_back(event);
        }
        // Check for elapsed time
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_build_elapsed)) {
            double elapsed = std::stod(match[1].str());
            // Note: critical_path (match[2]) available but not currently used

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Build timing - Elapsed: " + match[1].str() + "s, Critical Path: " + match[2].str() + "s";
            event.tool_name = "bazel";
            event.category = "performance";
            event.execution_time = elapsed;
            event.log_content = line;
            event.structured_data = "{\"elapsed_time\": " + match[1].str() +
                                   ", \"critical_path_time\": " + match[2].str() + "}";

            events.push_back(event);
        }
        // Check for passed tests
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_test_passed)) {
            std::string target = match[1].str();
            double duration = std::stod(match[2].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Test passed";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_result";
            event.execution_time = duration;
            event.log_content = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"duration\": " + match[2].str() + "}";

            events.push_back(event);
        }
        // Check for failed tests
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_test_failed)) {
            std::string target = match[1].str();
            double duration = std::stod(match[2].str());
            int current_attempt = std::stoi(match[3].str());
            int total_attempts = std::stoi(match[4].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Test failed (" + std::to_string(current_attempt) + "/" + std::to_string(total_attempts) + " attempts)";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_result";
            event.execution_time = duration;
            event.log_content = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"duration\": " + match[2].str() +
                                   ", \"current_attempt\": " + std::to_string(current_attempt) +
                                   ", \"total_attempts\": " + std::to_string(total_attempts) + "}";

            events.push_back(event);
        }
        // Check for timeout tests
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_test_timeout)) {
            std::string target = match[1].str();
            double duration = std::stod(match[2].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Test exceeded maximum time limit";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_timeout";
            event.execution_time = duration;
            event.log_content = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"duration\": " + match[2].str() + ", \"reason\": \"timeout\"}";

            events.push_back(event);
        }
        // Check for flaky tests
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_test_flaky)) {
            std::string target = match[1].str();
            int passed_attempts = std::stoi(match[2].str());
            int total_attempts = std::stoi(match[3].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = "Test is flaky - passed " + std::to_string(passed_attempts) + " out of " + std::to_string(total_attempts) + " attempts";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_flaky";
            event.log_content = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"passed_attempts\": " + std::to_string(passed_attempts) +
                                   ", \"total_attempts\": " + std::to_string(total_attempts) + "}";

            events.push_back(event);
        }
        // Check for skipped tests
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_test_skipped)) {
            std::string target = match[1].str();

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.status = ValidationEventStatus::SKIP;
            event.message = "Test skipped";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_result";
            event.log_content = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"reason\": \"skipped\"}";

            events.push_back(event);
        }
        // Check for ERROR patterns using safe string parsing (no regex backtracking risk)
        // Format: ERROR: file:line:col: message
        else if (line.find("ERROR:") != std::string::npos) {
            // Skip the "ERROR: " prefix
            size_t error_start = line.find("ERROR:");
            if (error_start != std::string::npos) {
                std::string error_part = line.substr(error_start + 7);  // Skip "ERROR: "

                // Try to parse file:line:col
                std::string file_path;
                int line_num, col_num;
                if (SafeParsing::ParseFileLineColumn(error_part, file_path, line_num, col_num)) {
                    // Find the message after the file:line:col:
                    size_t msg_start = error_part.find(':', error_part.find(':', error_part.find(':') + 1) + 1);
                    std::string error_msg = (msg_start != std::string::npos) ?
                        error_part.substr(msg_start + 1) : error_part;

                    // Trim leading whitespace from message
                    size_t content_start = error_msg.find_first_not_of(" \t");
                    if (content_start != std::string::npos && content_start > 0) {
                        error_msg = error_msg.substr(content_start);
                    }

                    // Determine error type based on content
                    std::string category = "build_error";
                    if (error_msg.find("Linking") != std::string::npos) {
                        category = "linking_error";
                    } else if (error_msg.find("failed (Exit") != std::string::npos) {
                        category = "compilation_error";
                    }

                    ValidationEvent event;
                    event.event_id = event_id++;
                    event.event_type = ValidationEventType::BUILD_ERROR;
                    event.severity = "error";
                    event.status = ValidationEventStatus::ERROR;
                    event.message = error_msg;
                    event.ref_file = file_path;
                    event.ref_line = line_num;
                    event.ref_column = col_num;
                    event.tool_name = "bazel";
                    event.category = category;
                    event.log_content = line;
                    event.log_line_start = current_line_num;
                    event.log_line_end = current_line_num;

                    events.push_back(event);
                }
            }
        }
        // Check for warnings
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_warning)) {
            std::string warning_msg = match[1].str();

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = warning_msg;
            event.tool_name = "bazel";
            event.category = "build_warning";
            event.log_content = line;
            event.structured_data = "{\"warning\": \"" + warning_msg + "\"}";

            events.push_back(event);
        }
        // Check for test failures with details
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_fail_msg)) {
            std::string file_path = match[1].str();
            int line_num = std::stoi(match[2].str());
            std::string failure_msg = match[3].str();

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = failure_msg;
            event.ref_file = file_path;
            event.ref_line = line_num;
            event.tool_name = "bazel";
            event.category = "test_failure";
            event.log_content = line;
            event.structured_data = "{\"failure_message\": \"" + failure_msg + "\"}";

            events.push_back(event);
        }
        // Check for loading phase
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_loading)) {
            int packages = std::stoi(match[1].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Loading packages: " + std::to_string(packages) + " loaded";
            event.tool_name = "bazel";
            event.category = "loading";
            event.log_content = line;
            event.structured_data = "{\"packages_loaded\": " + std::to_string(packages) + "}";

            events.push_back(event);
        }
        // Check for analyzing phase
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_analyzing)) {
            int targets = std::stoi(match[1].str());
            int packages = std::stoi(match[2].str());
            int configured = std::stoi(match[3].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Analyzing " + std::to_string(targets) + " targets (" +
                           std::to_string(packages) + " packages loaded, " +
                           std::to_string(configured) + " targets configured)";
            event.tool_name = "bazel";
            event.category = "analyzing";
            event.log_content = line;
            event.structured_data = "{\"targets\": " + std::to_string(targets) +
                                   ", \"packages\": " + std::to_string(packages) +
                                   ", \"configured\": " + std::to_string(configured) + "}";

            events.push_back(event);
        }
        // Check for test summary
        else if (SafeParsing::SafeRegexSearch(line, match, bazel_test_summary)) {
            int total = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int failed = std::stoi(match[3].str());
            int timeout = match[4].matched ? std::stoi(match[4].str()) : 0;
            int flaky = match[5].matched ? std::stoi(match[5].str()) : 0;
            int skipped = match[6].matched ? std::stoi(match[6].str()) : 0;

            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = (failed > 0) ? "error" : "info";
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.message = "Test Summary: " + std::to_string(total) + " tests, " +
                           std::to_string(passed) + " passed, " + std::to_string(failed) + " failed" +
                           (timeout > 0 ? ", " + std::to_string(timeout) + " timeout" : "") +
                           (flaky > 0 ? ", " + std::to_string(flaky) + " flaky" : "") +
                           (skipped > 0 ? ", " + std::to_string(skipped) + " skipped" : "");
            event.tool_name = "bazel";
            event.category = "test_summary";
            event.log_content = line;
            event.structured_data = "{\"total\": " + std::to_string(total) +
                                   ", \"passed\": " + std::to_string(passed) +
                                   ", \"failed\": " + std::to_string(failed) +
                                   ", \"timeout\": " + std::to_string(timeout) +
                                   ", \"flaky\": " + std::to_string(flaky) +
                                   ", \"skipped\": " + std::to_string(skipped) + "}";

            events.push_back(event);
        }
    }

    return events;
}


} // namespace duckdb
