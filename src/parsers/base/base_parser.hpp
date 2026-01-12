#pragma once

#include "core/parser_registry.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Base class for parsers that provides common functionality.
 * Inherit from this to create a new parser with minimal boilerplate.
 *
 * Example:
 *   class MyParser : public BaseParser {
 *   public:
 *       MyParser() : BaseParser("my_format", "My Parser", "my_category",
 *                               "Description of my format") {}
 *
 *       bool canParse(const std::string& content) const override {
 *           return content.find("MY_MARKER") != std::string::npos;
 *       }
 *
 *       std::vector<ValidationEvent> parse(const std::string& content) const override {
 *           // parsing logic
 *       }
 *   };
 */
class BaseParser : public IParser {
public:
	BaseParser(std::string format_name, std::string name, std::string category, std::string description,
	           int priority = 50)
	    : format_name_(std::move(format_name)), name_(std::move(name)), category_(std::move(category)),
	      description_(std::move(description)), priority_(priority) {
	}

	virtual ~BaseParser() = default;

	// Metadata accessors
	std::string getFormatName() const override {
		return format_name_;
	}
	std::string getName() const override {
		return name_;
	}
	std::string getCategory() const override {
		return category_;
	}
	std::string getDescription() const override {
		return description_;
	}
	int getPriority() const override {
		return priority_;
	}

	// Override these if needed
	std::vector<std::string> getAliases() const override {
		return aliases_;
	}
	std::string getRequiredExtension() const override {
		return required_extension_;
	}
	std::vector<std::string> getGroups() const override {
		return groups_;
	}

protected:
	// Subclasses can set these in constructor
	void addAlias(const std::string &alias) {
		aliases_.push_back(alias);
	}
	void setRequiredExtension(const std::string &ext) {
		required_extension_ = ext;
	}
	void addGroup(const std::string &group) {
		groups_.push_back(group);
	}
	void setGroups(const std::vector<std::string> &groups) {
		groups_ = groups;
	}

	// Helper to create a basic event with common fields pre-filled
	ValidationEvent createEvent() const {
		ValidationEvent event;
		event.tool_name = format_name_;
		event.category = category_;
		return event;
	}

private:
	std::string format_name_;
	std::string name_;
	std::string category_;
	std::string description_;
	int priority_;
	std::vector<std::string> aliases_;
	std::string required_extension_;
	std::vector<std::string> groups_;
};

/**
 * Template wrapper that delegates to an existing IParser implementation.
 * Use this when you have a parser that already implements IParser but need
 * to add aliases, descriptions, or register it with the registry.
 *
 * Example usage in init.cpp:
 *   registry.registerParser(make_uniq<DelegatingParser<PylintParser>>(
 *       "pylint_text", "Pylint Parser", ParserCategory::LINTING,
 *       "Python Pylint code quality output", ParserPriority::HIGH,
 *       {"pylint"}  // aliases
 *   ));
 */
template <typename ParserT>
class DelegatingParser : public BaseParser {
private:
	// Helper to get metadata from a temporary parser instance
	static std::string getParserFormatName() {
		ParserT p;
		return p.getFormatName();
	}
	static std::string getParserName() {
		ParserT p;
		return p.getName();
	}
	static std::string getParserCategory() {
		ParserT p;
		return p.getCategory();
	}
	static std::string getParserDescription() {
		ParserT p;
		return p.getDescription();
	}
	static int getParserPriority() {
		ParserT p;
		return p.getPriority();
	}
	static std::vector<std::string> getParserAliases() {
		ParserT p;
		return p.getAliases();
	}

public:
	/**
	 * Default constructor - pulls metadata from the underlying parser.
	 * The parser type must implement IParser methods: getFormatName(), getName(),
	 * getCategory(), getDescription(), getPriority(), getAliases().
	 */
	DelegatingParser()
	    : BaseParser(getParserFormatName(), getParserName(), getParserCategory(), getParserDescription(),
	                 getParserPriority()) {
		for (const auto &alias : getParserAliases()) {
			addAlias(alias);
		}
	}

	/**
	 * Explicit constructor - allows overriding parser metadata.
	 */
	DelegatingParser(std::string format_name, std::string name, std::string category, std::string description,
	                 int priority = 50, std::vector<std::string> aliases = {}, std::vector<std::string> groups = {})
	    : BaseParser(std::move(format_name), std::move(name), std::move(category), std::move(description), priority) {
		for (const auto &alias : aliases) {
			addAlias(alias);
		}
		for (const auto &group : groups) {
			addGroup(group);
		}
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

	std::vector<CommandPattern> getCommandPatterns() const override {
		return parser_.getCommandPatterns();
	}

private:
	ParserT parser_;
};

// Backward compatibility: alias in log_parsers namespace
namespace log_parsers {
using BaseParser = duckdb::BaseParser;
}

} // namespace duckdb
