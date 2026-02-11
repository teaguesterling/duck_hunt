#include "parse_content.hpp"
#include "parser_registry.hpp"
#include "../parsers/tool_outputs/regexp_parser.hpp"

namespace duckdb {

std::vector<ValidationEvent> ParseContent(ClientContext &context, const std::string &content,
                                          const std::string &format_name) {
	std::vector<ValidationEvent> events;

	if (format_name.empty() || format_name == "unknown" || format_name == "auto") {
		return events; // Invalid format, return empty
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

	auto &registry = ParserRegistry::getInstance();
	return registry.hasFormat(format_name) || registry.isGroup(format_name);
}

std::vector<ValidationEvent> ParseContentRegexp(const std::string &content, const std::string &pattern,
                                                bool include_unparsed) {
	std::vector<ValidationEvent> events;
	RegexpParser::ParseWithRegexp(content, pattern, events, include_unparsed);
	return events;
}

} // namespace duckdb
