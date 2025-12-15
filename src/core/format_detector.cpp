#include "format_detector.hpp"
#include "legacy_parser_registry.hpp"
#include "parsers/base/parser_interface.hpp"

namespace duckdb {

FormatDetector::FormatDetector(const ParserRegistry& registry) : registry_(registry) {
}

TestResultFormat FormatDetector::detectFormat(const std::string& content) const {
    IParser* parser = findBestParser(content);
    return parser ? parser->getFormat() : TestResultFormat::UNKNOWN;
}

IParser* FormatDetector::findBestParser(const std::string& content) const {
    return registry_.findParser(content);
}

bool FormatDetector::canParseContent(const std::string& content) const {
    return findBestParser(content) != nullptr;
}

} // namespace duckdb