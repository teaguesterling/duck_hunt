#pragma once

#include "parsers/base/parser_interface.hpp"
#include "include/validation_event_types.hpp"

namespace duckdb {

class YapfTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "yapf_text";
	}
	std::string getName() const override {
		return "yapf";
	}
	int getPriority() const override {
		return 95;
	}
	std::string getCategory() const override {
		return "linting_tools";
	}
};

} // namespace duckdb
