#pragma once

#include "../include/validation_event_types.hpp"
#include "../core/parser_registry.hpp"
#include "../core/format_detector.hpp"

namespace duckdb {

/**
 * New modular implementation of read_test_results table function.
 * Uses the parser registry system for clean, extensible parsing.
 */
class NewReadTestResultsFunction {
public:
    static void RegisterFunction(DatabaseInstance& db);
    
private:
    static void ReadTestResultsBind(ClientContext& context, TableFunctionBindInput& input, 
                                   vector<LogicalType>& return_types, vector<string>& names);
    
    static unique_ptr<FunctionData> ReadTestResultsInit(ClientContext& context, 
                                                        TableFunctionInitInput& input);
    
    static unique_ptr<GlobalTableFunctionState> ReadTestResultsInitGlobal(
        ClientContext& context, TableFunctionInitInput& input);
    
    static void ReadTestResultsFunction(ClientContext& context, TableFunctionInput& data, 
                                       DataChunk& output);
};

/**
 * State for the new table function
 */
struct NewReadTestResultsGlobalState : public GlobalTableFunctionState {
    std::vector<ValidationEvent> events;
    idx_t current_row = 0;
    ParserRegistry& registry;
    FormatDetector detector;
    
    NewReadTestResultsGlobalState() 
        : registry(ParserRegistry::getInstance()), detector(registry) {}
};

/**
 * Bind data for the new table function
 */
struct NewReadTestResultsBindData : public TableFunctionData {
    string file_path;
    string format_str;
    TestResultFormat format;
    bool ignore_errors = false;
    
    NewReadTestResultsBindData(const string& path, const string& fmt) 
        : file_path(path), format_str(fmt) {}
};

} // namespace duckdb