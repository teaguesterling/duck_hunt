#include "read_workflow_logs_function.hpp"
#include "read_test_results_function.hpp"
#include "workflow_engine_interface.hpp"

// Include parser implementations for static build
#include "parsers/workflow_engines/github_actions_parser.hpp"
#include "parsers/workflow_engines/gitlab_ci_parser.hpp"
#include "parsers/workflow_engines/jenkins_parser.hpp"
#include "parsers/workflow_engines/docker_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include <regex>
#include <sstream>
#include <iostream>
#include <fstream>

namespace duckdb {

// Convert workflow format enum to string
std::string WorkflowLogFormatToString(WorkflowLogFormat format) {
    switch (format) {
        case WorkflowLogFormat::AUTO: return "auto";
        case WorkflowLogFormat::GITHUB_ACTIONS: return "github_actions";
        case WorkflowLogFormat::GITLAB_CI: return "gitlab_ci";
        case WorkflowLogFormat::JENKINS: return "jenkins";
        case WorkflowLogFormat::DOCKER_BUILD: return "docker_build";
        case WorkflowLogFormat::UNKNOWN: return "unknown";
        default: return "unknown";
    }
}

// Convert string to workflow format enum
WorkflowLogFormat StringToWorkflowLogFormat(const std::string& format_str) {
    std::string lower_format = StringUtil::Lower(format_str);
    
    if (lower_format == "auto") return WorkflowLogFormat::AUTO;
    if (lower_format == "github_actions" || lower_format == "github") return WorkflowLogFormat::GITHUB_ACTIONS;
    if (lower_format == "gitlab_ci" || lower_format == "gitlab") return WorkflowLogFormat::GITLAB_CI;
    if (lower_format == "jenkins") return WorkflowLogFormat::JENKINS;
    if (lower_format == "docker_build" || lower_format == "docker") return WorkflowLogFormat::DOCKER_BUILD;
    
    return WorkflowLogFormat::UNKNOWN;
}

// Detect workflow log format from content
WorkflowLogFormat DetectWorkflowLogFormat(const std::string& content) {
    // GitHub Actions patterns
    if (content.find("##[group]") != std::string::npos ||
        content.find("##[endgroup]") != std::string::npos ||
        content.find("::group::") != std::string::npos ||
        content.find("::endgroup::") != std::string::npos ||
        content.find("Run actions/") != std::string::npos) {
        return WorkflowLogFormat::GITHUB_ACTIONS;
    }
    
    // GitLab CI patterns
    if (content.find("Running with gitlab-runner") != std::string::npos ||
        content.find("Preparing the \"docker\"") != std::string::npos ||
        content.find("$ docker run") != std::string::npos && content.find("gitlab") != std::string::npos ||
        content.find("Job succeeded") != std::string::npos && content.find("Pipeline #") != std::string::npos) {
        return WorkflowLogFormat::GITLAB_CI;
    }
    
    // Jenkins patterns
    if (content.find("Started by user") != std::string::npos ||
        content.find("Building in workspace") != std::string::npos ||
        content.find("Finished: SUCCESS") != std::string::npos ||
        content.find("Finished: FAILURE") != std::string::npos ||
        content.find("[Pipeline]") != std::string::npos) {
        return WorkflowLogFormat::JENKINS;
    }
    
    // Docker build patterns
    if (content.find("Step ") != std::string::npos && content.find("/") != std::string::npos ||
        content.find("Sending build context to Docker daemon") != std::string::npos ||
        content.find("Successfully built") != std::string::npos ||
        content.find("Successfully tagged") != std::string::npos ||
        content.find("COPY --from=") != std::string::npos) {
        return WorkflowLogFormat::DOCKER_BUILD;
    }
    
    return WorkflowLogFormat::UNKNOWN;
}


// Bind function for read_workflow_logs
unique_ptr<FunctionData> ReadWorkflowLogsBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names) {
    auto bind_data = make_uniq<ReadWorkflowLogsBindData>();
    
    // Get source parameter (required)
    if (input.inputs.empty()) {
        throw BinderException("read_workflow_logs requires at least one parameter (source)");
    }
    bind_data->source = input.inputs[0].ToString();
    
    // DEBUG: Force an exception to see if bind is being called
    // throw BinderException("DEBUG: Bind function was called with source: " + bind_data->source);
    
    // Get format parameter (optional, defaults to auto)
    if (input.inputs.size() > 1) {
        bind_data->format = StringToWorkflowLogFormat(input.inputs[1].ToString());
    } else {
        bind_data->format = WorkflowLogFormat::AUTO;
    }
    
    // Define return schema - includes all ValidationEvent fields plus workflow-specific ones
    return_types = {
        LogicalType::BIGINT,   // event_id
        LogicalType::VARCHAR,  // tool_name
        LogicalType::VARCHAR,  // event_type
        LogicalType::VARCHAR,  // file_path
        LogicalType::INTEGER,  // line_number
        LogicalType::INTEGER,  // column_number
        LogicalType::VARCHAR,  // function_name
        LogicalType::VARCHAR,  // status
        LogicalType::VARCHAR,  // severity
        LogicalType::VARCHAR,  // category
        LogicalType::VARCHAR,  // message
        LogicalType::VARCHAR,  // suggestion
        LogicalType::VARCHAR,  // error_code
        LogicalType::VARCHAR,  // test_name
        LogicalType::DOUBLE,   // execution_time
        LogicalType::VARCHAR,  // raw_output
        LogicalType::VARCHAR,  // structured_data
        LogicalType::VARCHAR,  // source_file
        LogicalType::VARCHAR,  // build_id
        LogicalType::VARCHAR,  // environment
        LogicalType::BIGINT,   // file_index
        LogicalType::VARCHAR,  // error_fingerprint
        LogicalType::DOUBLE,   // similarity_score
        LogicalType::BIGINT,   // pattern_id
        LogicalType::VARCHAR,  // root_cause_category
        
        // Phase 3C: Workflow-specific fields
        LogicalType::VARCHAR,  // workflow_name
        LogicalType::VARCHAR,  // job_name
        LogicalType::VARCHAR,  // step_name
        LogicalType::VARCHAR,  // workflow_run_id
        LogicalType::VARCHAR,  // job_id
        LogicalType::VARCHAR,  // step_id
        LogicalType::VARCHAR,  // workflow_status
        LogicalType::VARCHAR,  // job_status
        LogicalType::VARCHAR,  // step_status
        LogicalType::VARCHAR,  // started_at
        LogicalType::VARCHAR,  // completed_at
        LogicalType::DOUBLE,   // duration
        
        // Additional workflow-specific fields
        LogicalType::VARCHAR,  // workflow_type
        LogicalType::INTEGER,  // hierarchy_level
        LogicalType::VARCHAR,  // parent_id
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number", "column_number",
        "function_name", "status", "severity", "category", "message", "suggestion", 
        "error_code", "test_name", "execution_time", "raw_output", "structured_data",
        "source_file", "build_id", "environment", "file_index", "error_fingerprint",
        "similarity_score", "pattern_id", "root_cause_category",
        
        "workflow_name", "job_name", "step_name", "workflow_run_id", "job_id", "step_id",
        "workflow_status", "job_status", "step_status", "started_at", "completed_at", "duration",
        
        "workflow_type", "hierarchy_level", "parent_id"
    };
    
    return std::move(bind_data);
}

