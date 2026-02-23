#pragma once

#include <string>
#include <vector>
#include <memory>
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/client_context.hpp"
#include "include/validation_event_types.hpp"

namespace duckdb {

/**
 * Content family for framework-level content extraction.
 * The framework extracts structured content (JSON/XML) from mixed-format
 * input before dispatching to parsers, so parsers always receive clean content.
 */
enum class ContentFamily : uint8_t {
	TEXT = 0, // Line-based text (no extraction needed)
	JSON = 1, // JSON array or object
	XML = 2   // XML document
};

/**
 * Command pattern for format detection based on command string.
 * Used by tools like BIRD and blq to auto-detect format from command.
 */
struct CommandPattern {
	std::string pattern;      // The pattern to match against
	std::string pattern_type; // "literal", "like", or "regexp"

	CommandPattern(const std::string &p, const std::string &t) : pattern(p), pattern_type(t) {
	}

	// Convenience constructors
	static CommandPattern Literal(const std::string &p) {
		return CommandPattern(p, "literal");
	}
	static CommandPattern Like(const std::string &p) {
		return CommandPattern(p, "like");
	}
	static CommandPattern Regexp(const std::string &p) {
		return CommandPattern(p, "regexp");
	}
};

/**
 * Base interface for all log/test result parsers.
 * Each parser implements format detection and parsing logic for a specific tool/format.
 *
 * This is the unified parser interface - all parsers should implement this.
 * Uses string-based format names for flexibility and extensibility.
 */
class IParser {
public:
	virtual ~IParser() = default;

	// =========================================================================
	// Core parsing methods (required)
	// =========================================================================

	/**
	 * Check if this parser can handle the given content.
	 * Should be fast and lightweight for format detection.
	 */
	virtual bool canParse(const std::string &content) const = 0;

	/**
	 * Parse the content and return validation events.
	 * Only called if canParse() returns true.
	 */
	virtual std::vector<ValidationEvent> parse(const std::string &content) const = 0;

	// =========================================================================
	// Context-aware parsing (optional, has default implementation)
	// =========================================================================

	/**
	 * Parse the content with access to ClientContext.
	 * Used by parsers that need to call external functions (e.g., webbed for XML).
	 * Default implementation calls parse(content).
	 */
	virtual std::vector<ValidationEvent> parseWithContext(ClientContext &context, const std::string &content) const {
		return parse(content);
	}

	/**
	 * Check if this parser requires ClientContext for parsing.
	 * If true, parseWithContext() must be used instead of parse().
	 */
	virtual bool requiresContext() const {
		return false;
	}

	// =========================================================================
	// File-based parsing (optional, for parsers that work better with file paths)
	// =========================================================================

	/**
	 * Check if this parser supports file-based parsing.
	 * When true, parseFile() should be used instead of parseWithContext() when a file path is available.
	 * This is useful for XML parsers that can use read_xml() directly.
	 */
	virtual bool supportsFileParsing() const {
		return false;
	}

	/**
	 * Parse a file directly using the file path.
	 * Only called when supportsFileParsing() returns true and a file path is available.
	 * Default implementation reads file and calls parseWithContext().
	 */
	virtual std::vector<ValidationEvent> parseFile(ClientContext &context, const std::string &file_path) const {
		return {}; // Default: not supported
	}

	// =========================================================================
	// Streaming support (optional, enables line-by-line parsing)
	// =========================================================================

	/**
	 * Check if this parser supports streaming (line-by-line) parsing.
	 * When true, parseLine() can be used for incremental parsing, enabling:
	 * - Early termination with LIMIT without reading entire file
	 * - Reduced memory footprint for large files
	 *
	 * Default returns false - parsers must explicitly opt-in to streaming.
	 */
	virtual bool supportsStreaming() const {
		return false;
	}

	/**
	 * Parse a single line and return any events found.
	 * Only called when supportsStreaming() returns true.
	 *
	 * @param line The line content (without newline character)
	 * @param line_number The 1-based line number in the file
	 * @param event_id Reference to event ID counter (increment for each event)
	 * @return Vector of events found in this line (usually 0 or 1)
	 */
	virtual std::vector<ValidationEvent> parseLine(const std::string &line, int32_t line_number,
	                                               int64_t &event_id) const {
		// Default: not supported, return empty
		return {};
	}

	// =========================================================================
	// Metadata methods (required)
	// =========================================================================

	/**
	 * Get the format name this parser handles (e.g., "pytest_json", "flake8_text").
	 * This is the primary identifier used for format lookup.
	 */
	virtual std::string getFormatName() const = 0;

