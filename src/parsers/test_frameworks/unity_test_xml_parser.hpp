#pragma once

#include "parsers/base/xml_parser_base.hpp"
#include "include/validation_event_types.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Parser for Unity Test Framework XML results (NUnit 3 format).
 *
 * Unity Test Framework outputs test results in NUnit 3 XML format, which is used by:
 * - Unity Test Runner (Edit Mode and Play Mode tests)
 * - Unity's command-line test execution (-runTests)
 * - Unity Cloud Build and CI/CD integrations
 *
 * Structure:
 * <test-run testcasecount="N" result="Passed|Failed" total="N" passed="N" failed="N" ...>
 *   <test-suite type="TestSuite|Assembly|TestFixture" name="..." result="Passed|Failed">
 *     <test-case name="test_name" fullname="Namespace.Class.Method" result="Passed|Failed|Skipped">
 *       <failure>
 *         <message><![CDATA[error message]]></message>
 *         <stack-trace><![CDATA[stack trace]]></stack-trace>
 *       </failure>
 *       <reason>
 *         <message><![CDATA[skip reason]]></message>
 *       </reason>
 *       <output><![CDATA[test output]]></output>
 *     </test-case>
 *   </test-suite>
 * </test-run>
 */
class UnityTestXmlParser : public XmlParserBase {
public:
	bool canParse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "unity_test_xml";
	}
	std::string getName() const override {
		return "Unity Test XML Parser";
	}
	int getPriority() const override {
		return 86;
	} // Slightly higher than JUnit XML to prefer when both match
	std::string getCategory() const override {
		return "test_framework";
	}

	/**
	 * Parse XML content using webbed's parse_xml function.
	 * Framework extracts clean XML via MaybeExtractContent before calling this.
	 */
	std::vector<ValidationEvent> parseWithContext(ClientContext &context, const std::string &content) const override;

protected:
	std::vector<ValidationEvent> parseJsonContent(const std::string &json_content) const override {
		return {}; // Not used — parse_xml handles XML directly
	}
};

} // namespace duckdb
