#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Node.js/npm/yarn build output.
 * Handles npm errors, Jest test results, ESLint issues, Webpack errors, and dependency resolution.
 */
class NodeParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "node_build";
	}
	std::string getName() const override {
		return "Node.js Build Parser";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "build_system";
	}
	std::string getDescription() const override {
		return "Node.js/npm build output";
	}
	std::vector<std::string> getAliases() const override {
		return {"node", "npm", "yarn"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("npm run%"), CommandPattern::Like("npm install%"),
		    CommandPattern::Like("npm ci%"),  CommandPattern::Like("yarn %"),
		    CommandPattern::Like("pnpm %"),   CommandPattern::Regexp("(npm|yarn|pnpm)\\s+"),
		};
	}
};

} // namespace duckdb
