#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

// Parser for Hadoop HDFS logs
// Format: YYMMDD HHMMSS pid LEVEL component: message
// Example: 081109 203615 148 INFO dfs.DataNode$PacketResponder: PacketResponder 1 for block blk_38865049064139660
// terminating
class HdfsParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "hdfs";
	}
	std::string getName() const override {
		return "hdfs";
	}
	int getPriority() const override {
		return 60;
	} // Higher than generic log4j
	std::string getCategory() const override {
		return "distributed_systems";
	}
};

} // namespace duckdb
