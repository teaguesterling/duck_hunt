#pragma once

#include "validation_event_types.hpp"
#include <string>
#include <vector>

namespace duckdb {

class RegexpParser {
public:
    static void ParseWithRegexp(const std::string& content, const std::string& pattern,
                                std::vector<ValidationEvent>& events);

    std::string GetName() const { return "regexp"; }
    // Note: CanParse is not applicable for regexp - it always needs user-provided pattern
    void Parse(const std::string& content, const std::string& pattern,
               std::vector<ValidationEvent>& events) const;
};

} // namespace duckdb
