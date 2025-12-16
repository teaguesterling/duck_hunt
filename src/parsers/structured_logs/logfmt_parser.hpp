#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for logfmt format.
 *
 * Logfmt is a structured logging format using key=value pairs.
 * Popular in Go ecosystem (logrus, zap), Heroku, and many other tools.
 *
 * Example:
 *   level=info ts=2025-01-15T10:30:45Z msg="request completed" method=GET path=/api/users status=200
 *   level=error ts=2025-01-15T10:30:46Z msg="database error" err="connection timeout"
 *
 * Features:
 * - Unquoted values: key=value
 * - Quoted values: key="value with spaces"
 * - Empty values: key=
 */
class LogfmtParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "logfmt"; }
    std::string getName() const override { return "logfmt"; }
    int getPriority() const override { return 55; }  // Slightly higher than JSONL - more specific format
    std::string getCategory() const override { return "structured_log"; }
};

} // namespace duckdb
