#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "ansible_text_parser.hpp"

namespace duckdb {

/**
 * Ansible parser wrapper.
 */
class AnsibleParserImpl : public BaseParser {
public:
	AnsibleParserImpl()
	    : BaseParser("ansible_text", "Ansible Parser", ParserCategory::CI_SYSTEM, "Ansible playbook output",
	                 ParserPriority::HIGH) {
		addAlias("ansible");
		addGroup("infrastructure");
		addGroup("ci");
		addGroup("python");
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	AnsibleTextParser parser_;
};

/**
 * Register all infrastructure tools parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(InfrastructureTools);

void RegisterInfrastructureToolsParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<AnsibleParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(InfrastructureTools);

} // namespace duckdb
