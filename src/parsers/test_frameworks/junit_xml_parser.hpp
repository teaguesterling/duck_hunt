#pragma once

#include "parsers/base/xml_parser_base.hpp"
#include "include/validation_event_types.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Parser for JUnit XML test result format.
 *
 * JUnit XML is a widely-adopted standard format for test results, used by:
 * - Java: Maven Surefire, Gradle, Ant
 * - Python: pytest --junitxml
 * - JavaScript: jest-junit, mocha-junit-reporter
 * - Go: go-junit-report
 * - Ruby: rspec_junit_formatter
 * - Many CI systems natively consume this format
 *
 * Structure:
 * <testsuites>
 *   <testsuite name="..." tests="N" failures="N" errors="N" skipped="N" time="N.N">
 *     <testcase name="test_name" classname="TestClass" time="N.N">
 *       <failure message="...">stack trace</failure>
 *       <error message="..." type="ExceptionType">stack trace</error>
 *       <skipped message="..."/>
 *     </testcase>
 *   </testsuite>
 * </testsuites>
 */
class JUnitXmlParser : public XmlParserBase {
public:
    bool canParse(const std::string& content) const override;
    std::string getFormatName() const override { return "junit_xml"; }
    std::string getName() const override { return "JUnit XML Parser"; }
    int getPriority() const override { return 85; }  // High priority for XML detection
    std::string getCategory() const override { return "test_framework"; }

protected:
    std::vector<ValidationEvent> parseJsonContent(const std::string& json_content) const override;
};

} // namespace duckdb
