#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "junit_text_parser.hpp"
#include "gtest_text_parser.hpp"
#include "rspec_text_parser.hpp"
#include "mocha_chai_text_parser.hpp"
#include "nunit_xunit_text_parser.hpp"
#include "pytest_parser.hpp"
#include "pytest_json_parser.hpp"
#include "junit_xml_parser.hpp"
#include "duckdb_test_parser.hpp"
#include "pytest_cov_text_parser.hpp"

namespace duckdb {

/**
 * JUnit XML parser wrapper (requires webbed extension).
 * Special handling because it requires ClientContext for XML parsing.
 */
class JUnitXmlParserImpl : public BaseParser {
public:
	JUnitXmlParserImpl()
	    : BaseParser("junit_xml", "JUnit XML Parser", ParserCategory::TEST_FRAMEWORK, "JUnit XML test results format",
	                 ParserPriority::VERY_HIGH) {
		setRequiredExtension("webbed");
		addGroup("java");
	}

	bool canParse(const std::string &content) const override {
		return content.find("<testsuite") != std::string::npos || content.find("<testsuites") != std::string::npos;
	}

	bool requiresContext() const override {
		return true;
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		// XML parsing requires context - this shouldn't be called directly
		return {};
	}

	std::vector<ValidationEvent> parseWithContext(ClientContext &context, const std::string &content) const override {
		JUnitXmlParser parser;
		return parser.parseWithContext(context, content);
	}
};

/**
 * Register all test framework parsers with the registry.
 * Uses DelegatingParser for IParser-compliant parsers.
 */
DECLARE_PARSER_CATEGORY(TestFrameworks);

void RegisterTestFrameworksParsers(ParserRegistry &registry) {
	// IParser-compliant parsers use DelegatingParser template
	// Format: format_name, display_name, category, description, priority, aliases, groups
	registry.registerParser(make_uniq<DelegatingParser<PytestParser>>(
	    "pytest_text", "Pytest Text Parser", ParserCategory::TEST_FRAMEWORK, "Python pytest text output",
	    ParserPriority::HIGH, std::vector<std::string> {"pytest"}, std::vector<std::string> {"python"}));

	registry.registerParser(make_uniq<DelegatingParser<PytestJSONParser>>(
	    "pytest_json", "Pytest JSON Parser", ParserCategory::TEST_FRAMEWORK, "Python pytest JSON report output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {}, std::vector<std::string> {"python"}));

	registry.registerParser(make_uniq<DelegatingParser<JUnitTextParser>>(
	    "junit_text", "JUnit Text Parser", ParserCategory::TEST_FRAMEWORK, "JUnit/Maven test output in text format",
	    ParserPriority::HIGH, std::vector<std::string> {"junit"}, std::vector<std::string> {"java"}));

	registry.registerParser(make_uniq<DelegatingParser<GTestTextParser>>(
	    "gtest_text", "Google Test Parser", ParserCategory::TEST_FRAMEWORK, "Google Test (gtest) output format",
	    ParserPriority::HIGH, std::vector<std::string> {"gtest", "googletest"}, std::vector<std::string> {"c_cpp"}));

	registry.registerParser(make_uniq<DelegatingParser<RSpecTextParser>>(
	    "rspec_text", "RSpec Parser", ParserCategory::TEST_FRAMEWORK, "Ruby RSpec test output format",
	    ParserPriority::HIGH, std::vector<std::string> {"rspec"}, std::vector<std::string> {"ruby"}));

	registry.registerParser(make_uniq<DelegatingParser<MochaChaiTextParser>>(
	    "mocha_chai_text", "Mocha/Chai Parser", ParserCategory::TEST_FRAMEWORK, "Mocha/Chai JavaScript test output",
	    ParserPriority::HIGH, std::vector<std::string> {"mocha", "chai"}, std::vector<std::string> {"javascript"}));

	registry.registerParser(make_uniq<DelegatingParser<NUnitXUnitTextParser>>(
	    "nunit_xunit_text", "NUnit/xUnit Parser", ParserCategory::TEST_FRAMEWORK, ".NET NUnit/xUnit test output",
	    ParserPriority::HIGH, std::vector<std::string> {"nunit", "xunit"}, std::vector<std::string> {"dotnet"}));

	registry.registerParser(make_uniq<DelegatingParser<DuckDBTestParser>>(
	    "duckdb_test", "DuckDB Test Parser", ParserCategory::TEST_FRAMEWORK, "DuckDB unittest output format",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"c_cpp"}));

	registry.registerParser(make_uniq<DelegatingParser<PytestCovTextParser>>(
	    "pytest_cov_text", "Pytest Coverage Parser", ParserCategory::TEST_FRAMEWORK,
	    "Python pytest-cov text output with coverage", ParserPriority::HIGH,
	    std::vector<std::string> {"pytest_cov", "pytest-cov"}, std::vector<std::string> {"python"}));

	// JUnit XML requires special handling (context for XML parsing)
	registry.registerParser(make_uniq<JUnitXmlParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(TestFrameworks);

} // namespace duckdb
