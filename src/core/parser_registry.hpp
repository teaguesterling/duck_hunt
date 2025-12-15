#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/main/client_context.hpp"
#include "include/validation_event_types.hpp"

namespace duckdb {

// Nested namespace for new parser system during migration.
// Once migration is complete, these will move to duckdb namespace
// and replace the old IParser/ParserRegistry.
namespace log_parsers {

/**
 * Parser interface - string-based format names, no enum dependency.
 * All parsers should implement this interface.
 */
class IParser {
public:
    virtual ~IParser() = default;

    // Core parsing
    virtual bool canParse(const std::string& content) const = 0;
    virtual std::vector<ValidationEvent> parse(const std::string& content) const = 0;

    // Context-aware parsing (for XML parsers needing webbed, etc.)
    virtual std::vector<ValidationEvent> parseWithContext(ClientContext& context,
                                                          const std::string& content) const {
        return parse(content);
    }
    virtual bool requiresContext() const { return false; }

    // Metadata - all string-based
    virtual std::string getFormatName() const = 0;  // e.g., "pylint_text", "strace"
    virtual std::string getName() const = 0;         // Human-readable: "Pylint Parser"
    virtual std::string getCategory() const = 0;     // e.g., "linting_tool", "debugging_tool"
    virtual std::string getDescription() const = 0;  // e.g., "Pylint Python code quality output"
    virtual int getPriority() const = 0;             // Higher = checked first in auto-detect

    // Optional: aliases for format name (e.g., "pylint" -> "pylint_text")
    virtual std::vector<std::string> getAliases() const { return {}; }

    // Optional: required extensions (e.g., "webbed" for XML parsers)
    virtual std::string getRequiredExtension() const { return ""; }
};

using ParserPtr = unique_ptr<IParser>;

/**
 * Parser metadata for the formats table function.
 */
struct ParserInfo {
    std::string format_name;
    std::string description;
    std::string category;
    std::string required_extension;
    int priority;
};

/**
 * Central parser registry - string-based lookup, category organization.
 * Manages parser lifecycle and provides format detection.
 */
class ParserRegistry {
public:
    /**
     * Register a parser. The format name and aliases become lookup keys.
     */
    void registerParser(ParserPtr parser);

    /**
     * Find parser by format name (or alias).
     */
    IParser* getParser(const std::string& format_name) const;

    /**
     * Auto-detect: find the best parser for content.
     */
    IParser* findParser(const std::string& content) const;

    /**
     * Get all parsers in a category (sorted by priority).
     */
    std::vector<IParser*> getParsersByCategory(const std::string& category) const;

    /**
     * Get all registered format names (for formats table function).
     */
    std::vector<ParserInfo> getAllFormats() const;

    /**
     * Get all unique categories.
     */
    std::vector<std::string> getCategories() const;

    /**
     * Check if a format is registered.
     */
    bool hasFormat(const std::string& format_name) const;

    /**
     * Get singleton instance.
     */
    static ParserRegistry& getInstance();

    /**
     * Clear registry (for testing).
     */
    void clear();

private:
    ParserRegistry() = default;

    std::vector<ParserPtr> parsers_;
    std::unordered_map<std::string, IParser*> format_map_;  // format_name -> parser
    mutable std::vector<IParser*> sorted_parsers_;
    mutable bool needs_resort_ = false;

    void ensureSorted() const;
};

/**
 * Category registration helper.
 * Each parser category (debugging, linting, etc.) provides a registration function.
 */
using CategoryRegistrationFn = std::function<void(ParserRegistry&)>;

/**
 * Register a category's parsers.
 * Call this during extension initialization for each category.
 */
void RegisterParserCategory(const std::string& category_name, CategoryRegistrationFn register_fn);

/**
 * Initialize all registered categories.
 * Called once during extension load.
 */
void InitializeAllParsers();

// Convenience macros for parser registration

/**
 * Declare a parser registration function for a category.
 * Use in header: DECLARE_PARSER_CATEGORY(Debugging);
 */
#define DECLARE_PARSER_CATEGORY(category) \
    void Register##category##Parsers(log_parsers::ParserRegistry& registry)

/**
 * Register a category during static initialization.
 * Use in .cpp: REGISTER_PARSER_CATEGORY(Debugging);
 */
#define REGISTER_PARSER_CATEGORY(category) \
    static struct category##_category_registrar { \
        category##_category_registrar() { \
            log_parsers::RegisterParserCategory(#category, Register##category##Parsers); \
        } \
    } g_##category##_registrar

} // namespace log_parsers
} // namespace duckdb
