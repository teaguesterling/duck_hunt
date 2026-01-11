#include "read_duck_hunt_workflow_log_function.hpp"
#include "read_duck_hunt_log_function.hpp"
#include "workflow_engine_interface.hpp"

// Include parser implementations for static build
#include "parsers/workflow_engines/github_actions_parser.hpp"
#include "parsers/workflow_engines/github_actions_zip_parser.hpp"
#include "parsers/workflow_engines/gitlab_ci_parser.hpp"
#include "parsers/workflow_engines/jenkins_parser.hpp"
#include "parsers/workflow_engines/docker_parser.hpp"
#include "parsers/workflow_engines/spack_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <fstream>

namespace duckdb {

// Convert workflow format enum to string
std::string WorkflowLogFormatToString(WorkflowLogFormat format) {
	switch (format) {
	case WorkflowLogFormat::AUTO:
		return "auto";
	case WorkflowLogFormat::GITHUB_ACTIONS:
		return "github_actions";
	case WorkflowLogFormat::GITLAB_CI:
		return "gitlab_ci";
	case WorkflowLogFormat::JENKINS:
		return "jenkins";
	case WorkflowLogFormat::DOCKER_BUILD:
		return "docker_build";
	case WorkflowLogFormat::SPACK:
		return "spack";
	case WorkflowLogFormat::GITHUB_ACTIONS_ZIP:
		return "github_actions_zip";
	case WorkflowLogFormat::UNKNOWN:
		return "unknown";
	default:
		return "unknown";
	}
}

// Convert string to workflow format enum
WorkflowLogFormat StringToWorkflowLogFormat(const std::string &format_str) {
	std::string lower_format = StringUtil::Lower(format_str);

	if (lower_format == "auto")
		return WorkflowLogFormat::AUTO;
	if (lower_format == "github_actions" || lower_format == "github")
		return WorkflowLogFormat::GITHUB_ACTIONS;
	if (lower_format == "gitlab_ci" || lower_format == "gitlab")
		return WorkflowLogFormat::GITLAB_CI;
	if (lower_format == "jenkins")
		return WorkflowLogFormat::JENKINS;
	if (lower_format == "docker_build" || lower_format == "docker")
		return WorkflowLogFormat::DOCKER_BUILD;
	if (lower_format == "spack" || lower_format == "spack_build")
		return WorkflowLogFormat::SPACK;
	if (lower_format == "github_actions_zip")
		return WorkflowLogFormat::GITHUB_ACTIONS_ZIP;

	return WorkflowLogFormat::UNKNOWN;
}

// Detect workflow log format from content
WorkflowLogFormat DetectWorkflowLogFormat(const std::string &content) {
	// GitHub Actions patterns
	if (content.find("##[group]") != std::string::npos || content.find("##[endgroup]") != std::string::npos ||
	    content.find("::group::") != std::string::npos || content.find("::endgroup::") != std::string::npos ||
	    content.find("Run actions/") != std::string::npos) {
		return WorkflowLogFormat::GITHUB_ACTIONS;
	}

	// GitLab CI patterns
	if (content.find("Running with gitlab-runner") != std::string::npos ||
	    content.find("Preparing the \"docker\"") != std::string::npos ||
	    (content.find("$ docker run") != std::string::npos && content.find("gitlab") != std::string::npos) ||
	    (content.find("Job succeeded") != std::string::npos && content.find("Pipeline #") != std::string::npos)) {
		return WorkflowLogFormat::GITLAB_CI;
	}

	// Jenkins patterns
	if (content.find("Started by user") != std::string::npos ||
	    content.find("Building in workspace") != std::string::npos ||
	    content.find("Finished: SUCCESS") != std::string::npos ||
	    content.find("Finished: FAILURE") != std::string::npos || content.find("[Pipeline]") != std::string::npos) {
		return WorkflowLogFormat::JENKINS;
	}

	// Docker build patterns
	if ((content.find("Step ") != std::string::npos && content.find("/") != std::string::npos) ||
	    content.find("Sending build context to Docker daemon") != std::string::npos ||
	    content.find("Successfully built") != std::string::npos ||
	    content.find("Successfully tagged") != std::string::npos || content.find("COPY --from=") != std::string::npos) {
		return WorkflowLogFormat::DOCKER_BUILD;
	}

	// Spack build patterns
	if (content.find("==> ") != std::string::npos &&
	    (content.find("Executing phase:") != std::string::npos || content.find("spack-stage") != std::string::npos ||
	     content.find("spack/opt/spack") != std::string::npos)) {
		return WorkflowLogFormat::SPACK;
	}

	return WorkflowLogFormat::UNKNOWN;
}

// Bind function for read_duck_hunt_workflow_log
unique_ptr<FunctionData> ReadDuckHuntWorkflowLogBind(ClientContext &context, TableFunctionBindInput &input,
                                                     vector<LogicalType> &return_types, vector<string> &names) {
	auto bind_data = make_uniq<ReadDuckHuntWorkflowLogBindData>();

	// Get source parameter (required)
	if (input.inputs.empty()) {
		throw BinderException("read_duck_hunt_workflow_log requires at least one parameter (source)");
	}
	bind_data->source = input.inputs[0].ToString();

	// Get format parameter (optional, defaults to auto)
	if (input.inputs.size() > 1) {
		std::string format_str = input.inputs[1].ToString();
		bind_data->format = StringToWorkflowLogFormat(format_str);

		// Check for unknown format
		if (bind_data->format == WorkflowLogFormat::UNKNOWN) {
			throw BinderException(
			    "Unknown workflow format: '" + format_str +
			    "'. Use 'auto' for auto-detection. Supported: github_actions, gitlab_ci, jenkins, docker_build.");
		}
	} else {
		bind_data->format = WorkflowLogFormat::AUTO;
	}

	// Handle severity_threshold named parameter
	auto threshold_param = input.named_parameters.find("severity_threshold");
	if (threshold_param != input.named_parameters.end()) {
		std::string threshold_str = threshold_param->second.ToString();
		bind_data->severity_threshold = StringToSeverityLevel(threshold_str);
	}

	// Handle ignore_errors named parameter
	auto ignore_errors_param = input.named_parameters.find("ignore_errors");
	if (ignore_errors_param != input.named_parameters.end()) {
		bind_data->ignore_errors = ignore_errors_param->second.GetValue<bool>();
	}

	// Define return schema (Schema V2) - includes all ValidationEvent fields plus workflow-specific ones
	return_types = {
	    // Core identification
	    LogicalType::BIGINT,  // event_id
	    LogicalType::VARCHAR, // tool_name
	    LogicalType::VARCHAR, // event_type
	    // Code location
	    LogicalType::VARCHAR, // file_path
	    LogicalType::INTEGER, // line_number
	    LogicalType::INTEGER, // column_number
	    LogicalType::VARCHAR, // function_name
	    // Classification
	    LogicalType::VARCHAR, // status
	    LogicalType::VARCHAR, // severity
	    LogicalType::VARCHAR, // category
	    LogicalType::VARCHAR, // error_code
	    // Content
	    LogicalType::VARCHAR, // message
	    LogicalType::VARCHAR, // suggestion
	    LogicalType::VARCHAR, // raw_output
	    LogicalType::VARCHAR, // structured_data
	    // Log tracking
	    LogicalType::INTEGER, // log_line_start
	    LogicalType::INTEGER, // log_line_end
	    // Test-specific
	    LogicalType::VARCHAR, // test_name
	    LogicalType::DOUBLE,  // execution_time
	    // Identity & Network
	    LogicalType::VARCHAR, // principal
	    LogicalType::VARCHAR, // origin
	    LogicalType::VARCHAR, // target
	    LogicalType::VARCHAR, // actor_type
	    // Temporal
	    LogicalType::VARCHAR, // started_at
	    // Correlation
	    LogicalType::VARCHAR, // external_id
	    // Hierarchical context
	    LogicalType::VARCHAR, // scope
	    LogicalType::VARCHAR, // scope_id
	    LogicalType::VARCHAR, // scope_status
	    LogicalType::VARCHAR, // group
	    LogicalType::VARCHAR, // group_id
	    LogicalType::VARCHAR, // group_status
	    LogicalType::VARCHAR, // unit
	    LogicalType::VARCHAR, // unit_id
	    LogicalType::VARCHAR, // unit_status
	    LogicalType::VARCHAR, // subunit
	    LogicalType::VARCHAR, // subunit_id
	    // Pattern analysis
	    LogicalType::VARCHAR, // fingerprint
	    LogicalType::DOUBLE,  // similarity_score
	    LogicalType::BIGINT,  // pattern_id
	    // Workflow-specific fields
	    LogicalType::VARCHAR, // workflow_type
	    LogicalType::INTEGER, // hierarchy_level
	    LogicalType::VARCHAR, // parent_id
	    // ZIP archive metadata
	    LogicalType::INTEGER, // job_order
	    LogicalType::VARCHAR, // job_name
	};

	names = {// Core identification
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
	         "scope", "scope_id", "scope_status", "group", "group_id", "group_status", "unit", "unit_id", "unit_status",
	         "subunit", "subunit_id",
	         // Pattern analysis
	         "fingerprint", "similarity_score", "pattern_id",
	         // Workflow-specific
	         "workflow_type", "hierarchy_level", "parent_id",
	         // ZIP archive metadata
	         "job_order", "job_name"};

	return std::move(bind_data);
}

// Global initialization for read_duck_hunt_workflow_log
unique_ptr<GlobalTableFunctionState> ReadDuckHuntWorkflowLogInitGlobal(ClientContext &context,
                                                                       TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<ReadDuckHuntWorkflowLogBindData>();
	auto global_state = make_uniq<ReadDuckHuntWorkflowLogGlobalState>();

	// Handle GITHUB_ACTIONS_ZIP format specially - it reads from ZIP archive
	if (bind_data.format == WorkflowLogFormat::GITHUB_ACTIONS_ZIP) {
		GitHubActionsZipParser zip_parser;
		std::vector<WorkflowEvent> parsed_events = zip_parser.parseZipArchive(context, bind_data.source);
		global_state->events = std::move(parsed_events);

		// Apply severity threshold filtering
		if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
			auto new_end = std::remove_if(
			    global_state->events.begin(), global_state->events.end(), [&bind_data](const WorkflowEvent &event) {
				    return !ShouldEmitEvent(event.base_event.severity, bind_data.severity_threshold);
			    });
			global_state->events.erase(new_end, global_state->events.end());
		}

		return std::move(global_state);
	}

