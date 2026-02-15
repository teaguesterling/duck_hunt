#include "lcov_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>
#include <map>
#include <vector>
#include <tuple>
#include <cmath>

namespace duckdb {

bool LcovParser::canParse(const std::string &content) const {
	if (content.empty())
		return false;

	// LCOV files have specific markers
	// Must have SF: (source file) and end_of_record
	bool has_sf = content.find("SF:") != std::string::npos;
	bool has_end = content.find("end_of_record") != std::string::npos;

	// Additional markers that confirm LCOV format
	bool has_da = content.find("\nDA:") != std::string::npos || content.find("DA:") == 0;
	bool has_lf = content.find("\nLF:") != std::string::npos;

	return has_sf && has_end && (has_da || has_lf);
}

std::vector<ValidationEvent> LcovParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	if (content.empty())
		return events;

	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	// Current file context
	std::string current_file;
	int32_t record_start_line = 0;

	// Per-file data
	std::map<int32_t, int32_t> line_hits;                                 // line -> hit count
	std::map<std::string, std::pair<int32_t, int32_t>> functions;         // name -> (line, hits)
	std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t>> branches; // line, block, branch, hits
	int32_t lines_found = 0, lines_hit = 0;
	int32_t functions_found = 0, functions_hit = 0;
	int32_t branches_found = 0, branches_hit = 0;

	auto emit_file_events = [&]() {
		if (current_file.empty())
			return;

		// Emit uncovered line warnings
		for (const auto &line_entry : line_hits) {
			int32_t line_no = line_entry.first;
			int32_t hits = line_entry.second;
			if (hits == 0) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.tool_name = "lcov";
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.status = ValidationEventStatus::WARNING;
				event.severity = "warning";
				event.category = "coverage";
				event.ref_file = current_file;
				event.ref_line = line_no;
				event.ref_column = -1;
				event.message = "Line not covered";
				event.log_line_start = record_start_line;
				event.log_line_end = line_num;
				events.push_back(event);
			}
		}

		// Emit function coverage events
		for (const auto &func_entry : functions) {
			const std::string &func_name = func_entry.first;
			const auto &data = func_entry.second;
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "lcov";
			event.event_type = ValidationEventType::PERFORMANCE_METRIC;
			event.status = data.second > 0 ? ValidationEventStatus::PASS : ValidationEventStatus::WARNING;
			event.severity = data.second > 0 ? "info" : "warning";
			event.category = "function_coverage";
			event.ref_file = current_file;
			event.ref_line = data.first;
			event.ref_column = -1;
			event.function_name = func_name;
			event.message = func_name + " called " + std::to_string(data.second) + " time(s)";
			event.structured_data = "{\"hit_count\":" + std::to_string(data.second) + "}";
			event.log_line_start = record_start_line;
			event.log_line_end = line_num;
			events.push_back(event);
		}

		// Emit branch coverage events
		for (const auto &branch_entry : branches) {
			int32_t br_line = std::get<0>(branch_entry);
			int32_t block = std::get<1>(branch_entry);
			int32_t branch = std::get<2>(branch_entry);
			int32_t hits = std::get<3>(branch_entry);
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "lcov";
			event.event_type = ValidationEventType::PERFORMANCE_METRIC;
			event.status = hits > 0 ? ValidationEventStatus::PASS : ValidationEventStatus::WARNING;
			event.severity = hits > 0 ? "info" : "warning";
			event.category = "branch_coverage";
			event.ref_file = current_file;
			event.ref_line = br_line;
			event.ref_column = -1;
			event.message = "Branch " + std::to_string(branch) + " at line " + std::to_string(br_line) +
			                (hits > 0 ? " taken " + std::to_string(hits) + " time(s)" : " not taken");
			event.structured_data = "{\"branch_id\":" + std::to_string(branch) +
			                        ",\"block_id\":" + std::to_string(block) +
			                        ",\"hit_count\":" + std::to_string(hits) + "}";
			event.log_line_start = record_start_line;
			event.log_line_end = line_num;
			events.push_back(event);
		}

		// Emit file summary event
		double coverage_pct = lines_found > 0 ? (static_cast<double>(lines_hit) / lines_found * 100.0) : 0.0;
		// Round to 2 decimal places
		coverage_pct = std::round(coverage_pct * 100.0) / 100.0;

		ValidationEvent summary;
		summary.event_id = event_id++;
		summary.tool_name = "lcov";
		summary.event_type = ValidationEventType::SUMMARY;
		summary.status = (lines_hit == lines_found) ? ValidationEventStatus::INFO : ValidationEventStatus::WARNING;
		summary.severity = (lines_hit == lines_found) ? "info" : "warning";
		summary.category = "coverage";
		summary.ref_file = current_file;
		summary.ref_line = -1;
		summary.ref_column = -1;
		summary.message = std::to_string(lines_hit) + "/" + std::to_string(lines_found) + " lines covered (" +
		                  std::to_string(coverage_pct) + "%)";

