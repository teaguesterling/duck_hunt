#include "valgrind_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

// Pre-compiled regex patterns for Valgrind parsing (compiled once, reused)
namespace {
// PID and header patterns
static const std::regex RE_PID(R"(==(\d+)==)");
static const std::regex RE_MEMCHECK_HEADER(R"(==\d+== Memcheck, a memory error detector)");
static const std::regex RE_HELGRIND_HEADER(R"(==\d+== Helgrind, a thread error detector)");
static const std::regex RE_CACHEGRIND_HEADER(R"(==\d+== Cachegrind, a cache and branch-prediction profiler)");
static const std::regex RE_MASSIF_HEADER(R"(==\d+== Massif, a heap profiler)");
static const std::regex RE_DRD_HEADER(R"(==\d+== DRD, a thread error detector)");

// Error patterns
static const std::regex RE_ERROR_HEADER(R"(==\d+== (.+))");
static const std::regex RE_INVALID_READ(R"(==\d+== Invalid read of size (\d+))");
static const std::regex RE_INVALID_WRITE(R"(==\d+== Invalid write of size (\d+))");
static const std::regex RE_INVALID_FREE(R"(==\d+== Invalid free\(\) / delete / delete\[\])");
static const std::regex RE_MISMATCHED_FREE(R"(==\d+== Mismatched free\(\) / delete / delete\[\])");
static const std::regex RE_USE_AFTER_FREE(R"(==\d+== Use of uninitialised value of size (\d+))");
static const std::regex RE_DEFINITELY_LOST(
    R"(==\d+== (\d+) bytes in (\d+) blocks are definitely lost in loss record (\d+) of (\d+))");
static const std::regex RE_POSSIBLY_LOST(R"(==\d+== (\d+) bytes in (\d+) blocks are possibly lost in loss record (\d+) of (\d+))");
static const std::regex RE_STILL_REACHABLE(
    R"(==\d+== (\d+) bytes in (\d+) blocks are still reachable in loss record (\d+) of (\d+))");
static const std::regex RE_SUPPRESSED(R"(==\d+== (\d+) bytes in (\d+) blocks are suppressed)");

// Location patterns
static const std::regex RE_AT_LOCATION(R"(==\d+==    at (0x[0-9A-Fa-f]+): (.+) \((.+):(\d+)\))");
static const std::regex RE_AT_LOCATION_NO_FILE(R"(==\d+==    at (0x[0-9A-Fa-f]+): (.+))");
static const std::regex RE_BY_LOCATION(R"(==\d+==    by (0x[0-9A-Fa-f]+): (.+) \((.+):(\d+)\))");
static const std::regex RE_BY_LOCATION_NO_FILE(R"(==\d+==    by (0x[0-9A-Fa-f]+): (.+))");

// Summary patterns
static const std::regex RE_HEAP_SUMMARY(R"(==\d+== HEAP SUMMARY:)");
static const std::regex RE_IN_USE_AT_EXIT(R"(==\d+==     in use at exit: ([\d,]+) bytes in ([\d,]+) blocks)");
static const std::regex RE_TOTAL_HEAP_USAGE(
    R"(==\d+==   total heap usage: ([\d,]+) allocs, ([\d,]+) frees, ([\d,]+) bytes allocated)");
static const std::regex RE_LEAK_SUMMARY(R"(==\d+== LEAK SUMMARY:)");
static const std::regex RE_LEAK_DEFINITELY_LOST(R"(==\d+==    definitely lost: ([\d,]+) bytes in ([\d,]+) blocks)");
static const std::regex RE_LEAK_INDIRECTLY_LOST(R"(==\d+==    indirectly lost: ([\d,]+) bytes in ([\d,]+) blocks)");
static const std::regex RE_LEAK_POSSIBLY_LOST(R"(==\d+==      possibly lost: ([\d,]+) bytes in ([\d,]+) blocks)");
static const std::regex RE_LEAK_STILL_REACHABLE(R"(==\d+==    still reachable: ([\d,]+) bytes in ([\d,]+) blocks)");
static const std::regex RE_LEAK_SUPPRESSED(R"(==\d+==         suppressed: ([\d,]+) bytes in ([\d,]+) blocks)");

// Thread error patterns (Helgrind/DRD)
static const std::regex RE_DATA_RACE(
    R"(==\d+== Possible data race during (.+) of size (\d+) at (0x[0-9A-Fa-f]+) by thread #(\d+))");
static const std::regex RE_LOCK_ORDER(R"(==\d+== Lock order violation: (.+))");
static const std::regex RE_THREAD_FINISH(R"(==\d+== Thread #(\d+) was created)");

// Process patterns
static const std::regex RE_PROCESS_TERMINATING(R"(==\d+== Process terminating with default action of signal (\d+) \((.+)\))");
static const std::regex RE_ERROR_SUMMARY(R"(==\d+== ERROR SUMMARY: (\d+) errors from (\d+) contexts)");
} // anonymous namespace

