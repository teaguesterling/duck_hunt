#pragma once

#include "../../core/new_parser_registry.hpp"
#include <string>
#include <vector>

namespace duckdb {
namespace log_parsers {

/**
 * Base class for parsers that provides common functionality.
 * Inherit from this to create a new parser with minimal boilerplate.
 *
 * Example:
 *   class MyParser : public BaseParser {
 *   public:
 *       MyParser() : BaseParser("my_format", "My Parser", "my_category",
 *                               "Description of my format") {}
 *
 *       bool canParse(const std::string& content) const override {
 *           return content.find("MY_MARKER") != std::string::npos;
 *       }
 *
 *       std::vector<ValidationEvent> parse(const std::string& content) const override {
 *           // parsing logic
 *       }
 *   };
 */
class BaseParser : public IParser {
public:
    BaseParser(std::string format_name,
               std::string name,
               std::string category,
               std::string description,
               int priority = 50)
        : format_name_(std::move(format_name))
        , name_(std::move(name))
        , category_(std::move(category))
        , description_(std::move(description))
        , priority_(priority) {}

    virtual ~BaseParser() = default;

    // Metadata accessors
    std::string getFormatName() const override { return format_name_; }
    std::string getName() const override { return name_; }
    std::string getCategory() const override { return category_; }
    std::string getDescription() const override { return description_; }
    int getPriority() const override { return priority_; }

    // Override these if needed
    std::vector<std::string> getAliases() const override { return aliases_; }
    std::string getRequiredExtension() const override { return required_extension_; }

protected:
    // Subclasses can set these in constructor
    void addAlias(const std::string& alias) { aliases_.push_back(alias); }
    void setRequiredExtension(const std::string& ext) { required_extension_ = ext; }

    // Helper to create a basic event with common fields pre-filled
    ValidationEvent createEvent() const {
        ValidationEvent event;
        event.tool_name = format_name_;
        event.category = category_;
        return event;
    }

private:
    std::string format_name_;
    std::string name_;
    std::string category_;
    std::string description_;
    int priority_;
    std::vector<std::string> aliases_;
    std::string required_extension_;
};

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
    constexpr const char* CI_SYSTEM = "ci_system";
    constexpr const char* APP_LOGGING = "app_logging";
    constexpr const char* INFRASTRUCTURE = "infrastructure";
    constexpr const char* WEB_ACCESS = "web_access";
    constexpr const char* CLOUD_AUDIT = "cloud_audit";
    constexpr const char* STRUCTURED_LOG = "structured_log";
}

} // namespace log_parsers
} // namespace duckdb
