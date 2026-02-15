#pragma once

#include <string>
#include <regex>
#include <sstream>
#include <cctype>

namespace duckdb {

/**
 * Safe parsing utilities to prevent catastrophic regex backtracking.
 *
 * PROBLEM: Build logs often contain extremely long lines (compiler errors with
 * template expansions, linker commands, etc.). Regex patterns with backtracking
 * (e.g., `[^:]+` followed by optional groups) can cause exponential time
 * complexity and stack overflow on such lines.
 *
 * SOLUTION: Use these utilities instead of raw std::regex operations.
 *
 * GUIDELINES FOR PARSER AUTHORS:
 * 1. Use SafeRegexMatch/SafeRegexSearch instead of std::regex_match/search
 * 2. For file:line:column patterns, use ParseFileLineColumn() instead of regex
 * 3. Prefer string find/substr operations over regex for simple patterns
 * 4. If you must use regex directly, check line.length() < MAX_REGEX_LINE_LENGTH first
 */
namespace SafeParsing {

/**
 * Normalize line endings to Unix-style LF (\n).
 *
 * Handles three line ending styles:
 * - CRLF (\r\n) - Windows
 * - LF (\n) - Unix/Linux/macOS
 * - CR (\r) - Old Mac (pre-OS X)
 *
 * This is similar to how DuckDB's CSV parser handles mixed line endings
 * in non-strict mode. All line endings are converted to \n before processing.
 *
 * @param content The raw content with potentially mixed line endings
 * @return Content with all line endings normalized to \n
 */
inline std::string NormalizeLineEndings(const std::string &content) {
	if (content.empty()) {
		return content;
	}

	std::string result;
	result.reserve(content.size());

	for (size_t i = 0; i < content.size(); ++i) {
		char c = content[i];
		if (c == '\r') {
			// Check if this is \r\n (Windows) - consume the \n as part of single newline
			if (i + 1 < content.size() && content[i + 1] == '\n') {
				++i; // Skip the \n, we'll add a single \n
			}
			// Whether \r alone (old Mac) or \r\n (Windows), emit single \n
			result += '\n';
		} else {
			result += c;
		}
	}

	return result;
}

/**
 * Detect the predominant line ending style in content.
 *
 * Returns one of:
 * - "\\r\\n" for CRLF (Windows)
 * - "\\n" for LF (Unix/Linux/macOS)
 * - "\\r" for CR (Old Mac)
 * - "" if no line endings detected
 */
inline std::string DetectLineEnding(const std::string &content) {
	bool has_cr = false;
	bool has_lf = false;
	bool has_crlf = false;

	for (size_t i = 0; i < content.size(); ++i) {
		if (content[i] == '\r') {
			if (i + 1 < content.size() && content[i + 1] == '\n') {
				has_crlf = true;
				++i; // Skip the \n
			} else {
				has_cr = true;
			}
		} else if (content[i] == '\n') {
			has_lf = true;
		}
	}

	// Prioritize CRLF detection (most common mixed case)
	if (has_crlf) {
		return "\\r\\n";
	}
	if (has_lf) {
		return "\\n";
	}
	if (has_cr) {
		return "\\r";
	}
	return "";
}

// Maximum line length to attempt regex matching
// Lines longer than this are skipped to prevent catastrophic backtracking
constexpr size_t MAX_REGEX_LINE_LENGTH = 2000;

// Maximum length for the file path portion in file:line:column patterns
constexpr size_t MAX_FILE_PATH_LENGTH = 500;

/**
 * Safe wrapper for std::regex_match that skips long lines.
 *
 * @param line The line to match
 * @param match Output match results
 * @param pattern The regex pattern
 * @param max_length Maximum line length to attempt matching (default: MAX_REGEX_LINE_LENGTH)
 * @return true if matched, false if line too long or no match
 */
inline bool SafeRegexMatch(const std::string &line, std::smatch &match, const std::regex &pattern,
                           size_t max_length = MAX_REGEX_LINE_LENGTH) {
	if (line.length() > max_length) {
		return false;
	}
	return std::regex_match(line, match, pattern);
}

/**
 * Safe wrapper for std::regex_search that skips long lines.
 *
 * @param line The line to search
 * @param match Output match results
 * @param pattern The regex pattern
 * @param max_length Maximum line length to attempt searching (default: MAX_REGEX_LINE_LENGTH)
 * @return true if found, false if line too long or not found
 */
inline bool SafeRegexSearch(const std::string &line, std::smatch &match, const std::regex &pattern,
                            size_t max_length = MAX_REGEX_LINE_LENGTH) {
	if (line.length() > max_length) {
		return false;
	}
	return std::regex_search(line, match, pattern);
}

/**
 * Parse file:line:column format WITHOUT regex (no backtracking risk).
 * Handles formats like:
 *   - /path/file.cpp:42:10: error: message
 *   - file.cpp:42: error: message
 *   - C:\path\file.cpp:42:10: warning: message
 *
 * @param line The line to parse
 * @param file Output: the file path
 * @param line_num Output: the line number (-1 if not found)
 * @param column Output: the column number (-1 if not found)
 * @return true if successfully parsed file:line pattern
 */
inline bool ParseFileLineColumn(const std::string &line, std::string &file, int &line_num, int &column) {
	line_num = -1;
	column = -1;
	file.clear();

	if (line.empty())
		return false;

	// For very long lines, only check the first portion for file:line pattern
	size_t search_limit = std::min(line.length(), MAX_FILE_PATH_LENGTH + 20);

	// Find first colon that's followed by a digit (start of line number)
	// Handle Windows paths like C:\path by skipping single-char drive letters
	size_t pos1 = 0;
	while (pos1 < search_limit) {
		pos1 = line.find(':', pos1);
		if (pos1 == std::string::npos || pos1 == 0)
			return false;

		// Skip Windows drive letters (single char before colon)
		if (pos1 == 1 && std::isalpha(line[0])) {
			pos1++;
			continue;
		}

		// Check if followed by digit
		if (pos1 + 1 < line.length() && std::isdigit(line[pos1 + 1])) {
			break;
		}
		pos1++;
	}

	if (pos1 >= search_limit)
		return false;

	// Extract file path
	file = line.substr(0, pos1);
	if (file.length() > MAX_FILE_PATH_LENGTH)
		return false;

	// Find end of line number
	size_t pos2 = line.find(':', pos1 + 1);
	if (pos2 == std::string::npos)
		return false;

	// Extract and validate line number
	std::string line_str = line.substr(pos1 + 1, pos2 - pos1 - 1);
	if (line_str.empty() || !std::isdigit(line_str[0]))
		return false;

	try {
		line_num = std::stoi(line_str);
	} catch (...) {
		return false;
	}

	// Try to extract column (optional)
	if (pos2 + 1 < line.length() && std::isdigit(line[pos2 + 1])) {
		size_t pos3 = line.find(':', pos2 + 1);
		if (pos3 != std::string::npos) {
			std::string col_str = line.substr(pos2 + 1, pos3 - pos2 - 1);
			if (!col_str.empty() && std::isdigit(col_str[0])) {
				try {
					column = std::stoi(col_str);
				} catch (...) {
					column = -1;
				}
			}
		}
	}

	return true;
}

/**
 * Parse file:line:column: severity: message format WITHOUT regex.
 * For GCC/Clang style compiler output.
 *
 * @param line The line to parse
 * @param file Output: the file path
 * @param line_num Output: the line number
 * @param column Output: the column number (-1 if not found)
 * @param severity Output: "error", "warning", or "note"
 * @param message Output: the error/warning message
 * @return true if successfully parsed
 */
inline bool ParseCompilerDiagnostic(const std::string &line, std::string &file, int &line_num, int &column,
                                    std::string &severity, std::string &message) {
	// First parse file:line:column
	if (!ParseFileLineColumn(line, file, line_num, column)) {
		return false;
	}

	// Find severity marker
	size_t sev_pos = line.find(" error:");
	if (sev_pos == std::string::npos)
		sev_pos = line.find(" warning:");
	if (sev_pos == std::string::npos)
		sev_pos = line.find(" note:");
	if (sev_pos == std::string::npos)
		return false;

	// Determine severity
	if (line.find(" error:") != std::string::npos && line.find(" error:") == sev_pos) {
		severity = "error";
	} else if (line.find(" warning:") != std::string::npos && line.find(" warning:") == sev_pos) {
		severity = "warning";
	} else {
		severity = "note";
	}

	// Extract message (everything after "severity: ")
	size_t msg_start = line.find(':', sev_pos + 1);
	if (msg_start == std::string::npos)
		return false;

	message = line.substr(msg_start + 1);

	// Trim leading whitespace
	size_t content_start = message.find_first_not_of(" \t");
	if (content_start != std::string::npos && content_start > 0) {
		message = message.substr(content_start);
	}

	return true;
}

/**
 * Check if a regex pattern is potentially dangerous for backtracking.
 * This is a compile-time helper for code review, not runtime enforcement.
 *
 * Dangerous patterns include:
 * - Nested quantifiers: (a+)+, (a*)*
 * - Adjacent quantifiers with overlap: .*.*
 * - Unbounded character classes followed by specific chars: [^:]+:
 *
 * If your pattern matches any of these, use string parsing instead.
 */
inline bool HasPotentialBacktracking(const std::string &pattern) {
	// Check for [^x]+ or [^x]* patterns (common cause of backtracking)
	if (pattern.find("[^") != std::string::npos &&
	    (pattern.find("]+") != std::string::npos || pattern.find("]*") != std::string::npos)) {
		return true;
	}
	// Check for nested quantifiers
	if (pattern.find(")+)") != std::string::npos || pattern.find(")*)*") != std::string::npos ||
	    pattern.find("+)+") != std::string::npos) {
		return true;
	}
	// Check for .* followed by more patterns
	size_t dotstar = pattern.find(".*");
	if (dotstar != std::string::npos && dotstar < pattern.length() - 2) {
		return true;
	}
	return false;
}

/**
 * Iterator wrapper that limits line length during parsing.
 * Use this when you need to process content line-by-line safely.
 */
class SafeLineReader {
public:
	explicit SafeLineReader(const std::string &content, size_t max_line_length = MAX_REGEX_LINE_LENGTH)
	    : stream_(NormalizeLineEndings(content)), max_length_(max_line_length), line_number_(0) {
	}

