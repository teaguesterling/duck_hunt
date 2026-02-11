#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Apache HTTP Server error logs.
 *
 * Supports Apache 2.2 and earlier format:
 *   [Sun Dec 04 04:47:44 2005] [notice] workerEnv.init() ok /etc/httpd/conf/workers2.properties
 *   [Sun Dec 04 04:47:44 2005] [error] mod_jk child workerEnv in error state 6
 *
 * And Apache 2.4+ format:
 *   [Sun Dec 04 04:47:44.123456 2005] [core:error] [pid 12345] [client 192.168.1.1:12345] message
 *
 * Log levels: emerg, alert, crit, error, warn, notice, info, debug, trace1-8
 *
 * Extracted fields:
 * - timestamp -> started_at
 * - log level -> severity (error/warning/info)
 * - module (if present) -> category
 * - client IP (if present) -> origin
 * - message -> message
 */
class ApacheErrorParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "apache_error";
	}
	std::string getName() const override {
		return "apache_error";
	}
	int getPriority() const override {
		return 54; // Slightly higher than access logs
	}
	std::string getCategory() const override {
		return "web_access";
	}
	std::string getDescription() const override {
		return "Apache HTTP Server error log";
	}
};

} // namespace duckdb
