#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>

namespace duckdb {

/**
 * Parser for kube-score JSON output.
 * Handles Kubernetes resource analysis results with grades, comments, and metadata.
 */
class KubeScoreJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::KUBE_SCORE_JSON; }
    std::string getName() const override { return "kube-score"; }
    int getPriority() const override { return 70; }
    std::string getCategory() const override { return "infrastructure_analysis"; }

private:
    bool isValidKubeScoreJSON(const std::string& content) const;
    ValidationEventStatus mapGradeToStatus(const std::string& grade) const;
};

} // namespace duckdb