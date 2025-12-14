#include "../../core/new_parser_registry.hpp"
#include "../base/base_parser.hpp"
#include "../specialized/strace_parser.hpp"
#include "../specialized/valgrind_parser.hpp"
#include "../specialized/gdb_lldb_parser.hpp"
#include "../specialized/coverage_parser.hpp"

namespace duckdb {
namespace log_parsers {

/**
 * Strace parser - wraps existing static parser in new interface.
 */
class StraceParserImpl : public BaseParser {
public:
    StraceParserImpl()
        : BaseParser("strace",
                     "strace Parser",
                     ParserCategory::DEBUGGING,
                     "strace system call trace output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return duck_hunt::StraceParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::StraceParser::ParseStrace(content, events);
        return events;
    }
};

/**
 * Valgrind parser - wraps existing static parser.
 */
class ValgrindParserImpl : public BaseParser {
public:
    ValgrindParserImpl()
        : BaseParser("valgrind",
                     "Valgrind Parser",
                     ParserCategory::DEBUGGING,
                     "Valgrind memory analysis output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        // Valgrind output typically contains "==PID==" markers
        return content.find("==") != std::string::npos &&
               (content.find("Memcheck") != std::string::npos ||
                content.find("HEAP SUMMARY") != std::string::npos ||
                content.find("LEAK SUMMARY") != std::string::npos ||
                content.find("Invalid read") != std::string::npos ||
                content.find("Invalid write") != std::string::npos);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::ValgrindParser::ParseValgrind(content, events);
        return events;
    }
};

/**
 * GDB/LLDB parser - wraps existing static parser.
 */
class GdbLldbParserImpl : public BaseParser {
public:
    GdbLldbParserImpl()
        : BaseParser("gdb_lldb",
                     "GDB/LLDB Parser",
                     ParserCategory::DEBUGGING,
                     "GDB/LLDB debugger output",
                     ParserPriority::HIGH) {
        addAlias("gdb");
        addAlias("lldb");
    }

    bool canParse(const std::string& content) const override {
        return content.find("(gdb)") != std::string::npos ||
               content.find("(lldb)") != std::string::npos ||
               content.find("Program received signal") != std::string::npos ||
               content.find("Breakpoint") != std::string::npos ||
               content.find("Thread ") != std::string::npos;
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::GdbLldbParser::ParseGdbLldb(content, events);
        return events;
    }
};

/**
 * Coverage parser - wraps existing static parser.
 */
class CoverageParserImpl : public BaseParser {
public:
    CoverageParserImpl()
        : BaseParser("coverage_text",
                     "Coverage Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Code coverage report output",
                     ParserPriority::HIGH) {
        addAlias("coverage");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::CoverageParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::CoverageParser::ParseCoverageText(content, events);
        return events;
    }
};

/**
 * Pytest coverage parser.
 */
class PytestCovParserImpl : public BaseParser {
public:
    PytestCovParserImpl()
        : BaseParser("pytest_cov_text",
                     "Pytest Coverage Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Pytest coverage report output",
                     ParserPriority::HIGH) {
        addAlias("pytest_cov");
    }

    bool canParse(const std::string& content) const override {
        // Pytest-cov specific markers
        return content.find("TOTAL") != std::string::npos &&
               content.find("%") != std::string::npos &&
               (content.find("pytest") != std::string::npos ||
                content.find("coverage") != std::string::npos);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::CoverageParser::ParsePytestCovText(content, events);
        return events;
    }
};

/**
 * Register all debugging parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(Debugging);

void RegisterDebuggingParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<StraceParserImpl>());
    registry.registerParser(make_uniq<ValgrindParserImpl>());
    registry.registerParser(make_uniq<GdbLldbParserImpl>());
    registry.registerParser(make_uniq<CoverageParserImpl>());
    registry.registerParser(make_uniq<PytestCovParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(Debugging);

} // namespace log_parsers
} // namespace duckdb