// Global initialization for read_workflow_logs
unique_ptr<GlobalTableFunctionState> ReadWorkflowLogsInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<ReadWorkflowLogsBindData>();
    auto global_state = make_uniq<ReadWorkflowLogsGlobalState>();

    // Read source content
    std::string content;

    try {
        // Try to read as file first
        content = ReadContentFromSource(bind_data.source);
    } catch (const IOException&) {
        // If file reading fails, treat source as direct content
        content = bind_data.source;
    }

    // Auto-detect format if needed
    WorkflowLogFormat format = bind_data.format;
    if (format == WorkflowLogFormat::AUTO) {
        format = DetectWorkflowLogFormat(content);
    }

    // Parse content using the workflow engine registry
    auto& registry = WorkflowEngineRegistry::getInstance();

    // Ensure parsers are registered (static build workaround)
    if (registry.getParserCount() == 0) {
        registry.registerParser(make_uniq<GitHubActionsParser>());
        registry.registerParser(make_uniq<GitLabCIParser>());
        registry.registerParser(make_uniq<JenkinsParser>());
        registry.registerParser(make_uniq<DockerParser>());
    }

    const WorkflowEngineParser* parser_ptr = nullptr;

    if (format == WorkflowLogFormat::AUTO) {
        parser_ptr = registry.findParser(content);
    } else {
        parser_ptr = registry.getParser(format);
    }

    if (parser_ptr) {
        // Parse using the found parser
        std::vector<WorkflowEvent> parsed_events = parser_ptr->parseWorkflowLogs(content);
        global_state->events = std::move(parsed_events);
    }

    return std::move(global_state);
}

