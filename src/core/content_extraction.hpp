#pragma once

#include "parsers/base/parser_interface.hpp"
#include "duckdb/common/file_system.hpp"
#include <atomic>
#include <string>

namespace duckdb {

/**
 * Extract the JSON section from potentially mixed content.
 * Heuristic:
 * 1. If first non-whitespace is [ or {, return content as-is (fast path).
 * 2. Scan for start-of-line [ or { followed by JSON-like characters.
 * 3. Return content from that position, or unchanged if no candidate found.
 */
std::string ExtractJsonSection(const std::string &content);

/**
 * Extract the XML section from potentially mixed content.
 * 1. Find <?xml declaration, or
 * 2. Find first < followed by a letter (tag start).
 * 3. Return content from that position, or unchanged if no candidate found.
 */
std::string ExtractXmlSection(const std::string &content);

/**
 * Dispatch content extraction by family.
 * Returns the input unchanged for TEXT family (fast path).
 */
std::string MaybeExtractContent(const std::string &content, ContentFamily family);

/**
 * Generate a unique temp file path for content extraction.
 * Uses an atomic counter to avoid collisions between concurrent queries.
 */
std::string MakeExtractTempPath(FileSystem &fs, const std::string &suffix = ".tmp");

/**
 * RAII guard for temp file cleanup.
 * Removes the file on destruction.
 */
struct TempFileGuard {
	FileSystem &fs;
	std::string path;
	~TempFileGuard() {
		if (!path.empty()) {
			try {
				fs.RemoveFile(path);
			} catch (...) {
			}
		}
	}
};

} // namespace duckdb
