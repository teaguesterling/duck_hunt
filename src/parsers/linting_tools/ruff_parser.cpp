#include "ruff_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Ruff parsing (compiled once, reused)
namespace {
// canParse patterns
static const std::regex RE_RULE_CODE_DETECT(R"((^|\n)[A-Z]+\d+\s+\[\*?\])");

// parse patterns
// Rule line: "F401 [*] `mcp.ClientSession` imported but unused"
static const std::regex RE_RULE_LINE(R"(^([A-Z]+\d+)\s+(\[\*\])?\s*(.+))");
// Location line: "   --> tests/test_inspect.py:376:25"
static const std::regex RE_LOCATION_LINE(R"(^\s*-->\s*(.+):(\d+):(\d+))");
// Help line: "help: Remove unused import"
static const std::regex RE_HELP_LINE(R"(^help:\s*(.+))");
// Summary: "Found 3 errors."
static const std::regex RE_SUMMARY_LINE(R"(Found\s+(\d+)\s+error)");
// Fixable summary: "[*] 3 fixable with the `--fix` option."
static const std::regex RE_FIXABLE_LINE(R"(\[\*\]\s*(\d+)\s+fixable)");
} // anonymous namespace

bool RuffParser::canParse(const std::string &content) const {
	// Ruff uses Rust-style diagnostics with --> file:line:col
	// Also look for ruff-specific patterns
	bool has_arrow_location = content.find("   --> ") != std::string::npos;
	bool has_pipe_context = content.find("    |") != std::string::npos;

	if (has_arrow_location && has_pipe_context) {
		// Check for ruff rule codes (letter + numbers like F401, E501, W503)
		// or "Found N errors" summary
		// Use multiline flag or match at line start with \n prefix
		bool has_rule_code = std::regex_search(content, RE_RULE_CODE_DETECT);
		bool has_found_errors =
		    content.find("Found ") != std::string::npos && content.find(" error") != std::string::npos;
		bool has_fixable = content.find("fixable with") != std::string::npos;

		return has_rule_code || has_found_errors || has_fixable;
	}

	return false;
}

std::vector<ValidationEvent> RuffParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	// State for multi-line issue parsing
	ValidationEvent current_event;
	bool in_issue = false;
	int32_t issue_start_line = 0;
	std::string accumulated_context;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// Check for new rule/issue start
		if (std::regex_search(line, match, RE_RULE_LINE)) {
			// Save previous issue if exists
			if (in_issue && !current_event.message.empty()) {
				current_event.log_line_end = line_num - 1;
				events.push_back(current_event);
			}

			in_issue = true;
			issue_start_line = line_num;
			accumulated_context.clear();

			current_event = ValidationEvent();
			current_event.event_id = event_id++;
			current_event.event_type = ValidationEventType::LINT_ISSUE;
			current_event.tool_name = "ruff";
			current_event.category = "ruff_text";
			current_event.error_code = match[1].str();
			current_event.message = match[3].str();
			current_event.log_line_start = line_num;
			current_event.log_content = line;

			// Check if auto-fixable
			bool is_fixable = match[2].matched && match[2].str() == "[*]";

			// Determine severity based on rule code prefix
			std::string rule_prefix = current_event.error_code.substr(0, 1);
			if (rule_prefix == "E" || rule_prefix == "F") {
				// E = pycodestyle errors, F = pyflakes
				current_event.severity = "error";
				current_event.status = ValidationEventStatus::FAIL;
			} else if (rule_prefix == "W") {
				// W = warnings
				current_event.severity = "warning";
				current_event.status = ValidationEventStatus::WARNING;
			} else {
				// Other rules (C, I, N, etc.)
				current_event.severity = "warning";
				current_event.status = ValidationEventStatus::WARNING;
			}
		}
		// Check for location line
		else if (in_issue && std::regex_search(line, match, RE_LOCATION_LINE)) {
			current_event.ref_file = match[1].str();
			current_event.ref_line = std::stoi(match[2].str());
			current_event.ref_column = std::stoi(match[3].str());
		}
		// Check for help line
		else if (in_issue && std::regex_search(line, match, RE_HELP_LINE)) {
			current_event.suggestion = match[1].str();
			current_event.log_line_end = line_num;
		}
		// Summary line
		else if (std::regex_search(line, match, RE_SUMMARY_LINE)) {
			// Save any pending issue first
			if (in_issue && !current_event.message.empty()) {
				current_event.log_line_end = line_num - 1;
				events.push_back(current_event);
				in_issue = false;
			}

			int error_count = std::stoi(match[1].str());

			ValidationEvent summary;
			summary.event_id = event_id++;
			summary.event_type = ValidationEventType::SUMMARY;
			summary.severity = error_count > 0 ? "error" : "info";
			summary.status = error_count > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			summary.message = "Found " + std::to_string(error_count) + " error" + (error_count != 1 ? "s" : "");
			summary.tool_name = "ruff";
			summary.category = "ruff_text";
			summary.log_content = line;
			summary.log_line_start = line_num;
			summary.log_line_end = line_num;
			events.push_back(summary);
		}
		// Context lines (lines with | for source display)
		else if (in_issue && line.find("    |") != std::string::npos) {
			// Just extend the log_line_end
			current_event.log_line_end = line_num;
		}
	}

	// Don't forget the last issue
	if (in_issue && !current_event.message.empty()) {
		if (current_event.log_line_end < current_event.log_line_start) {
			current_event.log_line_end = line_num;
		}
		events.push_back(current_event);
	}

	return events;
}

} // namespace duckdb