		// Build structured data
		std::string json = "{\"lines_found\":" + std::to_string(lines_found) +
		                   ",\"lines_hit\":" + std::to_string(lines_hit) +
		                   ",\"line_coverage_pct\":" + std::to_string(coverage_pct);
		if (functions_found > 0) {
			double func_pct = static_cast<double>(functions_hit) / functions_found * 100.0;
			func_pct = std::round(func_pct * 100.0) / 100.0;
			json += ",\"functions_found\":" + std::to_string(functions_found) +
			        ",\"functions_hit\":" + std::to_string(functions_hit) +
			        ",\"function_coverage_pct\":" + std::to_string(func_pct);
		}
		if (branches_found > 0) {
			double branch_pct = static_cast<double>(branches_hit) / branches_found * 100.0;
			branch_pct = std::round(branch_pct * 100.0) / 100.0;
			json += ",\"branches_found\":" + std::to_string(branches_found) +
			        ",\"branches_hit\":" + std::to_string(branches_hit) +
			        ",\"branch_coverage_pct\":" + std::to_string(branch_pct);
		}
		json += "}";
		summary.structured_data = json;
		summary.log_line_start = record_start_line;
		summary.log_line_end = line_num;

		events.push_back(summary);

		// Reset for next file
		current_file.clear();
		line_hits.clear();
		functions.clear();
		branches.clear();
		lines_found = lines_hit = 0;
		functions_found = functions_hit = 0;
		branches_found = branches_hit = 0;
	};

	while (std::getline(stream, line)) {
		line_num++;

		// Trim whitespace
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);

		if (line.empty())
			continue;

		// Parse LCOV directives
		if (line.substr(0, 3) == "TN:") {
			// Test name - ignore for now
			continue;
		} else if (line.substr(0, 3) == "SF:") {
			// Start new source file
			if (!current_file.empty()) {
				emit_file_events();
			}
			current_file = line.substr(3);
			record_start_line = line_num;
		} else if (line.substr(0, 3) == "FN:") {
			// Function definition: FN:<line>,<name>
			size_t comma = line.find(',', 3);
			if (comma != std::string::npos) {
				int32_t fn_line = SafeParsing::SafeStoi(line.substr(3, comma - 3));
				std::string fn_name = line.substr(comma + 1);
				functions[fn_name] = {fn_line, 0};
			}
		} else if (line.substr(0, 5) == "FNDA:") {
			// Function data: FNDA:<count>,<name>
			size_t comma = line.find(',', 5);
			if (comma != std::string::npos) {
				int32_t count = SafeParsing::SafeStoi(line.substr(5, comma - 5));
				std::string fn_name = line.substr(comma + 1);
				if (functions.count(fn_name)) {
					functions[fn_name].second = count;
				} else {
					functions[fn_name] = {-1, count};
				}
			}
		} else if (line.substr(0, 4) == "FNF:") {
			functions_found = SafeParsing::SafeStoi(line.substr(4));
		} else if (line.substr(0, 4) == "FNH:") {
			functions_hit = SafeParsing::SafeStoi(line.substr(4));
		} else if (line.substr(0, 3) == "DA:") {
			// Line data: DA:<line>,<count>
			size_t comma = line.find(',', 3);
			if (comma != std::string::npos) {
				int32_t da_line = SafeParsing::SafeStoi(line.substr(3, comma - 3));
				int32_t count = SafeParsing::SafeStoi(line.substr(comma + 1));
				line_hits[da_line] = count;
			}
		} else if (line.substr(0, 3) == "LF:") {
			lines_found = SafeParsing::SafeStoi(line.substr(3));
		} else if (line.substr(0, 3) == "LH:") {
			lines_hit = SafeParsing::SafeStoi(line.substr(3));
		} else if (line.substr(0, 5) == "BRDA:") {
			// Branch data: BRDA:<line>,<block>,<branch>,<count>
			// count can be "-" for not taken
			std::string data = line.substr(5);
			size_t p1 = data.find(',');
			size_t p2 = data.find(',', p1 + 1);
			size_t p3 = data.find(',', p2 + 1);
			if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
				int32_t br_line = SafeParsing::SafeStoi(data.substr(0, p1));
				int32_t block = SafeParsing::SafeStoi(data.substr(p1 + 1, p2 - p1 - 1));
				int32_t branch = SafeParsing::SafeStoi(data.substr(p2 + 1, p3 - p2 - 1));
				std::string count_str = data.substr(p3 + 1);
				int32_t count = (count_str == "-") ? 0 : SafeParsing::SafeStoi(count_str);
				branches.push_back({br_line, block, branch, count});
			}
		} else if (line.substr(0, 4) == "BRF:") {
			branches_found = SafeParsing::SafeStoi(line.substr(4));
		} else if (line.substr(0, 4) == "BRH:") {
			branches_hit = SafeParsing::SafeStoi(line.substr(4));
		} else if (line == "end_of_record") {
			emit_file_events();
		}
	}

	// Handle case where file doesn't end with end_of_record
	if (!current_file.empty()) {
		emit_file_events();
	}

	return events;
}

} // namespace duckdb