// Local initialization for read_workflow_logs
unique_ptr<LocalTableFunctionState> ReadWorkflowLogsInitLocal(ExecutionContext &context, TableFunctionInitInput &input, 
                                                            GlobalTableFunctionState *global_state) {
    return make_uniq<ReadWorkflowLogsLocalState>();
}

// Main table function implementation
void ReadWorkflowLogsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &bind_data = data_p.bind_data->Cast<ReadWorkflowLogsBindData>();
    auto &global_state = data_p.global_state->Cast<ReadWorkflowLogsGlobalState>();
    auto &local_state = data_p.local_state->Cast<ReadWorkflowLogsLocalState>();
    
    idx_t current_row = local_state.chunk_offset;
    idx_t chunk_size = output.size();
    idx_t events_count = global_state.events.size();
    
    
    if (current_row >= events_count) {
        output.SetCardinality(0);
        return;
    }
    
    idx_t rows_to_output = std::min(chunk_size, events_count - current_row);
    
    for (idx_t i = 0; i < rows_to_output; i++) {
        const WorkflowEvent& event = global_state.events[current_row + i];
        const ValidationEvent& base = event.base_event;

        // Map all ValidationEvent fields from base_event
        output.SetValue(0, i, Value::BIGINT(base.event_id));
        output.SetValue(1, i, Value(base.tool_name));
        output.SetValue(2, i, Value(ValidationEventTypeToString(base.event_type)));
        output.SetValue(3, i, Value(base.file_path));
        output.SetValue(4, i, base.line_number == -1 ? Value() : Value::INTEGER(base.line_number));
        output.SetValue(5, i, base.column_number == -1 ? Value() : Value::INTEGER(base.column_number));
        output.SetValue(6, i, Value(base.function_name));
        output.SetValue(7, i, Value(ValidationEventStatusToString(base.status)));
        output.SetValue(8, i, Value(base.severity));
        output.SetValue(9, i, Value(base.category));
        output.SetValue(10, i, Value(base.message));
        output.SetValue(11, i, Value(base.suggestion));
        output.SetValue(12, i, Value(base.error_code));
        output.SetValue(13, i, Value(base.test_name));
        output.SetValue(14, i, Value::DOUBLE(base.execution_time));
        output.SetValue(15, i, Value(base.raw_output));
        output.SetValue(16, i, Value(base.structured_data));
        output.SetValue(17, i, Value(base.source_file));
        output.SetValue(18, i, Value(base.build_id));
        output.SetValue(19, i, Value(base.environment));
        output.SetValue(20, i, Value::BIGINT(base.file_index));
        output.SetValue(21, i, Value(base.error_fingerprint));
        output.SetValue(22, i, Value::DOUBLE(base.similarity_score));
        output.SetValue(23, i, Value::BIGINT(base.pattern_id));
        output.SetValue(24, i, Value(base.root_cause_category));

        // Map workflow-specific fields from base_event
        output.SetValue(25, i, Value(base.workflow_name));
        output.SetValue(26, i, Value(base.job_name));
        output.SetValue(27, i, Value(base.step_name));
        output.SetValue(28, i, Value(base.workflow_run_id));
        output.SetValue(29, i, Value(base.job_id));
        output.SetValue(30, i, Value(base.step_id));
        output.SetValue(31, i, Value(base.workflow_status));
        output.SetValue(32, i, Value(base.job_status));
        output.SetValue(33, i, Value(base.step_status));
        output.SetValue(34, i, Value(base.started_at));
        output.SetValue(35, i, Value(base.completed_at));
        output.SetValue(36, i, Value::DOUBLE(base.duration));

        // Map additional workflow fields from WorkflowEvent
        output.SetValue(37, i, Value(event.workflow_type));
        output.SetValue(38, i, Value::INTEGER(event.hierarchy_level));
        output.SetValue(39, i, Value(event.parent_id));
    }
    
    output.SetCardinality(rows_to_output);
    local_state.chunk_offset += rows_to_output;
}

// Create the table function
TableFunction GetReadWorkflowLogsFunction() {
    TableFunction function("read_workflow_logs", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                          ReadWorkflowLogsFunction, ReadWorkflowLogsBind, ReadWorkflowLogsInitGlobal, ReadWorkflowLogsInitLocal);
    
    return function;
}

} // namespace duckdb