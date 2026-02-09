#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Jenkins build log output.
 * Detects build steps, pipeline stages, and error patterns.
 */
class JenkinsTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "jenkins_text";
	}
	std::string getName() const override {
		return "jenkins";
	}
	std::string getDescription() const override {
		return "Jenkins build log output";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "ci_system";
	}
};

} // namespace duckdb
