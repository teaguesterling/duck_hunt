#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "ansible_text_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all infrastructure tools parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(InfrastructureTools);

void RegisterInfrastructureToolsParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<AnsibleTextParser>>(
	    "ansible_text", "Ansible Parser", ParserCategory::CI_SYSTEM, "Ansible playbook output", ParserPriority::HIGH,
	    std::vector<std::string> {"ansible"}, std::vector<std::string> {"infrastructure", "ci", "python"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(InfrastructureTools);

} // namespace duckdb