	// Read source content for non-ZIP formats
	std::string content;
	auto &fs = FileSystem::GetFileSystem(context);

	try {
		// Check if source is a virtual path (zip://, s3://, http://, etc.)
		// Virtual paths may not support FileExists but can still be read
		bool is_virtual_path = (bind_data.source.find("://") != std::string::npos);

		// Check if file exists first (this triggers proper path resolution in test framework)
		// Skip FileExists check for virtual paths - try to read directly
		bool file_exists = is_virtual_path || fs.FileExists(bind_data.source);

		if (file_exists) {
			// Read file using DuckDB's FileSystem (respects UNITTEST_ROOT_DIRECTORY)
			content = ReadContentFromSource(context, bind_data.source);
		} else {
			// If file doesn't exist, treat source as direct content
			content = bind_data.source;
		}
	} catch (const IOException &e) {
		// If file reading fails, treat source as direct content
		content = bind_data.source;
	}

	// Auto-detect format if needed
	WorkflowLogFormat format = bind_data.format;
	if (format == WorkflowLogFormat::AUTO) {
		format = DetectWorkflowLogFormat(content);
	}

	// Parse content using the workflow engine registry
	auto &registry = WorkflowEngineRegistry::getInstance();

	// Ensure parsers are registered (static build workaround)
	if (registry.getParserCount() == 0) {
		registry.registerParser(make_uniq<GitHubActionsParser>());
		registry.registerParser(make_uniq<GitLabCIParser>());
		registry.registerParser(make_uniq<JenkinsParser>());
		registry.registerParser(make_uniq<DockerParser>());
		registry.registerParser(make_uniq<SpackParser>());
	}

