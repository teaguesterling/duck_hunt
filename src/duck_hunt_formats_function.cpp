#include "include/duck_hunt_formats_function.hpp"
#include "duckdb/common/exception.hpp"
#include <vector>

namespace duckdb {

// Format metadata struct
struct FormatInfo {
    std::string format_name;
    std::string description;
    std::string category;
    std::string requires_extension;
    bool supports_workflow;
};

// All supported formats with metadata
static std::vector<FormatInfo> GetAllFormats() {
    return {
        // Meta formats
        {"auto", "Automatic format detection", "meta", "", false},
        {"regexp", "Custom regex pattern (use 'regexp:PATTERN')", "meta", "", false},

        // Test frameworks
        {"pytest_json", "pytest --json-report output", "test_framework", "", false},
        {"pytest_text", "pytest terminal output", "test_framework", "", false},
        {"gotest_json", "go test -json output", "test_framework", "", false},
        {"junit_xml", "JUnit XML test results", "test_framework", "webbed", false},
        {"junit_text", "JUnit text output", "test_framework", "", false},
        {"gtest_text", "Google Test output", "test_framework", "", false},
        {"rspec_text", "RSpec output", "test_framework", "", false},
        {"mocha_chai_text", "Mocha/Chai output", "test_framework", "", false},
        {"nunit_xunit_text", "NUnit/xUnit text output", "test_framework", "", false},
        {"nunit_xml", "NUnit XML results", "test_framework", "webbed", false},
        {"cargo_test_json", "Rust cargo test JSON", "test_framework", "", false},
        {"duckdb_test", "DuckDB test runner output", "test_framework", "", false},

        // Build systems
        {"make_error", "make/gcc build output", "build_system", "", false},
        {"cmake_build", "CMake build output", "build_system", "", false},
        {"cargo_build", "Rust cargo build output", "build_system", "", false},
        {"maven_build", "Maven build output", "build_system", "", false},
        {"gradle_build", "Gradle build output", "build_system", "", false},
        {"msbuild", "MSBuild output", "build_system", "", false},
        {"node_build", "npm/yarn/webpack output", "build_system", "", false},
        {"python_build", "Python build/pip output", "build_system", "", false},
        {"bazel_build", "Bazel build output", "build_system", "", false},
        {"docker_build", "Docker build output", "build_system", "", false},

        // Linting tools - JSON
        {"eslint_json", "ESLint JSON format", "linting_tool", "", false},
        {"clippy_json", "Rust clippy JSON", "linting_tool", "", false},
        {"shellcheck_json", "ShellCheck JSON", "linting_tool", "", false},
        {"rubocop_json", "RuboCop JSON", "linting_tool", "", false},
        {"phpstan_json", "PHPStan JSON", "linting_tool", "", false},
        {"stylelint_json", "Stylelint JSON", "linting_tool", "", false},
        {"markdownlint_json", "Markdownlint JSON", "linting_tool", "", false},
        {"yamllint_json", "yamllint JSON", "linting_tool", "", false},
        {"hadolint_json", "Hadolint (Dockerfile) JSON", "linting_tool", "", false},
        {"ktlint_json", "ktlint (Kotlin) JSON", "linting_tool", "", false},
        {"swiftlint_json", "SwiftLint JSON", "linting_tool", "", false},
        {"lintr_json", "lintr (R) JSON", "linting_tool", "", false},
        {"sqlfluff_json", "SQLFluff JSON", "linting_tool", "", false},
        {"tflint_json", "TFLint JSON", "linting_tool", "", false},
        {"spotbugs_json", "SpotBugs JSON", "linting_tool", "", false},
        {"checkstyle_xml", "Checkstyle XML", "linting_tool", "webbed", false},

        // Linting tools - Text
        {"pylint_text", "pylint output", "linting_tool", "", false},
        {"flake8_text", "flake8 output", "linting_tool", "", false},
        {"mypy_text", "mypy output", "linting_tool", "", false},
        {"black_text", "black formatter output", "linting_tool", "", false},
        {"clang_tidy_text", "clang-tidy output", "linting_tool", "", false},
        {"generic_lint", "Generic file:line:col: message format", "linting_tool", "", false},

        // Python tools
        {"autopep8_text", "autopep8 output", "python_tool", "", false},
        {"yapf_text", "yapf formatter output", "python_tool", "", false},
        {"isort_text", "isort output", "python_tool", "", false},
        {"coverage_text", "coverage.py output", "python_tool", "", false},
        {"pytest_cov_text", "pytest-cov output", "python_tool", "", false},

        // Security tools
        {"bandit_json", "Bandit security JSON", "security_tool", "", false},
        {"bandit_text", "Bandit security text", "security_tool", "", false},

        // CI/CD systems (workflow-aware)
        {"github_cli", "gh run view output", "ci_system", "", true},
        {"github_actions_text", "GitHub Actions log format", "ci_system", "", true},
        {"gitlab_ci_text", "GitLab CI log format", "ci_system", "", true},
        {"jenkins_text", "Jenkins log format", "ci_system", "", true},
        {"drone_ci_text", "Drone CI log format", "ci_system", "", true},

        // Infrastructure tools
        {"terraform_text", "Terraform output", "infrastructure_tool", "", false},
        {"ansible_text", "Ansible output", "infrastructure_tool", "", false},
        {"kube_score_json", "kube-score JSON", "infrastructure_tool", "", false},

        // Debugging tools
        {"valgrind", "Valgrind output", "debugging_tool", "", false},
        {"gdb_lldb", "GDB/LLDB output", "debugging_tool", "", false},
    };
}

// Bind data
struct DuckHuntFormatsBindData : public TableFunctionData {
    std::vector<FormatInfo> formats;

