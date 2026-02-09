#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Docker build output.
 * Detects build steps, layer caching, and error patterns.
 */
class DockerBuildParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "docker_build";
	}
	std::string getName() const override {
		return "docker";
	}
	std::string getDescription() const override {
		return "Docker build output";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "build_system";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("docker build"),
		    CommandPattern::Like("docker build%"),
		    CommandPattern::Like("docker-compose%"),
		    CommandPattern::Literal("buildx build"),
		};
	}
};

} // namespace duckdb
