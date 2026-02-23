#include "content_extraction.hpp"

namespace duckdb {

static std::atomic<uint64_t> temp_file_counter {0};

std::string MakeExtractTempPath(FileSystem &fs, const std::string &suffix) {
	auto id = temp_file_counter.fetch_add(1, std::memory_order_relaxed);
	return fs.JoinPath(fs.GetHomeDirectory(), ".duck_hunt_extract_tmp_" + std::to_string(id) + suffix);
}

// Helper: check if character is a line boundary (\n or \r)
static inline bool IsLineBreak(char c) {
	return c == '\n' || c == '\r';
}

std::string ExtractJsonSection(const std::string &content) {
	// Fast path: check if first non-whitespace is already JSON
	size_t pos = content.find_first_not_of(" \t\n\r");
	if (pos == std::string::npos) {
		return content;
	}
	if (content[pos] == '[' || content[pos] == '{') {
		return content; // Already starts with JSON
	}

	// Scan for start-of-line JSON: line-break followed by [ or {
	// Handles \n (Unix), \r\n (Windows), and \r (old Mac) line endings
	for (size_t i = 0; i < content.size(); i++) {
		if (IsLineBreak(content[i]) && i + 1 < content.size()) {
			size_t json_pos = i + 1;
			// Skip \n after \r for CRLF
			if (content[i] == '\r' && json_pos < content.size() && content[json_pos] == '\n') {
				json_pos++;
			}
			if (json_pos >= content.size()) {
				break;
			}
			char c = content[json_pos];
			if (c == '[' || c == '{') {
				// Check if followed by JSON-like character
				if (json_pos + 1 < content.size()) {
					char next = content[json_pos + 1];
					if (next == '"' || next == '{' || next == '[' || next == ']' || next == '}' || next == ' ' ||
					    next == '\t' || next == '\n' || next == '\r' || (next >= '0' && next <= '9')) {
						return content.substr(json_pos);
					}
				} else {
					// End of content right after [ or { — still valid
					return content.substr(json_pos);
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
