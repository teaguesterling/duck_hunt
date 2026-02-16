#include "error_patterns.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

namespace duckdb {

// Pre-compiled regexes for error message normalization (compiled once, reused)
namespace {
// File paths
const std::regex RE_FILE_EXT(R"([/\\][\w/\\.-]+\.(cpp|hpp|py|js|java|go|rs|rb|php|c|h)[:\s])");
const std::regex RE_UNIX_PATH(R"(/[\w/.-]+/)");
const std::regex RE_WIN_PATH(R"(\\[\w\\.-]+\\)");
// Timestamps
const std::regex RE_DATETIME(R"(\d{4}-\d{2}-\d{2}[T\s]\d{2}:\d{2}:\d{2})");
const std::regex RE_TIME(R"(\d{2}:\d{2}:\d{2})");
// Line/column numbers
const std::regex RE_LINE_COL(R"(:(\d+):(\d+):)");
const std::regex RE_LINE_NUM(R"(line\s+\d+)");
const std::regex RE_COL_NUM(R"(column\s+\d+)");
// IDs and addresses
const std::regex RE_HEX_ADDR(R"(0x[0-9a-fA-F]+)");
const std::regex RE_LONG_ID(R"(\b\d{6,}\b)");
// Quoted variables
const std::regex RE_SINGLE_QUOTED(R"('[\w.-]+')");
const std::regex RE_DOUBLE_QUOTED(R"("[\w.-]+")");
// Numbers
const std::regex RE_DECIMAL(R"(\b\d+\.\d+\b)");
const std::regex RE_INTEGER(R"(\b\d+\b)");
// Whitespace
const std::regex RE_WHITESPACE(R"(\s+)");
} // namespace

// Check if message likely contains content that needs normalization
// Used as a fast path to skip expensive regex operations
inline bool NeedsNormalization(const std::string &message) {
	for (char c : message) {
		// Check for characters that indicate normalizable content
		if (c == '/' || c == '\\' || c == ':' || c == '\'' || c == '"' ||
		    (c >= '0' && c <= '9') || c == '\t' || c == '\n') {
			return true;
		}
	}
	return false;
}

// Normalize error message for fingerprinting by removing variable content
// Optimized to reduce string copies where possible
std::string NormalizeErrorMessage(const std::string &message) {
	// Fast path: empty or very short messages don't need processing
	if (message.empty()) {
		return message;
	}

	// Reserve capacity to reduce reallocations
	std::string normalized;
	normalized.reserve(message.size());
	normalized = message;

	// Convert to lowercase for case-insensitive comparison
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

	// Fast path: if message has no normalizable content, skip regex operations
	if (!NeedsNormalization(normalized)) {
		// Just trim and return
		auto start = normalized.find_first_not_of(" \t");
		auto end = normalized.find_last_not_of(" \t");
		if (start != std::string::npos && end != std::string::npos && (start > 0 || end < normalized.size() - 1)) {
			return normalized.substr(start, end - start + 1);
		}
		return normalized;
	}

	// Apply regex replacements - order matters for efficiency
	// Do path replacements first (most specific to most general)
	normalized = std::regex_replace(normalized, RE_FILE_EXT, " <file> ");
	normalized = std::regex_replace(normalized, RE_UNIX_PATH, "/<path>/");
	normalized = std::regex_replace(normalized, RE_WIN_PATH, "\\<path>\\");

	// Remove timestamps
	normalized = std::regex_replace(normalized, RE_DATETIME, "<timestamp>");
	normalized = std::regex_replace(normalized, RE_TIME, "<time>");

	// Remove line and column numbers
	normalized = std::regex_replace(normalized, RE_LINE_COL, ":<line>:<col>:");
	normalized = std::regex_replace(normalized, RE_LINE_NUM, "line <num>");
	normalized = std::regex_replace(normalized, RE_COL_NUM, "column <num>");

	// Remove numeric IDs and memory addresses
	normalized = std::regex_replace(normalized, RE_HEX_ADDR, "<addr>");
	normalized = std::regex_replace(normalized, RE_LONG_ID, "<id>");

	// Remove variable names in quotes
	normalized = std::regex_replace(normalized, RE_SINGLE_QUOTED, "'<var>'");
	normalized = std::regex_replace(normalized, RE_DOUBLE_QUOTED, "\"<var>\"");

	// Remove specific values but keep structure
	normalized = std::regex_replace(normalized, RE_DECIMAL, "<decimal>");
	normalized = std::regex_replace(normalized, RE_INTEGER, "<num>");

	// Normalize whitespace
	normalized = std::regex_replace(normalized, RE_WHITESPACE, " ");

	// Trim whitespace
	auto start = normalized.find_first_not_of(" \t");
	auto end = normalized.find_last_not_of(" \t");
	if (start != std::string::npos && end != std::string::npos) {
		return normalized.substr(start, end - start + 1);
	}

	return normalized;
}

// Generate a fingerprint for an error based on normalized message and context
std::string GenerateErrorFingerprint(const ValidationEvent &event) {
	std::string normalized = NormalizeErrorMessage(event.message);

	// Create a composite fingerprint including tool and category context
	std::string fingerprint_source = event.tool_name + ":" + event.category + ":" + normalized;

	// Simple hash - in production, consider using a proper hash function
	std::hash<std::string> hasher;
	size_t hash_value = hasher(fingerprint_source);

	// Convert to hex string for readability
	std::stringstream ss;
	ss << std::hex << hash_value;

	return event.tool_name + "_" + event.category + "_" + ss.str();
}

// Calculate similarity between two error messages using simple edit distance approach
double CalculateMessageSimilarity(const std::string &msg1, const std::string &msg2) {
	std::string norm1 = NormalizeErrorMessage(msg1);
	std::string norm2 = NormalizeErrorMessage(msg2);

	if (norm1.empty() && norm2.empty())
		return 1.0;
	if (norm1.empty() || norm2.empty())
		return 0.0;
	if (norm1 == norm2)
		return 1.0;

	// Simple Levenshtein distance approximation
	size_t len1 = norm1.length();
	size_t len2 = norm2.length();
	size_t max_len = std::max(len1, len2);

	// Count common substrings for a simple similarity metric
	size_t common_chars = 0;
	size_t min_len = std::min(len1, len2);

	for (size_t i = 0; i < min_len; i++) {
		if (norm1[i] == norm2[i]) {
			common_chars++;
		}
	}

	// Add bonus for common keywords
	std::vector<std::string> keywords = {"error",   "warning",    "failed",   "exception",
	                                     "timeout", "permission", "not found"};
	size_t keyword_matches = 0;

	for (const auto &keyword : keywords) {
		if (norm1.find(keyword) != std::string::npos && norm2.find(keyword) != std::string::npos) {
			keyword_matches++;
		}
	}

	double base_similarity = static_cast<double>(common_chars) / max_len;
	double keyword_bonus = static_cast<double>(keyword_matches) * 0.1;

	return std::min(1.0, base_similarity + keyword_bonus);
}

// Detect root cause category based on error content and context
std::string DetectRootCauseCategory(const ValidationEvent &event) {
	std::string message_lower = event.message;
	std::transform(message_lower.begin(), message_lower.end(), message_lower.begin(), ::tolower);

	// Network-related errors
	if (message_lower.find("connection") != std::string::npos || message_lower.find("timeout") != std::string::npos ||
	    message_lower.find("unreachable") != std::string::npos || message_lower.find("network") != std::string::npos ||
	    message_lower.find("dns") != std::string::npos) {
		return "network";
	}

	// Permission and access errors
	if (message_lower.find("permission") != std::string::npos ||
	    message_lower.find("access denied") != std::string::npos ||
	    message_lower.find("unauthorized") != std::string::npos ||
	    message_lower.find("forbidden") != std::string::npos ||
	    message_lower.find("authentication") != std::string::npos) {
		return "permission";
	}

	// Configuration errors
	if (message_lower.find("config") != std::string::npos ||
	    message_lower.find("invalid resource") != std::string::npos ||
	    message_lower.find("not found") != std::string::npos ||
	    message_lower.find("does not exist") != std::string::npos ||
	    message_lower.find("missing") != std::string::npos) {
		return "configuration";
	}

	// Resource errors (memory, disk, etc.)
	if (message_lower.find("memory") != std::string::npos || message_lower.find("disk") != std::string::npos ||
	    message_lower.find("space") != std::string::npos || message_lower.find("quota") != std::string::npos ||
	    message_lower.find("limit") != std::string::npos) {
		return "resource";
	}

	// Syntax and validation errors
	if (message_lower.find("syntax") != std::string::npos || message_lower.find("parse") != std::string::npos ||
	    message_lower.find("invalid") != std::string::npos || message_lower.find("format") != std::string::npos ||
	    event.event_type == ValidationEventType::LINT_ISSUE || event.event_type == ValidationEventType::TYPE_ERROR) {
		return "syntax";
	}

	// Build and dependency errors
	if (message_lower.find("build") != std::string::npos || message_lower.find("compile") != std::string::npos ||
	    message_lower.find("dependency") != std::string::npos || message_lower.find("package") != std::string::npos ||
	    event.event_type == ValidationEventType::BUILD_ERROR) {
		return "build";
	}

	// Test-specific errors
	if (event.event_type == ValidationEventType::TEST_RESULT) {
		return "test_logic";
	}

	// Default category
	return "unknown";
}

// Process events to generate error pattern metadata
void ProcessErrorPatterns(std::vector<ValidationEvent> &events) {
	// Step 1: Generate fingerprints for each event
	for (auto &event : events) {
		event.fingerprint = GenerateErrorFingerprint(event);
	}

	// Step 2: Assign pattern IDs based on fingerprint clustering
	// Also track the representative (first) message for each pattern - O(n) instead of O(n²)
	std::map<std::string, int64_t> fingerprint_to_pattern_id;
	std::map<int64_t, std::string> pattern_id_to_representative;
	int64_t next_pattern_id = 1;

	for (auto &event : events) {
		auto it = fingerprint_to_pattern_id.find(event.fingerprint);
		if (it == fingerprint_to_pattern_id.end()) {
			int64_t pattern_id = next_pattern_id++;
			fingerprint_to_pattern_id[event.fingerprint] = pattern_id;
			// First occurrence becomes the representative message
			pattern_id_to_representative[pattern_id] = event.message;
			event.pattern_id = pattern_id;
		} else {
			event.pattern_id = it->second;
		}
	}

	// Step 3: Calculate similarity scores within pattern groups - O(n) lookup
	for (auto &event : events) {
		if (event.pattern_id == -1)
			continue;

		// Look up representative message in O(1)
		auto it = pattern_id_to_representative.find(event.pattern_id);
		if (it != pattern_id_to_representative.end() && !it->second.empty()) {
			event.similarity_score = CalculateMessageSimilarity(event.message, it->second);
		}
	}
}

} // namespace duckdb
