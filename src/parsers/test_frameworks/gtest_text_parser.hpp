#pragma once

#include "../base/parser_interface.hpp"
#include "../../validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class GTestTextParser : public ParserInterface {
public:
    static void ParseGoogleTest(const std::string& content, std::vector<ValidationEvent>& events);
    
    // ParserInterface implementation
    std::string GetName() const override { return "gtest_text"; }
    bool CanParse(const std::string& content) const override;
    void Parse(const std::string& content, std::vector<ValidationEvent>& events) const override;
};

} // namespace duck_hunt