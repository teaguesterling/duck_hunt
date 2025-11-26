#pragma once

#include "../base/parser_interface.hpp"
#include <vector>
#include <string>

namespace duckdb {

class DroneCITextParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::DRONE_CI_TEXT; }
    std::string getName() const override { return "drone-ci"; }
    int getPriority() const override { return 75; }
    std::string getCategory() const override { return "ci_systems"; }

private:
    bool isValidDroneCIText(const std::string& content) const;
};

} // namespace duckdb