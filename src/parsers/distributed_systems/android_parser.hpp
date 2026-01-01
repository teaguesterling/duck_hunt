#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

// Parser for Android logcat logs
// Format: MM-DD HH:MM:SS.mmm PID TID LEVEL Tag: message
// Example: 03-17 16:13:38.811  1702  2395 D WindowManager: printFreezingDisplayLogs...
class AndroidParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "android";
	}
	std::string getName() const override {
		return "android";
	}
	int getPriority() const override {
		return 65;
	} // Higher priority - specific format
	std::string getCategory() const override {
		return "distributed_systems";
	}
};

} // namespace duckdb
