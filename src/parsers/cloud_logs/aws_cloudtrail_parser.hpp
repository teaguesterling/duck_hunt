#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class AWSCloudTrailParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "aws_cloudtrail";
	}
	std::string getName() const override {
		return "aws_cloudtrail";
	}
	int getPriority() const override {
		return 53;
	} // Medium priority for cloud logs
	std::string getCategory() const override {
		return "cloud_audit";
	}
};

} // namespace duckdb
