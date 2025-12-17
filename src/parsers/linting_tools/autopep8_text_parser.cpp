#include "autopep8_text_parser.hpp"
#include <sstream>

namespace duckdb {

bool Autopep8TextParser::canParse(const std::string& content) const {
    // Check for autopep8-specific patterns
    if ((content.find("autopep8 --") != std::string::npos && content.find("--") != std::string::npos) ||
        (content.find("--- original/") != std::string::npos && content.find("+++ fixed/") != std::string::npos) ||
        (content.find("Files processed:") != std::string::npos && content.find("Files modified:") != std::string::npos)) {
        return true;
    }
    return false;
}

std::vector<ValidationEvent> Autopep8TextParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;

    // Regex patterns for autopep8 output
    std::regex diff_start(R"(--- original/(.+))");
    std::regex diff_fixed(R"(\+\+\+ fixed/(.+))");
    std::regex error_pattern(R"(ERROR: ([^:]+\.py):(\d+):(\d+): (E\d+) (.+))");
    std::regex warning_pattern(R"(WARNING: ([^:]+\.py):(\d+):(\d+): (E\d+) (.+))");
    std::regex info_pattern(R"(INFO: ([^:]+\.py): (.+))");
    std::regex fixed_pattern(R"(fixed ([^:]+\.py))");
    std::regex autopep8_cmd(R"(autopep8 (--[^\s]+.+))");
    std::regex config_line(R"(Applied configuration:)");
    std::regex summary_files_processed(R"(Files processed: (\d+))");
    std::regex summary_files_modified(R"(Files modified: (\d+))");
    std::regex summary_files_errors(R"(Files with errors: (\d+))");
    std::regex summary_fixes_applied(R"(Total fixes applied: (\d+))");
    std::regex summary_execution_time(R"(Execution time: ([\d\.]+)s)");
    std::regex syntax_error(R"(ERROR: ([^:]+\.py):(\d+):(\d+): SyntaxError: (.+))");
    std::regex encoding_error(R"(WARNING: ([^:]+\.py): could not determine file encoding)");
    std::regex already_formatted(R"(INFO: ([^:]+\.py): already formatted correctly)");

    std::smatch match;
    std::string current_file;
    bool in_config = false;

    while (std::getline(stream, line)) {
        // Handle diff sections
        if (std::regex_search(line, match, diff_start)) {
            current_file = match[1].str();

            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = current_file;
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "File formatting changes detected";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle error patterns (E999 syntax errors)
        if (std::regex_search(line, match, error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "syntax";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle warning patterns (line too long)
        if (std::regex_search(line, match, warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "style";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle info patterns (no changes needed)
        if (std::regex_search(line, match, info_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle fixed file patterns
        if (std::regex_search(line, match, fixed_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "File formatting applied";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle command patterns
        if (std::regex_search(line, match, autopep8_cmd)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Command: autopep8 " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle configuration section
        if (std::regex_search(line, match, config_line)) {
            in_config = true;

            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Configuration applied";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle summary statistics
        if (std::regex_search(line, match, summary_files_processed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files processed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        if (std::regex_search(line, match, summary_files_modified)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files modified: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        if (std::regex_search(line, match, summary_files_errors)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "summary";
            event.message = "Files with errors: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        if (std::regex_search(line, match, summary_fixes_applied)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Total fixes applied: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        if (std::regex_search(line, match, summary_execution_time)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "performance";
            event.message = "Execution time: " + match[1].str() + "s";
            event.execution_time = std::stod(match[1].str());
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle syntax errors
        if (std::regex_search(line, match, syntax_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "syntax";
            event.message = match[4].str();
            event.error_code = "SyntaxError";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle encoding errors
        if (std::regex_search(line, match, encoding_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "encoding";
            event.message = "Could not determine file encoding";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Handle already formatted files
        if (std::regex_search(line, match, already_formatted)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "Already formatted correctly";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // Configuration line continuation
        if (in_config && line.find(":") != std::string::npos && line.find(" ") == 0) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = line.substr(2); // Remove leading spaces
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";

            events.push_back(event);
            continue;
        }

        // End configuration section when we hit an empty line
        if (in_config && line.empty()) {
            in_config = false;
        }
    }

    return events;
}

} // namespace duckdb
