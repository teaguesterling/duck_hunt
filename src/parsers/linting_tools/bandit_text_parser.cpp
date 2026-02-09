#include "bandit_text_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

bool BanditTextParser::canParse(const std::string &content) const {
	// Bandit specific markers
	bool has_bandit_header = content.find("[main]") != std::string::npos &&
	                         content.find("running on Python") != std::string::npos;

	if (has_bandit_header) {
		return true;
	}

	// Bandit results summary
	bool has_results = content.find("Code scanned:") != std::string::npos &&
	                   content.find("Total lines of code:") != std::string::npos;

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
	bool has_severity_conf = content.find("Severity:") != std::string::npos &&
	                         content.find("Confidence:") != std::string::npos &&
	                         (content.find("Issue:") != std::string::npos || content.find("Test ID:") != std::string::npos);

	return has_severity_conf;
}

std::vector<ValidationEvent> BanditTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	// Patterns for Bandit output
	std::regex issue_header(R"(>>\s*Issue:\s*\[([^\]]+)\]\s*(.+))");
	std::regex severity_line(R"(Severity:\s*(Low|Medium|High)\s+Confidence:\s*(Low|Medium|High))");
	std::regex cwe_line(R"(CWE:\s*CWE-(\d+)\s*\(([^)]+)\))");
	std::regex location_line(R"(Location:\s*(.+):(\d+):?(\d+)?)");
	std::regex more_info(R"(More Info:\s*(.+))");
	std::regex code_line(R"(^\s*(\d+)\s+(.+))");
	std::regex summary_scanned(R"(Code scanned:\s*(\d+)\s*files)");
	std::regex summary_lines(R"(Total lines of code:\s*(\d+))");
	std::regex summary_issues(R"(Total issues:\s*(\d+))");
	std::regex test_id(R"(Test ID:\s*(B\d+))");

	// State for multi-line issue parsing
	bool in_issue = false;
	ValidationEvent current_event;
	std::string current_severity;
	std::string current_confidence;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// Issue header
		if (std::regex_search(line, match, issue_header)) {
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
		else if (in_issue && std::regex_search(line, match, test_id)) {
			current_event.error_code = match[1].str();
		}
		// Severity and Confidence
		else if (in_issue && std::regex_search(line, match, severity_line)) {
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
		else if (in_issue && std::regex_search(line, match, cwe_line)) {
			current_event.message += " (CWE-" + match[1].str() + ": " + match[2].str() + ")";
			current_event.log_line_end = line_num;
		}
		// Location
		else if (in_issue && std::regex_search(line, match, location_line)) {
			current_event.ref_file = match[1].str();
			current_event.ref_line = std::stoi(match[2].str());
			if (match[3].matched) {
				current_event.ref_column = std::stoi(match[3].str());
			}
			current_event.log_line_end = line_num;
		}
		// More info URL
		else if (in_issue && std::regex_search(line, match, more_info)) {
			// Just update end line, URL is implicit in rule_id
			current_event.log_line_end = line_num;
		}
		// Summary: files scanned
		else if (std::regex_search(line, match, summary_scanned)) {
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
		else if (std::regex_search(line, match, summary_issues)) {
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
