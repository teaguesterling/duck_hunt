#pragma once

#include <string>
#include <vector>
#include <memory>
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/main/client_context.hpp"
#include "include/validation_event_types.hpp"
#include "include/read_duck_hunt_log_function.hpp"

namespace duckdb {

/**
 * Base interface for all test result parsers.
 * Each parser implements format detection and parsing logic for a specific tool/format.
 */
class IParser {
public:
    virtual ~IParser() = default;

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

    /**
     * Get the format this parser handles.
     */
    virtual TestResultFormat getFormat() const = 0;

    /**
     * Get a human-readable name for this parser.
     */
    virtual std::string getName() const = 0;

    /**
     * Get the priority for format detection (higher = checked first).
     * Use this to ensure more specific formats are detected before generic ones.
     */
    virtual int getPriority() const = 0;

    /**
     * Get category for this parser (test_framework, linting_tool, build_system, etc.)
     */
    virtual std::string getCategory() const = 0;
};

/**
 * Helper typedef for parser instances
 */
using ParserPtr = unique_ptr<IParser>;

} // namespace duckdb