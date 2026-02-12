#include "include/read_duck_hunt_log_function.hpp"
#include "include/validation_event_types.hpp"
#include "core/parser_registry.hpp"
#include "core/parse_content.hpp"
#include "core/file_utils.hpp"
#include "core/format_utils.hpp"
#include "core/error_patterns.hpp"
#include "parsers/test_frameworks/duckdb_test_parser.hpp"
#include "parsers/test_frameworks/pytest_cov_text_parser.hpp"
#include "parsers/tool_outputs/generic_lint_parser.hpp"
#include "parsers/tool_outputs/regexp_parser.hpp"
#include "parsers/test_frameworks/junit_text_parser.hpp"
#include "parsers/test_frameworks/rspec_text_parser.hpp"
#include "parsers/test_frameworks/mocha_chai_text_parser.hpp"
#include "parsers/test_frameworks/gtest_text_parser.hpp"
#include "parsers/test_frameworks/nunit_xunit_text_parser.hpp"
#include "parsers/specialized/valgrind_parser.hpp"
#include "parsers/specialized/gdb_lldb_parser.hpp"
#include "parsers/specialized/strace_parser.hpp"
#include "parsers/specialized/coverage_parser.hpp"
#include "parsers/build_systems/maven_parser.hpp"
#include "parsers/build_systems/gradle_parser.hpp"
#include "parsers/build_systems/msbuild_parser.hpp"
#include "parsers/build_systems/node_parser.hpp"
#include "parsers/build_systems/python_parser.hpp"
#include "parsers/build_systems/cargo_parser.hpp"
#include "parsers/build_systems/cmake_parser.hpp"
#include "parsers/test_frameworks/junit_xml_parser.hpp"
#include "core/webbed_integration.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/file_glob_options.hpp"
#include "duckdb/common/enums/file_compression_type.hpp"
#include "yyjson.hpp"
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <cctype>
#include <functional>

