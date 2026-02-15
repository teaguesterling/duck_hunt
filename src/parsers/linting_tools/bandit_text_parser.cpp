#include "bandit_text_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Bandit text parsing (compiled once, reused)
namespace {
static const std::regex RE_ISSUE_HEADER(R"(>>\s*Issue:\s*\[([^\]]+)\]\s*(.+))");
static const std::regex RE_SEVERITY_LINE(R"(Severity:\s*(Low|Medium|High)\s+Confidence:\s*(Low|Medium|High))");
static const std::regex RE_CWE_LINE(R"(CWE:\s*CWE-(\d+)\s*\(([^)]+)\))");
static const std::regex RE_LOCATION_LINE(R"(Location:\s*(.+):(\d+):?(\d+)?)");
static const std::regex RE_MORE_INFO(R"(More Info:\s*(.+))");
static const std::regex RE_CODE_LINE(R"(^\s*(\d+)\s+(.+))");
static const std::regex RE_SUMMARY_SCANNED(R"(Code scanned:\s*(\d+)\s*files)");
static const std::regex RE_SUMMARY_LINES(R"(Total lines of code:\s*(\d+))");
static const std::regex RE_SUMMARY_ISSUES(R"(Total issues:\s*(\d+))");
static const std::regex RE_TEST_ID(R"(Test ID:\s*(B\d+))");
} // anonymous namespace

bool BanditTextParser::canParse(const std::string &content) const {
	// Bandit specific markers
	bool has_bandit_header =
	    content.find("[main]") != std::string::npos && content.find("running on Python") != std::string::npos;

	if (has_bandit_header) {
		return true;
	}

	// Bandit results summary
	bool has_results =
	    content.find("Code scanned:") != std::string::npos && content.find("Total lines of code:") != std::string::npos;

	if (has_results) {
		return true;
	}

	// Bandit documentation link
	if (content.find("More Info: https://bandit.readthedocs.io") != std::string::npos) {
		return true;
	}

	// Bandit issue format with CWE
	if (content.find("CWE:") != std::string::npos && content.find("https://cwe.mitre.org") != std::string::npos) {
		return true;
	}

	// Bandit severity/confidence markers
	bool has_severity_conf =
	    content.find("Severity:") != std::string::npos && content.find("Confidence:") != std::string::npos &&
	    (content.find("Issue:") != std::string::npos || content.find("Test ID:") != std::string::npos);

	return has_severity_conf;
}

std::vector<ValidationEvent> BanditTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	// State for multi-line issue parsing
	bool in_issue = false;
	ValidationEvent current_event;
	std::string current_severity;
	std::string current_confidence;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// Issue header
		if (std::regex_search(line, match, RE_ISSUE_HEADER)) {
			// Save previous issue if exists
			if (in_issue && !current_event.message.empty()) {
				events.push_back(current_event);
			}

			in_issue = true;
			current_event = ValidationEvent();
			current_event.event_id = event_id++;
			current_event.event_type = ValidationEventType::SECURITY_FINDING;
			current_event.tool_name = "bandit";
			current_event.category = "bandit_text";
			current_event.error_code = match[1].str();
			current_event.message = match[2].str();
			current_event.log_line_start = line_num;
			current_event.log_line_end = line_num;
			current_event.log_content = line;
		}
		// Test ID (alternative format)
		else if (in_issue && std::regex_search(line, match, RE_TEST_ID)) {
			current_event.error_code = match[1].str();
		}
		// Severity and Confidence
		else if (in_issue && std::regex_search(line, match, RE_SEVERITY_LINE)) {
			current_severity = match[1].str();
			current_confidence = match[2].str();

			// Map severity
			if (current_severity == "High") {
				current_event.severity = "error";
				current_event.status = ValidationEventStatus::FAIL;
			} else if (current_severity == "Medium") {
				current_event.severity = "warning";
				current_event.status = ValidationEventStatus::WARNING;
			} else {
				current_event.severity = "info";
				current_event.status = ValidationEventStatus::INFO;
			}

			current_event.log_line_end = line_num;
		}
		// CWE reference
		else if (in_issue && std::regex_search(line, match, RE_CWE_LINE)) {
			current_event.message += " (CWE-" + match[1].str() + ": " + match[2].str() + ")";
			current_event.log_line_end = line_num;
		}
		// Location
		else if (in_issue && std::regex_search(line, match, RE_LOCATION_LINE)) {
			current_event.ref_file = match[1].str();
			current_event.ref_line = std::stoi(match[2].str());
			if (match[3].matched) {
				current_event.ref_column = std::stoi(match[3].str());
			}
			current_event.log_line_end = line_num;
		}
		// More info URL
		else if (in_issue && std::regex_search(line, match, RE_MORE_INFO)) {
			// Just update end line, URL is implicit in rule_id
			current_event.log_line_end = line_num;
		}
		// Summary: files scanned
		else if (std::regex_search(line, match, RE_SUMMARY_SCANNED)) {
			// Finalize any pending issue
			if (in_issue && !current_event.message.empty()) {
				events.push_back(current_event);
				in_issue = false;
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Scanned " + match[1].str() + " files";
			event.tool_name = "bandit";
			event.category = "bandit_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Summary: total issues
		else if (std::regex_search(line, match, RE_SUMMARY_ISSUES)) {
			int issue_count = std::stoi(match[1].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = issue_count > 0 ? "warning" : "info";
			event.status = issue_count > 0 ? ValidationEventStatus::WARNING : ValidationEventStatus::PASS;
			event.message = "Total security issues: " + match[1].str();
			event.tool_name = "bandit";
			event.category = "bandit_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
	}

	// Don't forget the last issue
	if (in_issue && !current_event.message.empty()) {
		events.push_back(current_event);
	}

	return events;
}

} // namespace duckdb
