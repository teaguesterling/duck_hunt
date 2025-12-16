#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "make_parser.hpp"
#include "cmake_parser.hpp"
#include "maven_parser.hpp"
#include "gradle_parser.hpp"
#include "msbuild_parser.hpp"
#include "cargo_parser.hpp"
#include "node_parser.hpp"
#include "python_parser.hpp"
#include "bazel_parser.hpp"

namespace duckdb {
namespace log_parsers {

/**
 * Make/GCC/Clang parser wrapper.
 * Note: MakeParser already implements old IParser, so we delegate to it.
 */
class MakeParserImpl : public BaseParser {
public:
    MakeParserImpl()
        : BaseParser("make_error",
                     "Make/GCC/Clang Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Make build system with GCC/Clang errors",
                     ParserPriority::HIGH) {
        addAlias("make");
        addAlias("gcc");
        addAlias("clang");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    MakeParser parser_;
};

/**
 * CMake build parser wrapper.
 */
class CMakeParserImpl : public BaseParser {
public:
    CMakeParserImpl()
        : BaseParser("cmake_build",
                     "CMake Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "CMake configuration and build output",
                     ParserPriority::HIGH) {
        addAlias("cmake");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::CMakeParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::CMakeParser::ParseCMakeBuild(content, events);
        return events;
    }
};

/**
 * Maven build parser wrapper.
 */
class MavenParserImpl : public BaseParser {
public:
    MavenParserImpl()
        : BaseParser("maven_build",
                     "Maven Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Apache Maven build output",
                     ParserPriority::HIGH) {
        addAlias("maven");
        addAlias("mvn");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::MavenParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::MavenParser::ParseMavenBuild(content, events);
        return events;
    }
};

/**
 * Gradle build parser wrapper.
 */
class GradleParserImpl : public BaseParser {
public:
    GradleParserImpl()
        : BaseParser("gradle_build",
                     "Gradle Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Gradle build output",
                     ParserPriority::HIGH) {
        addAlias("gradle");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::GradleParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::GradleParser::ParseGradleBuild(content, events);
        return events;
    }
};

/**
 * MSBuild parser wrapper.
 */
class MSBuildParserImpl : public BaseParser {
public:
    MSBuildParserImpl()
        : BaseParser("msbuild",
                     "MSBuild Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Microsoft MSBuild/Visual Studio output",
                     ParserPriority::HIGH) {
        addAlias("visualstudio");
        addAlias("vs");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::MSBuildParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::MSBuildParser::ParseMSBuild(content, events);
        return events;
    }
};

/**
 * Cargo (Rust) build parser wrapper.
 */
class CargoParserImpl : public BaseParser {
public:
    CargoParserImpl()
        : BaseParser("cargo_build",
                     "Cargo Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Rust Cargo build output",
                     ParserPriority::HIGH) {
        addAlias("cargo");
        addAlias("rust");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::CargoParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::CargoParser::ParseCargoBuild(content, events);
        return events;
    }
};

/**
 * Node.js/npm build parser wrapper.
 */
class NodeParserImpl : public BaseParser {
public:
    NodeParserImpl()
        : BaseParser("node_build",
                     "Node.js Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Node.js/npm build output",
                     ParserPriority::HIGH) {
        addAlias("node");
        addAlias("npm");
        addAlias("yarn");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::NodeParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::NodeParser::ParseNodeBuild(content, events);
        return events;
    }
};

/**
 * Python build/pip parser wrapper.
 */
class PythonBuildParserImpl : public BaseParser {
public:
    PythonBuildParserImpl()
        : BaseParser("python_build",
                     "Python Build Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Python pip/setuptools build output",
                     ParserPriority::HIGH) {
        addAlias("pip");
        addAlias("setuptools");
    }

    bool canParse(const std::string& content) const override {
        return duck_hunt::PythonParser().CanParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        std::vector<ValidationEvent> events;
        duck_hunt::PythonParser::ParsePythonBuild(content, events);
        return events;
    }
};

/**
 * Bazel build parser wrapper.
 */
class BazelParserImpl : public BaseParser {
public:
    BazelParserImpl()
        : BaseParser("bazel_build",
                     "Bazel Parser",
                     ParserCategory::BUILD_SYSTEM,
                     "Bazel build and test output",
                     ParserPriority::HIGH) {
        addAlias("bazel");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    BazelParser parser_;
};

/**
 * Register all build system parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(BuildSystems);

void RegisterBuildSystemsParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<MakeParserImpl>());
    registry.registerParser(make_uniq<CMakeParserImpl>());
    registry.registerParser(make_uniq<MavenParserImpl>());
    registry.registerParser(make_uniq<GradleParserImpl>());
    registry.registerParser(make_uniq<MSBuildParserImpl>());
    registry.registerParser(make_uniq<CargoParserImpl>());
    registry.registerParser(make_uniq<NodeParserImpl>());
    registry.registerParser(make_uniq<PythonBuildParserImpl>());
    registry.registerParser(make_uniq<BazelParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(BuildSystems);

} // namespace log_parsers
} // namespace duckdb