namespace duckdb {

using namespace duckdb_yyjson;

// File utilities moved to src/core/file_utils.cpp:
// - ReadContentFromSource
// - IsValidJSON

unique_ptr<FunctionData> ReadDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names) {
	auto bind_data = make_uniq<ReadDuckHuntLogBindData>();

	// For in-out functions (LATERAL joins), source comes from the input DataChunk at execution time
	// input.inputs may be empty or contain column references, not literal values
	// We only need to process format (second arg) and named parameters at bind time

	// Get source parameter if available as a literal (for direct calls)
	// For LATERAL joins, this will be empty and source comes from the input DataChunk
	if (!input.inputs.empty() && !input.inputs[0].IsNull()) {
		bind_data->source = input.inputs[0].ToString();
	}

	// Get format parameter (optional, defaults to auto)
	if (input.inputs.size() > 1) {
		std::string format_str = input.inputs[1].ToString();
		bind_data->format = StringToTestResultFormat(format_str);

		// Get canonical format name for registry lookup (handles aliases)
		if (bind_data->format != TestResultFormat::UNKNOWN && bind_data->format != TestResultFormat::AUTO &&
		    bind_data->format != TestResultFormat::REGEXP) {
			bind_data->format_name = GetCanonicalFormatName(bind_data->format);
		} else {
			bind_data->format_name = format_str; // Use original string for registry-only formats
		}

		// Check for unknown format - but allow registry-only formats and format groups
		if (bind_data->format == TestResultFormat::UNKNOWN) {
			// Check if this format exists in the new parser registry or is a format group
			auto &registry = ParserRegistry::getInstance();
			if (!registry.hasFormat(format_str) && !registry.isGroup(format_str)) {
				throw BinderException("Unknown format: '" + format_str +
				                      "'. Use 'auto' for auto-detection or see docs/formats.md for supported formats.");
			}
			// Format exists in registry or is a group, keep UNKNOWN enum but format_name has the string
		}

		// For REGEXP format, extract the pattern after the "regexp:" prefix
		if (bind_data->format == TestResultFormat::REGEXP) {
			if (format_str.length() <= 7) {
				throw BinderException("regexp: format requires a pattern after the prefix, e.g., "
				                      "'regexp:(?P<severity>ERROR|WARN):\\s+(?P<message>.*)'");
			}
			bind_data->regexp_pattern = format_str.substr(7); // Remove "regexp:" prefix
			bind_data->format_name = "regexp";                // Canonical name for display
		}
	} else {
		bind_data->format = TestResultFormat::AUTO;
		bind_data->format_name = "auto";
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

	// Handle content named parameter (controls log_content truncation)
	auto content_param = input.named_parameters.find("content");
	if (content_param != input.named_parameters.end()) {
		auto &val = content_param->second;
		if (val.type().id() == LogicalTypeId::INTEGER || val.type().id() == LogicalTypeId::BIGINT) {
			int32_t limit = val.GetValue<int32_t>();
			if (limit <= 0) {
				bind_data->content_mode = ContentMode::NONE;
			} else {
				bind_data->content_mode = ContentMode::LIMIT;
				bind_data->content_limit = limit;
			}
		} else {
			std::string mode_str = StringUtil::Lower(val.ToString());
			if (mode_str == "full") {
				bind_data->content_mode = ContentMode::FULL;
			} else if (mode_str == "none") {
				bind_data->content_mode = ContentMode::NONE;
			} else if (mode_str == "smart") {
				bind_data->content_mode = ContentMode::SMART;
			} else {
				throw BinderException("Invalid content mode: '" + mode_str +
				                      "'. Use integer (char limit), 'full', 'none', or 'smart'.");
			}
		}
	}

	// Handle context named parameter (number of context lines around events)
	auto context_param = input.named_parameters.find("context");
	if (context_param != input.named_parameters.end()) {
		int32_t ctx_lines = context_param->second.GetValue<int32_t>();
		if (ctx_lines < 0) {
			throw BinderException("context must be a non-negative integer");
		}
		// Cap at reasonable maximum
		bind_data->context_lines = std::min(ctx_lines, static_cast<int32_t>(50));
	}

	// Handle include_unparsed named parameter (regexp format only)
	auto include_unparsed_param = input.named_parameters.find("include_unparsed");
	if (include_unparsed_param != input.named_parameters.end()) {
		bind_data->include_unparsed = include_unparsed_param->second.GetValue<bool>();
	}

	// Define return schema (Schema V2)
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
	    LogicalType::VARCHAR, // log_file
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
	    LogicalType::BIGINT   // pattern_id
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
	         "log_line_start", "log_line_end", "log_file",
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
	         "fingerprint", "similarity_score", "pattern_id"};

	// Add context column if requested
	if (bind_data->context_lines > 0) {
		return_types.push_back(GetContextColumnType());
		names.push_back("context");
	}

	return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ReadDuckHuntLogInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto global_state = make_uniq<ReadDuckHuntLogGlobalState>();

	// Phase 3A: Check if source contains glob patterns or multiple files
	std::vector<std::string> files;

	try {
		// Try to expand the source as a glob pattern or file list
		files = GetFilesFromPattern(context, bind_data.source);
	} catch (const IOException &) {
		// If glob expansion fails, treat as single file or direct content
		files.clear();
	}

	if (files.size() > 1) {
		// Multi-file processing path
		ProcessMultipleFiles(context, files, bind_data.format, bind_data.format_name, global_state->events,
		                     bind_data.ignore_errors);
	} else {
		// Single file processing path (original behavior)
		// Use the matched file path if available, otherwise use the source directly
		std::string source_path = files.size() == 1 ? files[0] : bind_data.source;
		std::string content;
		bool is_file = false;
		bool format_detected = true;

		// Auto-detect format if needed - use peek for format detection (CSV sniffer pattern)
		TestResultFormat format = bind_data.format;
		std::string format_name = bind_data.format_name;

		if (format == TestResultFormat::AUTO) {
			// First try to peek the file for format detection
			try {
				std::string peek_content = PeekContentFromSource(context, source_path, SNIFF_BUFFER_SIZE);
				is_file = true;
				format_name = DetectFormat(peek_content);
				if (format_name.empty()) {
					// No format detected - don't read full file, skip to return empty result
					format_detected = false;
				} else {
					format = TestResultFormat::UNKNOWN; // Use UNKNOWN so we use registry-based parsing below
				}
			} catch (const IOException &) {
				// Not a file - treat source as direct content
				format_name = DetectFormat(bind_data.source);
				if (!format_name.empty()) {
					format = TestResultFormat::UNKNOWN;
				} else {
					format_detected = false;
				}
			}
		}

		// Only read full content if format was detected or explicitly specified
		if (format_detected) {
			// Try to read as file first (both for AUTO-detected and explicitly specified formats)
			try {
				content = ReadContentFromSource(context, source_path);
				is_file = true;
			} catch (const IOException &) {
				// If file reading fails, treat source as direct content
				content = bind_data.source;
			}
		}

		// Parse content using core API (only if format was detected)
		if (format_detected) {
			if (format == TestResultFormat::REGEXP) {
				// REGEXP is special - requires user-provided pattern
				auto events = ParseContentRegexp(content, bind_data.regexp_pattern, bind_data.include_unparsed);
				global_state->events.insert(global_state->events.end(), events.begin(), events.end());
			} else if (!format_name.empty() && format_name != "unknown" && format_name != "auto") {
				auto events = ParseContent(context, content, format_name);
				global_state->events.insert(global_state->events.end(), events.begin(), events.end());
			}
		}

		// Set log_file on each event to track source file (single file mode)
		// Only set if source looks like a file path (contains / or \)
		if (bind_data.source.find('/') != std::string::npos || bind_data.source.find('\\') != std::string::npos) {
			for (auto &event : global_state->events) {
				if (event.log_file.empty()) {
					event.log_file = bind_data.source;
				}
			}
		}

		// Store log lines for context extraction if requested (single file mode)
		if (bind_data.context_lines > 0) {
			std::vector<std::string> lines;
			std::istringstream stream(content);
			std::string line;
			while (std::getline(stream, line)) {
				lines.push_back(line);
			}
			// Use source_path as file key
			global_state->log_lines_by_file[source_path] = std::move(lines);
		}
	}

	// Phase 3B: Process error patterns for intelligent categorization
	ProcessErrorPatterns(global_state->events);

	// Apply severity threshold filtering
	if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
		// Filter out events below the threshold
		auto new_end = std::remove_if(global_state->events.begin(), global_state->events.end(),
		                              [&bind_data](const ValidationEvent &event) {
			                              return !ShouldEmitEvent(event.severity, bind_data.severity_threshold);
		                              });
		global_state->events.erase(new_end, global_state->events.end());
	}

	return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ReadDuckHuntLogInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                             GlobalTableFunctionState *global_state) {
	return make_uniq<ReadDuckHuntLogLocalState>();
}

void ReadDuckHuntLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto &global_state = data_p.global_state->Cast<ReadDuckHuntLogGlobalState>();
	auto &local_state = data_p.local_state->Cast<ReadDuckHuntLogLocalState>();

	// Populate output chunk with content truncation and context settings
	PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE,
	                            bind_data.content_mode, bind_data.content_limit, bind_data.context_lines,
	                            bind_data.context_lines > 0 ? &global_state.log_lines_by_file : nullptr);

	// Update offset for next chunk
	local_state.chunk_offset += output.size();
}

// Context extraction functions moved to src/core/context_extraction.cpp:
// - TruncateLogContent
// - GetContextColumnType
// - ExtractContext
// - PopulateDataChunkFromEvents

// Parse test results implementation for string input
unique_ptr<FunctionData> ParseDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
	auto bind_data = make_uniq<ReadDuckHuntLogBindData>();

	// For in-out functions (LATERAL joins), content comes from the input DataChunk at execution time
	// input.inputs may be empty or contain column references, not literal values
	// We only need to process format (second arg) and named parameters at bind time

	// Get content parameter if available as a literal (for direct calls)
	// For LATERAL joins, this will be empty and content comes from the input DataChunk
	if (!input.inputs.empty() && !input.inputs[0].IsNull()) {
		bind_data->source = input.inputs[0].ToString();
	}

	// Get format parameter (optional, defaults to auto)
	// For single-arg calls, input.inputs.size() == 1
	// For two-arg calls, input.inputs.size() == 2
	if (input.inputs.size() > 1 && !input.inputs[1].IsNull()) {
		std::string format_str = input.inputs[1].ToString();
		bind_data->format = StringToTestResultFormat(format_str);

		// Get canonical format name for registry lookup (handles aliases)
		if (bind_data->format != TestResultFormat::UNKNOWN && bind_data->format != TestResultFormat::AUTO &&
		    bind_data->format != TestResultFormat::REGEXP) {
			bind_data->format_name = GetCanonicalFormatName(bind_data->format);
		} else {
			bind_data->format_name = format_str; // Use original string for registry-only formats
		}

		// Check for unknown format - but allow registry-only formats and format groups
		if (bind_data->format == TestResultFormat::UNKNOWN) {
			// Check if this format exists in the new parser registry or is a format group
			auto &registry = ParserRegistry::getInstance();
			if (!registry.hasFormat(format_str) && !registry.isGroup(format_str)) {
				throw BinderException("Unknown format: '" + format_str +
				                      "'. Use 'auto' for auto-detection or see docs/formats.md for supported formats.");
			}
			// Format exists in registry or is a group, keep UNKNOWN enum but format_name has the string
		}

		// For REGEXP format, extract the pattern after the "regexp:" prefix
		if (bind_data->format == TestResultFormat::REGEXP) {
			if (format_str.length() <= 7) {
				throw BinderException("regexp: format requires a pattern after the prefix, e.g., "
				                      "'regexp:(?P<severity>ERROR|WARN):\\s+(?P<message>.*)'");
			}
			bind_data->regexp_pattern = format_str.substr(7); // Remove "regexp:" prefix
			bind_data->format_name = "regexp";                // Canonical name for display
		}
	} else {
		bind_data->format = TestResultFormat::AUTO;
		bind_data->format_name = "auto";
	}

	// Handle severity_threshold named parameter
	auto threshold_param = input.named_parameters.find("severity_threshold");
	if (threshold_param != input.named_parameters.end()) {
		std::string threshold_str = threshold_param->second.ToString();
		bind_data->severity_threshold = StringToSeverityLevel(threshold_str);
	}

	// Handle ignore_errors named parameter (note: parse_duck_hunt_log doesn't use files, but kept for API consistency)
	auto ignore_errors_param = input.named_parameters.find("ignore_errors");
	if (ignore_errors_param != input.named_parameters.end()) {
		bind_data->ignore_errors = ignore_errors_param->second.GetValue<bool>();
	}

	// Handle content named parameter (controls log_content truncation)
	auto content_param = input.named_parameters.find("content");
	if (content_param != input.named_parameters.end()) {
		auto &val = content_param->second;
		if (val.type().id() == LogicalTypeId::INTEGER || val.type().id() == LogicalTypeId::BIGINT) {
			int32_t limit = val.GetValue<int32_t>();
			if (limit <= 0) {
				bind_data->content_mode = ContentMode::NONE;
			} else {
				bind_data->content_mode = ContentMode::LIMIT;
				bind_data->content_limit = limit;
			}
		} else {
			std::string mode_str = StringUtil::Lower(val.ToString());
			if (mode_str == "full") {
				bind_data->content_mode = ContentMode::FULL;
			} else if (mode_str == "none") {
				bind_data->content_mode = ContentMode::NONE;
			} else if (mode_str == "smart") {
				bind_data->content_mode = ContentMode::SMART;
			} else {
				throw BinderException("Invalid content mode: '" + mode_str +
				                      "'. Use integer (char limit), 'full', 'none', or 'smart'.");
			}
		}
	}

	// Handle context named parameter (number of context lines around events)
	auto context_param = input.named_parameters.find("context");
	if (context_param != input.named_parameters.end()) {
		int32_t ctx_lines = context_param->second.GetValue<int32_t>();
		if (ctx_lines < 0) {
			throw BinderException("context must be a non-negative integer");
		}
		// Cap at reasonable maximum
		bind_data->context_lines = std::min(ctx_lines, static_cast<int32_t>(50));
	}

	// Handle include_unparsed named parameter (regexp format only)
	auto include_unparsed_param = input.named_parameters.find("include_unparsed");
	if (include_unparsed_param != input.named_parameters.end()) {
		bind_data->include_unparsed = include_unparsed_param->second.GetValue<bool>();
	}

	// Define return schema (Schema V2 - same as read_duck_hunt_log)
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
	    LogicalType::VARCHAR, // log_file
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
	    LogicalType::BIGINT   // pattern_id
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
	         "log_line_start", "log_line_end", "log_file",
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
	         "fingerprint", "similarity_score", "pattern_id"};

	// Add context column if requested
	if (bind_data->context_lines > 0) {
		return_types.push_back(GetContextColumnType());
		names.push_back("context");
	}

	return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ParseDuckHuntLogInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto global_state = make_uniq<ReadDuckHuntLogGlobalState>();

	// Use source directly as content (no file reading)
	std::string content = bind_data.source;

	// Auto-detect format if needed
	TestResultFormat format = bind_data.format;
	std::string format_name = bind_data.format_name;
	if (format == TestResultFormat::AUTO) {
		format_name = DetectFormat(content);
		if (!format_name.empty()) {
			format = TestResultFormat::UNKNOWN;
		}
	}

	// Parse content using core API
	if (format == TestResultFormat::REGEXP) {
		// REGEXP is special - requires user-provided pattern
		auto events = ParseContentRegexp(content, bind_data.regexp_pattern, bind_data.include_unparsed);
		global_state->events.insert(global_state->events.end(), events.begin(), events.end());
	} else if (!format_name.empty() && format_name != "unknown" && format_name != "auto") {
		auto events = ParseContent(context, content, format_name);
		global_state->events.insert(global_state->events.end(), events.begin(), events.end());
	}

	// Phase 3B: Process error patterns for intelligent categorization
	ProcessErrorPatterns(global_state->events);

	// Apply severity threshold filtering
	if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
		// Filter out events below the threshold
		auto new_end = std::remove_if(global_state->events.begin(), global_state->events.end(),
		                              [&bind_data](const ValidationEvent &event) {
			                              return !ShouldEmitEvent(event.severity, bind_data.severity_threshold);
		                              });
		global_state->events.erase(new_end, global_state->events.end());
	}

	// Store log lines for context extraction if requested
	if (bind_data.context_lines > 0) {
		std::vector<std::string> lines;
		std::istringstream stream(content);
		std::string line;
		while (std::getline(stream, line)) {
			lines.push_back(line);
		}
		// For parse_duck_hunt_log, use empty string as file key (no file)
		global_state->log_lines_by_file[""] = std::move(lines);
	}

	return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ParseDuckHuntLogInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                              GlobalTableFunctionState *global_state) {
	return make_uniq<ReadDuckHuntLogLocalState>();
}

void ParseDuckHuntLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto &global_state = data_p.global_state->Cast<ReadDuckHuntLogGlobalState>();
	auto &local_state = data_p.local_state->Cast<ReadDuckHuntLogLocalState>();

	// Populate output chunk with content truncation and context settings
	PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE,
	                            bind_data.content_mode, bind_data.content_limit, bind_data.context_lines,
	                            bind_data.context_lines > 0 ? &global_state.log_lines_by_file : nullptr);

	// Update offset for next chunk
	local_state.chunk_offset += output.size();
}

// In-out function implementations for LATERAL join support
unique_ptr<GlobalTableFunctionState> ParseDuckHuntLogInOutInitGlobal(ClientContext &context,
                                                                     TableFunctionInitInput &input) {
	// For in-out functions, global state is minimal - we don't pre-parse
	return make_uniq<ReadDuckHuntLogGlobalState>();
}

unique_ptr<LocalTableFunctionState> ParseDuckHuntLogInOutInitLocal(ExecutionContext &context,
                                                                   TableFunctionInitInput &input,
                                                                   GlobalTableFunctionState *global_state) {
	return make_uniq<ParseDuckHuntLogInOutLocalState>();
}

OperatorResultType ParseDuckHuntLogInOutFunction(ExecutionContext &context, TableFunctionInput &data_p,
                                                 DataChunk &input, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto &lstate = data_p.local_state->Cast<ParseDuckHuntLogInOutLocalState>();

	if (!lstate.initialized) {
		// Get content from input DataChunk (first column)
		// For LATERAL joins, content comes from the input table
		// For direct calls, DuckDB creates a synthetic input DataChunk
		if (input.size() == 0 || input.data.empty()) {
			// No input data - signal we need more
			output.SetCardinality(0);
			return OperatorResultType::NEED_MORE_INPUT;
		}

		// Get the content value - handle both constant and flat vectors
		Value content_value = input.GetValue(0, 0);
		if (content_value.IsNull()) {
			// Null input - no output, request next row
			output.SetCardinality(0);
			lstate.initialized = false;
			return OperatorResultType::NEED_MORE_INPUT;
		}

		auto content = content_value.ToString();

		// Determine format - either from bind_data or auto-detect
		TestResultFormat format = bind_data.format;
		std::string format_name = bind_data.format_name;

		if (format == TestResultFormat::AUTO) {
			format_name = DetectFormat(content);
			if (!format_name.empty()) {
				format = TestResultFormat::UNKNOWN;
			}
		}

		// Parse content
		if (format == TestResultFormat::REGEXP) {
			lstate.events = ParseContentRegexp(content, bind_data.regexp_pattern, bind_data.include_unparsed);
		} else if (!format_name.empty() && format_name != "unknown" && format_name != "auto") {
			lstate.events = ParseContent(context.client, content, format_name);
		}

		// Process error patterns for intelligent categorization
		ProcessErrorPatterns(lstate.events);

		// Apply severity threshold filtering
		if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
			auto new_end =
			    std::remove_if(lstate.events.begin(), lstate.events.end(), [&bind_data](const ValidationEvent &event) {
				    return !ShouldEmitEvent(event.severity, bind_data.severity_threshold);
			    });
			lstate.events.erase(new_end, lstate.events.end());
		}

		// Cache log lines for context extraction if needed
		if (bind_data.context_lines > 0) {
			std::vector<std::string> lines;
			std::istringstream stream(content);
			std::string line;
			while (std::getline(stream, line)) {
				lines.push_back(line);
			}
			lstate.log_lines_by_file[""] = std::move(lines);
		}

		lstate.output_offset = 0;
		lstate.initialized = true;
	}

	// Calculate how many events to output this chunk
	idx_t remaining = lstate.events.size() > lstate.output_offset ? lstate.events.size() - lstate.output_offset : 0;
	idx_t output_size = std::min(remaining, static_cast<idx_t>(STANDARD_VECTOR_SIZE));

	// Populate the output chunk
	PopulateDataChunkFromEvents(output, lstate.events, lstate.output_offset, output_size, bind_data.content_mode,
	                            bind_data.content_limit, bind_data.context_lines,
	                            bind_data.context_lines > 0 ? &lstate.log_lines_by_file : nullptr);

	lstate.output_offset += output_size;

	// Follow JSON pattern: return NEED_MORE_INPUT only when output is empty
	// This signals we're done with this input row
	if (output.size() == 0) {
		// Reset state for next input row
		lstate.initialized = false;
		lstate.events.clear();
		lstate.log_lines_by_file.clear();
		lstate.output_offset = 0;
		return OperatorResultType::NEED_MORE_INPUT;
	}

	return OperatorResultType::HAVE_MORE_OUTPUT;
}

