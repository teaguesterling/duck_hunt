#pragma once

#include "parsers/base/parser_interface.hpp"
#include <vector>
#include <string>

namespace duckdb {

class DroneCITextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "drone_ci_text";
	}
	std::string getName() const override {
		return "drone-ci";
	}
	int getPriority() const override {
		return 75;
	}
	std::string getCategory() const override {
		return "ci_systems";
	}

private:
	bool isValidDroneCIText(const std::string &content) const;
};

} // namespace duckdb
