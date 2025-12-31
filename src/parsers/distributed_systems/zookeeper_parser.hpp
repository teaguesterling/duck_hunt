#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

// Parser for Apache Zookeeper logs
// Format: YYYY-MM-DD HH:MM:SS,mmm - LEVEL  [thread/context] - message
// Example: 2015-07-29 17:41:44,747 - INFO  [QuorumPeer[myid=1]/0:0:0:0:0:0:0:0:2181:FastLeaderElection@774] - Notification time out: 3200
class ZookeeperParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "zookeeper"; }
    std::string getName() const override { return "zookeeper"; }
    int getPriority() const override { return 62; }  // Higher than generic log4j
    std::string getCategory() const override { return "distributed_systems"; }
};

} // namespace duckdb
