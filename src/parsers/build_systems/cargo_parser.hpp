#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Rust Cargo build output.
 * Handles rustc errors, warnings, clippy lints, cargo test results, and rustfmt diffs.
 */
class CargoParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "cargo_build"; }
    std::string getName() const override { return "Cargo Build Parser"; }
    int getPriority() const override { return 80; }
    std::string getCategory() const override { return "build_system"; }
    std::string getDescription() const override { return "Rust Cargo build output"; }
    std::vector<std::string> getAliases() const override { return {"cargo", "rust"}; }
};

} // namespace duckdb
