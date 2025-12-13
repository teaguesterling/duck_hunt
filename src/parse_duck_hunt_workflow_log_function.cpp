#include "parse_duck_hunt_workflow_log_function.hpp"
#include "workflow_engine_interface.hpp"

// Include parser implementations for static build
#include "parsers/workflow_engines/github_actions_parser.hpp"
#include "parsers/workflow_engines/gitlab_ci_parser.hpp"
#include "parsers/workflow_engines/jenkins_parser.hpp"
#include "parsers/workflow_engines/docker_parser.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

// Convert string to workflow format enum
WorkflowLogFormat StringToWorkflowLogFormatForParse(const std::string& format_str) {
    std::string lower_format = StringUtil::Lower(format_str);
    
    if (lower_format == "auto") return WorkflowLogFormat::AUTO;
    if (lower_format == "github_actions" || lower_format == "github") return WorkflowLogFormat::GITHUB_ACTIONS;
    if (lower_format == "gitlab_ci" || lower_format == "gitlab") return WorkflowLogFormat::GITLAB_CI;
    if (lower_format == "jenkins") return WorkflowLogFormat::JENKINS;
    if (lower_format == "docker_build" || lower_format == "docker") return WorkflowLogFormat::DOCKER_BUILD;
    
    return WorkflowLogFormat::UNKNOWN;
}

// Parse workflow logs from string content
std::vector<WorkflowEvent> ParseDuckHuntWorkflowLogFromString(const std::string& content, const std::string& format_str) {
    try {
        if (content.empty()) {
            return {};
        }
        
        WorkflowLogFormat format = StringToWorkflowLogFormatForParse(format_str);
        
        // Use workflow engine registry
        auto& registry = WorkflowEngineRegistry::getInstance();
        
        // Ensure parsers are registered (static build workaround)
        if (registry.getParserCount() == 0) {
            registry.registerParser(make_uniq<GitHubActionsParser>());
            registry.registerParser(make_uniq<GitLabCIParser>());
            registry.registerParser(make_uniq<JenkinsParser>());
            registry.registerParser(make_uniq<DockerParser>());
        }
        
        const WorkflowEngineParser* parser = nullptr;
    
        if (format == WorkflowLogFormat::AUTO) {
            parser = registry.findParser(content);
        } else {
            parser = registry.getParser(format);
        }
    
        if (parser) {
            return parser->parseWorkflowLog(content);
        }
        
        return {};
        
    } catch (const std::exception& e) {
        // If parsing fails, create an error event
        std::vector<WorkflowEvent> error_events;
        WorkflowEvent error_event;
        
        error_event.base_event.event_id = 1;
        error_event.base_event.tool_name = "parse_duck_hunt_workflow_log";
        error_event.base_event.message = std::string("Parse error: ") + e.what();
        error_event.base_event.raw_output = content.substr(0, std::min(size_t(200), content.length()));
        error_event.base_event.event_type = ValidationEventType::SUMMARY;
        error_event.base_event.status = ValidationEventStatus::ERROR;
        error_event.base_event.severity = "error";
        
        error_event.workflow_type = "error";
        error_event.hierarchy_level = 1;
        error_event.parent_id = "parse_error";
        
        error_events.push_back(error_event);
        return error_events;
    }
}

struct ParseDuckHuntWorkflowLogBindData : public TableFunctionData {
    std::vector<WorkflowEvent> events;
};

struct ParseDuckHuntWorkflowLogGlobalState : public GlobalTableFunctionState {
    idx_t position = 0;
};

unique_ptr<FunctionData> ParseDuckHuntWorkflowLogBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
    auto result = make_uniq<ParseDuckHuntWorkflowLogBindData>();
    
    if (input.inputs.empty()) {
        throw BinderException("parse_duck_hunt_workflow_log requires at least one parameter (content)");
    }
    
    std::string content = input.inputs[0].ToString();
    std::string format = "auto";

    if (input.inputs.size() > 1) {
        format = input.inputs[1].ToString();

        // Check for unknown format
        WorkflowLogFormat fmt = StringToWorkflowLogFormatForParse(format);
        if (fmt == WorkflowLogFormat::UNKNOWN) {
            throw BinderException("Unknown workflow format: '" + format + "'. Use 'auto' for auto-detection. Supported: github_actions, gitlab_ci, jenkins, docker_build.");
        }
    }

    // Parse the workflow logs
    result->events = ParseDuckHuntWorkflowLogFromString(content, format);
    
    // Define return schema - same as read_duck_hunt_workflow_log
    return_types = {
        LogicalType::BIGINT,   // event_id
        LogicalType::VARCHAR,  // tool_name
        LogicalType::VARCHAR,  // event_type
        LogicalType::VARCHAR,  // file_path
        LogicalType::BIGINT,   // line_number
        LogicalType::BIGINT,   // column_number
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
        // Log line tracking
        LogicalType::INTEGER,  // log_line_start
        LogicalType::INTEGER,  // log_line_end
        LogicalType::VARCHAR,  // source_file
        LogicalType::VARCHAR,  // build_id
        LogicalType::VARCHAR,  // environment
        LogicalType::BIGINT,   // file_index
        LogicalType::VARCHAR,  // error_fingerprint
        LogicalType::DOUBLE,   // similarity_score
        LogicalType::BIGINT,   // pattern_id
        LogicalType::VARCHAR,  // root_cause_category

        // Workflow-specific fields
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
        
        // Additional workflow fields
        LogicalType::VARCHAR,  // workflow_type
        LogicalType::INTEGER,  // hierarchy_level
        LogicalType::VARCHAR   // parent_id
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number", "column_number",
        "function_name", "status", "severity", "category", "message", "suggestion",
        "error_code", "test_name", "execution_time", "raw_output", "structured_data",
        // Log line tracking
        "log_line_start", "log_line_end",
        "source_file", "build_id", "environment", "file_index", "error_fingerprint",
        "similarity_score", "pattern_id", "root_cause_category", "workflow_name",
        "job_name", "step_name", "workflow_run_id", "job_id", "step_id",
        "workflow_status", "job_status", "step_status", "started_at", "completed_at",
        "duration", "workflow_type", "hierarchy_level", "parent_id"
    };
    
    return std::move(result);
}

