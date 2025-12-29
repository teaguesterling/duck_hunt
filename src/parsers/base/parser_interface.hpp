#pragma once

#include <string>
#include <vector>
#include <memory>
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/main/client_context.hpp"
#include "include/validation_event_types.hpp"

namespace duckdb {

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
    virtual bool canParse(const std::string& content) const = 0;

    /**
     * Parse the content and return validation events.
     * Only called if canParse() returns true.
     */
    virtual std::vector<ValidationEvent> parse(const std::string& content) const = 0;

    // =========================================================================
    // Context-aware parsing (optional, has default implementation)
    // =========================================================================

    /**
     * Parse the content with access to ClientContext.
     * Used by parsers that need to call external functions (e.g., webbed for XML).
     * Default implementation calls parse(content).
     */
    virtual std::vector<ValidationEvent> parseWithContext(ClientContext &context,
                                                           const std::string& content) const {
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
        return getName();  // Default to parser name
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
    constexpr int VERY_HIGH = 100;  // Very specific formats (e.g., JSON with unique keys)
    constexpr int HIGH = 80;        // Specific formats with clear markers
    constexpr int MEDIUM = 50;      // Default priority
    constexpr int LOW = 30;         // Generic formats that match many inputs
    constexpr int VERY_LOW = 10;    // Catch-all parsers
}

/**
 * Category constants for consistency.
 */
namespace ParserCategory {
    constexpr const char* DEBUGGING = "debugging_tool";
    constexpr const char* TEST_FRAMEWORK = "test_framework";
    constexpr const char* BUILD_SYSTEM = "build_system";
    constexpr const char* LINTING = "linting_tool";
    constexpr const char* TOOL_OUTPUT = "tool_output";
    constexpr const char* CI_SYSTEM = "ci_system";
    constexpr const char* APP_LOGGING = "app_logging";
    constexpr const char* INFRASTRUCTURE = "infrastructure";
    constexpr const char* WEB_ACCESS = "web_access";
    constexpr const char* SYSTEM_LOG = "system_log";
    constexpr const char* CLOUD_AUDIT = "cloud_audit";
    constexpr const char* STRUCTURED_LOG = "structured_log";
    constexpr const char* PYTHON_TOOL = "python_tool";
    constexpr const char* SECURITY_TOOL = "security_tool";
    constexpr const char* COVERAGE = "coverage";
}

} // namespace duckdb