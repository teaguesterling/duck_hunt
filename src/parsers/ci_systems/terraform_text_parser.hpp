#pragma once

#include "parsers/base/parser_interface.hpp"
#include <vector>
#include <string>

namespace duckdb {

class TerraformTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "terraform_text";
	}
	std::string getName() const override {
		return "terraform";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "infrastructure_tools";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("terraform plan%"),
		    CommandPattern::Like("terraform apply%"),
		    CommandPattern::Like("terraform validate%"),
		    CommandPattern::Regexp("terraform\\s+(plan|apply|validate|destroy)"),
		};
	}

private:
	bool isValidTerraformText(const std::string &content) const;
};

} // namespace duckdb