// In-out function implementations for read_duck_hunt_log LATERAL join support
unique_ptr<GlobalTableFunctionState> ReadDuckHuntLogInOutInitGlobal(ClientContext &context,
                                                                    TableFunctionInitInput &input) {
	// For in-out functions, global state is minimal - we don't pre-parse
	return make_uniq<ReadDuckHuntLogGlobalState>();
}

unique_ptr<LocalTableFunctionState> ReadDuckHuntLogInOutInitLocal(ExecutionContext &context,
                                                                  TableFunctionInitInput &input,
                                                                  GlobalTableFunctionState *global_state) {
	return make_uniq<ReadDuckHuntLogInOutLocalState>();
}

// Helper function to check if a path contains glob characters
static bool ContainsGlobCharacters(const std::string &path) {
	return path.find('*') != std::string::npos || path.find('?') != std::string::npos ||
	       path.find('[') != std::string::npos || path.find('{') != std::string::npos;
}

OperatorResultType ReadDuckHuntLogInOutFunction(ExecutionContext &context, TableFunctionInput &data_p, DataChunk &input,
                                                DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto &lstate = data_p.local_state->Cast<ReadDuckHuntLogInOutLocalState>();

	if (!lstate.initialized) {
		// Get file path from input DataChunk (first column)
		if (input.size() == 0 || input.data.empty()) {
			output.SetCardinality(0);
			return OperatorResultType::NEED_MORE_INPUT;
		}

		Value path_value = input.GetValue(0, 0);
		if (path_value.IsNull()) {
			// Null input - no output, request next row
			output.SetCardinality(0);
			lstate.initialized = false;
			return OperatorResultType::NEED_MORE_INPUT;
		}

		std::string source_path = path_value.ToString();
		lstate.current_file_path = source_path;

		// Check if this is a glob pattern - streaming not supported for globs
		if (ContainsGlobCharacters(source_path)) {
			// Glob patterns: use batch mode
			std::vector<std::string> files;
			try {
				files = GetFilesFromPattern(context.client, source_path);
			} catch (const IOException &) {
				files.clear();
			}

			// Process all files in batch mode
			for (const auto &file_path : files) {
				std::string content;
				try {
					content = ReadContentFromSource(context.client, file_path);
				} catch (const IOException &) {
					continue;
				}

				if (content.empty()) {
					continue;
				}

				TestResultFormat format = bind_data.format;
				std::string format_name = bind_data.format_name;

				if (format == TestResultFormat::AUTO) {
					format_name = DetectFormat(content);
					if (!format_name.empty()) {
						format = TestResultFormat::UNKNOWN;
					}
				}

				std::vector<ValidationEvent> file_events;
				if (format == TestResultFormat::REGEXP) {
					file_events = ParseContentRegexp(content, bind_data.regexp_pattern, bind_data.include_unparsed);
				} else if (!format_name.empty() && format_name != "unknown" && format_name != "auto") {
					file_events = ParseContent(context.client, content, format_name);
				}

				for (auto &event : file_events) {
					if (event.log_file.empty()) {
						event.log_file = file_path;
					}
				}

				if (bind_data.context_lines > 0) {
					std::vector<std::string> lines;
					std::istringstream stream(content);
					std::string line;
					while (std::getline(stream, line)) {
						lines.push_back(line);
					}
					lstate.log_lines_by_file[file_path] = std::move(lines);
				}

				lstate.events.insert(lstate.events.end(), file_events.begin(), file_events.end());
			}
			lstate.streaming_mode = false;
		} else {
			// Single file - check if we can use streaming mode
			TestResultFormat format = bind_data.format;
			std::string format_name = bind_data.format_name;

			// First, peek at the file for format detection if needed
			if (format == TestResultFormat::AUTO) {
				std::string peek_content;
				try {
					peek_content = PeekContentFromSource(context.client, source_path, SNIFF_BUFFER_SIZE);
				} catch (const IOException &) {
					output.SetCardinality(0);
					lstate.initialized = false;
					return OperatorResultType::NEED_MORE_INPUT;
				}
				format_name = DetectFormat(peek_content);
			}

			// Check if the parser supports streaming
			IParser *parser = nullptr;
			if (!format_name.empty() && format_name != "unknown" && format_name != "auto" &&
			    format != TestResultFormat::REGEXP) {
				parser = ParserRegistry::getInstance().getParser(format_name);
			}

			// Use streaming mode if:
			// - Parser exists and supports streaming
			// - Not using REGEXP format (requires full content)
			// - Not using context lines (would need full file for context)
			bool can_stream = parser && parser->supportsStreaming() && format != TestResultFormat::REGEXP &&
			                  bind_data.context_lines == 0;

			if (can_stream) {
				// Initialize streaming mode
				try {
					lstate.line_reader = make_uniq<LineReader>(context.client, source_path);
				} catch (const IOException &) {
					output.SetCardinality(0);
					lstate.initialized = false;
					return OperatorResultType::NEED_MORE_INPUT;
				}
				lstate.streaming_mode = true;
				lstate.streaming_parser = parser;
				lstate.streaming_event_id = 0;
				lstate.output_offset = 0;
				lstate.initialized = true;
			} else {
				// Fall back to batch mode
				std::string content;
				try {
					content = ReadContentFromSource(context.client, source_path);
				} catch (const IOException &) {
					output.SetCardinality(0);
					lstate.initialized = false;
					return OperatorResultType::NEED_MORE_INPUT;
				}

				if (!content.empty()) {
					if (format == TestResultFormat::AUTO && format_name.empty()) {
						format_name = DetectFormat(content);
					}

					std::vector<ValidationEvent> file_events;
					if (format == TestResultFormat::REGEXP) {
						file_events = ParseContentRegexp(content, bind_data.regexp_pattern, bind_data.include_unparsed);
					} else if (!format_name.empty() && format_name != "unknown" && format_name != "auto") {
						file_events = ParseContent(context.client, content, format_name);
					}

					for (auto &event : file_events) {
						if (event.log_file.empty()) {
							event.log_file = source_path;
						}
					}

					if (bind_data.context_lines > 0) {
						std::vector<std::string> lines;
						std::istringstream stream(content);
						std::string line;
						while (std::getline(stream, line)) {
							lines.push_back(line);
						}
						lstate.log_lines_by_file[source_path] = std::move(lines);
					}

					lstate.events = std::move(file_events);
				}
				lstate.streaming_mode = false;
			}
		}

		// For batch mode: post-process events
		if (!lstate.streaming_mode) {
			if (lstate.events.empty()) {
				output.SetCardinality(0);
				lstate.initialized = false;
				lstate.log_lines_by_file.clear();
				return OperatorResultType::NEED_MORE_INPUT;
			}

			ProcessErrorPatterns(lstate.events);

			if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
				auto new_end = std::remove_if(lstate.events.begin(), lstate.events.end(),
				                              [&bind_data](const ValidationEvent &event) {
					                              return !ShouldEmitEvent(event.severity, bind_data.severity_threshold);
				                              });
				lstate.events.erase(new_end, lstate.events.end());
			}
		}

		lstate.output_offset = 0;
		lstate.initialized = true;
	}

	// Streaming mode: read lines and parse incrementally
	if (lstate.streaming_mode) {
		lstate.events.clear();

		// Read lines until we have enough events or reach EOF
		while (lstate.events.size() < STANDARD_VECTOR_SIZE && lstate.line_reader->HasNext()) {
			std::string line = lstate.line_reader->NextLine();
			int32_t line_number = lstate.line_reader->CurrentLineNumber();

			auto line_events = lstate.streaming_parser->parseLine(line, line_number, lstate.streaming_event_id);

			for (auto &event : line_events) {
				// Apply severity filtering
				if (bind_data.severity_threshold != SeverityLevel::DEBUG &&
				    !ShouldEmitEvent(event.severity, bind_data.severity_threshold)) {
					continue;
				}

				if (event.log_file.empty()) {
					event.log_file = lstate.current_file_path;
				}
				lstate.events.push_back(std::move(event));

				if (lstate.events.size() >= STANDARD_VECTOR_SIZE) {
					break;
				}
			}
		}

		if (lstate.events.empty()) {
			// No more events - done with this file
			output.SetCardinality(0);
			lstate.initialized = false;
			lstate.streaming_mode = false;
			lstate.line_reader.reset();
			lstate.streaming_parser = nullptr;
			lstate.current_file_path.clear();
			return OperatorResultType::NEED_MORE_INPUT;
		}

		// Populate output from streamed events
		idx_t output_size = std::min(lstate.events.size(), static_cast<size_t>(STANDARD_VECTOR_SIZE));
		PopulateDataChunkFromEvents(output, lstate.events, 0, output_size, bind_data.content_mode,
		                            bind_data.content_limit, 0, nullptr);

		return OperatorResultType::HAVE_MORE_OUTPUT;
	}

	// Batch mode: output from pre-parsed events
	idx_t remaining = lstate.events.size() > lstate.output_offset ? lstate.events.size() - lstate.output_offset : 0;
	idx_t output_size = std::min(remaining, static_cast<idx_t>(STANDARD_VECTOR_SIZE));

	PopulateDataChunkFromEvents(output, lstate.events, lstate.output_offset, output_size, bind_data.content_mode,
	                            bind_data.content_limit, bind_data.context_lines,
	                            bind_data.context_lines > 0 ? &lstate.log_lines_by_file : nullptr);

	lstate.output_offset += output_size;

	if (output.size() == 0) {
		lstate.initialized = false;
		lstate.events.clear();
		lstate.log_lines_by_file.clear();
		lstate.output_offset = 0;
		lstate.current_file_path.clear();
		return OperatorResultType::NEED_MORE_INPUT;
	}

	return OperatorResultType::HAVE_MORE_OUTPUT;
}

