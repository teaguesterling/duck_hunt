#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class LogrusParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "logrus";
	}
	std::string getName() const override {
		return "logrus";
	}
	int getPriority() const override {
		return 60;
	} // Higher than generic logfmt (55) - more specific format
	std::string getCategory() const override {
		return "app_logging";
	}
};

} // namespace duckdb
