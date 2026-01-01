#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for syslog format.
 *
 * Supports both BSD syslog (RFC 3164) and IETF syslog (RFC 5424) formats.
 *
 * BSD format example:
 *   Dec 12 10:15:42 localhost sshd[1234]: Accepted password for user from 10.0.0.1 port 22
 *
 * RFC 5424 format example:
 *   <165>1 2025-12-12T10:15:42.012345Z hostname app proc-id msgid - message
 *
 * Extracted fields:
 * - timestamp -> function_name
 * - hostname -> file_path
 * - process/app name -> category
 * - PID -> line_number (if available)
 * - message -> message
 * - severity from priority -> severity
 */
class SyslogParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "syslog";
	}
	std::string getName() const override {
		return "syslog";
	}
	int getPriority() const override {
		return 52;
	}
	std::string getCategory() const override {
		return "system_log";
	}
};

} // namespace duckdb