	/**
	 * Read the next line. Long lines are truncated with "..." suffix.
	 * @param line Output: the next line (possibly truncated)
	 * @return true if a line was read, false at end of input
	 */
	bool getLine(std::string &line) {
		if (!std::getline(stream_, line)) {
			return false;
		}
		line_number_++;

		// Remove trailing CR if present (Windows line endings)
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		// Truncate if too long
		if (line.length() > max_length_) {
			line = line.substr(0, max_length_ - 3) + "...";
			was_truncated_ = true;
		} else {
			was_truncated_ = false;
		}

		return true;
	}

	/**
	 * Get the current line number (1-based).
	 */
	int lineNumber() const {
		return line_number_;
	}

	/**
	 * Check if the last line was truncated.
	 */
	bool wasTruncated() const {
		return was_truncated_;
	}

private:
	std::istringstream stream_;
	size_t max_length_;
	int line_number_;
	bool was_truncated_ = false;
};

/**
 * Safe integer parsing utilities.
 * These wrap std::stoi/stol/stod with try-catch to return default values on failure.
 */

/**
 * Safe wrapper for std::stoi that returns a default value on failure.
 *
 * @param str The string to parse
 * @param default_value Value to return if parsing fails (default: -1)
 * @return Parsed integer or default_value on failure
 */
inline int SafeStoi(const std::string &str, int default_value = -1) {
	if (str.empty()) {
		return default_value;
	}
	try {
		return std::stoi(str);
	} catch (...) {
		return default_value;
	}
}

/**
 * Safe wrapper for std::stol that returns a default value on failure.
 *
 * @param str The string to parse
 * @param default_value Value to return if parsing fails (default: -1)
 * @return Parsed long or default_value on failure
 */
inline long SafeStol(const std::string &str, long default_value = -1) {
	if (str.empty()) {
		return default_value;
	}
	try {
		return std::stol(str);
	} catch (...) {
		return default_value;
	}
}

/**
 * Safe wrapper for std::stod that returns a default value on failure.
 *
 * @param str The string to parse
 * @param default_value Value to return if parsing fails (default: 0.0)
 * @return Parsed double or default_value on failure
 */
inline double SafeStod(const std::string &str, double default_value = 0.0) {
	if (str.empty()) {
		return default_value;
	}
	try {
		return std::stod(str);
	} catch (...) {
		return default_value;
	}
}

/**
 * Safe wrapper for std::stoi that returns success/failure status.
 *
 * @param str The string to parse
 * @param result Output: the parsed integer (unchanged on failure)
 * @return true if parsing succeeded, false otherwise
 */
inline bool TryStoi(const std::string &str, int &result) {
	if (str.empty()) {
		return false;
	}
	try {
		result = std::stoi(str);
		return true;
	} catch (...) {
		return false;
	}
}

/**
 * Safe wrapper for std::stol that returns success/failure status.
 *
 * @param str The string to parse
 * @param result Output: the parsed long (unchanged on failure)
 * @return true if parsing succeeded, false otherwise
 */
inline bool TryStol(const std::string &str, long &result) {
	if (str.empty()) {
		return false;
	}
	try {
		result = std::stol(str);
		return true;
	} catch (...) {
		return false;
	}
}

/**
 * Safe wrapper for std::stod that returns success/failure status.
 *
 * @param str The string to parse
 * @param result Output: the parsed double (unchanged on failure)
 * @return true if parsing succeeded, false otherwise
 */
inline bool TryStod(const std::string &str, double &result) {
	if (str.empty()) {
		return false;
	}
	try {
		result = std::stod(str);
		return true;
	} catch (...) {
		return false;
	}
}

} // namespace SafeParsing
} // namespace duckdb
