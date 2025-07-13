#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "validation_event_types.hpp"

namespace duckdb {

// Enum for test result format detection
enum class TestResultFormat : uint8_t {
    UNKNOWN = 0,
    AUTO = 1,
    PYTEST_JSON = 2,
    GOTEST_JSON = 3,
    ESLINT_JSON = 4,
    PYTEST_TEXT = 5,
    MAKE_ERROR = 6,
    GENERIC_LINT = 7,
    DUCKDB_TEST = 8,
    RUBOCOP_JSON = 9,
    CARGO_TEST_JSON = 10,
    SWIFTLINT_JSON = 11,
    PHPSTAN_JSON = 12,
    SHELLCHECK_JSON = 13,
    STYLELINT_JSON = 14,
    CLIPPY_JSON = 15,
    MARKDOWNLINT_JSON = 16,
    YAMLLINT_JSON = 17,
    BANDIT_JSON = 18,
    SPOTBUGS_JSON = 19,
    KTLINT_JSON = 20,
    HADOLINT_JSON = 21,
    LINTR_JSON = 22,
    SQLFLUFF_JSON = 23,
    TFLINT_JSON = 24,
    KUBE_SCORE_JSON = 25,
    CMAKE_BUILD = 26,
    PYTHON_BUILD = 27,
    NODE_BUILD = 28,
    CARGO_BUILD = 29,
    MAVEN_BUILD = 30,
    GRADLE_BUILD = 31,
    MSBUILD = 32,
    JUNIT_TEXT = 33,
    VALGRIND = 34,
    GDB_LLDB = 35,
    RSPEC_TEXT = 36,
    MOCHA_CHAI_TEXT = 37,
    GTEST_TEXT = 38,
    NUNIT_XUNIT_TEXT = 39,
    PYLINT_TEXT = 40,
    FLAKE8_TEXT = 41,
    BLACK_TEXT = 42,
    MYPY_TEXT = 43,
    DOCKER_BUILD = 44,
    BAZEL_BUILD = 45,
    ISORT_TEXT = 46,
    BANDIT_TEXT = 47,
    AUTOPEP8_TEXT = 48,
    YAPF_TEXT = 49,
    COVERAGE_TEXT = 50,
    PYTEST_COV_TEXT = 51,
    GITHUB_ACTIONS_TEXT = 52,
    GITLAB_CI_TEXT = 53,
    JENKINS_TEXT = 54,
    DRONE_CI_TEXT = 55,
    TERRAFORM_TEXT = 56,
    ANSIBLE_TEXT = 57
};

// Bind data for read_test_results table function
struct ReadTestResultsBindData : public TableFunctionData {
    std::string source;
    TestResultFormat format;
    
    ReadTestResultsBindData() {}
};

// Global state for read_test_results table function
struct ReadTestResultsGlobalState : public GlobalTableFunctionState {
    std::vector<ValidationEvent> events;
    idx_t max_threads;
    
    ReadTestResultsGlobalState() : max_threads(1) {}
};

// Local state for read_test_results table function
struct ReadTestResultsLocalState : public LocalTableFunctionState {
    idx_t chunk_offset;
    
    ReadTestResultsLocalState() : chunk_offset(0) {}
};

// Format detection
TestResultFormat DetectTestResultFormat(const std::string& content);
std::string TestResultFormatToString(TestResultFormat format);
TestResultFormat StringToTestResultFormat(const std::string& str);

// Content reading utilities
std::string ReadContentFromSource(const std::string& source);
bool IsValidJSON(const std::string& content);

// Main table function
TableFunction GetReadTestResultsFunction();
TableFunction GetParseTestResultsFunction();

// Table function implementation for read_test_results (file-based)
unique_ptr<FunctionData> ReadTestResultsBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names);

unique_ptr<GlobalTableFunctionState> ReadTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input);

unique_ptr<LocalTableFunctionState> ReadTestResultsInitLocal(ExecutionContext &context, TableFunctionInitInput &input, 
                                                            GlobalTableFunctionState *global_state);

void ReadTestResultsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);

// Table function implementation for parse_test_results (string-based)
unique_ptr<FunctionData> ParseTestResultsBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names);

unique_ptr<GlobalTableFunctionState> ParseTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input);

unique_ptr<LocalTableFunctionState> ParseTestResultsInitLocal(ExecutionContext &context, TableFunctionInitInput &input, 
                                                             GlobalTableFunctionState *global_state);

void ParseTestResultsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);

// Helper function to populate DataChunk from ValidationEvents
void PopulateDataChunkFromEvents(DataChunk &output, const std::vector<ValidationEvent> &events, 
                                idx_t start_offset, idx_t chunk_size);

// Format-specific parsers
void ParsePytestJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParsePytestText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseDuckDBTestOutput(const std::string& content, std::vector<ValidationEvent>& events);
void ParseESLintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseGoTestJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseMakeErrors(const std::string& content, std::vector<ValidationEvent>& events);
void ParseGenericLint(const std::string& content, std::vector<ValidationEvent>& events);
void ParseRuboCopJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseCargoTestJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseSwiftLintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParsePHPStanJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseShellCheckJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseStylelintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseClippyJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseMarkdownlintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseYamllintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseBanditJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseSpotBugsJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseKtlintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseHadolintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseLintrJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseSqlfluffJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseTflintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseKubeScoreJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseCMakeBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParsePythonBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseNodeBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseCargoBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseMavenBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseGradleBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseMSBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseJUnitText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseValgrind(const std::string& content, std::vector<ValidationEvent>& events);
void ParseGdbLldb(const std::string& content, std::vector<ValidationEvent>& events);
void ParseRSpecText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseMochaChai(const std::string& content, std::vector<ValidationEvent>& events);
void ParseGoogleTest(const std::string& content, std::vector<ValidationEvent>& events);
void ParseNUnitXUnit(const std::string& content, std::vector<ValidationEvent>& events);
void ParsePylintText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseFlake8Text(const std::string& content, std::vector<ValidationEvent>& events);
void ParseBlackText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseMypyText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseDockerBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseBazelBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseIsortText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseBanditText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseAutopep8Text(const std::string& content, std::vector<ValidationEvent>& events);
void ParseYapfText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseCoverageText(const std::string& content, std::vector<ValidationEvent>& events);
void ParsePytestCovText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseGitHubActionsText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseGitLabCIText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseJenkinsText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseDroneCIText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseTerraformText(const std::string& content, std::vector<ValidationEvent>& events);
void ParseAnsibleText(const std::string& content, std::vector<ValidationEvent>& events);

} // namespace duckdb