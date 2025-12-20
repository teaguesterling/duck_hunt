#include "strace_parser.hpp"
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>

namespace duck_hunt {

// Categorize syscalls by type
static std::string GetSyscallCategory(const std::string& syscall) {
    static const std::unordered_set<std::string> file_syscalls = {
        "open", "openat", "read", "write", "close", "stat", "lstat", "fstat",
        "access", "faccessat", "chmod", "fchmod", "chown", "fchown", "mkdir",
        "rmdir", "unlink", "unlinkat", "rename", "renameat", "link", "linkat",
        "symlink", "symlinkat", "readlink", "readlinkat", "truncate", "ftruncate",
        "getdents", "getdents64", "lseek", "pread64", "pwrite64", "readv", "writev",
        "fcntl", "dup", "dup2", "dup3", "pipe", "pipe2", "statx", "newfstatat"
    };

    static const std::unordered_set<std::string> network_syscalls = {
        "socket", "bind", "listen", "accept", "accept4", "connect",
        "send", "sendto", "sendmsg", "recv", "recvfrom", "recvmsg",
        "setsockopt", "getsockopt", "shutdown", "getpeername", "getsockname",
        "socketpair", "sendmmsg", "recvmmsg"
    };

    static const std::unordered_set<std::string> process_syscalls = {
        "fork", "vfork", "clone", "clone3", "execve", "execveat",
        "exit", "exit_group", "wait4", "waitpid", "waitid",
        "kill", "tgkill", "tkill", "getpid", "getppid", "gettid",
        "getuid", "geteuid", "getgid", "getegid", "setuid", "setgid",
        "prctl", "arch_prctl", "set_tid_address", "set_robust_list"
    };

    static const std::unordered_set<std::string> memory_syscalls = {
        "mmap", "mmap2", "munmap", "mprotect", "brk", "mremap",
        "madvise", "mincore", "mlock", "munlock", "mlockall"
    };

    static const std::unordered_set<std::string> signal_syscalls = {
        "rt_sigaction", "rt_sigprocmask", "rt_sigreturn", "rt_sigsuspend",
        "sigaltstack", "signalfd", "signalfd4"
    };

    static const std::unordered_set<std::string> ipc_syscalls = {
        "eventfd", "eventfd2", "epoll_create", "epoll_create1", "epoll_ctl",
        "epoll_wait", "epoll_pwait", "poll", "ppoll", "select", "pselect6",
        "futex", "shmget", "shmat", "shmdt", "shmctl", "msgget", "msgsnd",
        "msgrcv", "msgctl", "semget", "semop", "semctl"
    };

    static const std::unordered_set<std::string> time_syscalls = {
        "clock_gettime", "clock_nanosleep", "nanosleep", "gettimeofday",
        "time", "times", "alarm", "timer_create", "timer_settime"
    };

    if (file_syscalls.count(syscall)) return "file";
    if (network_syscalls.count(syscall)) return "network";
    if (process_syscalls.count(syscall)) return "process";
    if (memory_syscalls.count(syscall)) return "memory";
    if (signal_syscalls.count(syscall)) return "signal";
    if (ipc_syscalls.count(syscall)) return "ipc";
    if (time_syscalls.count(syscall)) return "time";

    return "syscall";
}

// Extract file path from syscall arguments
static std::string ExtractFilePath(const std::string& args) {
    // Look for quoted path at start of args
    // Pattern: starts with quote, captures non-quote chars, ends with quote
    std::regex path_pattern("^\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(args, match, path_pattern)) {
        return match[1].str();
    }
    return "";
}

bool StraceParser::CanParse(const std::string& content) const {
    // Check for common strace patterns
    // Basic syscall pattern: syscall(args) = result
    if (std::regex_search(content, std::regex(R"(\w+\([^)]*\)\s*=\s*[-\d])"))) {
        return true;
    }
    // Signal pattern: --- SIGNAME {...} ---
    if (std::regex_search(content, std::regex(R"(---\s+SIG\w+\s+\{)"))) {
        return true;
    }
    // strace header
    if (content.find("execve(") != std::string::npos) {
        return true;
    }
    return false;
}

void StraceParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseStrace(content, events);
}

