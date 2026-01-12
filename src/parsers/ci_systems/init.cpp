#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "drone_ci_text_parser.hpp"
#include "terraform_text_parser.hpp"
#include "github_cli_parser.hpp"

namespace duckdb {

/**
 * Drone CI parser wrapper.
 */
class DroneCIParserImpl : public BaseParser {
public:
	DroneCIParserImpl()
	    : BaseParser("drone_ci_text", "Drone CI Parser", ParserCategory::CI_SYSTEM, "Drone CI build output",
	                 ParserPriority::HIGH) {
		addAlias("drone");
		addAlias("drone_ci");
		addGroup("ci");
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	DroneCITextParser parser_;
};

/**
 * Terraform parser wrapper.
 */
class TerraformParserImpl : public BaseParser {
public:
	TerraformParserImpl()
	    : BaseParser("terraform_text", "Terraform Parser", ParserCategory::CI_SYSTEM, "Terraform plan/apply output",
	                 ParserPriority::HIGH) {
		addAlias("terraform");
		addAlias("tf");
		addGroup("ci");
		addGroup("infrastructure");
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	TerraformTextParser parser_;
};

/**
 * GitHub CLI parser wrapper.
 */
class GitHubCliParserImpl : public BaseParser {
public:
	GitHubCliParserImpl()
	    : BaseParser("github_cli", "GitHub CLI Parser", ParserCategory::CI_SYSTEM, "GitHub CLI (gh) command output",
	                 ParserPriority::HIGH) {
		addAlias("gh");
		addGroup("ci");
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	GitHubCliParser parser_;
};

/**
 * Register all CI system parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(CISystems);

void RegisterCISystemsParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<DroneCIParserImpl>());
	registry.registerParser(make_uniq<TerraformParserImpl>());
	registry.registerParser(make_uniq<GitHubCliParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(CISystems);

} // namespace duckdb
