#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "eslint_json_parser.hpp"
#include "gotest_json_parser.hpp"
#include "rubocop_json_parser.hpp"
#include "cargo_test_json_parser.hpp"
#include "swiftlint_json_parser.hpp"
#include "phpstan_json_parser.hpp"
#include "shellcheck_json_parser.hpp"
#include "stylelint_json_parser.hpp"
#include "clippy_json_parser.hpp"
#include "markdownlint_json_parser.hpp"
#include "yamllint_json_parser.hpp"
#include "bandit_json_parser.hpp"
#include "spotbugs_json_parser.hpp"
#include "ktlint_json_parser.hpp"
#include "kubescore_json_parser.hpp"
#include "hadolint_json_parser.hpp"
#include "lintr_json_parser.hpp"
#include "sqlfluff_json_parser.hpp"
#include "tflint_json_parser.hpp"
#include "generic_lint_parser.hpp"

namespace duckdb {
namespace log_parsers {

// Helper macro to reduce boilerplate for JSON parser wrappers
#define DEFINE_JSON_PARSER_IMPL(ImplName, ParserClass, format_name, display_name, desc, ...) \
class ImplName : public BaseParser { \
public: \
    ImplName() \
        : BaseParser(format_name, display_name, ParserCategory::LINTING, desc, ParserPriority::VERY_HIGH) \
        { __VA_ARGS__ } \
    bool canParse(const std::string& content) const override { return parser_.canParse(content); } \
    std::vector<ValidationEvent> parse(const std::string& content) const override { return parser_.parse(content); } \
private: \
    ParserClass parser_; \
}

/**
 * ESLint JSON parser wrapper.
 */
class ESLintJSONParserImpl : public BaseParser {
public:
    ESLintJSONParserImpl()
        : BaseParser("eslint_json",
                     "ESLint JSON Parser",
                     ParserCategory::LINTING,
                     "ESLint JavaScript/TypeScript linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("eslint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    ESLintJSONParser parser_;
};

/**
 * GoTest JSON parser wrapper.
 */
class GoTestJSONParserImpl : public BaseParser {
public:
    GoTestJSONParserImpl()
        : BaseParser("gotest_json",
                     "Go Test JSON Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Go test JSON output (go test -json)",
                     ParserPriority::VERY_HIGH) {
        addAlias("gotest");
        addAlias("go_test");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    GoTestJSONParser parser_;
};

/**
 * RuboCop JSON parser wrapper.
 */
class RuboCopJSONParserImpl : public BaseParser {
public:
    RuboCopJSONParserImpl()
        : BaseParser("rubocop_json",
                     "RuboCop JSON Parser",
                     ParserCategory::LINTING,
                     "Ruby RuboCop linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("rubocop");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    RuboCopJSONParser parser_;
};

/**
 * Cargo Test JSON parser wrapper.
 */
class CargoTestJSONParserImpl : public BaseParser {
public:
    CargoTestJSONParserImpl()
        : BaseParser("cargo_test_json",
                     "Cargo Test JSON Parser",
                     ParserCategory::TEST_FRAMEWORK,
                     "Rust Cargo test JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("cargo_test");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    CargoTestJSONParser parser_;
};

/**
 * SwiftLint JSON parser wrapper.
 */
class SwiftLintJSONParserImpl : public BaseParser {
public:
    SwiftLintJSONParserImpl()
        : BaseParser("swiftlint_json",
                     "SwiftLint JSON Parser",
                     ParserCategory::LINTING,
                     "Swift SwiftLint linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("swiftlint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    SwiftLintJSONParser parser_;
};

/**
 * PHPStan JSON parser wrapper.
 */
class PHPStanJSONParserImpl : public BaseParser {
public:
    PHPStanJSONParserImpl()
        : BaseParser("phpstan_json",
                     "PHPStan JSON Parser",
                     ParserCategory::LINTING,
                     "PHP PHPStan static analyzer JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("phpstan");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    PHPStanJSONParser parser_;
};

/**
 * ShellCheck JSON parser wrapper.
 */
class ShellCheckJSONParserImpl : public BaseParser {
public:
    ShellCheckJSONParserImpl()
        : BaseParser("shellcheck_json",
                     "ShellCheck JSON Parser",
                     ParserCategory::LINTING,
                     "ShellCheck shell script linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("shellcheck");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    ShellCheckJSONParser parser_;
};

/**
 * Stylelint JSON parser wrapper.
 */
class StylelintJSONParserImpl : public BaseParser {
public:
    StylelintJSONParserImpl()
        : BaseParser("stylelint_json",
                     "Stylelint JSON Parser",
                     ParserCategory::LINTING,
                     "Stylelint CSS linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("stylelint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    StylelintJSONParser parser_;
};

/**
 * Clippy JSON parser wrapper.
 */
class ClippyJSONParserImpl : public BaseParser {
public:
    ClippyJSONParserImpl()
        : BaseParser("clippy_json",
                     "Clippy JSON Parser",
                     ParserCategory::LINTING,
                     "Rust Clippy linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("clippy");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    ClippyJSONParser parser_;
};

/**
 * Markdownlint JSON parser wrapper.
 */
class MarkdownlintJSONParserImpl : public BaseParser {
public:
    MarkdownlintJSONParserImpl()
        : BaseParser("markdownlint_json",
                     "Markdownlint JSON Parser",
                     ParserCategory::LINTING,
                     "Markdownlint Markdown linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("markdownlint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    MarkdownlintJSONParser parser_;
};

/**
 * Yamllint JSON parser wrapper.
 */
class YamllintJSONParserImpl : public BaseParser {
public:
    YamllintJSONParserImpl()
        : BaseParser("yamllint_json",
                     "Yamllint JSON Parser",
                     ParserCategory::LINTING,
                     "Yamllint YAML linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("yamllint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    YamllintJSONParser parser_;
};

/**
 * Bandit JSON parser wrapper.
 */
class BanditJSONParserImpl : public BaseParser {
public:
    BanditJSONParserImpl()
        : BaseParser("bandit_json",
                     "Bandit JSON Parser",
                     ParserCategory::LINTING,
                     "Python Bandit security linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("bandit");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    BanditJSONParser parser_;
};

/**
 * SpotBugs JSON parser wrapper.
 */
class SpotBugsJSONParserImpl : public BaseParser {
public:
    SpotBugsJSONParserImpl()
        : BaseParser("spotbugs_json",
                     "SpotBugs JSON Parser",
                     ParserCategory::LINTING,
                     "Java SpotBugs static analyzer JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("spotbugs");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    SpotBugsJSONParser parser_;
};

/**
 * Ktlint JSON parser wrapper.
 */
class KtlintJSONParserImpl : public BaseParser {
public:
    KtlintJSONParserImpl()
        : BaseParser("ktlint_json",
                     "Ktlint JSON Parser",
                     ParserCategory::LINTING,
                     "Kotlin ktlint linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("ktlint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    KtlintJSONParser parser_;
};

/**
 * KubeScore JSON parser wrapper.
 */
class KubeScoreJSONParserImpl : public BaseParser {
public:
    KubeScoreJSONParserImpl()
        : BaseParser("kube_score_json",
                     "KubeScore JSON Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "Kubernetes kube-score analyzer JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("kubescore");
        addAlias("kube_score");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    KubeScoreJSONParser parser_;
};

/**
 * Hadolint JSON parser wrapper.
 */
class HadolintJSONParserImpl : public BaseParser {
public:
    HadolintJSONParserImpl()
        : BaseParser("hadolint_json",
                     "Hadolint JSON Parser",
                     ParserCategory::LINTING,
                     "Dockerfile Hadolint linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("hadolint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    HadolintJSONParser parser_;
};

/**
 * Lintr JSON parser wrapper.
 */
class LintrJSONParserImpl : public BaseParser {
public:
    LintrJSONParserImpl()
        : BaseParser("lintr_json",
                     "Lintr JSON Parser",
                     ParserCategory::LINTING,
                     "R lintr linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("lintr");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    LintrJSONParser parser_;
};

/**
 * SQLFluff JSON parser wrapper.
 */
class SqlfluffJSONParserImpl : public BaseParser {
public:
    SqlfluffJSONParserImpl()
        : BaseParser("sqlfluff_json",
                     "SQLFluff JSON Parser",
                     ParserCategory::LINTING,
                     "SQL SQLFluff linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("sqlfluff");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    SqlfluffJSONParser parser_;
};

/**
 * TFLint JSON parser wrapper.
 */
class TflintJSONParserImpl : public BaseParser {
public:
    TflintJSONParserImpl()
        : BaseParser("tflint_json",
                     "TFLint JSON Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "Terraform TFLint linter JSON output",
                     ParserPriority::VERY_HIGH) {
        addAlias("tflint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    TflintJSONParser parser_;
};

/**
 * Generic Lint parser wrapper.
 * Handles generic lint format: file:line:column: level: message
 */
class GenericLintParserImpl : public BaseParser {
public:
    GenericLintParserImpl()
        : BaseParser("generic_lint",
                     "Generic Lint Parser",
                     ParserCategory::LINTING,
                     "Generic lint format (file:line:col: level: message)",
                     ParserPriority::LOW) {
        addAlias("lint");
    }

    bool canParse(const std::string& content) const override {
        return parser_.CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        parser_.Parse(content, events);
        return events;
    }

private:
    duck_hunt::GenericLintParser parser_;
};

/**
 * Register all tool output parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(ToolOutputs);

void RegisterToolOutputsParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<ESLintJSONParserImpl>());
    registry.registerParser(make_uniq<GoTestJSONParserImpl>());
    registry.registerParser(make_uniq<RuboCopJSONParserImpl>());
    registry.registerParser(make_uniq<CargoTestJSONParserImpl>());
    registry.registerParser(make_uniq<SwiftLintJSONParserImpl>());
    registry.registerParser(make_uniq<PHPStanJSONParserImpl>());
    registry.registerParser(make_uniq<ShellCheckJSONParserImpl>());
    registry.registerParser(make_uniq<StylelintJSONParserImpl>());
    registry.registerParser(make_uniq<ClippyJSONParserImpl>());
    registry.registerParser(make_uniq<MarkdownlintJSONParserImpl>());
    registry.registerParser(make_uniq<YamllintJSONParserImpl>());
    registry.registerParser(make_uniq<BanditJSONParserImpl>());
    registry.registerParser(make_uniq<SpotBugsJSONParserImpl>());
    registry.registerParser(make_uniq<KtlintJSONParserImpl>());
    registry.registerParser(make_uniq<KubeScoreJSONParserImpl>());
    registry.registerParser(make_uniq<HadolintJSONParserImpl>());
    registry.registerParser(make_uniq<LintrJSONParserImpl>());
    registry.registerParser(make_uniq<SqlfluffJSONParserImpl>());
    registry.registerParser(make_uniq<TflintJSONParserImpl>());
    registry.registerParser(make_uniq<GenericLintParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(ToolOutputs);

} // namespace log_parsers
} // namespace duckdb
