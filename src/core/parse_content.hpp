#pragma once

#include "duckdb.hpp"
#include "../include/validation_event_types.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Core parsing API for Duck Hunt.
 *
 * These functions provide a clean interface for parsing log content that can be used by:
 * - Table functions (read_duck_hunt_log, parse_duck_hunt_log)
 * - Workflow parsers (for internal delegation)
 * - Future lateral join variants
 *
 * Usage:
 *   // With explicit format
 *   auto events = ParseContent(context, log_content, "pytest_json");
 *
 *   // With auto-detection
 *   auto events = ParseContentAuto(context, log_content);
 *
 *   // Just detect format without parsing
 *   std::string format = DetectFormat(log_content);
 */

/**
 * Parse content using a specific format.
 *
 * @param context DuckDB client context (needed for some parsers that access files)
 * @param content The log/test output content to parse
 * @param format_name The format to use (e.g., "pytest_json", "make_error")
 *                    Can also be a format group (e.g., "python", "rust")
 * @return Vector of parsed validation events
 */
std::vector<ValidationEvent> ParseContent(ClientContext &context, const std::string &content,
                                          const std::string &format_name);

/**
 * Parse content with automatic format detection.
 *
 * @param context DuckDB client context
 * @param content The log/test output content to parse
 * @return Vector of parsed validation events (empty if no format detected)
 */
std::vector<ValidationEvent> ParseContentAuto(ClientContext &context, const std::string &content);

/**
 * Detect the format of content without parsing.
 *
 * @param content The log/test output content to analyze
 * @return Format name (e.g., "pytest_json") or empty string if unknown
 */
std::string DetectFormat(const std::string &content);

/**
 * Check if a format name is valid (exists in registry or is a group).
 *
 * @param format_name The format name to check
 * @return true if the format is recognized
 */
bool IsValidFormat(const std::string &format_name);

/**
 * Parse content using a regexp pattern.
 *
 * @param content The log/test output content to parse
 * @param pattern The regexp pattern with named capture groups
 * @param include_unparsed If true, include lines that don't match as UNKNOWN events
 * @return Vector of parsed validation events
 */
std::vector<ValidationEvent> ParseContentRegexp(const std::string &content, const std::string &pattern,
                                                bool include_unparsed = false);

/**
 * Parse a file directly using file-based parsing when supported.
 * For parsers that support file-based parsing (e.g., XML parsers using read_xml),
 * this is more efficient than reading content first.
 *
 * @param context DuckDB client context
 * @param file_path Path to the file to parse
 * @param format_name The format to use (e.g., "unity_test_xml")
 * @return Vector of parsed validation events
 */
std::vector<ValidationEvent> ParseFile(ClientContext &context, const std::string &file_path,
                                       const std::string &format_name);

} // namespace duckdb
