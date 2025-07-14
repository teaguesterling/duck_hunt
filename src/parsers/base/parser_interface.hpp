#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../../include/validation_event_types.hpp"

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
using ParserPtr = std::unique_ptr<IParser>;

} // namespace duckdb