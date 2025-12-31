#include "clang_tidy_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

bool ClangTidyParser::canParse(const std::string& content) const {
    // Check for clang-tidy specific patterns with high priority
    if (isValidClangTidyOutput(content)) {
        return true;
    }
    return false;
}

bool ClangTidyParser::isValidClangTidyOutput(const std::string& content) const {
    // Look for clang-tidy specific patterns that distinguish it from MyPy
    // clang-tidy format: file:line:column: error/warning: message [rule-name]
    // MyPy format: file:line: error/warning: message [error-code]

    // Check for clang-tidy specific rule names in square brackets
    std::vector<std::string> clang_tidy_rules = {
        "readability-", "performance-", "modernize-", "bugprone-",
        "cppcoreguidelines-", "google-", "llvm-", "misc-", "portability-",
        "hicpp-", "cert-", "fuchsia-", "abseil-", "android-", "boost-",
        "darwin-", "linuxkernel-", "mpi-", "objc-", "openmp-", "zircon-",
        "clang-analyzer-", "concurrency-", "altera-", "readability-braces-around-statements",
        "bugprone-narrowing-conversions", "cppcoreguidelines-avoid-non-const-global-variables",
        "google-build-using-namespace"
    };

    // First check if we have any clang-tidy specific rules
    for (const auto& rule : clang_tidy_rules) {
        if (content.find(rule) != std::string::npos) {
            return true;
        }
    }

    // Check for clang-tidy specific output patterns using safe parsing
    // Guard: only check if we have error/warning/note patterns
    if (content.find(": error:") != std::string::npos ||
        content.find(": warning:") != std::string::npos ||
        content.find(": note:") != std::string::npos) {
        // Process line-by-line using safe parsing (no regex backtracking risk)
        SafeParsing::SafeLineReader reader(content);
        std::string line;
        int lines_checked = 0;

        while (reader.getLine(line) && lines_checked < 50) {
            lines_checked++;
            std::string file, severity, message;
            int line_num, col;
            if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
                // Check for [rule-name] pattern at end of message
                if (message.find('[') != std::string::npos && message.back() == ']') {
                    // Additional validation: check if it looks like clang-tidy vs mypy
                    std::vector<std::string> cpp_terms = {
                        "function", "variable", "parameter", "struct", "class",
                        "namespace", "const", "static", "inline", "template", "typename"
                    };

                    for (const auto& term : cpp_terms) {
                        if (content.find(term) != std::string::npos) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    // Check for clang-tidy summary patterns
    if (content.find("clang-tidy") != std::string::npos ||
        content.find("warnings generated") != std::string::npos ||
        content.find("errors generated") != std::string::npos) {
        return true;
    }

    // Additional patterns from main detection logic
    if ((content.find("google-") != std::string::npos && content.find("build") != std::string::npos)) {
        return true;
    }

    return false;
}

std::vector<ValidationEvent> ClangTidyParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int64_t event_id = 1;

    // Safe regex for summary (short, predictable pattern)
    static const std::regex clang_tidy_summary(R"((\d+)\s+(warnings?|errors?)\s+generated)");

    while (reader.getLine(line)) {
        int32_t current_line_num = reader.lineNumber();

        // Parse using safe string-based parsing (no regex backtracking risk)
        std::string file_path, severity, message;
        int line_number, column_number;

        if (SafeParsing::ParseCompilerDiagnostic(line, file_path, line_number, column_number, severity, message)) {
            // Extract rule name from message if present: "message text [rule-name]"
            std::string rule_name;
            size_t bracket_start = message.rfind('[');
            size_t bracket_end = message.rfind(']');
            if (bracket_start != std::string::npos && bracket_end != std::string::npos &&
                bracket_end > bracket_start && bracket_end == message.length() - 1) {
                rule_name = message.substr(bracket_start + 1, bracket_end - bracket_start - 1);
                // Trim the rule from the message
                message = message.substr(0, bracket_start);
                // Trim trailing whitespace from message
                while (!message.empty() && (message.back() == ' ' || message.back() == '\t')) {
                    message.pop_back();
                }
            }

            ValidationEvent event;
            event.event_id = event_id++;

            // Map clang-tidy severity to ValidationEventStatus
            if (severity == "error") {
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (severity == "warning") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity == "note") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }

            event.message = message;
            event.ref_file = file_path;
            event.ref_line = line_number;
            event.ref_column = column_number;
            event.error_code = rule_name;
            event.tool_name = "clang-tidy";
            event.category = "static_analysis";
            event.execution_time = 0.0;
            event.log_content = line;
            if (!rule_name.empty()) {
                event.structured_data = "{\"rule\": \"" + rule_name + "\", \"severity\": \"" + severity + "\"}";
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Check for summary (safe regex - short predictable pattern)
        else {
            std::smatch match;
            if (SafeParsing::SafeRegexSearch(line, match, clang_tidy_summary)) {
                std::string count = match[1].str();
                std::string type = match[2].str();

                ValidationEvent event;
                event.event_id = event_id++;
                event.event_type = ValidationEventType::SUMMARY;
                event.severity = type.find("error") != std::string::npos ? "error" : "warning";
                event.status = type.find("error") != std::string::npos ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
                event.message = count + " " + type + " generated by clang-tidy";
                event.tool_name = "clang-tidy";
                event.category = "static_analysis";
                event.ref_line = -1;
                event.ref_column = -1;
                event.execution_time = 0.0;
                event.log_content = line;
                event.structured_data = "{\"count\": " + count + ", \"type\": \"" + type + "\"}";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
    }

    return events;
}

} // namespace duckdb