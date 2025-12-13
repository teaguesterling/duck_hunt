#include "yapf_text_parser.hpp"
#include "../../core/parser_registry.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

bool YapfTextParser::canParse(const std::string& content) const {
    // First check for exclusions - if this is Ansible output, don't parse as YAPF
    if (content.find("PLAY [") != std::string::npos ||
        content.find("TASK [") != std::string::npos ||
        content.find("PLAY RECAP") != std::string::npos) {
        return false;
    }
    
    // Check for YAPF-specific patterns
    return content.find("yapf") != std::string::npos ||
           content.find("Reformatted ") != std::string::npos ||
           content.find("--- a/") != std::string::npos && content.find("(original)") != std::string::npos ||
           content.find("+++ b/") != std::string::npos && content.find("(reformatted)") != std::string::npos ||
           content.find("files reformatted") != std::string::npos ||
           content.find("Files processed:") != std::string::npos;
}

std::vector<ValidationEvent> YapfTextParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex patterns for yapf output
    std::regex diff_start_yapf(R"(--- a/(.+) \(original\))");
    std::regex diff_fixed_yapf(R"(\+\+\+ b/(.+) \(reformatted\))");
    std::regex reformatted_file(R"(Reformatted (.+))");
    std::regex yapf_command(R"(yapf (--[^\s]+.+))");
    std::regex processing_verbose(R"(Processing (.+))");
    std::regex style_config(R"(Style configuration: (.+))");
    std::regex line_length_config(R"(Line length: (\d+))");
    std::regex indent_width_config(R"(Indent width: (\d+))");
    std::regex files_processed(R"(Files processed: (\d+))");
    std::regex files_reformatted(R"(Files reformatted: (\d+))");
    std::regex files_no_changes(R"(Files with no changes: (\d+))");
    std::regex execution_time(R"(Total execution time: ([\d\.]+)s)");
    std::regex check_error(R"(ERROR: Files would be reformatted but yapf was run with --check)");
    std::regex yapf_error(R"(yapf: error: (.+))");
    std::regex syntax_error(R"(ERROR: ([^:]+\.py):(\d+):(\d+): (.+))");
    std::regex encoding_warning(R"(WARNING: ([^:]+\.py): cannot determine encoding)");
    std::regex info_no_changes(R"(INFO: ([^:]+\.py): no changes needed)");
    std::regex files_left_unchanged(R"((\d+) files reformatted, (\d+) files left unchanged\.)");
    
    std::smatch match;
    std::string current_file;
    bool in_diff = false;
    bool in_config = false;

    while (std::getline(stream, line)) {
        current_line_num++;
        // Handle yapf diff sections
        if (std::regex_search(line, match, diff_start_yapf)) {
            current_file = match[1].str();
            in_diff = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
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
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle reformatted file patterns
        if (std::regex_search(line, match, reformatted_file)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "File reformatted";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle yapf command patterns
        if (std::regex_search(line, match, yapf_command)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Command: yapf " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle verbose processing
        if (std::regex_search(line, match, processing_verbose)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "processing";
            event.message = "Processing file";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle style configuration
        if (std::regex_search(line, match, style_config)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Style configuration: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle line length configuration
        if (std::regex_search(line, match, line_length_config)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Line length: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle indent width configuration
        if (std::regex_search(line, match, indent_width_config)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Indent width: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle summary statistics
        if (std::regex_search(line, match, files_processed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
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
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, files_reformatted)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files reformatted: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, files_no_changes)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files with no changes: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, execution_time)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
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
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle combined summary (e.g., "5 files reformatted, 3 files left unchanged.")
        if (std::regex_search(line, match, files_left_unchanged)) {
            ValidationEvent event1;
            event1.event_id = event_id++;
            event1.tool_name = "yapf";
            event1.event_type = ValidationEventType::SUMMARY;
            event1.file_path = "";
            event1.line_number = -1;
            event1.column_number = -1;
            event1.status = ValidationEventStatus::INFO;
            event1.severity = "info";
            event1.category = "summary";
            event1.message = "Files reformatted: " + match[1].str();
            event1.execution_time = 0.0;
            event1.raw_output = content;
            event1.structured_data = "yapf_text";
            
            ValidationEvent event2;
            event2.event_id = event_id++;
            event2.tool_name = "yapf";
            event2.event_type = ValidationEventType::SUMMARY;
            event2.file_path = "";
            event2.line_number = -1;
            event2.column_number = -1;
            event2.status = ValidationEventStatus::INFO;
            event2.severity = "info";
            event2.category = "summary";
            event2.message = "Files left unchanged: " + match[2].str();
            event2.execution_time = 0.0;
            event2.raw_output = content;
            event2.structured_data = "yapf_text";
            event1.log_line_start = current_line_num;
            event1.log_line_end = current_line_num;
            event2.log_line_start = current_line_num;
            event2.log_line_end = current_line_num;

            events.push_back(event1);
            events.push_back(event2);
            continue;
        }
        
        // Handle check mode error
        if (std::regex_search(line, match, check_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "check_mode";
            event.message = "Files would be reformatted but yapf was run with --check";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle yapf errors
        if (std::regex_search(line, match, yapf_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "command_error";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle syntax errors
        if (std::regex_search(line, match, syntax_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
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
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle encoding warnings
        if (std::regex_search(line, match, encoding_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "encoding";
            event.message = "Cannot determine encoding";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Handle info messages (no changes needed)
        if (std::regex_search(line, match, info_no_changes)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "No changes needed";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
    }
    
    return events;
}

// Register the parser
REGISTER_PARSER(YapfTextParser);

} // namespace duckdb