	const WorkflowEngineParser *parser_ptr = nullptr;

	if (format == WorkflowLogFormat::AUTO) {
		parser_ptr = registry.findParser(content);
	} else {
		parser_ptr = registry.getParser(WorkflowLogFormatToString(format));
	}

	if (parser_ptr) {
		// Parse using the found parser
		std::vector<WorkflowEvent> parsed_events = parser_ptr->parseWorkflowLog(content);
		global_state->events = std::move(parsed_events);
	}

	// Apply severity threshold filtering
	if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
		// Filter out events below the threshold
		auto new_end = std::remove_if(
		    global_state->events.begin(), global_state->events.end(), [&bind_data](const WorkflowEvent &event) {
			    return !ShouldEmitEvent(event.base_event.severity, bind_data.severity_threshold);
		    });
		global_state->events.erase(new_end, global_state->events.end());
	}

	return std::move(global_state);
}

// Local initialization for read_duck_hunt_workflow_log
unique_ptr<LocalTableFunctionState> ReadDuckHuntWorkflowLogInitLocal(ExecutionContext &context,
                                                                     TableFunctionInitInput &input,
                                                                     GlobalTableFunctionState *global_state) {
	return make_uniq<ReadDuckHuntWorkflowLogLocalState>();
}

// Main table function implementation
void ReadDuckHuntWorkflowLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &global_state = data_p.global_state->Cast<ReadDuckHuntWorkflowLogGlobalState>();
	auto &local_state = data_p.local_state->Cast<ReadDuckHuntWorkflowLogLocalState>();

	idx_t current_row = local_state.chunk_offset;
	idx_t events_count = global_state.events.size();

	if (current_row >= events_count) {
		output.SetCardinality(0);
		return;
	}

	// Use STANDARD_VECTOR_SIZE for chunk size, not output.size() which returns current cardinality
	idx_t rows_to_output = std::min<idx_t>(STANDARD_VECTOR_SIZE, events_count - current_row);

	// CRITICAL: Set cardinality BEFORE populating values (DuckDB requirement)
	output.SetCardinality(rows_to_output);

	for (idx_t i = 0; i < rows_to_output; i++) {
		const WorkflowEvent &event = global_state.events[current_row + i];
		const ValidationEvent &base = event.base_event;

		// Core identification
		output.SetValue(0, i, Value::BIGINT(base.event_id));
		output.SetValue(1, i, Value(base.tool_name));
		output.SetValue(2, i, Value(ValidationEventTypeToString(base.event_type)));
		// Code location
		output.SetValue(3, i, Value(base.ref_file));
		output.SetValue(4, i, base.ref_line == -1 ? Value() : Value::INTEGER(base.ref_line));
		output.SetValue(5, i, base.ref_column == -1 ? Value() : Value::INTEGER(base.ref_column));
		output.SetValue(6, i, Value(base.function_name));
		// Classification
		output.SetValue(7, i, Value(ValidationEventStatusToString(base.status)));
		output.SetValue(8, i, Value(base.severity));
		output.SetValue(9, i, Value(base.category));
		output.SetValue(10, i, Value(base.error_code));
		// Content
		output.SetValue(11, i, Value(base.message));
		output.SetValue(12, i, Value(base.suggestion));
		output.SetValue(13, i, Value(base.log_content));
		output.SetValue(14, i, Value(base.structured_data));
		// Log tracking
		output.SetValue(15, i, base.log_line_start == -1 ? Value() : Value::INTEGER(base.log_line_start));
		output.SetValue(16, i, base.log_line_end == -1 ? Value() : Value::INTEGER(base.log_line_end));
		// Test-specific
		output.SetValue(17, i, Value(base.test_name));
		output.SetValue(18, i, Value::DOUBLE(base.execution_time));
		// Identity & Network
		output.SetValue(19, i, base.principal.empty() ? Value() : Value(base.principal));
		output.SetValue(20, i, base.origin.empty() ? Value() : Value(base.origin));
		output.SetValue(21, i, base.target.empty() ? Value() : Value(base.target));
		output.SetValue(22, i, base.actor_type.empty() ? Value() : Value(base.actor_type));
		// Temporal
		output.SetValue(23, i, base.started_at.empty() ? Value() : Value(base.started_at));
		// Correlation
		output.SetValue(24, i, base.external_id.empty() ? Value() : Value(base.external_id));
		// Hierarchical context
		output.SetValue(25, i, base.scope.empty() ? Value() : Value(base.scope));
		output.SetValue(26, i, base.scope_id.empty() ? Value() : Value(base.scope_id));
		output.SetValue(27, i, base.scope_status.empty() ? Value() : Value(base.scope_status));
		output.SetValue(28, i, base.group.empty() ? Value() : Value(base.group));
		output.SetValue(29, i, base.group_id.empty() ? Value() : Value(base.group_id));
		output.SetValue(30, i, base.group_status.empty() ? Value() : Value(base.group_status));
		output.SetValue(31, i, base.unit.empty() ? Value() : Value(base.unit));
		output.SetValue(32, i, base.unit_id.empty() ? Value() : Value(base.unit_id));
		output.SetValue(33, i, base.unit_status.empty() ? Value() : Value(base.unit_status));
		output.SetValue(34, i, base.subunit.empty() ? Value() : Value(base.subunit));
		output.SetValue(35, i, base.subunit_id.empty() ? Value() : Value(base.subunit_id));
		// Pattern analysis
		output.SetValue(36, i, base.fingerprint.empty() ? Value() : Value(base.fingerprint));
		output.SetValue(37, i, base.similarity_score == 0.0 ? Value() : Value::DOUBLE(base.similarity_score));
		output.SetValue(38, i, base.pattern_id == -1 ? Value() : Value::BIGINT(base.pattern_id));

		// Map additional workflow fields from WorkflowEvent
		output.SetValue(39, i, Value(event.workflow_type));
		output.SetValue(40, i, Value::INTEGER(event.hierarchy_level));
		output.SetValue(41, i, Value(event.parent_id));
		// ZIP archive metadata
		output.SetValue(42, i, event.job_order == -1 ? Value() : Value::INTEGER(event.job_order));
		output.SetValue(43, i, event.job_name.empty() ? Value() : Value(event.job_name));
	}

	local_state.chunk_offset += rows_to_output;
}

// Create the table function set with single-arg and two-arg overloads
TableFunctionSet GetReadDuckHuntWorkflowLogFunction() {
	TableFunctionSet set("read_duck_hunt_workflow_log");

	// Single argument version: read_duck_hunt_workflow_log(source) - auto-detects format
	TableFunction single_arg("read_duck_hunt_workflow_log", {LogicalType::VARCHAR}, ReadDuckHuntWorkflowLogFunction,
	                         ReadDuckHuntWorkflowLogBind, ReadDuckHuntWorkflowLogInitGlobal,
	                         ReadDuckHuntWorkflowLogInitLocal);
	single_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	single_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	set.AddFunction(single_arg);

	// Two argument version: read_duck_hunt_workflow_log(source, format)
	TableFunction two_arg("read_duck_hunt_workflow_log", {LogicalType::VARCHAR, LogicalType::VARCHAR},
	                      ReadDuckHuntWorkflowLogFunction, ReadDuckHuntWorkflowLogBind,
	                      ReadDuckHuntWorkflowLogInitGlobal, ReadDuckHuntWorkflowLogInitLocal);
	two_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	two_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	set.AddFunction(two_arg);

	return set;
}

} // namespace duckdb
