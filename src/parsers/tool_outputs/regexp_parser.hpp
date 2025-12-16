#pragma once

#include "validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class RegexpParser {
public:
    static void ParseWithRegexp(const std::string& content, const std::string& pattern,
                                std::vector<duckdb::ValidationEvent>& events);

    std::string GetName() const { return "regexp"; }
    // Note: CanParse is not applicable for regexp - it always needs user-provided pattern
    void Parse(const std::string& content, const std::string& pattern,
               std::vector<duckdb::ValidationEvent>& events) const;
};

} // namespace duck_hunt
