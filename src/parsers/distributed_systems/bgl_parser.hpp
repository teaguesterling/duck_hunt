#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

// Parser for Blue Gene/L (BGL) supercomputer logs
// Format: ALERT_TYPE UNIX_TS DATE NODE TIMESTAMP NODE SOURCE COMPONENT LEVEL message
// Example: - 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 R02-M1-N0-C:J12-U11 RAS KERNEL INFO
// instruction cache parity error corrected
class BglParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "bgl";
	}
	std::string getName() const override {
		return "bgl";
	}
	int getPriority() const override {
		return 65;
	} // Specific format
	std::string getCategory() const override {
		return "distributed_systems";
	}
};

} // namespace duckdb
