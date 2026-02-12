#include "gcc_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

bool CompilerDiagnosticParser::canParse(const std::string &content) const {
	// Must have compiler diagnostic markers
	if (content.find(" error:") == std::string::npos && content.find(" warning:") == std::string::npos &&
	    content.find(" note:") == std::string::npos) {
		return false;
	}

	return isCompilerDiagnostic(content);
}

// Check if file extension is for a compiled language (C/C++/Fortran/etc.)
static bool isCompiledLanguageFile(const std::string &file) {
	// Find the last dot for extension
	size_t dot_pos = file.rfind('.');
	if (dot_pos == std::string::npos || dot_pos == file.length() - 1) {
		return true; // No extension or ends with dot - be permissive
	}
	std::string ext = file.substr(dot_pos + 1);

	// C/C++ source files
	if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c++") {
		return true;
	}
	// C/C++ header files
	if (ext == "h" || ext == "hpp" || ext == "hh" || ext == "hxx" || ext == "h++") {
		return true;
	}
	// Fortran files
	if (ext == "f" || ext == "f90" || ext == "f95" || ext == "f03" || ext == "f08" || ext == "for" || ext == "fpp") {
		return true;
	}
	// Objective-C/C++
	if (ext == "m" || ext == "mm") {
		return true;
	}
	// CUDA
	if (ext == "cu" || ext == "cuh") {
		return true;
	}
	// Assembly
	if (ext == "s" || ext == "S" || ext == "asm") {
		return true;
	}
	// Ada (gnat)
	if (ext == "adb" || ext == "ads") {
		return true;
	}

	// Explicitly exclude interpreted language files
	if (ext == "py" || ext == "pyi" || ext == "js" || ext == "ts" || ext == "rb" || ext == "php") {
		return false;
	}

	// Unknown extension - be permissive
	return true;
}

bool CompilerDiagnosticParser::isCompilerDiagnostic(const std::string &content) const {
	// Look for GCC-style diagnostics using safe string parsing
	// Format: file:line:column: severity: message
	// or:     file:line: severity: message (no column)
	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int lines_checked = 0;
	int diagnostic_count = 0;

	while (reader.getLine(line) && lines_checked < 100) {
		lines_checked++;

		std::string file, severity, message;
		int line_num, col;

		if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
			// Skip files that are clearly not compiled languages (e.g., Python)
			if (!isCompiledLanguageFile(file)) {
				continue;
			}

			// Exclude clang-tidy style output (has rule names in brackets)
			// clang-tidy: "message [rule-name]"
			// GCC/Clang compiler: "message" or "message [-Wflag]"
			bool is_clang_tidy = false;
			if (!message.empty() && message.back() == ']') {
				size_t bracket_pos = message.rfind('[');
				if (bracket_pos != std::string::npos) {
					std::string bracket_content = message.substr(bracket_pos + 1, message.length() - bracket_pos - 2);
					// clang-tidy rules have patterns like "modernize-", "bugprone-", etc.
					// GCC warnings have patterns like "-Wunused" (start with -W)
					if (bracket_content.find('-') != std::string::npos && bracket_content[0] != '-') {
						is_clang_tidy = true;
					}
				}
			}

			if (!is_clang_tidy) {
				diagnostic_count++;
				// Need at least one diagnostic to claim we can parse
				if (diagnostic_count >= 1) {
					return true;
				}
			}
		}
	}

	return false;
}

std::vector<ValidationEvent> CompilerDiagnosticParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int64_t event_id = 1;
	std::string current_function;

	while (reader.getLine(line)) {
		int32_t current_line_num = reader.lineNumber();

		// Parse function context: "file: In function 'function_name':"
		// or: "file: In member function 'Class::method':"
		size_t in_func_pos = line.find(": In function '");
		if (in_func_pos == std::string::npos) {
			in_func_pos = line.find(": In member function '");
		}
		if (in_func_pos == std::string::npos) {
			in_func_pos = line.find(": In constructor '");
		}
		if (in_func_pos == std::string::npos) {
			in_func_pos = line.find(": In destructor '");
		}

		if (in_func_pos != std::string::npos) {
			size_t func_start = line.find('\'', in_func_pos) + 1;
			size_t func_end = line.find('\'', func_start);
			if (func_end != std::string::npos) {
				current_function = line.substr(func_start, func_end - func_start);
			}
			continue;
		}

		// Parse compiler diagnostic using safe string parsing
		std::string file, severity, message;
		int line_num, col;

		if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
			// Skip files that are clearly not compiled languages (e.g., Python)
			if (!isCompiledLanguageFile(file)) {
				continue;
			}

			// Skip clang-tidy style output (has rule names in brackets like [modernize-use-nullptr])
			// GCC warnings have patterns like [-Wunused] (start with -W)
			if (!message.empty() && message.back() == ']') {
				size_t bracket_pos = message.rfind('[');
				if (bracket_pos != std::string::npos) {
					std::string bracket_content = message.substr(bracket_pos + 1, message.length() - bracket_pos - 2);
					// clang-tidy rules contain '-' but don't start with '-'
					if (bracket_content.find('-') != std::string::npos && !bracket_content.empty() &&
					    bracket_content[0] != '-') {
						continue; // Skip clang-tidy output
					}
				}
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "compiler";
			event.ref_file = file;
			event.ref_line = line_num;
			event.ref_column = col;
			event.function_name = current_function;
			event.message = message;
			event.execution_time = 0.0;
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			if (severity == "error") {
				event.event_type = ValidationEventType::BUILD_ERROR;
				event.status = ValidationEventStatus::ERROR;
				event.category = "compilation";
				event.severity = "error";
			} else if (severity == "warning") {
				event.event_type = ValidationEventType::BUILD_ERROR;
				event.status = ValidationEventStatus::WARNING;
				event.category = "compilation";
				event.severity = "warning";
			} else { // note
				event.event_type = ValidationEventType::BUILD_ERROR;
				event.status = ValidationEventStatus::INFO;
				event.category = "compilation";
				event.severity = "info";
			}

			// Extract warning flag if present: "message [-Wflag]"
			if (!message.empty() && message.back() == ']') {
				size_t bracket_pos = message.rfind('[');
				if (bracket_pos != std::string::npos) {
					std::string flag = message.substr(bracket_pos + 1, message.length() - bracket_pos - 2);
					if (!flag.empty() && flag[0] == '-') {
						event.error_code = flag;
						// Trim flag from message
						event.message = message.substr(0, bracket_pos);
						// Trim trailing whitespace
						while (!event.message.empty() &&
						       (event.message.back() == ' ' || event.message.back() == '\t')) {
							event.message.pop_back();
						}
					}
				}
			}

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