TableFunctionSet GetReadDuckHuntLogFunction() {
	TableFunctionSet set("read_duck_hunt_log");

	// Single argument version: read_duck_hunt_log(source) - auto-detects format
	// Uses in-out function pattern for LATERAL join support
	TableFunction single_arg("read_duck_hunt_log", {LogicalType::VARCHAR}, nullptr, ReadDuckHuntLogBind,
	                         ReadDuckHuntLogInOutInitGlobal, ReadDuckHuntLogInOutInitLocal);
	single_arg.in_out_function = ReadDuckHuntLogInOutFunction;
	single_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	single_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	single_arg.named_parameters["content"] = LogicalType::ANY;
	single_arg.named_parameters["context"] = LogicalType::INTEGER;
	single_arg.named_parameters["include_unparsed"] = LogicalType::BOOLEAN;
	set.AddFunction(single_arg);

	// Two argument version: read_duck_hunt_log(source, format)
	// Uses in-out function pattern for LATERAL join support
	TableFunction two_arg("read_duck_hunt_log", {LogicalType::VARCHAR, LogicalType::VARCHAR}, nullptr,
	                      ReadDuckHuntLogBind, ReadDuckHuntLogInOutInitGlobal, ReadDuckHuntLogInOutInitLocal);
	two_arg.in_out_function = ReadDuckHuntLogInOutFunction;
	two_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	two_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	two_arg.named_parameters["content"] = LogicalType::ANY;
	two_arg.named_parameters["context"] = LogicalType::INTEGER;
	two_arg.named_parameters["include_unparsed"] = LogicalType::BOOLEAN;
	set.AddFunction(two_arg);

	return set;
}

