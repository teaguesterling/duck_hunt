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
    ANSIBLE_TEXT = 57,
    GITHUB_CLI = 58,
    CLANG_TIDY_TEXT = 59,
    REGEXP = 60,  // Dynamic regexp pattern
    // XML-based formats (require webbed extension)
    JUNIT_XML = 61,
    NUNIT_XML = 62,
    CHECKSTYLE_XML = 63,
    // Cross-language structured formats
    JSONL = 64,
    LOGFMT = 65,
    // Web access and system logs
    SYSLOG = 66,
    APACHE_ACCESS = 67,
    NGINX_ACCESS = 68,
    // Cloud provider logs
    AWS_CLOUDTRAIL = 69,
    GCP_CLOUD_LOGGING = 70,
    AZURE_ACTIVITY = 71,
    // Application logging formats
    PYTHON_LOGGING = 72,
    LOG4J = 73,
    LOGRUS = 74,
    // Infrastructure formats
    IPTABLES = 75,
    PF_FIREWALL = 76,
    CISCO_ASA = 77,
    VPC_FLOW = 78,
    KUBERNETES = 79,
    WINDOWS_EVENT = 80,
    AUDITD = 81,
    S3_ACCESS = 82,
    // Additional application logging formats
    WINSTON = 83,
    PINO = 84,
    BUNYAN = 85,
    SERILOG = 86,
    NLOG = 87,
    RUBY_LOGGER = 88,
    RAILS_LOG = 89,
    // System tracing formats
    STRACE = 90
};

// Bind data for read_duck_hunt_log table function
struct ReadDuckHuntLogBindData : public TableFunctionData {
    std::string source;
    TestResultFormat format;
    std::string regexp_pattern;  // For REGEXP format: stores the user-provided pattern

    ReadDuckHuntLogBindData() {}
};

// Global state for read_duck_hunt_log table function
struct ReadDuckHuntLogGlobalState : public GlobalTableFunctionState {
    std::vector<ValidationEvent> events;
    idx_t max_threads;
    
    ReadDuckHuntLogGlobalState() : max_threads(1) {}
};

// Local state for read_duck_hunt_log table function
struct ReadDuckHuntLogLocalState : public LocalTableFunctionState {
    idx_t chunk_offset;
    
    ReadDuckHuntLogLocalState() : chunk_offset(0) {}
};

// Format detection
TestResultFormat DetectTestResultFormat(const std::string& content);
std::string TestResultFormatToString(TestResultFormat format);
TestResultFormat StringToTestResultFormat(const std::string& str);

// Content reading utilities
std::string ReadContentFromSource(ClientContext& context, const std::string& source);
bool IsValidJSON(const std::string& content);

// Phase 3A: Multi-file processing utilities
std::vector<std::string> GetFilesFromPattern(ClientContext& context, const std::string& pattern);
std::vector<std::string> GetGlobFiles(ClientContext& context, const std::string& pattern);
void ProcessMultipleFiles(ClientContext& context, const std::vector<std::string>& files, 
                         TestResultFormat format, std::vector<ValidationEvent>& events);
std::string ExtractBuildIdFromPath(const std::string& file_path);
std::string ExtractEnvironmentFromPath(const std::string& file_path);

// Main table function
TableFunction GetReadDuckHuntLogFunction();
TableFunction GetParseDuckHuntLogFunction();

// Table function implementation for read_duck_hunt_log (file-based)
unique_ptr<FunctionData> ReadDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names);

unique_ptr<GlobalTableFunctionState> ReadDuckHuntLogInitGlobal(ClientContext &context, TableFunctionInitInput &input);

unique_ptr<LocalTableFunctionState> ReadDuckHuntLogInitLocal(ExecutionContext &context, TableFunctionInitInput &input, 
                                                            GlobalTableFunctionState *global_state);

void ReadDuckHuntLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);

// Table function implementation for parse_duck_hunt_log (string-based)
unique_ptr<FunctionData> ParseDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names);

unique_ptr<GlobalTableFunctionState> ParseDuckHuntLogInitGlobal(ClientContext &context, TableFunctionInitInput &input);

unique_ptr<LocalTableFunctionState> ParseDuckHuntLogInitLocal(ExecutionContext &context, TableFunctionInitInput &input, 
                                                             GlobalTableFunctionState *global_state);

void ParseDuckHuntLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);

// Helper function to populate DataChunk from ValidationEvents
void PopulateDataChunkFromEvents(DataChunk &output, const std::vector<ValidationEvent> &events, 
                                idx_t start_offset, idx_t chunk_size);

// Dynamic regexp parser
void ParseWithRegexp(const std::string& content, const std::string& pattern, std::vector<ValidationEvent>& events);

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
void ParseHadolintJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseLintrJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseSqlfluffJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseTflintJSON(const std::string& content, std::vector<ValidationEvent>& events);
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
void ParseBazelBuild(const std::string& content, std::vector<ValidationEvent>& events);
void ParseAutopep8Text(const std::string& content, std::vector<ValidationEvent>& events);
void ParseCoverageText(const std::string& content, std::vector<ValidationEvent>& events);

} // namespace duckdb