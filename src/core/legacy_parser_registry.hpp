#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "duckdb/common/unique_ptr.hpp"
#include "include/validation_event_types.hpp"
#include "include/read_duck_hunt_log_function.hpp"
#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Central registry for all test result parsers.
 * Manages parser lifecycle and provides efficient format detection.
 */
class ParserRegistry {
public:
    /**
     * Register a new parser with the registry.
     * Parsers are automatically sorted by priority.
     */
    void registerParser(ParserPtr parser);
    
    /**
     * Find the best parser for the given content.
     * Returns nullptr if no parser can handle the content.
     */
    IParser* findParser(const std::string& content) const;
    
    /**
     * Get parser for a specific format (if registered).
     */
    IParser* getParser(TestResultFormat format) const;
    
    /**
     * Get all registered parsers (sorted by priority).
     */
    const std::vector<IParser*>& getAllParsers() const;
    
    /**
     * Get parsers by category.
     */
    std::vector<IParser*> getParsersByCategory(const std::string& category) const;
    
    /**
     * Get registry statistics.
     */
    struct Stats {
        size_t total_parsers = 0;
        size_t categories = 0;
        std::unordered_map<std::string, size_t> parsers_by_category;
    };
    Stats getStats() const;
    
    /**
     * Clear all registered parsers (mainly for testing).
     */
    void clear();
    
    /**
     * Get singleton instance.
     */
    static ParserRegistry& getInstance();
    
private:
    std::vector<ParserPtr> parsers_;
    mutable std::vector<IParser*> sorted_parsers_;  // Cached sorted view
    std::unordered_map<TestResultFormat, IParser*> format_map_;
    mutable bool needs_resort_ = false;
    
    void ensureSorted() const;
    ParserRegistry() = default;
};

/**
 * Helper class for automatic parser registration.
 * Use REGISTER_PARSER macro for convenience.
 */
template<typename ParserType>
class ParserRegistrar {
public:
    ParserRegistrar() {
        ParserRegistry::getInstance().registerParser(make_uniq<ParserType>());
    }
};

/**
 * Macro to automatically register a parser class.
 * Place in parser .cpp files to auto-register on startup.
 */
#define REGISTER_PARSER(ParserClass) \
    static ParserRegistrar<ParserClass> g_##ParserClass##_registrar

} // namespace duckdb