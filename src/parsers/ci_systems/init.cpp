#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "drone_ci_text_parser.hpp"
#include "terraform_text_parser.hpp"
#include "github_cli_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all CI system parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(CISystems);

void RegisterCISystemsParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<DroneCITextParser>>(
	    "drone_ci_text", "Drone CI Parser", ParserCategory::CI_SYSTEM, "Drone CI build output", ParserPriority::HIGH,
	    std::vector<std::string> {"drone", "drone_ci"}, std::vector<std::string> {"ci"}));

	registry.registerParser(make_uniq<P<TerraformTextParser>>(
	    "terraform_text", "Terraform Parser", ParserCategory::CI_SYSTEM, "Terraform plan/apply output",
	    ParserPriority::HIGH, std::vector<std::string> {"terraform", "tf"},
	    std::vector<std::string> {"ci", "infrastructure"}));

	registry.registerParser(make_uniq<P<GitHubCliParser>>(
	    "github_cli", "GitHub CLI Parser", ParserCategory::CI_SYSTEM, "GitHub CLI (gh) command output",
	    ParserPriority::HIGH, std::vector<std::string> {"gh"}, std::vector<std::string> {"ci"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(CISystems);

} // namespace duckdb
