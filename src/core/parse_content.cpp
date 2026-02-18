#include "parse_content.hpp"
#include "parser_registry.hpp"
#include "file_utils.hpp"
#include "../parsers/tool_outputs/regexp_parser.hpp"
#include "../parsers/config_based/config_parser.hpp"
#include <algorithm>

namespace duckdb {

// Check if format string looks like a config file path
static bool IsConfigFilePath(const std::string &format_name) {
	// Check for explicit config: prefix
	if (format_name.substr(0, 7) == "config:") {
		return true;
	}

	// Check for .json extension
	if (format_name.length() > 5) {
		std::string ext = format_name.substr(format_name.length() - 5);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (ext == ".json") {
			return true;
		}
	}

	// Check for URL patterns that might be config files
	if ((format_name.substr(0, 7) == "http://" || format_name.substr(0, 8) == "https://") &&
	    format_name.find(".json") != std::string::npos) {
		return true;
	}

	return false;
}

// Extract config file path from format string
static std::string ExtractConfigPath(const std::string &format_name) {
	if (format_name.substr(0, 7) == "config:") {
		return format_name.substr(7);
	}
	return format_name;
}

std::vector<ValidationEvent> ParseContent(ClientContext &context, const std::string &content,
                                          const std::string &format_name) {
	std::vector<ValidationEvent> events;

	if (format_name.empty() || format_name == "unknown" || format_name == "auto") {
		return events; // Invalid format, return empty
	}

	// Check if this is an inline config file path
	if (IsConfigFilePath(format_name)) {
		std::string config_path = ExtractConfigPath(format_name);

		// Read config file content
		std::string config_content = ReadContentFromSource(context, config_path);
		if (config_content.empty()) {
			return events; // Could not read config file
		}

		// Create temporary parser from config
		try {
			auto parser = ConfigBasedParser::FromJson(config_content);
			events = parser->parse(content);
		} catch (const std::exception &) {
			// Config parsing failed, return empty
			return events;
		}

		return events;
	}

	auto &registry = ParserRegistry::getInstance();

	// Check if this is a format group (e.g., "python", "rust", "ci")
	if (registry.isGroup(format_name)) {
		// Get all parsers in this group, sorted by priority
		auto group_parsers = registry.getParsersByGroup(format_name);

		// Try each parser in priority order until one produces events
		for (auto *parser : group_parsers) {
			if (parser->canParse(content)) {
				if (parser->requiresContext()) {
					auto parsed_events = parser->parseWithContext(context, content);
					events.insert(events.end(), parsed_events.begin(), parsed_events.end());
				} else {
					auto parsed_events = parser->parse(content);
					events.insert(events.end(), parsed_events.begin(), parsed_events.end());
				}
				if (!events.empty()) {
					return events; // Found a parser that produced events
				}
			}
		}
		return events; // No parser in group could parse this content
	}

	// Not a group - try direct format lookup
	auto *parser = registry.getParser(format_name);

	if (!parser) {
		return events; // No parser found
	}

	// Parser found - use it
	if (parser->requiresContext()) {
		events = parser->parseWithContext(context, content);
	} else {
		events = parser->parse(content);
	}

	return events;
}

std::vector<ValidationEvent> ParseContentAuto(ClientContext &context, const std::string &content) {
	std::string format = DetectFormat(content);
	if (format.empty()) {
		return {}; // No format detected
	}
	return ParseContent(context, content, format);
}

std::string DetectFormat(const std::string &content) {
	auto &registry = ParserRegistry::getInstance();
	auto *parser = registry.findParser(content);
	if (parser) {
		return parser->getFormatName();
	}
	return ""; // No format detected
}

bool IsValidFormat(const std::string &format_name) {
	if (format_name.empty() || format_name == "unknown") {
		return false;
	}
	if (format_name == "auto") {
		return true; // "auto" is always valid
	}

	// Config file paths are valid format specifiers
	if (IsConfigFilePath(format_name)) {
		return true;
	}

	auto &registry = ParserRegistry::getInstance();
	return registry.hasFormat(format_name) || registry.isGroup(format_name);
}

std::vector<ValidationEvent> ParseContentRegexp(const std::string &content, const std::string &pattern,
                                                bool include_unparsed) {
	std::vector<ValidationEvent> events;
	RegexpParser::ParseWithRegexp(content, pattern, events, include_unparsed);
	return events;
}

std::vector<ValidationEvent> ParseFile(ClientContext &context, const std::string &file_path,
                                       const std::string &format_name) {
	std::vector<ValidationEvent> events;

	if (format_name.empty() || format_name == "unknown" || format_name == "auto") {
		return events;
	}

	// Check if this is an inline config file path
	if (IsConfigFilePath(format_name)) {
		std::string config_path = ExtractConfigPath(format_name);

		// Read config file content
		std::string config_content = ReadContentFromSource(context, config_path);
		if (config_content.empty()) {
			return events; // Could not read config file
		}

		// Read log file content
		std::string log_content = ReadContentFromSource(context, file_path);
		if (log_content.empty()) {
			return events; // Could not read log file
		}

		// Create temporary parser from config and parse
		try {
			auto parser = ConfigBasedParser::FromJson(config_content);
			events = parser->parse(log_content);
		} catch (const std::exception &) {
			// Config parsing failed, return empty
			return events;
		}

		return events;
	}

	auto &registry = ParserRegistry::getInstance();
	auto *parser = registry.getParser(format_name);

	if (!parser) {
		return events;
	}

	// If parser supports file-based parsing, use it directly
	if (parser->supportsFileParsing()) {
		return parser->parseFile(context, file_path);
	}

	// Otherwise, fall back to reading content and parsing
	std::string content = ReadContentFromSource(context, file_path);
	if (content.empty()) {
		return events;
	}

	if (parser->requiresContext()) {
		events = parser->parseWithContext(context, content);
	} else {
		events = parser->parse(content);
	}

	return events;
}

} // namespace duckdb
