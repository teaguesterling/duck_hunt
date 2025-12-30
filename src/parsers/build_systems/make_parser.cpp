#include "make_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

bool MakeParser::canParse(const std::string& content) const {
    // Only match when there are actual make-specific markers.
    // Generic ": error:" / ": warning:" should be handled by generic_lint.
    // Make-specific patterns:
    // - "make: ***" (make error message)
    // - "undefined reference" with make context
    // - "make[N]:" (submake output)
    if (content.find("make: ***") != std::string::npos ||
        content.find("make[") != std::string::npos) {
        return isValidMakeError(content);
    }
    // For "undefined reference", require make markers too
    if (content.find("undefined reference") != std::string::npos &&
        (content.find("make") != std::string::npos ||
         content.find("collect2") != std::string::npos)) {
        return isValidMakeError(content);
    }
    return false;
}

bool MakeParser::isValidMakeError(const std::string& content) const {
    // Check for GCC/Clang error patterns or make failure patterns using safe parsing
    // Check a sample of lines (first 50 lines) to detect patterns without full regex scan
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int lines_checked = 0;

    while (reader.getLine(line) && lines_checked < 50) {
        lines_checked++;

        // Check for make failure pattern
        if (line.find("make: ***") != std::string::npos && line.find("Error") != std::string::npos) {
            return true;
        }

        // Check for GCC/Clang error pattern using safe string parsing
        std::string file, severity, message;
        int line_num, col;
        if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
            return true;
        }
    }

    return false;
}

std::vector<ValidationEvent> MakeParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_function;

    // Pre-compiled regex patterns for short, predictable patterns only
    static const std::regex target_pattern(R"(\[([^:\]]+):(\d+):\s*([^\]]+)\])");

    while (reader.getLine(line)) {
        int32_t current_line_num = reader.lineNumber();

        // Parse function context: "file.c: In function 'function_name':"
        // Use string search instead of regex for safety
        size_t in_func_pos = line.find(": In function '");
        if (in_func_pos != std::string::npos) {
            size_t func_start = in_func_pos + 15;  // length of ": In function '"
            size_t func_end = line.find("':", func_start);
            if (func_end != std::string::npos) {
                current_function = line.substr(func_start, func_end - func_start);
            }
            continue;
        }

        // Parse GCC/Clang error format using safe parsing (no regex backtracking risk)
        std::string file, severity, message;
        int line_num, col;
        if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.ref_file = file;
            event.ref_line = line_num;
            event.ref_column = col;
            event.function_name = current_function;
            event.message = message;
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "make_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

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
            event.ref_line = -1;
            event.ref_column = -1;
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "make_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            // Extract makefile target from pattern like "[Makefile:23: build/main]"
            // This regex is safe - the pattern has bounded character classes [^:\]]+
            std::smatch target_match;
            if (SafeParsing::SafeRegexSearch(line, target_match, target_pattern)) {
                event.ref_file = target_match[1].str();  // Makefile
                // Don't extract line_number for make build failures - keep it as -1 (NULL)
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
            event.ref_line = -1;
            event.ref_column = -1;
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "make_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            // Extract symbol name using string search (no regex needed)
            size_t ref_start = line.find("undefined reference to `");
            if (ref_start != std::string::npos) {
                ref_start += 24;  // length of "undefined reference to `"
                size_t ref_end = line.find("'", ref_start);
                if (ref_end != std::string::npos) {
                    event.function_name = line.substr(ref_start, ref_end - ref_start);
                    event.suggestion = "Link the library containing '" + event.function_name + "'";
                }
            }

            events.push_back(event);
        }
    }

    return events;
}


} // namespace duckdb