#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for NGINX access logs.
 *
 * Default NGINX combined log format (similar to Apache):
 *   192.168.1.10 - - [12/Dec/2025:12:34:56 +0000] "POST /api/v1/login HTTP/2.0" 401 512 "-" "curl/7.68.0"
 *
 * Extended format with request time:
 *   192.168.1.10 - - [12/Dec/2025:12:34:56 +0000] "GET /api HTTP/1.1" 200 1024 "-" "Mozilla/5.0" 0.042
 *
 * Extracted fields:
 * - IP address -> file_path
 * - timestamp -> function_name
 * - request method -> category
 * - request path -> message
 * - status code -> severity (4xx=warning, 5xx=error, else info)
 * - response size -> line_number (as int)
 * - user agent -> suggestion
 * - request time -> execution_time (if present)
 */
class NginxAccessParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "nginx_access"; }
    std::string getName() const override { return "nginx_access"; }
    int getPriority() const override { return 54; }
    std::string getCategory() const override { return "web_access"; }
};

} // namespace duckdb
