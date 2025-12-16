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

/**
 * Register all build system parsers with the registry.
 * All parsers now use the DelegatingParser template for IParser-compliant implementations.
 */
DECLARE_PARSER_CATEGORY(BuildSystems);

void RegisterBuildSystemsParsers(ParserRegistry& registry) {
    // Core build systems
    registry.registerParser(make_uniq<DelegatingParser<MakeParser>>());
    registry.registerParser(make_uniq<DelegatingParser<CMakeParser>>());
    registry.registerParser(make_uniq<DelegatingParser<BazelParser>>());

    // Java/JVM build systems
    registry.registerParser(make_uniq<DelegatingParser<MavenParser>>());
    registry.registerParser(make_uniq<DelegatingParser<GradleParser>>());

    // Microsoft build systems
    registry.registerParser(make_uniq<DelegatingParser<MSBuildParser>>());

    // Language-specific build systems
    registry.registerParser(make_uniq<DelegatingParser<CargoParser>>());
    registry.registerParser(make_uniq<DelegatingParser<NodeParser>>());
    registry.registerParser(make_uniq<DelegatingParser<PythonBuildParser>>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(BuildSystems);


} // namespace duckdb
