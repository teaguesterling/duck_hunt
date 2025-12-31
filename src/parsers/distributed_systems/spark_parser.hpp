#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

// Parser for Apache Spark logs
// Format: YY/MM/DD HH:MM:SS LEVEL component: message
// Example: 17/06/09 20:10:40 INFO executor.CoarseGrainedExecutorBackend: Registered signal handlers for [TERM, HUP, INT]
class SparkParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "spark"; }
    std::string getName() const override { return "spark"; }
    int getPriority() const override { return 60; }  // Higher than generic log4j
    std::string getCategory() const override { return "distributed_systems"; }
};

} // namespace duckdb
