#include "content_extraction.hpp"

namespace duckdb {

std::string ExtractJsonSection(const std::string &content) {
	// Fast path: check if first non-whitespace is already JSON
	size_t pos = content.find_first_not_of(" \t\n\r");
	if (pos == std::string::npos) {
		return content;
	}
	if (content[pos] == '[' || content[pos] == '{') {
		return content; // Already starts with JSON
	}

	// Scan for start-of-line JSON: \n[ or \n{
	for (size_t i = 0; i < content.size(); i++) {
		if (content[i] == '\n' && i + 1 < content.size()) {
			char c = content[i + 1];
			if (c == '[' || c == '{') {
				// Check if followed by JSON-like character
				if (i + 2 < content.size()) {
					char next = content[i + 2];
					if (next == '"' || next == '{' || next == '[' || next == ']' || next == '}' ||
					    next == ' ' || next == '\t' || next == '\n' || next == '\r' ||
					    (next >= '0' && next <= '9')) {
						return content.substr(i + 1);
					}
				} else {
					// End of content right after [ or { — still valid
					return content.substr(i + 1);
				}
			}
		}
	}

	// No valid candidate found — return unchanged
	return content;
}

std::string ExtractXmlSection(const std::string &content) {
	// Try <?xml declaration first
	auto xml_decl = content.find("<?xml");
	if (xml_decl != std::string::npos) {
		return content.substr(xml_decl);
	}

	// Find first < followed by a letter (XML tag start, not <!-- comment)
	for (size_t i = 0; i < content.size(); i++) {
		if (content[i] == '<' && i + 1 < content.size()) {
			char next = content[i + 1];
			if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
				return content.substr(i);
			}
		}
	}

	// No XML found — return unchanged
	return content;
}

std::string MaybeExtractContent(const std::string &content, ContentFamily family) {
	switch (family) {
	case ContentFamily::JSON:
		return ExtractJsonSection(content);
	case ContentFamily::XML:
		return ExtractXmlSection(content);
	default:
		return content;
	}
}

} // namespace duckdb
