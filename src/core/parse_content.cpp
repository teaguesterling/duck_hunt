#include "parse_content.hpp"
#include "parser_registry.hpp"
#include "file_utils.hpp"
#include "content_extraction.hpp"
#include "../parsers/tool_outputs/regexp_parser.hpp"
#include "../parsers/config_based/config_parser.hpp"
#include <algorithm>
#include <sstream>

namespace duckdb {

// Split a comma-separated format string into individual format names.
// Trims whitespace from each entry. Returns empty vector if no commas found.
static std::vector<std::string> SplitFormatList(const std::string &format_name) {
	if (format_name.find(',') == std::string::npos) {
		return {};
	}
	std::vector<std::string> formats;
	std::istringstream stream(format_name);
	std::string token;
	while (std::getline(stream, token, ',')) {
		// Trim whitespace
		size_t start = token.find_first_not_of(" \t");
		size_t end = token.find_last_not_of(" \t");
		if (start != std::string::npos) {
			formats.push_back(token.substr(start, end - start + 1));
		}
	}
	return formats;
}

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

	// Check for comma-separated format list (e.g., "gcc_text,make_error,cake_error")
	// Try each format in order, return events from first one that produces results.
	auto format_list = SplitFormatList(format_name);
	if (!format_list.empty()) {
		for (const auto &fmt : format_list) {
			auto fmt_events = ParseContent(context, content, fmt);
			if (!fmt_events.empty()) {
				return fmt_events;
			}
		}
		return events; // No format in list produced events
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
				auto effective = MaybeExtractContent(content, parser->getContentFamily());
				if (parser->requiresContext()) {
					auto parsed_events = parser->parseWithContext(context, effective);
					events.insert(events.end(), parsed_events.begin(), parsed_events.end());
				} else {
					auto parsed_events = parser->parse(effective);
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

	// Parser found - extract structured content if needed, then dispatch
	auto effective = MaybeExtractContent(content, parser->getContentFamily());
	if (parser->requiresContext()) {
		events = parser->parseWithContext(context, effective);
	} else {
		events = parser->parse(effective);
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

	// Comma-separated format lists: each entry must be valid
	auto format_list = SplitFormatList(format_name);
	if (!format_list.empty()) {
		for (const auto &fmt : format_list) {
			if (!IsValidFormat(fmt)) {
				return false;
			}
		}
		return true;
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

	// Check for comma-separated format list
	auto format_list = SplitFormatList(format_name);
	if (!format_list.empty()) {
		for (const auto &fmt : format_list) {
			auto fmt_events = ParseFile(context, file_path, fmt);
			if (!fmt_events.empty()) {
				return fmt_events;
			}
		}
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

	// If parser supports file-based parsing with TEXT content, use file path directly
	if (parser->supportsFileParsing() && parser->getContentFamily() == ContentFamily::TEXT) {
		return parser->parseFile(context, file_path);
	}

	// Read content, extract structured section if needed, then dispatch
	std::string content = ReadContentFromSource(context, file_path);
	if (content.empty()) {
		return events;
	}

	auto effective = MaybeExtractContent(content, parser->getContentFamily());
	if (parser->requiresContext()) {
		events = parser->parseWithContext(context, effective);
	} else {
		events = parser->parse(effective);
	}

	return events;
}

} // namespace duckdb
