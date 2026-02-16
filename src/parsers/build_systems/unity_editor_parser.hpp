#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Unity Editor build and test output.
 *
 * Handles:
 * - C# compilation errors: file.cs(line,col): error CS0234: message
 * - Unity build progress: [545/613 0s] CopyFiles ...
 * - Unity licensing/module messages: [Licensing::Module] Error: ...
 * - Build results: Build succeeded/failed
 *
 * Unity uses Roslyn/MSBuild internally but outputs without project suffix.
 */
class UnityEditorParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "unity_editor";
	}
	std::string getName() const override {
		return "Unity Editor Parser";
	}
	int getPriority() const override {
		return 85; // Higher than MSBuild to match Unity-specific patterns first
	}
	std::string getCategory() const override {
		return "build_system";
	}
	std::string getDescription() const override {
		return "Unity Editor build and test logs";
	}
	std::vector<std::string> getAliases() const override {
		return {"unity", "unity_build"};
	}
	std::vector<std::string> getGroups() const override {
		return {"csharp", "gamedev"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {CommandPattern::Like("unity-editor%"), CommandPattern::Like("Unity%")};
	}

	// Streaming support for line-by-line parsing
	bool supportsStreaming() const override {
		return true;
	}
	std::vector<ValidationEvent> parseLine(const std::string &line, int32_t line_number,
	                                       int64_t &event_id) const override;
};

} // namespace duckdb
