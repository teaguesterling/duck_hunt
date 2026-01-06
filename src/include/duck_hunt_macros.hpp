#pragma once

namespace duckdb {

class ExtensionLoader;

/**
 * Register Duck Hunt table macros.
 *
 * Currently registers:
 *
 * duck_hunt_match_command_patterns(cmd VARCHAR) -> TABLE
 *   Matches a command string against format command patterns from duck_hunt_formats().
 *   Results are ordered by priority (highest first, most specific formats).
 *
 *   Parameters:
 *     cmd - The command string to match (e.g., 'pytest tests/', 'cargo clippy')
 *
 *   Returns:
 *     format           - The format name that matched
 *     priority         - The format's detection priority (higher = more specific)
 *     matched_patterns - List of {matched_pattern, pattern_type} structs
 *
 *   Example:
 *     SELECT * FROM duck_hunt_match_command_patterns('pytest tests/');
 *     -- Returns one row per format: pytest_json (100), pytest_text (80)
 *     SELECT format FROM duck_hunt_match_command_patterns('cargo test') LIMIT 1;
 *     -- Returns the best matching format
 */
void RegisterDuckHuntMacros(ExtensionLoader &loader);

} // namespace duckdb
