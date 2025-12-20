#include "gdb_lldb_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool GdbLldbParser::CanParse(const std::string& content) const {
    // Check for GDB/LLDB patterns (should be checked early due to unique format)
    return ((content.find("GNU gdb") != std::string::npos || content.find("(gdb)") != std::string::npos) ||
            (content.find("lldb") != std::string::npos && content.find("target create") != std::string::npos) ||
            (content.find("Program received signal") != std::string::npos && content.find("Segmentation fault") != std::string::npos) ||
            (content.find("Process") != std::string::npos && content.find("stopped") != std::string::npos && content.find("EXC_BAD_ACCESS") != std::string::npos) ||
            (content.find("frame #") != std::string::npos && content.find("0x") != std::string::npos));
}

void GdbLldbParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseGdbLldb(content, events);
}

void GdbLldbParser::ParseGdbLldb(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    uint64_t event_id = 1;
    int32_t current_line_num = 0;

    // Track current context
    std::string current_debugger = "GDB";
    std::string current_program;
    std::string current_frame;
    std::vector<std::string> stack_trace;
    bool in_backtrace = false;
    
    // Regular expressions for GDB/LLDB patterns
    std::regex gdb_header(R"(GNU gdb \(.*\) ([\d.]+))");
    std::regex lldb_header(R"(lldb.*version ([\d.]+))");
    std::regex program_start(R"(Starting program: (.+))");
    std::regex target_create(R"(target create \"(.+)\")");
    
    // Signal/crash patterns
    std::regex signal_received(R"(Program received signal (\w+), (.+))");
    std::regex exc_bad_access(R"(stop reason = EXC_BAD_ACCESS \(code=(\d+), address=(0x[0-9a-fA-F]+)\))");
    std::regex segfault_location(R"(0x([0-9a-fA-F]+) in (.+) \(.*\) at (.+):(\d+))");
    std::regex lldb_crash_frame(R"(frame #\d+: (0x[0-9a-fA-F]+) .+`(.+) at (.+):(\d+):(\d+))");
    
    // Backtrace patterns
    std::regex gdb_frame(R"(#(\d+)\s+(0x[0-9a-fA-F]+) in (.+) \(.*\) at (.+):(\d+))");
    std::regex gdb_frame_no_file(R"(#(\d+)\s+(0x[0-9a-fA-F]+) in (.+))");
    std::regex lldb_frame(R"(\* frame #(\d+): (0x[0-9a-fA-F]+) .+`(.+) at (.+):(\d+):(\d+))");
    std::regex lldb_frame_simple(R"(frame #(\d+): (0x[0-9a-fA-F]+) .+`(.+))");
    
    // Breakpoint patterns
    std::regex breakpoint_hit(R"(Breakpoint (\d+), (.+) \(.*\) at (.+):(\d+))");
    std::regex lldb_breakpoint_hit(R"(stop reason = breakpoint (\d+)\.(\d+))");
    std::regex breakpoint_set(R"(Breakpoint (\d+):.*where = .+`(.+) \+ \d+ at (.+):(\d+))");
    
    // Register and memory patterns
    std::regex register_line(R"((\w+)\s+(0x[0-9a-fA-F]+))");
    std::regex memory_access(R"(Cannot access memory at address (0x[0-9a-fA-F]+))");
    
    // Thread patterns
    std::regex thread_info(R"(\* thread #(\d+).*tid = (0x[0-9a-fA-F]+))");
    std::regex gdb_thread_info(R"(\* (\d+)\s+Thread (0x[0-9a-fA-F]+) \(LWP (\d+)\))");
    
    // Watchpoint patterns
    std::regex watchpoint_hit(R"(Hardware watchpoint (\d+): (.+))");
    std::regex watchpoint_set(R"(Watchpoint (\d+): addr = (0x[0-9a-fA-F]+))");
    
    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;

        // Detect debugger type
        if (std::regex_search(line, match, gdb_header)) {
            current_debugger = "GDB";
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "debugger_startup";
            event.message = "GDB version " + match[1].str() + " started";
            event.error_code = "DEBUGGER_START";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        } else if (std::regex_search(line, match, lldb_header)) {
            current_debugger = "LLDB";
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "debugger_startup";
            event.message = "LLDB version " + match[1].str() + " started";
            event.error_code = "DEBUGGER_START";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Program startup
        else if (std::regex_search(line, match, program_start)) {
            current_program = match[1].str();
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "program_start";
            event.message = "Started program: " + current_program;
            event.error_code = "PROGRAM_START";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        } else if (std::regex_search(line, match, target_create)) {
            current_program = match[1].str();
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "target_create";
            event.message = "Target created: " + current_program;
            event.error_code = "TARGET_CREATE";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Signal/crash detection
        else if (std::regex_search(line, match, signal_received)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::CRASH_SIGNAL;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "signal_crash";
            event.message = "Signal " + match[1].str() + ": " + match[2].str();
            event.error_code = match[1].str();
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        } else if (std::regex_search(line, match, exc_bad_access)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::CRASH_SIGNAL;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = "EXC_BAD_ACCESS at address " + match[2].str();
            event.error_code = "EXC_BAD_ACCESS";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Crash location detection
        else if (std::regex_search(line, match, segfault_location)) {
            if (!events.empty() && events.back().event_type == duckdb::ValidationEventType::CRASH_SIGNAL) {
                auto& last_event = events.back();
                last_event.function_name = match[2].str();
                last_event.ref_file = match[3].str();
                try {
                    last_event.ref_line = std::stoi(match[4].str());
                } catch (...) {
                    last_event.ref_line = -1;
                }
            }
        } else if (std::regex_search(line, match, lldb_crash_frame)) {
            if (!events.empty() && events.back().event_type == duckdb::ValidationEventType::CRASH_SIGNAL) {
                auto& last_event = events.back();
                last_event.function_name = match[2].str();
                last_event.ref_file = match[3].str();
                try {
                    last_event.ref_line = std::stoi(match[4].str());
                    last_event.ref_column = std::stoi(match[5].str());
                } catch (...) {
                    last_event.ref_line = -1;
                    last_event.ref_column = -1;
                }
            }
        }
        
        // Backtrace parsing
        else if (line.find("(gdb) bt") != std::string::npos || line.find("(lldb) bt") != std::string::npos) {
            in_backtrace = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, gdb_frame)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
                if (stack_trace.size() == 1 && !events.empty()) {
                    // First frame - update crash event with location details
                    auto& last_event = events.back();
                    if (last_event.ref_file.empty()) {
                        last_event.function_name = match[3].str();
                        last_event.ref_file = match[4].str();
                        try {
                            last_event.ref_line = std::stoi(match[5].str());
                        } catch (...) {
                            last_event.ref_line = -1;
                        }
                    }
                }
            }
        } else if (std::regex_search(line, match, gdb_frame_no_file)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
            }
        } else if (std::regex_search(line, match, lldb_frame)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
                if (stack_trace.size() == 1 && !events.empty()) {
                    auto& last_event = events.back();
                    if (last_event.ref_file.empty()) {
                        last_event.function_name = match[3].str();
                        last_event.ref_file = match[4].str();
                        try {
                            last_event.ref_line = std::stoi(match[5].str());
                            last_event.ref_column = std::stoi(match[6].str());
                        } catch (...) {
                            last_event.ref_line = -1;
                            last_event.ref_column = -1;
                        }
                    }
                }
            }
        } else if (std::regex_search(line, match, lldb_frame_simple)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
            }
        }
        
        // Breakpoint events
        else if (std::regex_search(line, match, breakpoint_hit)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_hit";
            event.function_name = match[2].str();
            event.ref_file = match[3].str();
            try {
                event.ref_line = std::stoi(match[4].str());
            } catch (...) {
                event.ref_line = -1;
            }
            event.message = "Breakpoint " + match[1].str() + " hit at " + match[2].str();
            event.error_code = "BREAKPOINT_HIT";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        } else if (std::regex_search(line, match, lldb_breakpoint_hit)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_hit";
            event.message = "Breakpoint " + match[1].str() + "." + match[2].str() + " hit";
            event.error_code = "BREAKPOINT_HIT";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        } else if (std::regex_search(line, match, breakpoint_set)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_set";
            event.function_name = match[2].str();
            event.ref_file = match[3].str();
            try {
                event.ref_line = std::stoi(match[4].str());
            } catch (...) {
                event.ref_line = -1;
            }
            event.message = "Breakpoint " + match[1].str() + " set at " + match[2].str();
            event.error_code = "BREAKPOINT_SET";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Watchpoint events
        else if (std::regex_search(line, match, watchpoint_hit)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "watchpoint_hit";
            event.message = "Watchpoint " + match[1].str() + " hit: " + match[2].str();
            event.error_code = "WATCHPOINT_HIT";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        } else if (std::regex_search(line, match, watchpoint_set)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "watchpoint_set";
            event.message = "Watchpoint " + match[1].str() + " set at address " + match[2].str();
            event.error_code = "WATCHPOINT_SET";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Memory access errors
        else if (std::regex_search(line, match, memory_access)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::MEMORY_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = "Cannot access memory at address " + match[1].str();
            event.error_code = "MEMORY_ACCESS_ERROR";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Thread information
        else if (std::regex_search(line, match, thread_info)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "thread_info";
            event.message = "Thread #" + match[1].str() + " (TID: " + match[2].str() + ")";
            event.error_code = "THREAD_INFO";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        } else if (std::regex_search(line, match, gdb_thread_info)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "thread_info";
            event.message = "Thread " + match[1].str() + " (LWP: " + match[3].str() + ")";
            event.error_code = "THREAD_INFO";
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // End backtrace when we see a new command prompt
        if ((line.find("(gdb)") != std::string::npos || line.find("(lldb)") != std::string::npos) && 
            line.find("bt") == std::string::npos) {
            if (in_backtrace && !stack_trace.empty()) {
                // Add stack trace to the last crash event
                if (!events.empty()) {
                    std::string complete_trace;
                    for (const auto& trace_line : stack_trace) {
                        if (!complete_trace.empty()) complete_trace += "\\n";
                        complete_trace += trace_line;
                    }
                    // Find the most recent crash or debug event to attach the stack trace
                    for (auto it = events.rbegin(); it != events.rend(); ++it) {
                        if (it->event_type == duckdb::ValidationEventType::CRASH_SIGNAL ||
                            it->event_type == duckdb::ValidationEventType::DEBUG_EVENT) {
                            it->structured_data = complete_trace;
                            break;
                        }
                    }
                }
                in_backtrace = false;
                stack_trace.clear();
            }
        }
    }
}

} // namespace duck_hunt