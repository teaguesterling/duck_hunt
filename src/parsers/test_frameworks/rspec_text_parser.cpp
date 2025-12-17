#include "rspec_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool RSpecTextParser::CanParse(const std::string& content) const {
    // Check for RSpec text patterns
    // RSpec documentation format has nested describe/context blocks with indented test names
    // May use ✓/✗ markers or just indentation
    bool has_markers = (content.find("✓") != std::string::npos || content.find("✗") != std::string::npos);
    bool has_rspec_keywords = (content.find("examples") != std::string::npos ||
                               content.find("failures") != std::string::npos ||
                               content.find("Failure/Error:") != std::string::npos ||
                               content.find("rspec") != std::string::npos);
    // RSpec documentation format without markers - look for nested describe patterns
    // Common patterns: "should", "is valid", "is not valid", "does not", "returns"
    bool has_rspec_test_patterns = (content.find("is valid") != std::string::npos ||
                                    content.find("is not valid") != std::string::npos ||
                                    content.find("should") != std::string::npos ||
                                    content.find("does not") != std::string::npos ||
                                    content.find("FAILED") != std::string::npos ||
                                    content.find("PENDING") != std::string::npos);

    return (has_markers && has_rspec_keywords) ||
           (has_rspec_test_patterns && (has_rspec_keywords ||
            // Check for RSpec-style nested structure
            (content.find("\n  ") != std::string::npos && content.find("\n    ") != std::string::npos)));
}

void RSpecTextParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseRSpecText(content, events);
}

void RSpecTextParser::ParseRSpecText(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex patterns for RSpec output
    // With markers
    std::regex test_passed_marker(R"(\s*✓\s*(.+))");
    std::regex test_failed_marker(R"(\s*✗\s*(.+))");
    // Documentation format - tests are deeply indented (4+ spaces) with behavior descriptions
    // Test names typically start with "is", "should", "does", "returns", "raises", "has", etc.
    std::regex doc_test_line(R"(^(\s{4,})(.+?)\s*(\(FAILED - \d+\)|\(PENDING.*\))?\s*$)");
    // Context/describe blocks - 2 spaces indent, often start with # for methods or capitalized words
    std::regex doc_context_2space(R"(^  (#?\w.+?)\s*$)");
    // Top-level context - no indent, capitalized
    std::regex doc_context_top(R"(^([A-Z][A-Za-z0-9_:]+)\s*$)");
    std::regex test_pending(R"(\s*pending:\s*(.+)\s*\(PENDING:\s*(.+)\))");
    std::regex failure_error(R"(Failure/Error:\s*(.+))");
    std::regex expected_pattern(R"(\s*expected\s*(.+))");
    std::regex got_pattern(R"(\s*got:\s*(.+))");
    std::regex file_line_pattern(R"(# (.+):(\d+):in)");
    std::regex summary_pattern(R"(Finished in (.+) seconds .* (\d+) examples?, (\d+) failures?(, (\d+) pending)?)");
    std::regex failed_example(R"(rspec (.+):(\d+) # (.+))");
    // Alternative summary patterns
    std::regex simple_summary(R"((\d+) examples?, (\d+) failures?)");

    std::string current_context;
    std::string current_method;
    std::string current_failure_file;
    int current_failure_line = -1;
    std::string current_failure_message;
    bool in_failure_section = false;
    bool in_failed_examples = false;
    std::vector<std::string> context_stack;  // Track nested contexts
    
    while (std::getline(stream, line)) {
        current_line_num++;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.test_name = match[3].str();
            event.message = "Test failed: " + match[3].str();
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
            continue;
        }
        
        // Parse top-level context (class/module names) - no indent
        if (std::regex_match(line, match, doc_context_top)) {
            current_context = match[1].str();
            context_stack.clear();
            context_stack.push_back(current_context);
            current_method = "";
            continue;
        }

        // Parse 2-space indented context (describe/context blocks)
        if (std::regex_match(line, match, doc_context_2space)) {
            std::string ctx = match[1].str();
            // This is a sub-context
            if (context_stack.size() > 1) {
                context_stack.resize(1);  // Keep only top-level
            }
            context_stack.push_back(ctx);
            current_method = ctx;
            continue;
        }

        // Parse documentation format test lines (4+ spaces indent)
        // These are actual test cases (it blocks)
        if (!in_failure_section && !in_failed_examples && std::regex_match(line, match, doc_test_line)) {
            std::string indent = match[1].str();
            std::string test_name = match[2].str();
            std::string status_marker = match[3].matched ? match[3].str() : "";

            // Skip if this looks like a context header (starts with #, or is very short capitalized word)
            if (test_name.length() > 0 && test_name[0] == '#') {
                // This is a method context like "#login"
                current_method = test_name.substr(1);  // Remove #
                continue;
            }

            // Determine status based on marker
            duckdb::ValidationEventStatus status = duckdb::ValidationEventStatus::PASS;
            std::string severity = "info";
            std::string category = "test_success";

            if (status_marker.find("FAILED") != std::string::npos) {
                status = duckdb::ValidationEventStatus::FAIL;
                severity = "error";
                category = "test_failure";
            } else if (status_marker.find("PENDING") != std::string::npos) {
                status = duckdb::ValidationEventStatus::SKIP;
                severity = "warning";
                category = "test_pending";
            }

            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = status;
            event.severity = severity;
            event.category = category;

            // Build full context name
            std::string full_context;
            for (const auto& ctx : context_stack) {
                if (!full_context.empty()) full_context += " ";
                full_context += ctx;
            }
            event.function_name = full_context;
            event.test_name = test_name;
            event.message = (status == duckdb::ValidationEventStatus::PASS ? "Test passed: " :
                           status == duckdb::ValidationEventStatus::FAIL ? "Test failed: " : "Test pending: ") + test_name;
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
            continue;
        }

        // Parse passed tests with ✓ marker
        if (std::regex_search(line, match, test_passed_marker)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "test_success";
            std::string full_context;
            for (const auto& ctx : context_stack) {
                if (!full_context.empty()) full_context += " ";
                full_context += ctx;
            }
            event.function_name = full_context;
            event.test_name = match[1].str();
            event.message = "Test passed: " + match[1].str();
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }

        // Parse failed tests with ✗ marker
        else if (std::regex_search(line, match, test_failed_marker)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
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
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Parse pending tests
        else if (std::regex_search(line, match, test_pending)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::SKIP;
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
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
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
                if (it->tool_name == "RSpec" && it->status == duckdb::ValidationEventStatus::FAIL && 
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
    }
}

} // namespace duck_hunt