void StraceParser::ParseStrace(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    uint64_t event_id = 1;
    int32_t current_line_num = 0;

    // Patterns for strace output
    // Optional: [pid NNNN] prefix
    // Optional: HH:MM:SS or HH:MM:SS.usec timestamp
    // syscall(args) = result [error_name (error_message)] [<duration>]

    // Pattern breakdown:
    // ^(?:\[pid\s+(\d+)\]\s+)?          - Optional [pid N] prefix, capture pid
    // (?:(\d{2}:\d{2}:\d{2}(?:\.\d+)?)\s+)?  - Optional timestamp
    // (\w+)                              - Syscall name
    // \(([^)]*(?:\([^)]*\)[^)]*)*)\)    - Arguments (handles nested parens)
    // \s*=\s*                            - = separator
    // (-?\d+|0x[0-9a-fA-F]+|\?)         - Return value (number, hex, or ?)
    // (?:\s+(\w+)\s+\(([^)]+)\))?       - Optional error name and message
    // (?:\s+<([\d.]+)>)?                - Optional duration

    std::regex syscall_pattern(
        R"(^(?:\[pid\s+(\d+)\]\s+)?)"          // Optional [pid N]
        R"((?:(\d{2}:\d{2}:\d{2}(?:\.\d+)?)\s+)?)"  // Optional timestamp
        R"((\w+)\()"                            // Syscall name and opening paren
    );

    std::regex result_pattern(
        R"(\)\s*=\s*(-?\d+|0x[0-9a-fA-F]+|\?))"  // = result
        R"((?:\s+(\w+)(?:\s+\(([^)]+)\))?)?)"     // Optional errno and message
        R"((?:\s+<([\d.]+)>)?)"                   // Optional duration
        R"(\s*$)"
    );

    // Signal pattern: --- SIGNAME {si_signo=..., ...} ---
    std::regex signal_pattern(R"(^(?:\[pid\s+(\d+)\]\s+)?---\s+(SIG\w+)\s+\{([^}]+)\}\s+---)");

    // Unfinished syscall: syscall(args <unfinished ...>
    std::regex unfinished_pattern(R"(^(?:\[pid\s+(\d+)\]\s+)?(?:(\d{2}:\d{2}:\d{2}(?:\.\d+)?)\s+)?(\w+)\([^)]*<unfinished\s*\.\.\.>)");

    // Resumed syscall: <... syscall resumed>) = result
    std::regex resumed_pattern(R"(^(?:\[pid\s+(\d+)\]\s+)?(?:(\d{2}:\d{2}:\d{2}(?:\.\d+)?)\s+)?<\.\.\.\s+(\w+)\s+resumed>)");

    // Exit status: +++ exited with N +++
    std::regex exit_pattern(R"(\+\+\+\s+exited\s+with\s+(\d+)\s+\+\+\+)");

    // Killed by signal: +++ killed by SIGNAME +++
    std::regex killed_pattern(R"(\+\+\+\s+killed\s+by\s+(SIG\w+)(?:\s+\(core\s+dumped\))?\s+\+\+\+)");

    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;

        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        // Check for signal
        if (std::regex_search(line, match, signal_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "strace";
            event.event_type = duckdb::ValidationEventType::CRASH_SIGNAL;
            event.status = duckdb::ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "signal";

            std::string pid = match[1].str();
            std::string signal_name = match[2].str();
            std::string signal_info = match[3].str();

            event.function_name = signal_name;
            event.message = "Signal " + signal_name + ": " + signal_info;
            event.error_code = signal_name;
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            if (!pid.empty()) {
                event.scope_id = pid;
            }

            // Parse signal info for additional details
            if (signal_info.find("si_pid=") != std::string::npos) {
                std::regex sender_pattern(R"(si_pid=(\d+))");
                std::smatch sender_match;
                if (std::regex_search(signal_info, sender_match, sender_pattern)) {
                    event.origin = "pid:" + sender_match[1].str();
                }
            }

            events.push_back(event);
            continue;
        }

        // Check for exit status
        if (std::regex_search(line, match, exit_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "strace";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.severity = "info";
            event.category = "exit";
            event.function_name = "exit";
            event.error_code = match[1].str();

            int exit_code = std::stoi(match[1].str());
            event.status = (exit_code == 0) ? duckdb::ValidationEventStatus::PASS
                                            : duckdb::ValidationEventStatus::FAIL;
            event.message = "Process exited with code " + match[1].str();
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }

        // Check for killed by signal
        if (std::regex_search(line, match, killed_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "strace";
            event.event_type = duckdb::ValidationEventType::CRASH_SIGNAL;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "signal";
            event.function_name = match[1].str();
            event.error_code = match[1].str();
            event.message = "Process killed by " + match[1].str();

            if (line.find("core dumped") != std::string::npos) {
                event.message += " (core dumped)";
            }

            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }

        // Check for syscall pattern
        if (std::regex_search(line, match, syscall_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "strace";
            event.event_type = duckdb::ValidationEventType::DEBUG_EVENT;
            event.log_content = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            // Extract pid if present
            std::string pid = match[1].str();
            if (!pid.empty()) {
                event.scope_id = pid;
            }

            // Extract timestamp if present
            std::string timestamp = match[2].str();
            if (!timestamp.empty()) {
                event.started_at = timestamp;
            }

            // Extract syscall name
            std::string syscall = match[3].str();
            event.function_name = syscall;
            event.category = GetSyscallCategory(syscall);

            // Find the arguments and result
            size_t open_paren = line.find(syscall + "(");
            if (open_paren != std::string::npos) {
                open_paren += syscall.length() + 1;

                // Find matching close paren (handle nested parens)
                int depth = 1;
                size_t close_paren = open_paren;
                while (close_paren < line.length() && depth > 0) {
                    if (line[close_paren] == '(') depth++;
                    else if (line[close_paren] == ')') depth--;
                    if (depth > 0) close_paren++;
                }

                std::string args = line.substr(open_paren, close_paren - open_paren);

                // Extract file path from file-related syscalls
                if (event.category == "file") {
                    std::string path = ExtractFilePath(args);
                    if (!path.empty()) {
                        event.target = path;
                    }
                }

                // Parse result portion
                std::string result_part = line.substr(close_paren + 1);
                std::smatch result_match;
                if (std::regex_search(result_part, result_match, result_pattern)) {
                    std::string return_val = result_match[1].str();
                    std::string error_name = result_match[2].str();
                    std::string error_msg = result_match[3].str();
                    std::string duration = result_match[4].str();

                    // Determine success/failure
                    if (return_val == "-1" || (!error_name.empty() && error_name[0] == 'E')) {
                        event.status = duckdb::ValidationEventStatus::FAIL;
                        event.severity = "error";
                        event.error_code = error_name;

                        if (!error_msg.empty()) {
                            event.message = syscall + " failed: " + error_name + " (" + error_msg + ")";
                        } else if (!error_name.empty()) {
                            event.message = syscall + " failed: " + error_name;
                        } else {
                            event.message = syscall + " failed";
                        }
                    } else {
                        event.status = duckdb::ValidationEventStatus::PASS;
                        event.severity = "info";
                        event.message = syscall + "() = " + return_val;
                    }

                    // Parse duration if present
                    if (!duration.empty()) {
                        try {
                            // Convert seconds to milliseconds
                            event.execution_time = std::stod(duration) * 1000.0;
                        } catch (...) {
                            // Ignore parse errors
                        }
                    }
                } else {
                    // Unfinished or malformed
                    event.status = duckdb::ValidationEventStatus::INFO;
                    event.severity = "info";
                    event.message = syscall + " (incomplete)";
                }
            }

            events.push_back(event);
        }
    }

    // If no events were parsed, create a summary event
    if (events.empty()) {
        duckdb::ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "strace";
        event.event_type = duckdb::ValidationEventType::SUMMARY;
        event.status = duckdb::ValidationEventStatus::INFO;
        event.severity = "info";
        event.category = "summary";
        event.message = "No strace events parsed";
        event.log_line_start = 1;
        event.log_line_end = current_line_num;
        events.push_back(event);
    }
}

} // namespace duck_hunt
