#include "isort_parser.hpp"
#include <sstream>

namespace duckdb {

bool IsortParser::canParse(const std::string &content) const {
	if (content.empty())
		return false;

	// Look for isort-specific patterns
	// "Fixing <file>.py" - fix mode
	bool has_fixing = content.find("Fixing ") != std::string::npos && content.find(".py") != std::string::npos;

	// "ERROR: isort found an import in the wrong position" - check mode
	bool has_error = content.find("ERROR: ") != std::string::npos && content.find("isort") != std::string::npos;

	// "Skipped X files" - summary
	bool has_skipped = content.find("Skipped ") != std::string::npos && content.find(" files") != std::string::npos;

	// Diff with .py files
	bool has_py_diff = content.find("--- ") != std::string::npos && content.find(".py") != std::string::npos &&
	                   content.find(":before") != std::string::npos;

	// "would be reformatted" / "would be left unchanged" - check mode summary
	bool has_check_summary = (content.find("would be reformatted") != std::string::npos ||
	                          content.find("would be left unchanged") != std::string::npos);

	return has_fixing || has_error || (has_skipped && has_py_diff) || has_check_summary;
}

std::vector<ValidationEvent> IsortParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_file;
	int32_t diff_start_line = 0;
	bool in_diff = false;

	while (std::getline(stream, line)) {
		current_line_num++;

		// Remove trailing whitespace
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
			line.pop_back();
		}

		if (line.empty()) {
			in_diff = false;
			continue;
		}

		// Check for "Fixing <file>" messages
		if (line.size() > 7 && line.substr(0, 7) == "Fixing ") {
			std::string file_path = line.substr(7);

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "isort fixed import ordering";
			event.ref_file = file_path;
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "isort";
			event.category = "import_sorting";
			event.log_content = line;
			event.structured_data = "{\"action\":\"fixed\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for "ERROR: isort found..." messages
		if (line.find("ERROR:") != std::string::npos && line.find("isort") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "error";
			event.status = ValidationEventStatus::ERROR;
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "isort";
			event.category = "import_sorting";
			event.log_content = line;
			event.structured_data = "{\"action\":\"error\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for "Skipped X files" summary
		if (line.find("Skipped ") != std::string::npos && line.find(" files") != std::string::npos) {
			// Extract count
			size_t start = line.find("Skipped ") + 8;
			size_t end = line.find(" files");
			std::string count_str = "0";
			if (end > start) {
				count_str = line.substr(start, end - start);
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "isort";
			event.category = "import_sorting";
			event.log_content = line;
			event.structured_data = "{\"skipped_count\":" + count_str + "}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for check mode summary "X would be reformatted"
		if (line.find("would be reformatted") != std::string::npos ||
		    line.find("would be left unchanged") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = line.find("would be reformatted") != std::string::npos ? "warning" : "info";
			event.status = line.find("would be reformatted") != std::string::npos ? ValidationEventStatus::WARNING
			                                                                      : ValidationEventStatus::INFO;
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "isort";
			event.category = "import_sorting";
			event.log_content = line;
			event.structured_data = "{\"action\":\"check_summary\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for diff header "--- <file>:before"
		if (line.size() > 4 && line.substr(0, 4) == "--- " && line.find(":before") != std::string::npos) {
			size_t end_pos = line.find(":before");
			current_file = line.substr(4, end_pos - 4);
			diff_start_line = current_line_num;
			in_diff = true;

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "isort would reorder imports";
			event.ref_file = current_file;
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "isort";
			event.category = "import_sorting";
			event.log_content = line;
			event.structured_data = "{\"action\":\"diff_start\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle diff content - removed imports (lines starting with -)
		if (in_diff && !line.empty() && line[0] == '-' && line.size() > 1) {
			// Skip the "---" header line
			if (line.size() >= 3 && line.substr(0, 3) == "---")
				continue;

			std::string removed_line = line.substr(1);
			// Only report import lines
			if (removed_line.find("import ") != std::string::npos || removed_line.find("from ") != std::string::npos) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
				event.message = "Remove: " + removed_line;
				event.ref_file = current_file;
				event.ref_line = -1;
				event.ref_column = -1;
				event.tool_name = "isort";
				event.category = "import_sorting";
				event.log_content = line;
				event.structured_data = "{\"action\":\"remove\"}";
				event.log_line_start = current_line_num;
				event.log_line_end = current_line_num;

				events.push_back(event);
			}
			continue;
		}

		// Handle diff content - added imports (lines starting with +)
		if (in_diff && !line.empty() && line[0] == '+' && line.size() > 1) {
			// Skip the "+++" header line
			if (line.size() >= 3 && line.substr(0, 3) == "+++")
				continue;

			std::string added_line = line.substr(1);
			// Only report import lines
			if (added_line.find("import ") != std::string::npos || added_line.find("from ") != std::string::npos) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
				event.message = "Add: " + added_line;
				event.ref_file = current_file;
				event.ref_line = -1;
				event.ref_column = -1;
				event.tool_name = "isort";
				event.category = "import_sorting";
				event.log_content = line;
				event.structured_data = "{\"action\":\"add\"}";
				event.log_line_start = current_line_num;
				event.log_line_end = current_line_num;

				events.push_back(event);
			}
			continue;
		}
	}

	return events;
}

} // namespace duckdb
