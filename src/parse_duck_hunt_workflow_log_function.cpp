#include "parse_duck_hunt_workflow_log_function.hpp"
#include "workflow_engine_interface.hpp"

// Include parser implementations for static build
#include "parsers/workflow_engines/github_actions_parser.hpp"
#include "parsers/workflow_engines/gitlab_ci_parser.hpp"
#include "parsers/workflow_engines/jenkins_parser.hpp"
#include "parsers/workflow_engines/docker_parser.hpp"
#include "parsers/workflow_engines/spack_parser.hpp"

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
    if (lower_format == "spack" || lower_format == "spack_build") return WorkflowLogFormat::SPACK;

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
            registry.registerParser(make_uniq<SpackParser>());
        }
        
        const WorkflowEngineParser* parser = nullptr;
    
        if (format == WorkflowLogFormat::AUTO) {
            parser = registry.findParser(content);
        } else {
            parser = registry.getParser(WorkflowLogFormatToString(format));
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
        error_event.base_event.log_content = content.substr(0, std::min(size_t(200), content.length()));
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
        // Core identification
        LogicalType::BIGINT,   // event_id
        LogicalType::VARCHAR,  // tool_name
        LogicalType::VARCHAR,  // event_type
        // Code location
        LogicalType::VARCHAR,  // file_path
        LogicalType::INTEGER,  // line_number
        LogicalType::INTEGER,  // column_number
        LogicalType::VARCHAR,  // function_name
        // Classification
        LogicalType::VARCHAR,  // status
        LogicalType::VARCHAR,  // severity
        LogicalType::VARCHAR,  // category
        LogicalType::VARCHAR,  // error_code
        // Content
        LogicalType::VARCHAR,  // message
        LogicalType::VARCHAR,  // suggestion
        LogicalType::VARCHAR,  // raw_output
        LogicalType::VARCHAR,  // structured_data
        // Log tracking
        LogicalType::INTEGER,  // log_line_start
        LogicalType::INTEGER,  // log_line_end
        // Test-specific
        LogicalType::VARCHAR,  // test_name
        LogicalType::DOUBLE,   // execution_time
        // Identity & Network
        LogicalType::VARCHAR,  // principal
        LogicalType::VARCHAR,  // origin
        LogicalType::VARCHAR,  // target
        LogicalType::VARCHAR,  // actor_type
        // Temporal
        LogicalType::VARCHAR,  // started_at
        // Correlation
        LogicalType::VARCHAR,  // external_id
        // Hierarchical context
        LogicalType::VARCHAR,  // scope
        LogicalType::VARCHAR,  // scope_id
        LogicalType::VARCHAR,  // scope_status
        LogicalType::VARCHAR,  // group
        LogicalType::VARCHAR,  // group_id
        LogicalType::VARCHAR,  // group_status
        LogicalType::VARCHAR,  // unit
        LogicalType::VARCHAR,  // unit_id
        LogicalType::VARCHAR,  // unit_status
        LogicalType::VARCHAR,  // subunit
        LogicalType::VARCHAR,  // subunit_id
        // Pattern analysis
        LogicalType::VARCHAR,  // fingerprint
        LogicalType::DOUBLE,   // similarity_score
        LogicalType::BIGINT,   // pattern_id
        // Workflow-specific fields
        LogicalType::VARCHAR,  // workflow_type
        LogicalType::INTEGER,  // hierarchy_level
        LogicalType::VARCHAR   // parent_id
    };

    names = {
        // Core identification
        "event_id", "tool_name", "event_type",
        // Code location
        "ref_file", "ref_line", "ref_column", "function_name",
        // Classification
        "status", "severity", "category", "error_code",
        // Content
        "message", "suggestion", "log_content", "structured_data",
        // Log tracking
        "log_line_start", "log_line_end",
        // Test-specific
        "test_name", "execution_time",
        // Identity & Network
        "principal", "origin", "target", "actor_type",
        // Temporal
        "started_at",
        // Correlation
        "external_id",
        // Hierarchical context
        "scope", "scope_id", "scope_status",
        "group", "group_id", "group_status",
        "unit", "unit_id", "unit_status",
        "subunit", "subunit_id",
        // Pattern analysis
        "fingerprint", "similarity_score", "pattern_id",
        // Workflow-specific
        "workflow_type", "hierarchy_level", "parent_id"
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
        
        const ValidationEvent& base = event.base_event;
        // Core identification
        output.SetValue(0, count, Value::BIGINT(base.event_id));
        output.SetValue(1, count, Value(base.tool_name));
        output.SetValue(2, count, Value(ValidationEventTypeToString(base.event_type)));
        // Code location
        output.SetValue(3, count, Value(base.ref_file));
        output.SetValue(4, count, base.ref_line == -1 ? Value() : Value::INTEGER(base.ref_line));
        output.SetValue(5, count, base.ref_column == -1 ? Value() : Value::INTEGER(base.ref_column));
        output.SetValue(6, count, Value(base.function_name));
        // Classification
        output.SetValue(7, count, Value(ValidationEventStatusToString(base.status)));
        output.SetValue(8, count, Value(base.severity));
        output.SetValue(9, count, Value(base.category));
        output.SetValue(10, count, Value(base.error_code));
        // Content
        output.SetValue(11, count, Value(base.message));
        output.SetValue(12, count, Value(base.suggestion));
        output.SetValue(13, count, Value(base.log_content));
        output.SetValue(14, count, Value(base.structured_data));
        // Log tracking
        output.SetValue(15, count, base.log_line_start == -1 ? Value() : Value::INTEGER(base.log_line_start));
        output.SetValue(16, count, base.log_line_end == -1 ? Value() : Value::INTEGER(base.log_line_end));
        // Test-specific
        output.SetValue(17, count, Value(base.test_name));
        output.SetValue(18, count, Value::DOUBLE(base.execution_time));
        // Identity & Network
        output.SetValue(19, count, base.principal.empty() ? Value() : Value(base.principal));
        output.SetValue(20, count, base.origin.empty() ? Value() : Value(base.origin));
        output.SetValue(21, count, base.target.empty() ? Value() : Value(base.target));
        output.SetValue(22, count, base.actor_type.empty() ? Value() : Value(base.actor_type));
        // Temporal
        output.SetValue(23, count, base.started_at.empty() ? Value() : Value(base.started_at));
        // Correlation
        output.SetValue(24, count, base.external_id.empty() ? Value() : Value(base.external_id));
        // Hierarchical context
        output.SetValue(25, count, base.scope.empty() ? Value() : Value(base.scope));
        output.SetValue(26, count, base.scope_id.empty() ? Value() : Value(base.scope_id));
        output.SetValue(27, count, base.scope_status.empty() ? Value() : Value(base.scope_status));
        output.SetValue(28, count, base.group.empty() ? Value() : Value(base.group));
        output.SetValue(29, count, base.group_id.empty() ? Value() : Value(base.group_id));
        output.SetValue(30, count, base.group_status.empty() ? Value() : Value(base.group_status));
        output.SetValue(31, count, base.unit.empty() ? Value() : Value(base.unit));
        output.SetValue(32, count, base.unit_id.empty() ? Value() : Value(base.unit_id));
        output.SetValue(33, count, base.unit_status.empty() ? Value() : Value(base.unit_status));
        output.SetValue(34, count, base.subunit.empty() ? Value() : Value(base.subunit));
        output.SetValue(35, count, base.subunit_id.empty() ? Value() : Value(base.subunit_id));
        // Pattern analysis
        output.SetValue(36, count, base.fingerprint.empty() ? Value() : Value(base.fingerprint));
        output.SetValue(37, count, base.similarity_score == 0.0 ? Value() : Value::DOUBLE(base.similarity_score));
        output.SetValue(38, count, base.pattern_id == -1 ? Value() : Value::BIGINT(base.pattern_id));
        // Workflow-specific fields
        output.SetValue(39, count, Value(event.workflow_type));
        output.SetValue(40, count, Value::INTEGER(event.hierarchy_level));
        output.SetValue(41, count, Value(event.parent_id));
        
        count++;
        gstate.position++;
    }
    
    output.SetCardinality(count);
}

TableFunctionSet GetParseDuckHuntWorkflowLogFunction() {
    TableFunctionSet set("parse_duck_hunt_workflow_log");

    // Single argument version: parse_duck_hunt_workflow_log(content) - auto-detects format
    TableFunction single_arg("parse_duck_hunt_workflow_log", {LogicalType::VARCHAR},
                            ParseDuckHuntWorkflowLogFunction, ParseDuckHuntWorkflowLogBind, ParseDuckHuntWorkflowLogInitGlobal);
    set.AddFunction(single_arg);

    // Two argument version: parse_duck_hunt_workflow_log(content, format)
    TableFunction two_arg("parse_duck_hunt_workflow_log", {LogicalType::VARCHAR, LogicalType::VARCHAR},
                         ParseDuckHuntWorkflowLogFunction, ParseDuckHuntWorkflowLogBind, ParseDuckHuntWorkflowLogInitGlobal);
    set.AddFunction(two_arg);

    return set;
}

} // namespace duckdb