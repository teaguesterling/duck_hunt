#pragma once

#include "validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class PythonParser {
public:
    static void ParsePythonBuild(const std::string& content, std::vector<duckdb::ValidationEvent>& events);
    
    std::string GetName() const { return "python"; }
    bool CanParse(const std::string& content) const;
    void Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const;
};

} // namespace duck_hunt