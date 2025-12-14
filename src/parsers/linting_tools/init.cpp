#include "../../core/new_parser_registry.hpp"
#include "../base/base_parser.hpp"
#include "pylint_parser.hpp"
#include "mypy_parser.hpp"
#include "flake8_parser.hpp"
#include "black_parser.hpp"
#include "yapf_text_parser.hpp"
#include "clang_tidy_parser.hpp"
#include "autopep8_text_parser.hpp"

namespace duckdb {
namespace log_parsers {

/**
 * Pylint parser wrapper.
 * Delegates to existing IParser implementation.
 */
class PylintParserImpl : public BaseParser {
public:
    PylintParserImpl()
        : BaseParser("pylint_text",
                     "Pylint Parser",
                     ParserCategory::LINTING,
                     "Python Pylint code quality output",
                     ParserPriority::HIGH) {
        addAlias("pylint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    PylintParser parser_;
};

/**
 * Mypy parser wrapper.
 */
class MypyParserImpl : public BaseParser {
public:
    MypyParserImpl()
        : BaseParser("mypy_text",
                     "Mypy Parser",
                     ParserCategory::LINTING,
                     "Python Mypy type checker output",
                     ParserPriority::HIGH) {
        addAlias("mypy");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    MypyParser parser_;
};

/**
 * Flake8 parser wrapper.
 */
class Flake8ParserImpl : public BaseParser {
public:
    Flake8ParserImpl()
        : BaseParser("flake8_text",
                     "Flake8 Parser",
                     ParserCategory::LINTING,
                     "Python Flake8 style checker output",
                     ParserPriority::HIGH) {
        addAlias("flake8");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    Flake8Parser parser_;
};

/**
 * Black parser wrapper.
 */
class BlackParserImpl : public BaseParser {
public:
    BlackParserImpl()
        : BaseParser("black_text",
                     "Black Parser",
                     ParserCategory::LINTING,
                     "Python Black formatter output",
                     ParserPriority::MEDIUM) {
        addAlias("black");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    BlackParser parser_;
};

/**
 * YAPF parser wrapper.
 */
class YapfParserImpl : public BaseParser {
public:
    YapfParserImpl()
        : BaseParser("yapf_text",
                     "YAPF Parser",
                     ParserCategory::LINTING,
                     "Python YAPF formatter output",
                     ParserPriority::VERY_HIGH) {
        addAlias("yapf");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    YapfTextParser parser_;
};

/**
 * Clang-Tidy parser wrapper.
 */
class ClangTidyParserImpl : public BaseParser {
public:
    ClangTidyParserImpl()
        : BaseParser("clang_tidy_text",
                     "Clang-Tidy Parser",
                     ParserCategory::LINTING,
                     "LLVM Clang-Tidy C++ linter output",
                     ParserPriority::HIGH) {
        addAlias("clang_tidy");
        addAlias("clang-tidy");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    ClangTidyParser parser_;
};

/**
 * Autopep8 text parser wrapper.
 */
class Autopep8ParserImpl : public BaseParser {
public:
    Autopep8ParserImpl()
        : BaseParser("autopep8_text",
                     "Autopep8 Parser",
                     ParserCategory::LINTING,
                     "Python autopep8 formatter output",
                     ParserPriority::HIGH) {
        addAlias("autopep8");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    Autopep8TextParser parser_;
};

/**
 * Register all linting tool parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(LintingTools);

void RegisterLintingToolsParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<PylintParserImpl>());
    registry.registerParser(make_uniq<MypyParserImpl>());
    registry.registerParser(make_uniq<Flake8ParserImpl>());
    registry.registerParser(make_uniq<BlackParserImpl>());
    registry.registerParser(make_uniq<YapfParserImpl>());
    registry.registerParser(make_uniq<ClangTidyParserImpl>());
    registry.registerParser(make_uniq<Autopep8ParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(LintingTools);

} // namespace log_parsers
} // namespace duckdb
