#include "valgrind_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool ValgrindParser::CanParse(const std::string& content) const {
    // Check for Valgrind patterns (should be checked early due to unique format)
    return ((content.find("==") != std::string::npos && content.find("Memcheck") != std::string::npos) ||
            (content.find("==") != std::string::npos && content.find("Helgrind") != std::string::npos) ||
            (content.find("==") != std::string::npos && content.find("Cachegrind") != std::string::npos) ||
            (content.find("==") != std::string::npos && content.find("Massif") != std::string::npos) ||
            (content.find("==") != std::string::npos && content.find("DRD") != std::string::npos) ||
            (content.find("==") != std::string::npos && content.find("Valgrind") != std::string::npos));
}

void ValgrindParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseValgrind(content, events);
}

void ValgrindParser::ParseValgrind(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    uint64_t event_id = 1;
    
    std::string current_tool = "Valgrind";
    std::string current_pid;
    std::string current_error_type;
    std::string current_message;
    std::string current_location;
    std::string current_file;
    int current_line = 0;
    std::vector<std::string> stack_trace;
    bool in_error_block = false;
    
    // Regular expressions for different Valgrind patterns
    std::regex pid_regex(R"(==(\d+)==)");
    std::regex memcheck_header(R"(==\d+== Memcheck, a memory error detector)");
    std::regex helgrind_header(R"(==\d+== Helgrind, a thread error detector)");
    std::regex cachegrind_header(R"(==\d+== Cachegrind, a cache and branch-prediction profiler)");
    std::regex massif_header(R"(==\d+== Massif, a heap profiler)");
    std::regex drd_header(R"(==\d+== DRD, a thread error detector)");
    
    // Error patterns
    std::regex error_header(R"(==\d+== (.+))");
    std::regex invalid_read(R"(==\d+== Invalid read of size (\d+))");
    std::regex invalid_write(R"(==\d+== Invalid write of size (\d+))");
    std::regex invalid_free(R"(==\d+== Invalid free\(\) / delete / delete\[\])");
    std::regex mismatched_free(R"(==\d+== Mismatched free\(\) / delete / delete\[\])");
    std::regex use_after_free(R"(==\d+== Use of uninitialised value of size (\d+))");
    std::regex definitely_lost(R"(==\d+== (\d+) bytes in (\d+) blocks are definitely lost in loss record (\d+) of (\d+))");
    std::regex possibly_lost(R"(==\d+== (\d+) bytes in (\d+) blocks are possibly lost in loss record (\d+) of (\d+))");
    std::regex still_reachable(R"(==\d+== (\d+) bytes in (\d+) blocks are still reachable in loss record (\d+) of (\d+))");
    std::regex suppressed(R"(==\d+== (\d+) bytes in (\d+) blocks are suppressed)");
    
    // Location patterns
    std::regex at_location(R"(==\d+==    at (0x[0-9A-Fa-f]+): (.+) \((.+):(\d+)\))");
    std::regex at_location_no_file(R"(==\d+==    at (0x[0-9A-Fa-f]+): (.+))");
    std::regex by_location(R"(==\d+==    by (0x[0-9A-Fa-f]+): (.+) \((.+):(\d+)\))");
    std::regex by_location_no_file(R"(==\d+==    by (0x[0-9A-Fa-f]+): (.+))");
    
    // Summary patterns
    std::regex heap_summary(R"(==\d+== HEAP SUMMARY:)");
    std::regex in_use_at_exit(R"(==\d+==     in use at exit: ([\d,]+) bytes in ([\d,]+) blocks)");
    std::regex total_heap_usage(R"(==\d+==   total heap usage: ([\d,]+) allocs, ([\d,]+) frees, ([\d,]+) bytes allocated)");
    std::regex leak_summary(R"(==\d+== LEAK SUMMARY:)");
    std::regex leak_definitely_lost(R"(==\d+==    definitely lost: ([\d,]+) bytes in ([\d,]+) blocks)");
    std::regex leak_indirectly_lost(R"(==\d+==    indirectly lost: ([\d,]+) bytes in ([\d,]+) blocks)");
    std::regex leak_possibly_lost(R"(==\d+==      possibly lost: ([\d,]+) bytes in ([\d,]+) blocks)");
    std::regex leak_still_reachable(R"(==\d+==    still reachable: ([\d,]+) bytes in ([\d,]+) blocks)");
    std::regex leak_suppressed(R"(==\d+==         suppressed: ([\d,]+) bytes in ([\d,]+) blocks)");
    
    // Thread error patterns (Helgrind/DRD)
    std::regex data_race(R"(==\d+== Possible data race during (.+) of size (\d+) at (0x[0-9A-Fa-f]+) by thread #(\d+))");
    std::regex lock_order(R"(==\d+== Lock order violation: (.+))");
    std::regex thread_finish(R"(==\d+== Thread #(\d+) was created)");
    
    // Process patterns
    std::regex process_terminating(R"(==\d+== Process terminating with default action of signal (\d+) \((.+)\))");
    std::regex error_summary(R"(==\d+== ERROR SUMMARY: (\d+) errors from (\d+) contexts)");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Extract PID from Valgrind output
        if (std::regex_search(line, match, pid_regex)) {
            current_pid = match[1].str();
        }
        
        // Detect Valgrind tool type
        if (std::regex_search(line, memcheck_header)) {
            current_tool = "Memcheck";
        } else if (std::regex_search(line, helgrind_header)) {
            current_tool = "Helgrind";
        } else if (std::regex_search(line, cachegrind_header)) {
            current_tool = "Cachegrind";
        } else if (std::regex_search(line, massif_header)) {
            current_tool = "Massif";
        } else if (std::regex_search(line, drd_header)) {
            current_tool = "DRD";
        }
        
        // Handle different error types
        if (std::regex_search(line, match, invalid_read)) {
            current_error_type = "Invalid read";
            current_message = "Invalid read of size " + match[1].str();
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, invalid_write)) {
            current_error_type = "Invalid write";
            current_message = "Invalid write of size " + match[1].str();
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, invalid_free)) {
            current_error_type = "Invalid free";
            current_message = "Invalid free() / delete / delete[]";
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, mismatched_free)) {
            current_error_type = "Mismatched free";
            current_message = "Mismatched free() / delete / delete[]";
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, use_after_free)) {
            current_error_type = "Use of uninitialised value";
            current_message = "Use of uninitialised value of size " + match[1].str();
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, definitely_lost)) {
            current_error_type = "Memory leak";
            current_message = match[1].str() + " bytes in " + match[2].str() + " blocks are definitely lost";
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, possibly_lost)) {
            current_error_type = "Possible memory leak";
            current_message = match[1].str() + " bytes in " + match[2].str() + " blocks are possibly lost";
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, data_race)) {
            current_error_type = "Data race";
            current_message = "Possible data race during " + match[1].str() + " of size " + match[2].str() + " by thread #" + match[4].str();
            in_error_block = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, lock_order)) {
            current_error_type = "Lock order violation";
            current_message = match[1].str();
            in_error_block = true;
            stack_trace.clear();
        }
        
        // Handle stack trace locations
        if (std::regex_search(line, match, at_location)) {
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
                event.file_path = current_file;
                event.line_number = current_line;
                event.function_name = current_location;
                event.execution_time = 0;
                event.raw_output = content;
                event.structured_data = "valgrind";
                
                events.push_back(event);
                in_error_block = false;
            }
        } else if (std::regex_search(line, match, at_location_no_file)) {
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
                event.raw_output = content;
                event.structured_data = "valgrind";
                
                events.push_back(event);
                in_error_block = false;
            }
        } else if (std::regex_search(line, match, by_location)) {
            stack_trace.push_back(line);
        } else if (std::regex_search(line, match, by_location_no_file)) {
            stack_trace.push_back(line);
        }
        
        // Handle summaries
        if (std::regex_search(line, match, in_use_at_exit)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "heap_summary";
            event.message = "In use at exit: " + match[1].str() + " bytes in " + match[2].str() + " blocks";
            event.execution_time = 0;
            event.raw_output = content;
            event.structured_data = "valgrind";
            
            events.push_back(event);
        } else if (std::regex_search(line, match, total_heap_usage)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "heap_summary";
            event.message = "Total heap usage: " + match[1].str() + " allocs, " + match[2].str() + " frees, " + match[3].str() + " bytes allocated";
            event.execution_time = 0;
            event.raw_output = content;
            event.structured_data = "valgrind";
            
            events.push_back(event);
        } else if (std::regex_search(line, match, leak_definitely_lost)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "leak_summary";
            event.message = "Definitely lost: " + match[1].str() + " bytes in " + match[2].str() + " blocks";
            event.execution_time = 0;
            event.raw_output = content;
            event.structured_data = "valgrind";
            
            events.push_back(event);
        } else if (std::regex_search(line, match, leak_possibly_lost)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.status = duckdb::ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "leak_summary";
            event.message = "Possibly lost: " + match[1].str() + " bytes in " + match[2].str() + " blocks";
            event.execution_time = 0;
            event.raw_output = content;
            event.structured_data = "valgrind";
            
            events.push_back(event);
        } else if (std::regex_search(line, match, process_terminating)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = duckdb::ValidationEventType::MEMORY_ERROR;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "process_termination";
            event.message = "Process terminating with signal " + match[1].str() + " (" + match[2].str() + ")";
            event.execution_time = 0;
            event.raw_output = content;
            event.structured_data = "valgrind";
            
            events.push_back(event);
        } else if (std::regex_search(line, match, error_summary)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.status = (std::stoi(match[1].str()) > 0) ? duckdb::ValidationEventStatus::FAIL : duckdb::ValidationEventStatus::PASS;
            event.severity = (std::stoi(match[1].str()) > 0) ? "error" : "info";
            event.category = "error_summary";
            event.message = "Error summary: " + match[1].str() + " errors from " + match[2].str() + " contexts";
            event.execution_time = 0;
            event.raw_output = content;
            event.structured_data = "valgrind";
            
            events.push_back(event);
        }
    }
}

} // namespace duck_hunt