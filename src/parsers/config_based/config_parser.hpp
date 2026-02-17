#pragma once

#include "parsers/base/parser_interface.hpp"
#include "validation_event_types.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>

namespace duckdb {

/**
 * A pattern definition for config-based parsing.
 */
struct ConfigPattern {
	std::string name;                                          // Optional name for documentation
	std::regex compiled_regex;                                 // Compiled regex pattern
	std::string original_pattern;                              // Original pattern string for error messages
	std::vector<std::string> group_names;                      // Named capture group names in order
	ValidationEventType event_type;                            // Event type to produce
	std::string fixed_severity;                                // Fixed severity (if set)
	std::unordered_map<std::string, std::string> severity_map; // Maps captured value -> severity
	std::unordered_map<std::string, std::string> status_map;   // Maps captured value -> status
};

/**
 * Detection configuration for config-based parser.
 */
struct ConfigDetection {
	std::vector<std::string> contains;     // Match if ANY of these are present
	std::vector<std::string> contains_all; // Match if ALL of these are present
	std::string regex_pattern;             // Match if this regex matches
	std::regex compiled_regex;             // Compiled regex (if regex_pattern set)
	bool has_regex = false;
};

/**
 * Parser that uses JSON configuration for parsing rules.
 * Supports dynamic regex patterns, detection methods, severity mapping, etc.
 */
class ConfigBasedParser : public IParser {
public:
	/**
	 * Create a ConfigBasedParser from JSON configuration.
	 * Throws std::runtime_error on invalid config.
	 */
	static unique_ptr<ConfigBasedParser> FromJson(const std::string &json_config);

	/**
	 * Create a ConfigBasedParser from parsed configuration values.
	 */
	ConfigBasedParser(std::string format_name, std::string display_name, std::string category, std::string description,
	                  std::string tool_name, int priority, std::vector<std::string> aliases,
	                  std::vector<std::string> groups, ConfigDetection detection, std::vector<ConfigPattern> patterns);

	// IParser interface
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return format_name_;
	}
	std::string getName() const override {
		return display_name_;
	}
	std::string getCategory() const override {
		return category_;
	}
	std::string getDescription() const override {
		return description_;
	}
	int getPriority() const override {
		return priority_;
	}
	std::vector<std::string> getAliases() const override {
		return aliases_;
	}
	std::vector<std::string> getGroups() const override {
		return groups_;
	}

	// Config-based parsers support streaming (line-by-line parsing)
	bool supportsStreaming() const override {
		return true;
	}
	std::vector<ValidationEvent> parseLine(const std::string &line, int32_t line_number,
	                                       int64_t &event_id) const override;

	/**
	 * Get the tool name (may differ from format name).
	 */
	std::string getToolName() const {
		return tool_name_;
	}

	/**
	 * Check if this parser is a built-in (cannot be unloaded).
	 * Config-based parsers are always custom (not built-in).
	 */
	bool isBuiltIn() const {
		return false;
	}

private:
	std::string format_name_;
	std::string display_name_;
	std::string category_;
	std::string description_;
	std::string tool_name_;
	int priority_;
	std::vector<std::string> aliases_;
	std::vector<std::string> groups_;
	ConfigDetection detection_;
	std::vector<ConfigPattern> patterns_;

	// Helper to parse a single line with all patterns
	std::vector<ValidationEvent> parseLineInternal(const std::string &line, int32_t line_number,
	                                               int64_t &event_id) const;

	// Helper to get named group value from match
	static std::string getGroupValue(const std::smatch &match, const std::vector<std::string> &group_names,
	                                 const std::vector<std::string> &target_names);
};

/**
 * Validate event type string and return enum.
 * Throws std::runtime_error on invalid event type.
 */
ValidationEventType ParseEventType(const std::string &event_type_str);

} // namespace duckdb
