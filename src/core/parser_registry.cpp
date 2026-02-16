#include "parser_registry.hpp"
#include <algorithm>
#include <mutex>
#include <regex>
#include <unordered_map>

// Parser tracing disabled - no-op macro
#define PARSER_TRACE(msg)                                                                                              \
	do {                                                                                                               \
	} while (0)

namespace duckdb {

// Regex cache to avoid recompiling patterns on every match
// Thread-safe: protected by mutex
namespace {
std::unordered_map<std::string, std::regex> regex_cache_;
std::mutex regex_cache_mutex_;

// Get or compile a regex pattern (thread-safe, cached)
const std::regex *GetCachedRegex(const std::string &pattern, bool ignore_case = true) {
	std::lock_guard<std::mutex> lock(regex_cache_mutex_);
	auto it = regex_cache_.find(pattern);
	if (it != regex_cache_.end()) {
		return &it->second;
	}
	try {
		auto flags = ignore_case ? std::regex::icase : std::regex::ECMAScript;
		auto result = regex_cache_.emplace(pattern, std::regex(pattern, flags));
		return &result.first->second;
	} catch (const std::regex_error &) {
		return nullptr;
	}
}
} // anonymous namespace

// Forward declarations of all category registration functions
// These are defined in each category's init.cpp
void RegisterToolOutputsParsers(ParserRegistry &registry);
void RegisterTestFrameworksParsers(ParserRegistry &registry);
void RegisterBuildSystemsParsers(ParserRegistry &registry);
void RegisterLintingToolsParsers(ParserRegistry &registry);
void RegisterDebuggingParsers(ParserRegistry &registry);
void RegisterCISystemsParsers(ParserRegistry &registry);
void RegisterStructuredLogsParsers(ParserRegistry &registry);
void RegisterWebAccessParsers(ParserRegistry &registry);
void RegisterCloudLogsParsers(ParserRegistry &registry);
void RegisterAppLoggingParsers(ParserRegistry &registry);
void RegisterInfrastructureParsers(ParserRegistry &registry);
void RegisterInfrastructureToolsParsers(ParserRegistry &registry);
void RegisterCoverageParsers(ParserRegistry &registry);
void RegisterDistributedSystemsParsers(ParserRegistry &registry);

// Category registration storage (kept for backwards compatibility)
static std::vector<std::pair<std::string, CategoryRegistrationFn>> &GetCategoryRegistry() {
	static std::vector<std::pair<std::string, CategoryRegistrationFn>> registry;
	return registry;
}

static std::once_flag g_init_flag;

void RegisterParserCategory(const std::string &category_name, CategoryRegistrationFn register_fn) {
	GetCategoryRegistry().emplace_back(category_name, std::move(register_fn));
}

void InitializeAllParsers() {
	std::call_once(g_init_flag, []() {
		PARSER_TRACE("=== Starting parser initialization ===");
		auto &registry = ParserRegistry::getInstance();

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
		PARSER_TRACE("Registering Coverage category...");
		RegisterCoverageParsers(registry);
		PARSER_TRACE("Registering DistributedSystems category...");
		RegisterDistributedSystemsParsers(registry);
		PARSER_TRACE("=== Parser initialization complete ===");
	});
}

// ParserRegistry implementation

ParserRegistry &ParserRegistry::getInstance() {
	static ParserRegistry instance;
	return instance;
}

void ParserRegistry::registerParser(ParserPtr parser) {
	if (!parser) {
		PARSER_TRACE("WARNING: Attempted to register null parser!");
		return;
	}

	std::lock_guard<std::mutex> lock(registry_mutex_);

	// Register by primary format name
	std::string format_name = parser->getFormatName();
	PARSER_TRACE("Registering parser: " << format_name);
	format_map_[format_name] = parser.get();

	// Register aliases
	for (const auto &alias : parser->getAliases()) {
		PARSER_TRACE("  Adding alias: " << alias);
		format_map_[alias] = parser.get();
	}

	parsers_.push_back(std::move(parser));
	needs_resort_ = true;
}

IParser *ParserRegistry::getParser(const std::string &format_name) const {
	// Ensure parsers are initialized (call_once ensures this only runs once)
	InitializeAllParsers();

	std::lock_guard<std::mutex> lock(registry_mutex_);
	auto it = format_map_.find(format_name);
	return (it != format_map_.end()) ? it->second : nullptr;
}

IParser *ParserRegistry::findParser(const std::string &content) const {
	// Ensure parsers are initialized (call_once ensures this only runs once)
	InitializeAllParsers();

	std::lock_guard<std::mutex> lock(registry_mutex_);
	ensureSortedLocked();

	// Try parsers in priority order
	for (IParser *parser : sorted_parsers_) {
		if (parser->canParse(content)) {
			return parser;
		}
	}

	return nullptr;
}

// Convert SQL LIKE pattern to regex pattern string
static std::string likePatternToRegex(const std::string &pattern) {
	// Convert SQL LIKE pattern to regex
	// % matches any sequence, _ matches any single character
	std::string regex_pattern;
	regex_pattern.reserve(pattern.size() * 2);

	for (char c : pattern) {
		switch (c) {
		case '%':
			regex_pattern += ".*";
			break;
		case '_':
			regex_pattern += ".";
			break;
		case '.':
		case '^':
		case '$':
		case '+':
		case '?':
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case '|':
		case '\\':
			regex_pattern += '\\';
			regex_pattern += c;
			break;
		default:
			regex_pattern += c;
			break;
		}
	}
	return regex_pattern;
}

// Helper for SQL LIKE pattern matching (uses cached regex)
static bool matchLikePattern(const std::string &str, const std::string &pattern) {
	// Use a prefix to distinguish LIKE patterns in the cache
	std::string cache_key = "LIKE:" + pattern;
	const std::regex *re = GetCachedRegex(likePatternToRegex(pattern));
	if (!re) {
		return false;
	}
	return std::regex_match(str, *re);
}

// Normalize command by stripping path prefix from the executable.
// e.g., "/usr/bin/eslint ." -> "eslint ."
//       "./node_modules/.bin/prettier --check" -> "prettier --check"
static std::string normalizeCommand(const std::string &command) {
	if (command.empty()) {
		return command;
	}

	// Find the first space (end of executable name)
	size_t first_space = command.find(' ');
	std::string executable = (first_space == std::string::npos) ? command : command.substr(0, first_space);
	std::string rest = (first_space == std::string::npos) ? "" : command.substr(first_space);

	// Strip path prefix from executable (everything up to last /)
	size_t last_slash = executable.rfind('/');
	if (last_slash != std::string::npos) {
		executable = executable.substr(last_slash + 1);
	}

	return executable + rest;
}

IParser *ParserRegistry::findParserByCommand(const std::string &command) const {
	// Ensure parsers are initialized
	InitializeAllParsers();

	// Normalize command by stripping path prefix from executable
	// Do this before acquiring lock to minimize lock hold time
	std::string normalized = normalizeCommand(command);

	std::lock_guard<std::mutex> lock(registry_mutex_);
	ensureSortedLocked();

	IParser *best_match = nullptr;
	int best_priority = -1;

	// Try parsers in priority order, find highest priority match
	for (IParser *parser : sorted_parsers_) {
		// Skip if we already have a better match
		if (best_match && parser->getPriority() <= best_priority) {
			continue;
		}

		const auto &patterns = parser->getCommandPatterns();
		for (const auto &cp : patterns) {
			bool matched = false;

			if (cp.pattern_type == "literal") {
				matched = (normalized == cp.pattern);
			} else if (cp.pattern_type == "like") {
				matched = matchLikePattern(normalized, cp.pattern);
			} else if (cp.pattern_type == "regexp") {
				const std::regex *re = GetCachedRegex(cp.pattern);
				if (re) {
					matched = std::regex_search(normalized, *re);
				}
			}

			if (matched) {
				if (parser->getPriority() > best_priority) {
					best_match = parser;
					best_priority = parser->getPriority();
				}
				break; // Found a match for this parser, no need to check other patterns
			}
		}
	}

	return best_match;
}

std::vector<IParser *> ParserRegistry::getParsersByCategory(const std::string &category) const {
	// Ensure parsers are registered (call_once ensures this only runs once)
	InitializeAllParsers();

	std::vector<IParser *> result;

	{
		std::lock_guard<std::mutex> lock(registry_mutex_);
		for (const auto &parser : parsers_) {
			if (parser->getCategory() == category) {
				result.push_back(parser.get());
			}
		}
	}

	// Sort by priority within category (stable for determinism)
	// Sorting is done outside lock since result is a local copy
	std::stable_sort(result.begin(), result.end(),
	                 [](IParser *a, IParser *b) { return a->getPriority() > b->getPriority(); });

	return result;
}

std::vector<IParser *> ParserRegistry::getParsersByGroup(const std::string &group) const {
	// Ensure parsers are registered (call_once ensures this only runs once)
	InitializeAllParsers();

	std::vector<IParser *> result;

	{
		std::lock_guard<std::mutex> lock(registry_mutex_);
		for (const auto &parser : parsers_) {
			const auto &groups = parser->getGroups();
			if (std::find(groups.begin(), groups.end(), group) != groups.end()) {
				result.push_back(parser.get());
			}
		}
	}

	// Sort by priority within group (stable for determinism)
	// Sorting is done outside lock since result is a local copy
	std::stable_sort(result.begin(), result.end(),
	                 [](IParser *a, IParser *b) { return a->getPriority() > b->getPriority(); });

	return result;
}

bool ParserRegistry::isGroup(const std::string &name) const {
	// Ensure parsers are registered (call_once ensures this only runs once)
	InitializeAllParsers();

	std::lock_guard<std::mutex> lock(registry_mutex_);
	for (const auto &parser : parsers_) {
		const auto &groups = parser->getGroups();
		if (std::find(groups.begin(), groups.end(), name) != groups.end()) {
			return true;
		}
	}
	return false;
}

std::vector<std::string> ParserRegistry::getGroups() const {
	// Ensure parsers are registered (call_once ensures this only runs once)
	InitializeAllParsers();

	std::vector<std::string> groups;
	std::unordered_map<std::string, bool> seen;

	{
		std::lock_guard<std::mutex> lock(registry_mutex_);
		for (const auto &parser : parsers_) {
			for (const auto &group : parser->getGroups()) {
				if (!seen[group]) {
					groups.push_back(group);
					seen[group] = true;
				}
			}
		}
	}

	// Sort outside lock since groups is a local copy
	std::sort(groups.begin(), groups.end());
	return groups;
}

std::vector<ParserInfo> ParserRegistry::getAllFormats() const {
	// Ensure parsers are registered (call_once ensures this only runs once)
	InitializeAllParsers();

	std::vector<ParserInfo> result;

	{
		std::lock_guard<std::mutex> lock(registry_mutex_);
		result.reserve(parsers_.size());

		for (const auto &parser : parsers_) {
			ParserInfo info;
			info.format_name = parser->getFormatName();
			info.description = parser->getDescription();
			info.category = parser->getCategory();
			info.required_extension = parser->getRequiredExtension();
			info.priority = parser->getPriority();
			info.command_patterns = parser->getCommandPatterns();
			info.groups = parser->getGroups();
			result.push_back(info);
		}
	}

	// Sort outside lock since result is a local copy
	std::sort(result.begin(), result.end(), [](const ParserInfo &a, const ParserInfo &b) {
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

	{
		std::lock_guard<std::mutex> lock(registry_mutex_);
		for (const auto &parser : parsers_) {
			const auto &cat = parser->getCategory();
			if (!seen[cat]) {
				categories.push_back(cat);
				seen[cat] = true;
			}
		}
	}

	// Sort outside lock since categories is a local copy
	std::sort(categories.begin(), categories.end());
	return categories;
}

bool ParserRegistry::hasFormat(const std::string &format_name) const {
	// Ensure parsers are registered (call_once ensures this only runs once)
	InitializeAllParsers();

	std::lock_guard<std::mutex> lock(registry_mutex_);
	return format_map_.find(format_name) != format_map_.end();
}

void ParserRegistry::clear() {
	std::lock_guard<std::mutex> lock(registry_mutex_);
	parsers_.clear();
	sorted_parsers_.clear();
	format_map_.clear();
	needs_resort_ = false;
}

void ParserRegistry::ensureSortedLocked() const {
	// Note: Caller must hold registry_mutex_
	if (!needs_resort_) {
		return;
	}

	sorted_parsers_.clear();
	sorted_parsers_.reserve(parsers_.size());

	for (const auto &parser : parsers_) {
		sorted_parsers_.push_back(parser.get());
	}

	// Sort by priority (highest first), using stable_sort to preserve
	// registration order as tie-breaker when priorities are equal.
	// This ensures deterministic detection across platforms. (Issue #21)
	std::stable_sort(sorted_parsers_.begin(), sorted_parsers_.end(),
	                 [](IParser *a, IParser *b) { return a->getPriority() > b->getPriority(); });

	needs_resort_ = false;
}

} // namespace duckdb
