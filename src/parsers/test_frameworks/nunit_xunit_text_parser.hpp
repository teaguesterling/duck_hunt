#pragma once

#include "../base/parser_interface.hpp"
#include "../../validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class NUnitXUnitTextParser : public ParserInterface {
public:
    static void ParseNUnitXUnit(const std::string& content, std::vector<ValidationEvent>& events);
    
    // ParserInterface implementation
    std::string GetName() const override { return "nunit_xunit_text"; }
    bool CanParse(const std::string& content) const override;
    void Parse(const std::string& content, std::vector<ValidationEvent>& events) const override;
};

} // namespace duck_hunt