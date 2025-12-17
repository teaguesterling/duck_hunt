#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for JSON Lines (JSONL/NDJSON) format.
 *
 * JSON Lines is a text format where each line is a valid JSON object.
 * Common in structured logging across many languages and platforms.
 *
 * Example:
 *   {"timestamp":"2025-01-15T10:30:45Z","level":"info","message":"Request received"}
 *   {"timestamp":"2025-01-15T10:30:46Z","level":"error","message":"Request failed"}
 *
 * Common field names detected:
 * - Level/severity: level, severity, lvl, loglevel, log_level
 * - Message: message, msg, text, content
 * - Timestamp: timestamp, ts, time, @timestamp, datetime
 * - Error: error, err, exception
 */
class JSONLParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "jsonl"; }
    std::string getName() const override { return "jsonl"; }
    int getPriority() const override { return 50; }  // Lower priority - very generic format
    std::string getCategory() const override { return "structured_log"; }
};

} // namespace duckdb
