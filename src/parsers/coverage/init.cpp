#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "lcov_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all coverage parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(Coverage);

void RegisterCoverageParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<LcovParser>>(
	    "lcov", "LCOV Parser", ParserCategory::COVERAGE, "LCOV/gcov code coverage format", ParserPriority::HIGH,
	    std::vector<std::string> {"gcov", "lcov_info"}, std::vector<std::string> {"c_cpp", "coverage"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(Coverage);

} // namespace duckdb
