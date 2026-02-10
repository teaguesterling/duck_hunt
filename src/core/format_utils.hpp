#pragma once

#include "../include/read_duck_hunt_log_function.hpp"
#include <string>
#include <unordered_map>

namespace duckdb {

/**
 * Format conversion utilities for Duck Hunt.
 *
 * These functions handle conversion between format strings, enums, and canonical names.
 * They support format aliases (e.g., "pytest" -> PYTEST_TEXT, "python_log" -> PYTHON_LOGGING).
 */

/**
 * Get the format map for string-to-enum conversion.
 * Includes both primary names and aliases.
 */
const std::unordered_map<std::string, TestResultFormat> &GetFormatMap();

/**
 * Convert a format string to TestResultFormat enum.
 * Handles:
 * - Primary names (e.g., "pytest_json")
 * - Aliases (e.g., "pytest" -> PYTEST_TEXT)
 * - Special "regexp:pattern" syntax -> REGEXP
 *
 * @param str The format string to convert
 * @return The corresponding enum value, or UNKNOWN if not found
 */
TestResultFormat StringToTestResultFormat(const std::string &str);

/**
 * Get the canonical format name from an enum value.
 * Used for registry lookup when user provides an alias.
 *
 * @param format The enum value
 * @return The canonical format name, or empty string if unknown/auto
 */
std::string GetCanonicalFormatName(TestResultFormat format);

} // namespace duckdb
