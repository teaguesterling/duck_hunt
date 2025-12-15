#pragma once

#include "include/validation_event_types.hpp"
#include "core/parser_registry.hpp"
#include "core/format_detector.hpp"

namespace duckdb {

/**
 * New modular implementation of read_duck_hunt_log table function.
 * Uses the parser registry system for clean, extensible parsing.
 */
class NewReadDuckHuntLogFunction {
public:
    static void RegisterFunction(ExtensionLoader &loader);
    
private:
    static void ReadDuckHuntLogBind(ClientContext& context, TableFunctionBindInput& input, 
                                   vector<LogicalType>& return_types, vector<string>& names);
    
    static unique_ptr<FunctionData> ReadDuckHuntLogInit(ClientContext& context, 
                                                        TableFunctionInitInput& input);
    
    static unique_ptr<GlobalTableFunctionState> ReadDuckHuntLogInitGlobal(
        ClientContext& context, TableFunctionInitInput& input);
    
    static void ReadDuckHuntLogFunction(ClientContext& context, TableFunctionInput& data, 
                                       DataChunk& output);
};

/**
 * State for the new table function
 */
struct NewReadDuckHuntLogGlobalState : public GlobalTableFunctionState {
    std::vector<ValidationEvent> events;
    idx_t current_row = 0;
    ParserRegistry& registry;
    FormatDetector detector;
    
    NewReadDuckHuntLogGlobalState() 
        : registry(ParserRegistry::getInstance()), detector(registry) {}
};

/**
 * Bind data for the new table function
 */
struct NewReadDuckHuntLogBindData : public TableFunctionData {
    string file_path;
    string format_str;
    TestResultFormat format;
    bool ignore_errors = false;
    
    NewReadDuckHuntLogBindData(const string& path, const string& fmt) 
        : file_path(path), format_str(fmt) {}
};

} // namespace duckdb