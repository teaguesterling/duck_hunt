#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for MSBuild/Visual Studio build output.
 * Handles C# compilation errors, test results, code analysis warnings.
 */
class MSBuildParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "msbuild";
	}
	std::string getName() const override {
		return "MSBuild Parser";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "build_system";
	}
	std::string getDescription() const override {
		return "Microsoft MSBuild/Visual Studio output";
	}
	std::vector<std::string> getAliases() const override {
		return {"visualstudio", "vs"};
	}
};

} // namespace duckdb
