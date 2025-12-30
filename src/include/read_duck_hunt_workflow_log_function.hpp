#pragma once

#include "duckdb.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/function/table_function.hpp"
#include "validation_event_types.hpp"
#include <vector>
#include <string>

namespace duckdb {

// Enum for workflow log formats
enum class WorkflowLogFormat : uint8_t {
    AUTO = 0,
    GITHUB_ACTIONS = 1,
    GITLAB_CI = 2,
    JENKINS = 3,
    DOCKER_BUILD = 4,
    SPACK = 5,
    UNKNOWN = 255
};

// Forward declarations for workflow log parsers
struct WorkflowEvent {
    ValidationEvent base_event;           // All the standard validation event fields
    std::string workflow_type;            // Type of workflow system (github, gitlab, jenkins, docker)
    int32_t hierarchy_level;              // 0=workflow, 1=job, 2=step, 3=tool_output
    std::string parent_id;                // ID of the parent element in hierarchy
    
    WorkflowEvent() : hierarchy_level(0) {}
};

// Convert workflow format enum to/from string
std::string WorkflowLogFormatToString(WorkflowLogFormat format);
WorkflowLogFormat StringToWorkflowLogFormat(const std::string& format_str);

// Format detection for workflow logs
WorkflowLogFormat DetectWorkflowLogFormat(const std::string& content);

// Workflow log parsing functions
void ParseGitHubActionsLog(const std::string& content, std::vector<WorkflowEvent>& events);
void ParseGitLabCILog(const std::string& content, std::vector<WorkflowEvent>& events);
void ParseJenkinsLog(const std::string& content, std::vector<WorkflowEvent>& events);
void ParseDockerBuildLog(const std::string& content, std::vector<WorkflowEvent>& events);

// Bind data structure for read_duck_hunt_workflow_log
struct ReadDuckHuntWorkflowLogBindData : public TableFunctionData {
    std::string source;
    WorkflowLogFormat format;
    SeverityLevel severity_threshold;  // Minimum severity level to emit (default: DEBUG = include all)

    ReadDuckHuntWorkflowLogBindData() : severity_threshold(SeverityLevel::DEBUG) {}
};

// Global state for read_duck_hunt_workflow_log table function
struct ReadDuckHuntWorkflowLogGlobalState : public GlobalTableFunctionState {
    std::vector<WorkflowEvent> events;
    idx_t max_threads;

    ReadDuckHuntWorkflowLogGlobalState() : max_threads(1) {}
};

// Local state for read_duck_hunt_workflow_log table function
struct ReadDuckHuntWorkflowLogLocalState : public LocalTableFunctionState {
    idx_t chunk_offset;
    
    ReadDuckHuntWorkflowLogLocalState() : chunk_offset(0) {}
};

// Main table function (returns set with single-arg and two-arg overloads)
TableFunctionSet GetReadDuckHuntWorkflowLogFunction();

// Table function implementation
unique_ptr<FunctionData> ReadDuckHuntWorkflowLogBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names);

unique_ptr<GlobalTableFunctionState> ReadDuckHuntWorkflowLogInitGlobal(ClientContext &context, TableFunctionInitInput &input);

unique_ptr<LocalTableFunctionState> ReadDuckHuntWorkflowLogInitLocal(ExecutionContext &context, TableFunctionInitInput &input, 
                                                            GlobalTableFunctionState *global_state);

void ReadDuckHuntWorkflowLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);

} // namespace duckdb