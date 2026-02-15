#include "eslint_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for ESLint text parsing (compiled once, reused)
namespace {
// canParse patterns
static const std::regex RE_ISSUE_PATTERN(R"(\s+\d+:\d+\s+(error|warning)\s+)");
static const std::regex RE_STYLISH_PATTERN(R"([^\s].*\.(js|ts|jsx|tsx|mjs|cjs)\s*(\n|$))");
static const std::regex RE_ISSUE_LINE(R"(\n\s+\d+:\d+\s+(error|warning)\s+.+\s+\S+)");

// parse patterns
static const std::regex RE_FILE_PATTERN(R"(^([^\s].*\.(js|ts|jsx|tsx|mjs|cjs|vue))\s*$)");
static const std::regex RE_ISSUE_DETAIL(R"(^\s+(\d+):(\d+)\s+(error|warning)\s+(.+?)\s{2,}(\S+)\s*$)");
static const std::regex RE_ISSUE_DETAIL_ALT(R"(^\s+(\d+):(\d+)\s+(error|warning)\s+(.+)\s+(\S+)\s*$)");
} // anonymous namespace

bool EslintTextParser::canParse(const std::string &content) const {
	// Look for ESLint stylish format patterns:
	// 1. Lines with "error" or "warning" followed by rule name
	// 2. Indented lines with line:col format
	// 3. Problems summary line

	// Check for the typical ESLint summary line
	if (content.find("problem") != std::string::npos &&
	    (content.find("error") != std::string::npos || content.find("warning") != std::string::npos)) {
		// Look for the characteristic indented issue pattern: line:col severity
		if (std::regex_search(content, RE_ISSUE_PATTERN)) {
			return true;
		}
	}

	// Also check for file path followed by indented issues (stylish format)
	if (std::regex_search(content, RE_STYLISH_PATTERN) && std::regex_search(content, RE_ISSUE_LINE)) {
		return true;
	}

	return false;
}

std::vector<ValidationEvent> EslintTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;
	std::string current_file;

	int error_count = 0;
	int warning_count = 0;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check if this is a file path line
		if (std::regex_match(line, match, RE_FILE_PATTERN)) {
			current_file = match[1].str();
			continue;
		}

		// Check if this is an issue line
		if (std::regex_match(line, match, RE_ISSUE_DETAIL) || std::regex_match(line, match, RE_ISSUE_DETAIL_ALT)) {
			int32_t line_number = 0;
			int32_t column_number = 0;

			try {
				line_number = SafeParsing::SafeStoi(match[1].str());
				column_number = SafeParsing::SafeStoi(match[2].str());
			} catch (...) {
				// Keep as 0 if parsing fails
			}

			std::string severity = match[3].str();
			std::string message = match[4].str();
			std::string rule = match[5].str();

			// Trim trailing spaces from message
			while (!message.empty() && message.back() == ' ') {
				message.pop_back();
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.tool_name = "eslint";
			event.ref_file = current_file;
			event.ref_line = line_number;
			event.ref_column = column_number;
			event.message = message;
			event.error_code = rule;
			event.category = "lint";

			if (severity == "error") {
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				error_count++;
			} else {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
				warning_count++;
			}

			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			event.structured_data = "{\"rule\": \"" + rule + "\", \"severity\": \"" + severity + "\"}";

			events.push_back(event);
		}
	}

	// Add summary event
	ValidationEvent summary;
	summary.event_id = event_id++;
	summary.event_type = ValidationEventType::SUMMARY;
	summary.tool_name = "eslint";
	summary.category = "lint_summary";
	summary.ref_file = "";
	summary.ref_line = -1;
	summary.ref_column = -1;

	size_t total_issues = events.size();
	if (total_issues == 0) {
		summary.status = ValidationEventStatus::INFO;
		summary.severity = "info";
		summary.message = "No issues found";
	} else if (error_count > 0) {
		summary.status = ValidationEventStatus::ERROR;
		summary.severity = "error";
		summary.message = std::to_string(total_issues) + " problem(s) (" + std::to_string(error_count) + " error(s), " +
		                  std::to_string(warning_count) + " warning(s))";
	} else {
		summary.status = ValidationEventStatus::WARNING;
		summary.severity = "warning";
		summary.message =
		    std::to_string(total_issues) + " problem(s) (" + std::to_string(warning_count) + " warning(s))";
	}

	summary.structured_data = "{\"total\": " + std::to_string(total_issues) +
	                          ", \"errors\": " + std::to_string(error_count) +
	                          ", \"warnings\": " + std::to_string(warning_count) + "}";
	events.push_back(summary);

	return events;
}

} // namespace duckdb
