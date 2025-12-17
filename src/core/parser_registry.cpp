#include "parser_registry.hpp"
#include <algorithm>
#include <mutex>
#include <iostream>

// Debug tracing for parser registration - disabled for release
// #define PARSER_TRACE(msg) std::cerr << "[ParserRegistry] " << msg << std::endl
#define PARSER_TRACE(msg) do {} while(0)

namespace duckdb {

// Forward declarations of all category registration functions
// These are defined in each category's init.cpp
void RegisterToolOutputsParsers(ParserRegistry& registry);
void RegisterTestFrameworksParsers(ParserRegistry& registry);
void RegisterBuildSystemsParsers(ParserRegistry& registry);
void RegisterLintingToolsParsers(ParserRegistry& registry);
void RegisterDebuggingParsers(ParserRegistry& registry);
void RegisterCISystemsParsers(ParserRegistry& registry);
void RegisterStructuredLogsParsers(ParserRegistry& registry);
void RegisterWebAccessParsers(ParserRegistry& registry);
void RegisterCloudLogsParsers(ParserRegistry& registry);
void RegisterAppLoggingParsers(ParserRegistry& registry);
void RegisterInfrastructureParsers(ParserRegistry& registry);
void RegisterInfrastructureToolsParsers(ParserRegistry& registry);

// Category registration storage (kept for backwards compatibility)
static std::vector<std::pair<std::string, CategoryRegistrationFn>>& GetCategoryRegistry() {
    static std::vector<std::pair<std::string, CategoryRegistrationFn>> registry;
    return registry;
}

static std::once_flag g_init_flag;

void RegisterParserCategory(const std::string& category_name, CategoryRegistrationFn register_fn) {
    GetCategoryRegistry().emplace_back(category_name, std::move(register_fn));
}

void InitializeAllParsers() {
    std::call_once(g_init_flag, []() {
        PARSER_TRACE("=== Starting parser initialization ===");
        auto& registry = ParserRegistry::getInstance();

        // Explicitly call all registration functions to avoid static initialization issues
        PARSER_TRACE("Registering ToolOutputs category...");
        RegisterToolOutputsParsers(registry);
        PARSER_TRACE("Registering TestFrameworks category...");
        RegisterTestFrameworksParsers(registry);
        PARSER_TRACE("Registering BuildSystems category...");
        RegisterBuildSystemsParsers(registry);
        PARSER_TRACE("Registering LintingTools category...");
        RegisterLintingToolsParsers(registry);
        PARSER_TRACE("Registering Debugging category...");
        RegisterDebuggingParsers(registry);
        PARSER_TRACE("Registering CISystems category...");
        RegisterCISystemsParsers(registry);
        PARSER_TRACE("Registering StructuredLogs category...");
        RegisterStructuredLogsParsers(registry);
        PARSER_TRACE("Registering WebAccess category...");
        RegisterWebAccessParsers(registry);
        PARSER_TRACE("Registering CloudLogs category...");
        RegisterCloudLogsParsers(registry);
        PARSER_TRACE("Registering AppLogging category...");
        RegisterAppLoggingParsers(registry);
        PARSER_TRACE("Registering Infrastructure category...");
        RegisterInfrastructureParsers(registry);
        PARSER_TRACE("Registering InfrastructureTools category...");
        RegisterInfrastructureToolsParsers(registry);
        PARSER_TRACE("=== Parser initialization complete ===");
    });
}

// ParserRegistry implementation

ParserRegistry& ParserRegistry::getInstance() {
    static ParserRegistry instance;
    return instance;
}

void ParserRegistry::registerParser(ParserPtr parser) {
    if (!parser) {
        PARSER_TRACE("WARNING: Attempted to register null parser!");
        return;
    }

    // Register by primary format name
    std::string format_name = parser->getFormatName();
    PARSER_TRACE("Registering parser: " << format_name);
    format_map_[format_name] = parser.get();

    // Register aliases
    for (const auto& alias : parser->getAliases()) {
        PARSER_TRACE("  Adding alias: " << alias);
        format_map_[alias] = parser.get();
    }

    parsers_.push_back(std::move(parser));
    needs_resort_ = true;
}

IParser* ParserRegistry::getParser(const std::string& format_name) const {
    // Ensure parsers are initialized
    // Ensure parsers are registered (call_once ensures this only runs once)
    InitializeAllParsers();

    auto it = format_map_.find(format_name);
    return (it != format_map_.end()) ? it->second : nullptr;
}

IParser* ParserRegistry::findParser(const std::string& content) const {
    // Ensure parsers are initialized
    // Ensure parsers are registered (call_once ensures this only runs once)
    InitializeAllParsers();

    ensureSorted();

    // Try parsers in priority order
    for (IParser* parser : sorted_parsers_) {
        if (parser->canParse(content)) {
            return parser;
        }
    }

    return nullptr;
}

std::vector<IParser*> ParserRegistry::getParsersByCategory(const std::string& category) const {
    // Ensure parsers are registered (call_once ensures this only runs once)
    InitializeAllParsers();

    std::vector<IParser*> result;

    for (const auto& parser : parsers_) {
        if (parser->getCategory() == category) {
            result.push_back(parser.get());
        }
    }

    // Sort by priority within category
    std::sort(result.begin(), result.end(), [](IParser* a, IParser* b) {
        return a->getPriority() > b->getPriority();
    });

    return result;
}

std::vector<ParserInfo> ParserRegistry::getAllFormats() const {
    // Ensure parsers are registered (call_once ensures this only runs once)
    InitializeAllParsers();

    std::vector<ParserInfo> result;
    result.reserve(parsers_.size());

    for (const auto& parser : parsers_) {
        ParserInfo info;
        info.format_name = parser->getFormatName();
        info.description = parser->getDescription();
        info.category = parser->getCategory();
        info.required_extension = parser->getRequiredExtension();
        info.priority = parser->getPriority();
        result.push_back(info);
    }

    // Sort by category, then by format name
    std::sort(result.begin(), result.end(), [](const ParserInfo& a, const ParserInfo& b) {
        if (a.category != b.category) {
            return a.category < b.category;
        }
        return a.format_name < b.format_name;
    });

    return result;
}

std::vector<std::string> ParserRegistry::getCategories() const {
    // Ensure parsers are registered (call_once ensures this only runs once)
    InitializeAllParsers();

    std::vector<std::string> categories;
    std::unordered_map<std::string, bool> seen;

    for (const auto& parser : parsers_) {
        const auto& cat = parser->getCategory();
        if (!seen[cat]) {
            categories.push_back(cat);
            seen[cat] = true;
        }
    }

    std::sort(categories.begin(), categories.end());
    return categories;
}

bool ParserRegistry::hasFormat(const std::string& format_name) const {
    // Ensure parsers are registered (call_once ensures this only runs once)
    InitializeAllParsers();
    return format_map_.find(format_name) != format_map_.end();
}

void ParserRegistry::clear() {
    parsers_.clear();
    sorted_parsers_.clear();
    format_map_.clear();
    needs_resort_ = false;
}

void ParserRegistry::ensureSorted() const {
    if (!needs_resort_) {
        return;
    }

    sorted_parsers_.clear();
    sorted_parsers_.reserve(parsers_.size());

    for (const auto& parser : parsers_) {
        sorted_parsers_.push_back(parser.get());
    }

    // Sort by priority (highest first)
    std::sort(sorted_parsers_.begin(), sorted_parsers_.end(),
              [](IParser* a, IParser* b) {
                  return a->getPriority() > b->getPriority();
              });

    needs_resort_ = false;
}

} // namespace duckdb