TableFunctionSet GetParseDuckHuntLogFunction() {
	TableFunctionSet set("parse_duck_hunt_log");

	// Single argument version: parse_duck_hunt_log(content) - auto-detects format
	// Uses in-out function pattern for LATERAL join support
	TableFunction single_arg("parse_duck_hunt_log", {LogicalType::VARCHAR}, nullptr, ParseDuckHuntLogBind,
	                         ParseDuckHuntLogInOutInitGlobal, ParseDuckHuntLogInOutInitLocal);
	single_arg.in_out_function = ParseDuckHuntLogInOutFunction;
	single_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	single_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	single_arg.named_parameters["content"] = LogicalType::ANY;
	single_arg.named_parameters["context"] = LogicalType::INTEGER;
	single_arg.named_parameters["include_unparsed"] = LogicalType::BOOLEAN;
	set.AddFunction(single_arg);

	// Two argument version: parse_duck_hunt_log(content, format)
	// Uses in-out function pattern for LATERAL join support
	TableFunction two_arg("parse_duck_hunt_log", {LogicalType::VARCHAR, LogicalType::VARCHAR}, nullptr,
	                      ParseDuckHuntLogBind, ParseDuckHuntLogInOutInitGlobal, ParseDuckHuntLogInOutInitLocal);
	two_arg.in_out_function = ParseDuckHuntLogInOutFunction;
	two_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	two_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	two_arg.named_parameters["content"] = LogicalType::ANY;
	two_arg.named_parameters["context"] = LogicalType::INTEGER;
	two_arg.named_parameters["include_unparsed"] = LogicalType::BOOLEAN;
	set.AddFunction(two_arg);

	return set;
}

// Inline parsers moved to modular files:
// - ParseBazelBuild: src/parsers/build_systems/bazel_parser.cpp
// - ParseAutopep8Text: src/parsers/linting_tools/autopep8_text_parser.cpp
// - ParsePytestCovText: src/parsers/test_frameworks/pytest_cov_text_parser.cpp
// - ParseDuckDBTestOutput: src/parsers/test_frameworks/duckdb_test_parser.cpp
// - ParseGenericLint: src/parsers/tool_outputs/generic_lint_parser.cpp
// - ParseWithRegexp: src/parsers/tool_outputs/regexp_parser.cpp

// Multi-file processing utilities moved to src/core/file_utils.cpp:
// - GetFilesFromPattern
// - GetGlobFiles
// - ProcessMultipleFiles
// - ExtractBuildIdFromPath
// - ExtractEnvironmentFromPath

} // namespace duckdb
