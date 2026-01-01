#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for TFLint JSON output (Terraform linter).
 * Handles format:
 * {"issues":[{"rule":{"name":"...","severity":"warning"},"message":"...","range":{"filename":"...","start":{"line":1,"column":1}}}]}
 */
class TflintJSONParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "tflint_json";
	}
	std::string getName() const override {
		return "tflint";
	}
	int getPriority() const override {
		return 120;
	}
	std::string getCategory() const override {
		return "linter_json";
	}

private:
	bool isValidTflintJSON(const std::string &content) const;
};

} // namespace duckdb
