#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "lcov_parser.hpp"

namespace duckdb {

/**
 * LCOV coverage parser wrapper.
 */
class LcovParserImpl : public BaseParser {
public:
	LcovParserImpl()
	    : BaseParser("lcov", "LCOV Parser", ParserCategory::COVERAGE, "LCOV/gcov code coverage format",
	                 ParserPriority::HIGH) {
		addAlias("gcov");
		addAlias("lcov_info");
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	LcovParser parser_;
};

/**
 * Register all coverage parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(Coverage);

void RegisterCoverageParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<LcovParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(Coverage);

} // namespace duckdb
