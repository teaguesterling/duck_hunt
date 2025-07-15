#pragma once

#include "validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class JUnitTextParser : public ParserInterface {
public:
    static void ParseJUnitText(const std::string& content, std::vector<ValidationEvent>& events);
    
    // ParserInterface implementation
    std::string GetName() const override { return "junit_text"; }
    bool CanParse(const std::string& content) const override;
    void Parse(const std::string& content, std::vector<ValidationEvent>& events) const override;
};

} // namespace duck_hunt