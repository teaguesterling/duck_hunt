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
#include "unity_test_xml_parser.hpp"
#include "duckdb_test_parser.hpp"
#include "pytest_cov_text_parser.hpp"
#include "playwright_text_parser.hpp"
#include "playwright_json_parser.hpp"
#include "gotest_text_parser.hpp"

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
		addGroup("test");
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
 * Unity Test XML parser wrapper (requires webbed extension).
 * Parses Unity Test Framework results in NUnit 3 XML format.
 */
class UnityTestXmlParserImpl : public BaseParser {
public:
	UnityTestXmlParserImpl()
	    : BaseParser("unity_test_xml", "Unity Test XML Parser", ParserCategory::TEST_FRAMEWORK,
	                 "Unity Test Framework XML results (NUnit 3 format)", ParserPriority::VERY_HIGH) {
		setRequiredExtension("webbed");
		addGroup("unity");
		addGroup("dotnet");
		addGroup("test");
	}

	bool canParse(const std::string &content) const override {
		// Check for NUnit 3 / Unity test-run format
		if (content.find("<test-run") == std::string::npos) {
			return false;
		}
		// Additional markers for Unity/NUnit 3
		return content.find("testcasecount=") != std::string::npos ||
		       content.find("engine-version=") != std::string::npos;
	}

	bool requiresContext() const override {
		return true;
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		// XML parsing requires context - this shouldn't be called directly
		return {};
	}

	std::vector<ValidationEvent> parseWithContext(ClientContext &context, const std::string &content) const override {
		UnityTestXmlParser parser;
		return parser.parseWithContext(context, content);
	}

	bool supportsFileParsing() const override {
		return true;
	}

	std::vector<ValidationEvent> parseFile(ClientContext &context, const std::string &file_path) const override {
		UnityTestXmlParser parser;
		return parser.parseFile(context, file_path);
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
	    ParserPriority::HIGH, std::vector<std::string> {"pytest"}, std::vector<std::string> {"python", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<PytestJSONParser>>(
	    "pytest_json", "Pytest JSON Parser", ParserCategory::TEST_FRAMEWORK, "Python pytest JSON report output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {}, std::vector<std::string> {"python", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<JUnitTextParser>>(
	    "junit_text", "JUnit Text Parser", ParserCategory::TEST_FRAMEWORK, "JUnit/Maven test output in text format",
	    ParserPriority::HIGH, std::vector<std::string> {"junit"}, std::vector<std::string> {"java", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<GTestTextParser>>(
	    "gtest_text", "Google Test Parser", ParserCategory::TEST_FRAMEWORK, "Google Test (gtest) output format",
	    ParserPriority::HIGH, std::vector<std::string> {"gtest", "googletest"},
	    std::vector<std::string> {"c_cpp", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<RSpecTextParser>>(
	    "rspec_text", "RSpec Parser", ParserCategory::TEST_FRAMEWORK, "Ruby RSpec test output format",
	    ParserPriority::HIGH, std::vector<std::string> {"rspec"}, std::vector<std::string> {"ruby", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<MochaChaiTextParser>>(
	    "mocha_chai_text", "Mocha/Chai Parser", ParserCategory::TEST_FRAMEWORK, "Mocha/Chai JavaScript test output",
	    ParserPriority::HIGH, std::vector<std::string> {"mocha", "chai"},
	    std::vector<std::string> {"javascript", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<NUnitXUnitTextParser>>(
	    "nunit_xunit_text", "NUnit/xUnit Parser", ParserCategory::TEST_FRAMEWORK, ".NET NUnit/xUnit test output",
	    ParserPriority::HIGH, std::vector<std::string> {"nunit", "xunit"},
	    std::vector<std::string> {"dotnet", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<DuckDBTestParser>>(
	    "duckdb_test", "DuckDB Test Parser", ParserCategory::TEST_FRAMEWORK, "DuckDB unittest output format",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"c_cpp", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<PytestCovTextParser>>(
	    "pytest_cov_text", "Pytest Coverage Parser", ParserCategory::TEST_FRAMEWORK,
	    "Python pytest-cov text output with coverage", ParserPriority::HIGH,
	    std::vector<std::string> {"pytest_cov", "pytest-cov"},
	    std::vector<std::string> {"python", "test", "coverage"}));

	// Playwright parsers
	registry.registerParser(make_uniq<DelegatingParser<PlaywrightTextParser>>(
	    "playwright_text", "Playwright Text Parser", ParserCategory::TEST_FRAMEWORK,
	    "Playwright test runner text output (list/line reporter)", ParserPriority::HIGH,
	    std::vector<std::string> {"playwright"}, std::vector<std::string> {"javascript", "test"}));

	registry.registerParser(make_uniq<DelegatingParser<PlaywrightJSONParser>>(
	    "playwright_json", "Playwright JSON Parser", ParserCategory::TEST_FRAMEWORK, "Playwright JSON reporter output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {}, std::vector<std::string> {"javascript", "test"}));

	// Go test text parser
	registry.registerParser(make_uniq<DelegatingParser<GoTestTextParser>>(
	    "gotest_text", "Go Test Text Parser", ParserCategory::TEST_FRAMEWORK, "Go test text output (default format)",
	    ParserPriority::HIGH, std::vector<std::string> {"gotest"}, std::vector<std::string> {"go", "test"}));

	// JUnit XML requires special handling (context for XML parsing)
	registry.registerParser(make_uniq<JUnitXmlParserImpl>());

	// Unity Test XML requires special handling (context for XML parsing)
	registry.registerParser(make_uniq<UnityTestXmlParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(TestFrameworks);

} // namespace duckdb