    DuckHuntFormatsBindData() : formats(GetAllFormats()) {}
};

// Global state
struct DuckHuntFormatsGlobalState : public GlobalTableFunctionState {
    idx_t current_idx;

    DuckHuntFormatsGlobalState() : current_idx(0) {}
};

static unique_ptr<FunctionData> DuckHuntFormatsBind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<string> &names) {
    // Define return schema
    return_types = {
        LogicalType::VARCHAR,  // format
        LogicalType::VARCHAR,  // description
        LogicalType::VARCHAR,  // category
        LogicalType::VARCHAR,  // requires_extension
        LogicalType::BOOLEAN   // supports_workflow
    };

    names = {
        "format",
        "description",
        "category",
        "requires_extension",
        "supports_workflow"
    };

    return make_uniq<DuckHuntFormatsBindData>();
}

static unique_ptr<GlobalTableFunctionState> DuckHuntFormatsInitGlobal(ClientContext &context,
                                                                       TableFunctionInitInput &input) {
    return make_uniq<DuckHuntFormatsGlobalState>();
}

static void DuckHuntFormatsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &bind_data = data_p.bind_data->Cast<DuckHuntFormatsBindData>();
    auto &state = data_p.global_state->Cast<DuckHuntFormatsGlobalState>();

    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE;

    while (state.current_idx < bind_data.formats.size() && count < max_count) {
        const auto &fmt = bind_data.formats[state.current_idx];

        output.SetValue(0, count, Value(fmt.format_name));
        output.SetValue(1, count, Value(fmt.description));
        output.SetValue(2, count, Value(fmt.category));
        output.SetValue(3, count, fmt.requires_extension.empty() ? Value() : Value(fmt.requires_extension));
        output.SetValue(4, count, Value::BOOLEAN(fmt.supports_workflow));

        state.current_idx++;
        count++;
    }

    output.SetCardinality(count);
}

TableFunction GetDuckHuntFormatsFunction() {
    TableFunction func("duck_hunt_formats", {}, DuckHuntFormatsFunction, DuckHuntFormatsBind,
                       DuckHuntFormatsInitGlobal);
    return func;
}

} // namespace duckdb
