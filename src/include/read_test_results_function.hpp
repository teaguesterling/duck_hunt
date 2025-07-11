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
    DUCKDB_TEST = 8
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

// Table function implementation
unique_ptr<FunctionData> ReadTestResultsBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names);

unique_ptr<GlobalTableFunctionState> ReadTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input);

unique_ptr<LocalTableFunctionState> ReadTestResultsInitLocal(ExecutionContext &context, TableFunctionInitInput &input, 
                                                            GlobalTableFunctionState *global_state);

void ReadTestResultsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);

// Helper function to populate DataChunk from ValidationEvents
void PopulateDataChunkFromEvents(DataChunk &output, const std::vector<ValidationEvent> &events, 
                                idx_t start_offset, idx_t chunk_size);

// Format-specific parsers
void ParsePytestJSON(const std::string& content, std::vector<ValidationEvent>& events);
void ParseDuckDBTestOutput(const std::string& content, std::vector<ValidationEvent>& events);

} // namespace duckdb