unique_ptr<GlobalTableFunctionState> ParseDuckHuntWorkflowLogInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    return make_uniq<ParseDuckHuntWorkflowLogGlobalState>();
}

void ParseDuckHuntWorkflowLogFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
    auto &bind_data = data.bind_data->Cast<ParseDuckHuntWorkflowLogBindData>();
    auto &gstate = data.global_state->Cast<ParseDuckHuntWorkflowLogGlobalState>();
    
    idx_t count = 0;
    idx_t chunk_size = output.size();
    
    while (count < chunk_size && gstate.position < bind_data.events.size()) {
        const WorkflowEvent& event = bind_data.events[gstate.position];
        
        // Map all fields
        output.SetValue(0, count, Value::BIGINT(event.base_event.event_id));
        output.SetValue(1, count, Value(event.base_event.tool_name)); 
        output.SetValue(2, count, Value(ValidationEventTypeToString(event.base_event.event_type)));
        output.SetValue(3, count, Value(event.base_event.file_path));
        output.SetValue(4, count, Value::BIGINT(event.base_event.line_number));
        output.SetValue(5, count, Value::BIGINT(event.base_event.column_number));
        output.SetValue(6, count, Value(event.base_event.function_name));
        output.SetValue(7, count, Value(ValidationEventStatusToString(event.base_event.status)));
        output.SetValue(8, count, Value(event.base_event.severity));
        output.SetValue(9, count, Value(event.base_event.category));
        output.SetValue(10, count, Value(event.base_event.message));
        output.SetValue(11, count, Value(event.base_event.suggestion));
        output.SetValue(12, count, Value(event.base_event.error_code));
        output.SetValue(13, count, Value(event.base_event.test_name));
        output.SetValue(14, count, Value::DOUBLE(event.base_event.execution_time));
        output.SetValue(15, count, Value(event.base_event.raw_output));
        output.SetValue(16, count, Value(event.base_event.structured_data));
        // Log line tracking
        output.SetValue(17, count, event.base_event.log_line_start == -1 ? Value() : Value::INTEGER(event.base_event.log_line_start));
        output.SetValue(18, count, event.base_event.log_line_end == -1 ? Value() : Value::INTEGER(event.base_event.log_line_end));
        output.SetValue(19, count, Value(event.base_event.source_file));
        output.SetValue(20, count, Value(event.base_event.build_id));
        output.SetValue(21, count, Value(event.base_event.environment));
        output.SetValue(22, count, Value::BIGINT(event.base_event.file_index));
        output.SetValue(23, count, Value(event.base_event.error_fingerprint));
        output.SetValue(24, count, Value::DOUBLE(event.base_event.similarity_score));
        output.SetValue(25, count, Value::BIGINT(event.base_event.pattern_id));
        output.SetValue(26, count, Value(event.base_event.root_cause_category));

        // Workflow-specific fields
        output.SetValue(27, count, Value(event.base_event.workflow_name));
        output.SetValue(28, count, Value(event.base_event.job_name));
        output.SetValue(29, count, Value(event.base_event.step_name));
        output.SetValue(30, count, Value(event.base_event.workflow_run_id));
        output.SetValue(31, count, Value(event.base_event.job_id));
        output.SetValue(32, count, Value(event.base_event.step_id));
        output.SetValue(33, count, Value(event.base_event.workflow_status));
        output.SetValue(34, count, Value(event.base_event.job_status));
        output.SetValue(35, count, Value(event.base_event.step_status));
        output.SetValue(36, count, Value(event.base_event.started_at));
        output.SetValue(37, count, Value(event.base_event.completed_at));
        output.SetValue(38, count, Value::DOUBLE(event.base_event.duration));

        // Additional workflow fields
        output.SetValue(39, count, Value(event.workflow_type));
        output.SetValue(40, count, Value::INTEGER(event.hierarchy_level));
        output.SetValue(41, count, Value(event.parent_id));
        
        count++;
        gstate.position++;
    }
    
    output.SetCardinality(count);
}

TableFunction GetParseDuckHuntWorkflowLogFunction() {
    TableFunction function("parse_duck_hunt_workflow_log", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                          ParseDuckHuntWorkflowLogFunction, ParseDuckHuntWorkflowLogBind, ParseDuckHuntWorkflowLogInitGlobal);
    
    return function;
}

} // namespace duckdb