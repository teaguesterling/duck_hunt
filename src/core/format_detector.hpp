#pragma once

#include <string>
#include "include/validation_event_types.hpp"
#include "include/read_duck_hunt_log_function.hpp"

namespace duckdb {

class ParserRegistry;

/**
 * Optimized format detection engine.
 * Uses the parser registry to efficiently detect test result formats.
 */
class FormatDetector {
public:
    explicit FormatDetector(const ParserRegistry& registry);
    
    /**
     * Detect the format of the given content.
     * Returns UNKNOWN if no parser can handle it.
     */
    TestResultFormat detectFormat(const std::string& content) const;
    
    /**
     * Find the best parser for the given content.
     * Returns nullptr if no parser can handle it.
     */
    class IParser* findBestParser(const std::string& content) const;
    
    /**
     * Check if the content can be parsed by any registered parser.
     */
    bool canParseContent(const std::string& content) const;
    
private:
    const ParserRegistry& registry_;
};

} // namespace duckdb