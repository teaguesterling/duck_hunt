#pragma once

#include "validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class MochaChaiTextParser : public ParserInterface {
public:
    static void ParseMochaChai(const std::string& content, std::vector<ValidationEvent>& events);
    
    // ParserInterface implementation
    std::string GetName() const override { return "mocha_chai_text"; }
    bool CanParse(const std::string& content) const override;
    void Parse(const std::string& content, std::vector<ValidationEvent>& events) const override;
};

} // namespace duck_hunt