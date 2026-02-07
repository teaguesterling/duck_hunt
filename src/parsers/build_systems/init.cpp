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

void RegisterBuildSystemsParsers(ParserRegistry &registry) {
	// Core build systems - Format: format_name, display_name, category, description, priority, aliases, groups
	registry.registerParser(make_uniq<DelegatingParser<MakeParser>>(
	    "make_error", "Make Error Parser", ParserCategory::BUILD_SYSTEM, "GNU Make build output with errors",
	    ParserPriority::HIGH, std::vector<std::string> {"make"}, std::vector<std::string> {"c_cpp", "build"}));

	registry.registerParser(make_uniq<DelegatingParser<CMakeParser>>(
	    "cmake_build", "CMake Parser", ParserCategory::BUILD_SYSTEM, "CMake build system output", ParserPriority::HIGH,
	    std::vector<std::string> {"cmake"}, std::vector<std::string> {"c_cpp", "build"}));

	registry.registerParser(make_uniq<DelegatingParser<BazelParser>>(
	    "bazel_build", "Bazel Parser", ParserCategory::BUILD_SYSTEM, "Google Bazel build system output",
	    ParserPriority::HIGH, std::vector<std::string> {"bazel"}, std::vector<std::string> {"c_cpp", "java", "build"}));

	// Java/JVM build systems
	registry.registerParser(make_uniq<DelegatingParser<MavenParser>>(
	    "maven_build", "Maven Parser", ParserCategory::BUILD_SYSTEM, "Apache Maven build output", ParserPriority::HIGH,
	    std::vector<std::string> {"maven", "mvn"}, std::vector<std::string> {"java", "build"}));

	registry.registerParser(make_uniq<DelegatingParser<GradleParser>>(
	    "gradle_build", "Gradle Parser", ParserCategory::BUILD_SYSTEM, "Gradle build system output",
	    ParserPriority::HIGH, std::vector<std::string> {"gradle"}, std::vector<std::string> {"java", "build"}));

	// Microsoft build systems
	registry.registerParser(make_uniq<DelegatingParser<MSBuildParser>>(
	    "msbuild", "MSBuild Parser", ParserCategory::BUILD_SYSTEM, "Microsoft MSBuild output", ParserPriority::HIGH,
	    std::vector<std::string> {}, std::vector<std::string> {"dotnet", "build"}));

	// Language-specific build systems
	registry.registerParser(make_uniq<DelegatingParser<CargoParser>>(
	    "cargo_build", "Cargo Parser", ParserCategory::BUILD_SYSTEM, "Rust Cargo build output", ParserPriority::HIGH,
	    std::vector<std::string> {"cargo"}, std::vector<std::string> {"rust", "build"}));

	registry.registerParser(make_uniq<DelegatingParser<NodeParser>>(
	    "node_build", "Node.js Parser", ParserCategory::BUILD_SYSTEM, "Node.js/npm build output", ParserPriority::HIGH,
	    std::vector<std::string> {"node", "npm"}, std::vector<std::string> {"javascript", "build"}));

	registry.registerParser(make_uniq<DelegatingParser<PythonBuildParser>>(
	    "python_build", "Python Build Parser", ParserCategory::BUILD_SYSTEM, "Python build/setup.py output",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"python", "build"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(BuildSystems);

} // namespace duckdb
