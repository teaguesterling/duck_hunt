#include "legacy_parser_registry.hpp"
#include <algorithm>

namespace duckdb {

void ParserRegistry::registerParser(ParserPtr parser) {
    if (!parser) {
        return;
    }
    
    TestResultFormat format = parser->getFormat();
    format_map_[format] = parser.get();
    parsers_.push_back(std::move(parser));
    needs_resort_ = true;
}

IParser* ParserRegistry::findParser(const std::string& content) const {
    ensureSorted();
    
    // Try parsers in priority order
    for (IParser* parser : sorted_parsers_) {
        if (parser->canParse(content)) {
            return parser;
        }
    }
    
    return nullptr;
}

IParser* ParserRegistry::getParser(TestResultFormat format) const {
    auto it = format_map_.find(format);
    return (it != format_map_.end()) ? it->second : nullptr;
}

const std::vector<IParser*>& ParserRegistry::getAllParsers() const {
    ensureSorted();
    return sorted_parsers_;
}

std::vector<IParser*> ParserRegistry::getParsersByCategory(const std::string& category) const {
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

ParserRegistry::Stats ParserRegistry::getStats() const {
    Stats stats;
    stats.total_parsers = parsers_.size();
    
    std::unordered_map<std::string, size_t> categories;
    for (const auto& parser : parsers_) {
        categories[parser->getCategory()]++;
    }
    
    stats.categories = categories.size();
    stats.parsers_by_category = std::move(categories);
    
    return stats;
}

void ParserRegistry::clear() {
    parsers_.clear();
    sorted_parsers_.clear();
    format_map_.clear();
    needs_resort_ = false;
}

ParserRegistry& ParserRegistry::getInstance() {
    static ParserRegistry instance;
    return instance;
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