bool ValgrindParser::CanParse(const std::string &content) const {
	// Check for Valgrind patterns (should be checked early due to unique format)
	return ((content.find("==") != std::string::npos && content.find("Memcheck") != std::string::npos) ||
	        (content.find("==") != std::string::npos && content.find("Helgrind") != std::string::npos) ||
	        (content.find("==") != std::string::npos && content.find("Cachegrind") != std::string::npos) ||
	        (content.find("==") != std::string::npos && content.find("Massif") != std::string::npos) ||
	        (content.find("==") != std::string::npos && content.find("DRD") != std::string::npos) ||
	        (content.find("==") != std::string::npos && content.find("Valgrind") != std::string::npos));
}

void ValgrindParser::Parse(const std::string &content, std::vector<duckdb::ValidationEvent> &events) const {
	ParseValgrind(content, events);
}

void ValgrindParser::ParseValgrind(const std::string &content, std::vector<duckdb::ValidationEvent> &events) {
	std::istringstream stream(content);
	std::string line;
	uint64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_tool = "Valgrind";
	std::string current_pid;
	std::string current_error_type;
	std::string current_message;
	std::string current_location;
	std::string current_file;
	int current_line = 0;
	std::vector<std::string> stack_trace;
	bool in_error_block = false;

	// Reserve space for events (estimate based on content size)
	events.reserve(content.size() / 100);

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Extract PID from Valgrind output
		if (std::regex_search(line, match, RE_PID)) {
			current_pid = match[1].str();
		}

		// Detect Valgrind tool type
		if (std::regex_search(line, RE_MEMCHECK_HEADER)) {
			current_tool = "Memcheck";
		} else if (std::regex_search(line, RE_HELGRIND_HEADER)) {
			current_tool = "Helgrind";
		} else if (std::regex_search(line, RE_CACHEGRIND_HEADER)) {
			current_tool = "Cachegrind";
		} else if (std::regex_search(line, RE_MASSIF_HEADER)) {
			current_tool = "Massif";
		} else if (std::regex_search(line, RE_DRD_HEADER)) {
			current_tool = "DRD";
		}

		// Handle different error types
		if (std::regex_search(line, match, RE_INVALID_READ)) {
			current_error_type = "Invalid read";
			current_message = "Invalid read of size " + match[1].str();
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, match, RE_INVALID_WRITE)) {
			current_error_type = "Invalid write";
			current_message = "Invalid write of size " + match[1].str();
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, RE_INVALID_FREE)) {
			current_error_type = "Invalid free";
			current_message = "Invalid free() / delete / delete[]";
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, RE_MISMATCHED_FREE)) {
			current_error_type = "Mismatched free";
			current_message = "Mismatched free() / delete / delete[]";
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, match, RE_USE_AFTER_FREE)) {
			current_error_type = "Use of uninitialised value";
			current_message = "Use of uninitialised value of size " + match[1].str();
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, match, RE_DEFINITELY_LOST)) {
			current_error_type = "Memory leak";
			current_message = match[1].str() + " bytes in " + match[2].str() + " blocks are definitely lost";
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, match, RE_POSSIBLY_LOST)) {
			current_error_type = "Possible memory leak";
			current_message = match[1].str() + " bytes in " + match[2].str() + " blocks are possibly lost";
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, match, RE_DATA_RACE)) {
			current_error_type = "Data race";
			current_message = "Possible data race during " + match[1].str() + " of size " + match[2].str() +
			                  " by thread #" + match[4].str();
			in_error_block = true;
			stack_trace.clear();
		} else if (std::regex_search(line, match, RE_LOCK_ORDER)) {
			current_error_type = "Lock order violation";
			current_message = match[1].str();
			in_error_block = true;
			stack_trace.clear();
		}

		// Handle stack trace locations
		if (std::regex_search(line, match, RE_AT_LOCATION)) {
			current_location = match[2].str();
			current_file = match[3].str();
			current_line = std::stoi(match[4].str());
			stack_trace.push_back(line);

			if (in_error_block && !current_error_type.empty()) {
				duckdb::ValidationEvent event;
				event.event_id = event_id++;
				event.tool_name = current_tool;
				event.event_type = duckdb::ValidationEventType::MEMORY_ERROR;
				event.status = duckdb::ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = current_error_type;
				event.message = current_message;
				event.ref_file = current_file;
				event.ref_line = current_line;
				event.function_name = current_location;
				event.execution_time = 0;
				event.log_content = content;
				event.structured_data = "valgrind";
				event.log_line_start = current_line_num;
				event.log_line_end = current_line_num;

				events.push_back(event);
				in_error_block = false;
			}
		} else if (std::regex_search(line, match, RE_AT_LOCATION_NO_FILE)) {
			current_location = match[2].str();
			stack_trace.push_back(line);

			if (in_error_block && !current_error_type.empty()) {
				duckdb::ValidationEvent event;
				event.event_id = event_id++;
				event.tool_name = current_tool;
				event.event_type = duckdb::ValidationEventType::MEMORY_ERROR;
				event.status = duckdb::ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = current_error_type;
				event.message = current_message;
				event.function_name = current_location;
				event.execution_time = 0;
				event.log_content = content;
				event.structured_data = "valgrind";
				event.log_line_start = current_line_num;
				event.log_line_end = current_line_num;

				events.push_back(event);
				in_error_block = false;
			}
		} else if (std::regex_search(line, match, RE_BY_LOCATION)) {
			stack_trace.push_back(line);
		} else if (std::regex_search(line, match, RE_BY_LOCATION_NO_FILE)) {
			stack_trace.push_back(line);
		}

		// Handle summaries
		if (std::regex_search(line, match, RE_IN_USE_AT_EXIT)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = current_tool;
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "heap_summary";
			event.message = "In use at exit: " + match[1].str() + " bytes in " + match[2].str() + " blocks";
			event.execution_time = 0;
			event.log_content = content;
			event.structured_data = "valgrind";

			events.push_back(event);
		} else if (std::regex_search(line, match, RE_TOTAL_HEAP_USAGE)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = current_tool;
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "heap_summary";
			event.message = "Total heap usage: " + match[1].str() + " allocs, " + match[2].str() + " frees, " +
			                match[3].str() + " bytes allocated";
			event.execution_time = 0;
			event.log_content = content;
			event.structured_data = "valgrind";

			events.push_back(event);
		} else if (std::regex_search(line, match, RE_LEAK_DEFINITELY_LOST)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = current_tool;
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.status = duckdb::ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "leak_summary";
			event.message = "Definitely lost: " + match[1].str() + " bytes in " + match[2].str() + " blocks";
			event.execution_time = 0;
			event.log_content = content;
			event.structured_data = "valgrind";

			events.push_back(event);
		} else if (std::regex_search(line, match, RE_LEAK_POSSIBLY_LOST)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = current_tool;
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.status = duckdb::ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "leak_summary";
			event.message = "Possibly lost: " + match[1].str() + " bytes in " + match[2].str() + " blocks";
			event.execution_time = 0;
			event.log_content = content;
			event.structured_data = "valgrind";

			events.push_back(event);
		} else if (std::regex_search(line, match, RE_PROCESS_TERMINATING)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = current_tool;
			event.event_type = duckdb::ValidationEventType::MEMORY_ERROR;
			event.status = duckdb::ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "process_termination";
			event.message = "Process terminating with signal " + match[1].str() + " (" + match[2].str() + ")";
			event.execution_time = 0;
			event.log_content = content;
			event.structured_data = "valgrind";

			events.push_back(event);
		} else if (std::regex_search(line, match, RE_ERROR_SUMMARY)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = current_tool;
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.status = (std::stoi(match[1].str()) > 0) ? duckdb::ValidationEventStatus::FAIL
			                                               : duckdb::ValidationEventStatus::PASS;
			event.severity = (std::stoi(match[1].str()) > 0) ? "error" : "info";
			event.category = "error_summary";
			event.message = "Error summary: " + match[1].str() + " errors from " + match[2].str() + " contexts";
			event.execution_time = 0;
			event.log_content = content;
			event.structured_data = "valgrind";

			events.push_back(event);
		}
	}
}

} // namespace duck_hunt
