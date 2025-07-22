#pragma once

#include "../base/parser_interface.hpp"
#include "../../include/validation_event_types.hpp"
#include <vector>
#include <string>

namespace duckdb {

class ClangTidyParser : public IParser {
public:
    ClangTidyParser() = default;
    ~ClangTidyParser() = default;

    std::string getName() const override { return "Clang-Tidy Parser"; }
    std::string getCategory() const override { return "linting_tool"; }
    TestResultFormat getFormat() const override { return TestResultFormat::CLANG_TIDY_TEXT; }
    int getPriority() const override { return 90; } // Higher priority than MyPy to be checked first
    
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

private:
    bool isValidClangTidyOutput(const std::string& content) const;
};

} // namespace duckdb