	/**
	 * Get a human-readable name for this parser (e.g., "Pytest JSON Parser").
	 */
	virtual std::string getName() const = 0;

	/**
	 * Get the priority for format detection (higher = checked first).
	 * Use this to ensure more specific formats are detected before generic ones.
	 * Standard priorities: 100=very_high, 80=high, 50=medium, 30=low, 10=very_low
	 */
	virtual int getPriority() const = 0;

	/**
	 * Get category for this parser (test_framework, linting_tool, build_system, etc.)
	 */
	virtual std::string getCategory() const = 0;

	// =========================================================================
	// Optional metadata (have default implementations)
	// =========================================================================

	/**
	 * Get a description of this parser's format.
	 * Used in duck_hunt_formats() output.
	 */
	virtual std::string getDescription() const {
		return getName(); // Default to parser name
	}

	/**
	 * Get alternative names for this format (e.g., "pytest" for "pytest_json").
	 * Used for format lookup flexibility.
	 */
	virtual std::vector<std::string> getAliases() const {
		return {};
	}

	/**
	 * Get required extension name (e.g., "webbed" for XML parsers).
	 * Empty string means no external extension required.
	 */
	virtual std::string getRequiredExtension() const {
		return "";
	}

	/**
	 * Get command patterns for format detection.
	 * Used by tools like BIRD and blq to auto-detect format from command string.
	 * Patterns match against the executable name (not full path).
	 *
	 * Pattern types:
	 * - "literal": Exact match (e.g., "pytest" matches "pytest")
	 * - "like": SQL LIKE pattern (e.g., "cargo test%" matches "cargo test --release")
	 * - "regexp": Regular expression (e.g., "cargo\\s+test.*" for complex matching)
	 *
	 * Returns empty vector if no command patterns are defined.
	 */
	virtual std::vector<CommandPattern> getCommandPatterns() const {
		return {};
	}

	/**
	 * Get format groups this parser belongs to (e.g., "python", "rust", "ci").
	 * Groups allow users to specify a language/ecosystem hint instead of exact format.
	 * Returns empty vector if no groups are defined.
	 */
	virtual std::vector<std::string> getGroups() const {
		return {};
	}

	/**
	 * Get the content family for framework-level extraction.
	 * Smart default infers from format name suffix: _json → JSON, _xml → XML.
	 * Override if the naming convention doesn't apply.
	 */
	virtual ContentFamily getContentFamily() const {
		auto name = getFormatName();
		if (StringUtil::EndsWith(name, std::string("_json"))) {
			return ContentFamily::JSON;
		}
		if (StringUtil::EndsWith(name, std::string("_xml"))) {
			return ContentFamily::XML;
		}
		return ContentFamily::TEXT;
	}
};

/**
 * Helper typedef for parser instances
 */
using ParserPtr = unique_ptr<IParser>;

/**
 * Priority constants for consistent ordering.
 * Higher priority parsers are tried first during auto-detection.
 */
namespace ParserPriority {
constexpr int VERY_HIGH = 100; // Very specific formats (e.g., JSON with unique keys)
constexpr int HIGH = 80;       // Specific formats with clear markers
constexpr int MEDIUM = 50;     // Default priority
constexpr int LOW = 30;        // Generic formats that match many inputs
constexpr int VERY_LOW = 10;   // Catch-all parsers
} // namespace ParserPriority

/**
 * Category constants for consistency.
 */
namespace ParserCategory {
constexpr const char *DEBUGGING = "debugging_tool";
constexpr const char *TEST_FRAMEWORK = "test_framework";
constexpr const char *BUILD_SYSTEM = "build_system";
constexpr const char *LINTING = "linting_tool";
constexpr const char *TOOL_OUTPUT = "tool_output";
constexpr const char *CI_SYSTEM = "ci_system";
constexpr const char *APP_LOGGING = "app_logging";
constexpr const char *INFRASTRUCTURE = "infrastructure";
constexpr const char *WEB_ACCESS = "web_access";
constexpr const char *SYSTEM_LOG = "system_log";
constexpr const char *CLOUD_AUDIT = "cloud_audit";
constexpr const char *STRUCTURED_LOG = "structured_log";
constexpr const char *PYTHON_TOOL = "python_tool";
constexpr const char *SECURITY_TOOL = "security_tool";
constexpr const char *COVERAGE = "coverage";
constexpr const char *DISTRIBUTED_SYSTEMS = "distributed_systems";
} // namespace ParserCategory

} // namespace duckdb
