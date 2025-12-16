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
 * JUnit text parser wrapper.
 */
class JUnitTextParserImpl : public BaseParser {
public:
    JUnitTextParserImpl()
        : BaseParser("junit_text",
                     "JUnit Text Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "JUnit/Maven test output in text format",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return duck_hunt::JUnitTextParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::JUnitTextParser::ParseJUnitText(content, events);
        return events;
    }
};

/**
 * Google Test text parser wrapper.
 */
class GTestTextParserImpl : public BaseParser {
public:
    GTestTextParserImpl()
        : BaseParser("gtest_text",
                     "Google Test Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Google Test (gtest) output format",
                     ParserPriority::HIGH) {
        addAlias("gtest");
        addAlias("googletest");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::GTestTextParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::GTestTextParser::ParseGoogleTest(content, events);
        return events;
    }
};

/**
 * RSpec text parser wrapper.
 */
class RSpecTextParserImpl : public BaseParser {
public:
    RSpecTextParserImpl()
        : BaseParser("rspec_text",
                     "RSpec Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Ruby RSpec test output format",
                     ParserPriority::HIGH) {
        addAlias("rspec");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::RSpecTextParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::RSpecTextParser::ParseRSpecText(content, events);
        return events;
    }
};

/**
 * Mocha/Chai text parser wrapper.
 */
class MochaChaiTextParserImpl : public BaseParser {
public:
    MochaChaiTextParserImpl()
        : BaseParser("mocha_chai_text",
                     "Mocha/Chai Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Mocha/Chai JavaScript test output",
                     ParserPriority::HIGH) {
        addAlias("mocha");
        addAlias("chai");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::MochaChaiTextParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::MochaChaiTextParser::ParseMochaChai(content, events);
        return events;
    }
};

/**
 * NUnit/xUnit text parser wrapper.
 */
class NUnitXUnitTextParserImpl : public BaseParser {
public:
    NUnitXUnitTextParserImpl()
        : BaseParser("nunit_xunit_text",
                     "NUnit/xUnit Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     ".NET NUnit/xUnit test output",
                     ParserPriority::HIGH) {
        addAlias("nunit");
        addAlias("xunit");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::NUnitXUnitTextParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::NUnitXUnitTextParser::ParseNUnitXUnit(content, events);
        return events;
    }
};

/**
 * Pytest text parser wrapper.
 * Delegates to existing duckdb::PytestParser IParser implementation.
 */
class PytestTextParserImpl : public BaseParser {
public:
    PytestTextParserImpl()
        : BaseParser("pytest_text",
                     "Pytest Text Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Python pytest text output",
                     ParserPriority::HIGH) {
        addAlias("pytest");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    duckdb::PytestParser parser_;
};

/**
 * Pytest JSON parser wrapper.
 * Delegates to existing duckdb::PytestJSONParser IParser implementation.
 */
class PytestJSONParserImpl : public BaseParser {
public:
    PytestJSONParserImpl()
        : BaseParser("pytest_json",
                     "Pytest JSON Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Python pytest JSON report output",
                     ParserPriority::VERY_HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    duckdb::PytestJSONParser parser_;
};

/**
 * JUnit XML parser wrapper (requires webbed extension).
 */
class JUnitXmlParserImpl : public BaseParser {
public:
    JUnitXmlParserImpl()
        : BaseParser("junit_xml",
                     "JUnit XML Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "JUnit XML test results format",
                     ParserPriority::VERY_HIGH) {
        setRequiredExtension("webbed");
    }

    bool canParse(const std::string& content) const override {
        return content.find("<testsuite") != std::string::npos ||
               content.find("<testsuites") != std::string::npos;
    }

    bool requiresContext() const override { return true; }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        // XML parsing requires context - this shouldn't be called directly
        return {};
    }

    std::vector<ValidationEvent> parseWithContext(ClientContext& context,
                                                   const std::string& content) const override {
        duckdb::JUnitXmlParser parser;
        return parser.parseWithContext(context, content);
    }
};

/**
 * DuckDB test output parser wrapper.
 * Handles DuckDB's unittest output format.
 */
class DuckDBTestParserImpl : public BaseParser {
public:
    DuckDBTestParserImpl()
        : BaseParser("duckdb_test",
                     "DuckDB Test Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "DuckDB unittest output format",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        parser_.Parse(content, events);
        return events;
    }

private:
    duck_hunt::DuckDBTestParser parser_;
};

/**
 * Pytest coverage text parser wrapper.
 * Handles pytest-cov output with coverage information.
 */
class PytestCovTextParserImpl : public BaseParser {
public:
    PytestCovTextParserImpl()
        : BaseParser("pytest_cov_text",
                     "Pytest Coverage Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Python pytest-cov text output with coverage",
                     ParserPriority::HIGH) {
        addAlias("pytest_cov");
        addAlias("pytest-cov");
    }

    bool canParse(const std::string& content) const override {
        return parser_.CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        parser_.Parse(content, events);
        return events;
    }

private:
    duck_hunt::PytestCovTextParser parser_;
};

/**
 * Register all test framework parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(TestFrameworks);

void RegisterTestFrameworksParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<JUnitTextParserImpl>());
    registry.registerParser(make_uniq<GTestTextParserImpl>());
    registry.registerParser(make_uniq<RSpecTextParserImpl>());
    registry.registerParser(make_uniq<MochaChaiTextParserImpl>());
    registry.registerParser(make_uniq<NUnitXUnitTextParserImpl>());
    registry.registerParser(make_uniq<PytestTextParserImpl>());
    registry.registerParser(make_uniq<PytestJSONParserImpl>());
    registry.registerParser(make_uniq<JUnitXmlParserImpl>());
    registry.registerParser(make_uniq<DuckDBTestParserImpl>());
    registry.registerParser(make_uniq<PytestCovTextParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(TestFrameworks);


} // namespace duckdb
