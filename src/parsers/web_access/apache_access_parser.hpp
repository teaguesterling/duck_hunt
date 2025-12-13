#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Apache HTTP Server access logs.
 *
 * Supports Common Log Format and Combined Log Format.
 *
 * Common Log Format:
 *   127.0.0.1 - - [12/Dec/2025:10:15:42 +0000] "GET /index.html HTTP/1.1" 200 1024
 *
 * Combined Log Format (adds referrer and user-agent):
 *   127.0.0.1 - - [12/Dec/2025:10:15:42 +0000] "GET /index.html HTTP/1.1" 200 1024 "http://referrer.com" "Mozilla/5.0"
 *
 * Extracted fields:
 * - IP address -> file_path
 * - timestamp -> function_name
 * - request method -> category
 * - request path -> message
 * - status code -> severity (4xx=warning, 5xx=error, else info)
 * - response size -> line_number (as int)
 * - user agent -> suggestion
 */
class ApacheAccessParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::APACHE_ACCESS; }
    std::string getName() const override { return "apache_access"; }
    int getPriority() const override { return 53; }
    std::string getCategory() const override { return "web_access"; }
};

} // namespace duckdb
