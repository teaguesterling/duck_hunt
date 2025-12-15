#pragma once

#include "validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class DuckDBTestParser {
public:
    static void ParseDuckDBTestOutput(const std::string& content, std::vector<duckdb::ValidationEvent>& events);

    std::string GetName() const { return "duckdb_test"; }
    bool CanParse(const std::string& content) const;
    void Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const;
};

} // namespace duck_hunt
