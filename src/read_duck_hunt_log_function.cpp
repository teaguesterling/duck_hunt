#include "include/read_duck_hunt_log_function.hpp"
#include "include/validation_event_types.hpp"
#include "core/parser_registry.hpp"
#include "core/parse_content.hpp"
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

std::string ReadContentFromSource(ClientContext &context, const std::string &source) {
	// Use DuckDB's FileSystem to properly handle file paths including UNITTEST_ROOT_DIRECTORY
	auto &fs = FileSystem::GetFileSystem(context);

	// Open the file with automatic compression detection based on file extension
	// This handles .gz, .zst, etc. transparently
	auto flags = FileFlags::FILE_FLAGS_READ | FileCompressionType::AUTO_DETECT;
	auto file_handle = fs.OpenFile(source, flags);

	// Check compression type - for compressed files we can't seek or get size upfront
	auto compression = file_handle->GetFileCompressionType();
	bool can_get_size = (compression == FileCompressionType::UNCOMPRESSED) && file_handle->CanSeek();

	if (can_get_size) {
		// Uncompressed file - read using known size for efficiency
		auto file_size = file_handle->GetFileSize();
		if (file_size > 0) {
			std::string content;
			content.resize(static_cast<size_t>(file_size));
			file_handle->Read((void *)content.data(), file_size);
			return content;
		}
	}

	// Compressed files, pipes, or empty files - read in chunks until EOF
	std::string content;
	constexpr size_t chunk_size = 8192;
	char buffer[chunk_size];

	while (true) {
		auto bytes_read = file_handle->Read(buffer, chunk_size);
		if (bytes_read == 0) {
			break; // EOF
		}
		content.append(buffer, static_cast<size_t>(bytes_read));
	}

	return content;
}

bool IsValidJSON(const std::string &content) {
	// Simple heuristic - starts with { or [
	std::string trimmed = content;
	StringUtil::Trim(trimmed);
	return !trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '[');
}

unique_ptr<FunctionData> ReadDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names) {
	auto bind_data = make_uniq<ReadDuckHuntLogBindData>();

	// Get source parameter (required)
	if (input.inputs.empty()) {
		throw BinderException("read_duck_hunt_log requires at least one parameter (source)");
	}
	bind_data->source = input.inputs[0].ToString();

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
			bind_data->format_name = "regexp"; // Canonical name for display
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
		try {
			content = ReadContentFromSource(context, source_path);
		} catch (const IOException &) {
			// If file reading fails, treat source as direct content
			content = bind_data.source;
		}

		// Auto-detect format if needed
		TestResultFormat format = bind_data.format;
		std::string format_name = bind_data.format_name;
		if (format == TestResultFormat::AUTO) {
			format_name = DetectFormat(content);
			if (!format_name.empty()) {
				format = TestResultFormat::UNKNOWN; // Use UNKNOWN so we use registry-based parsing below
			}
		}

		// Parse content using core API
		if (format == TestResultFormat::REGEXP) {
			// REGEXP is special - requires user-provided pattern
			auto events = ParseContentRegexp(content, bind_data.regexp_pattern);
			global_state->events.insert(global_state->events.end(), events.begin(), events.end());
		} else if (!format_name.empty() && format_name != "unknown" && format_name != "auto") {
			auto events = ParseContent(context, content, format_name);
			global_state->events.insert(global_state->events.end(), events.begin(), events.end());
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

std::string TruncateLogContent(const std::string &content, ContentMode mode, int32_t limit, int32_t event_line_start,
                               int32_t event_line_end) {
	switch (mode) {
	case ContentMode::FULL:
		return content;

	case ContentMode::NONE:
		return ""; // Will be converted to NULL in output

	case ContentMode::LIMIT:
		if (content.length() <= static_cast<size_t>(limit)) {
			return content;
		}
		return content.substr(0, limit) + "...";

	case ContentMode::SMART: {
		// Smart truncation: try to preserve context around event lines
		if (content.length() <= static_cast<size_t>(limit)) {
			return content;
		}

		// If we have line info, try to extract around those lines
		if (event_line_start > 0) {
			std::vector<std::string> lines;
			std::istringstream stream(content);
			std::string line;
			while (std::getline(stream, line)) {
				lines.push_back(line);
			}

			// Calculate which lines to include
			int32_t start_line = std::max(0, event_line_start - 2);
			int32_t end_line = std::min(static_cast<int32_t>(lines.size()),
			                            (event_line_end > 0 ? event_line_end : event_line_start) + 2);

			std::string result;
			if (start_line > 0) {
				result = "...\n";
			}
			for (int32_t i = start_line; i < end_line && i < static_cast<int32_t>(lines.size()); i++) {
				result += lines[i] + "\n";
			}
			if (end_line < static_cast<int32_t>(lines.size())) {
				result += "...";
			}

			// If still too long, fall back to simple truncation
			if (result.length() > static_cast<size_t>(limit)) {
				return result.substr(0, limit) + "...";
			}
			return result;
		}

		// No line info, fall back to simple truncation
		return content.substr(0, limit) + "...";
	}

	default:
		return content;
	}
}

// Get the LogicalType for the context column: LIST(STRUCT(line_number, content, is_event))
LogicalType GetContextColumnType() {
	// Context line struct: {line_number: INTEGER, content: VARCHAR, is_event: BOOLEAN}
	child_list_t<LogicalType> line_struct_children;
	line_struct_children.push_back(make_pair("line_number", LogicalType::INTEGER));
	line_struct_children.push_back(make_pair("content", LogicalType::VARCHAR));
	line_struct_children.push_back(make_pair("is_event", LogicalType::BOOLEAN));
	auto line_struct_type = LogicalType::STRUCT(std::move(line_struct_children));

	// Return LIST of line structs directly (no wrapper struct)
	return LogicalType::LIST(line_struct_type);
}

// Extract context lines around an event - returns LIST(STRUCT(line_number, content, is_event))
Value ExtractContext(const std::vector<std::string> &log_lines, int32_t event_line_start, int32_t event_line_end,
                     int32_t context_lines) {
	// If no line information, return NULL
	if (event_line_start <= 0 || log_lines.empty()) {
		return Value();
	}

	// Convert 1-indexed line numbers to 0-indexed
	int32_t start_idx = event_line_start - 1;
	int32_t end_idx = (event_line_end > 0 ? event_line_end : event_line_start) - 1;

	// Calculate context window (clamped to valid range)
	int32_t context_start = std::max(0, start_idx - context_lines);
	int32_t context_end = std::min(static_cast<int32_t>(log_lines.size()) - 1, end_idx + context_lines);

	// Build the lines list directly (no wrapper struct)
	vector<Value> lines_list;
	for (int32_t i = context_start; i <= context_end; i++) {
		// Determine if this line is part of the event
		bool is_event_line = (i >= start_idx && i <= end_idx);

		// Create line struct: {line_number, content, is_event}
		child_list_t<Value> line_values;
		line_values.push_back(make_pair("line_number", Value::INTEGER(i + 1))); // 1-indexed
		line_values.push_back(make_pair("content", Value(log_lines[i])));
		line_values.push_back(make_pair("is_event", Value::BOOLEAN(is_event_line)));

		lines_list.push_back(Value::STRUCT(std::move(line_values)));
	}

	// Return the list directly
	return Value::LIST(std::move(lines_list));
}

void PopulateDataChunkFromEvents(DataChunk &output, const std::vector<ValidationEvent> &events, idx_t start_offset,
                                 idx_t chunk_size, ContentMode content_mode, int32_t content_limit,
                                 int32_t context_lines,
                                 const std::unordered_map<std::string, std::vector<std::string>> *log_lines_by_file) {
	idx_t events_remaining = events.size() > start_offset ? events.size() - start_offset : 0;
	idx_t output_size = std::min(chunk_size, events_remaining);

	if (output_size == 0) {
		output.SetCardinality(0);
		return;
	}

	output.SetCardinality(output_size);

	for (idx_t i = 0; i < output_size; i++) {
		const auto &event = events[start_offset + i];
		idx_t col = 0;

		// Core identification
		output.SetValue(col++, i, Value::BIGINT(event.event_id));
		output.SetValue(col++, i, Value(event.tool_name));
		output.SetValue(col++, i, Value(ValidationEventTypeToString(event.event_type)));
		// Code location
		output.SetValue(col++, i, Value(event.ref_file));
		output.SetValue(col++, i, event.ref_line == -1 ? Value() : Value::INTEGER(event.ref_line));
		output.SetValue(col++, i, event.ref_column == -1 ? Value() : Value::INTEGER(event.ref_column));
		output.SetValue(col++, i, Value(event.function_name));
		// Classification
		output.SetValue(col++, i, Value(ValidationEventStatusToString(event.status)));
		output.SetValue(col++, i, Value(event.severity));
		output.SetValue(col++, i, Value(event.category));
		output.SetValue(col++, i, Value(event.error_code));
		// Content
		output.SetValue(col++, i, Value(event.message));
		output.SetValue(col++, i, Value(event.suggestion));

		// Apply content truncation to log_content
		if (content_mode == ContentMode::NONE) {
			output.SetValue(col++, i, Value());
		} else {
			std::string truncated = TruncateLogContent(event.log_content, content_mode, content_limit,
			                                           event.log_line_start, event.log_line_end);
			output.SetValue(col++, i, truncated.empty() ? Value() : Value(truncated));
		}
		output.SetValue(col++, i, Value(event.structured_data));
		// Log tracking
		output.SetValue(col++, i, event.log_line_start == -1 ? Value() : Value::INTEGER(event.log_line_start));
		output.SetValue(col++, i, event.log_line_end == -1 ? Value() : Value::INTEGER(event.log_line_end));
		output.SetValue(col++, i, event.log_file.empty() ? Value() : Value(event.log_file));
		// Test-specific
		output.SetValue(col++, i, Value(event.test_name));
		output.SetValue(col++, i, Value::DOUBLE(event.execution_time));
		// Identity & Network
		output.SetValue(col++, i, event.principal.empty() ? Value() : Value(event.principal));
		output.SetValue(col++, i, event.origin.empty() ? Value() : Value(event.origin));
		output.SetValue(col++, i, event.target.empty() ? Value() : Value(event.target));
		output.SetValue(col++, i, event.actor_type.empty() ? Value() : Value(event.actor_type));
		// Temporal
		output.SetValue(col++, i, event.started_at.empty() ? Value() : Value(event.started_at));
		// Correlation
		output.SetValue(col++, i, event.external_id.empty() ? Value() : Value(event.external_id));
		// Hierarchical context
		output.SetValue(col++, i, event.scope.empty() ? Value() : Value(event.scope));
		output.SetValue(col++, i, event.scope_id.empty() ? Value() : Value(event.scope_id));
		output.SetValue(col++, i, event.scope_status.empty() ? Value() : Value(event.scope_status));
		output.SetValue(col++, i, event.group.empty() ? Value() : Value(event.group));
		output.SetValue(col++, i, event.group_id.empty() ? Value() : Value(event.group_id));
		output.SetValue(col++, i, event.group_status.empty() ? Value() : Value(event.group_status));
		output.SetValue(col++, i, event.unit.empty() ? Value() : Value(event.unit));
		output.SetValue(col++, i, event.unit_id.empty() ? Value() : Value(event.unit_id));
		output.SetValue(col++, i, event.unit_status.empty() ? Value() : Value(event.unit_status));
		output.SetValue(col++, i, event.subunit.empty() ? Value() : Value(event.subunit));
		output.SetValue(col++, i, event.subunit_id.empty() ? Value() : Value(event.subunit_id));
		// Pattern analysis
		output.SetValue(col++, i, event.fingerprint.empty() ? Value() : Value(event.fingerprint));
		output.SetValue(col++, i, event.similarity_score == 0.0 ? Value() : Value::DOUBLE(event.similarity_score));
		output.SetValue(col++, i, event.pattern_id == -1 ? Value() : Value::BIGINT(event.pattern_id));

		// Context column (if requested)
		if (context_lines > 0 && log_lines_by_file != nullptr) {
			// Find the log lines for this event's file
			std::string file_key = event.log_file.empty() ? "" : event.log_file;
			auto it = log_lines_by_file->find(file_key);
			if (it != log_lines_by_file->end()) {
				output.SetValue(col++, i,
				                ExtractContext(it->second, event.log_line_start, event.log_line_end, context_lines));
			} else {
				output.SetValue(col++, i, Value()); // NULL if no lines available
			}
		}
	}
}

// Parse test results implementation for string input
unique_ptr<FunctionData> ParseDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
	auto bind_data = make_uniq<ReadDuckHuntLogBindData>();

	// Get content parameter (required)
	if (input.inputs.empty()) {
		throw BinderException("parse_duck_hunt_log requires at least one parameter (content)");
	}
	bind_data->source = input.inputs[0].ToString();

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
			bind_data->format_name = "regexp"; // Canonical name for display
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
		auto events = ParseContentRegexp(content, bind_data.regexp_pattern);
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

TableFunctionSet GetReadDuckHuntLogFunction() {
	TableFunctionSet set("read_duck_hunt_log");

	// Single argument version: read_duck_hunt_log(source) - auto-detects format
	TableFunction single_arg("read_duck_hunt_log", {LogicalType::VARCHAR}, ReadDuckHuntLogFunction, ReadDuckHuntLogBind,
	                         ReadDuckHuntLogInitGlobal, ReadDuckHuntLogInitLocal);
	single_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	single_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	single_arg.named_parameters["content"] = LogicalType::ANY;
	single_arg.named_parameters["context"] = LogicalType::INTEGER;
	set.AddFunction(single_arg);

	// Two argument version: read_duck_hunt_log(source, format)
	TableFunction two_arg("read_duck_hunt_log", {LogicalType::VARCHAR, LogicalType::VARCHAR}, ReadDuckHuntLogFunction,
	                      ReadDuckHuntLogBind, ReadDuckHuntLogInitGlobal, ReadDuckHuntLogInitLocal);
	two_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	two_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	two_arg.named_parameters["content"] = LogicalType::ANY;
	two_arg.named_parameters["context"] = LogicalType::INTEGER;
	set.AddFunction(two_arg);

	return set;
}

TableFunctionSet GetParseDuckHuntLogFunction() {
	TableFunctionSet set("parse_duck_hunt_log");

	// Single argument version: parse_duck_hunt_log(content) - auto-detects format
	TableFunction single_arg("parse_duck_hunt_log", {LogicalType::VARCHAR}, ParseDuckHuntLogFunction,
	                         ParseDuckHuntLogBind, ParseDuckHuntLogInitGlobal, ParseDuckHuntLogInitLocal);
	single_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	single_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	single_arg.named_parameters["content"] = LogicalType::ANY;
	single_arg.named_parameters["context"] = LogicalType::INTEGER;
	set.AddFunction(single_arg);

	// Two argument version: parse_duck_hunt_log(content, format)
	TableFunction two_arg("parse_duck_hunt_log", {LogicalType::VARCHAR, LogicalType::VARCHAR}, ParseDuckHuntLogFunction,
	                      ParseDuckHuntLogBind, ParseDuckHuntLogInitGlobal, ParseDuckHuntLogInitLocal);
	two_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	two_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	two_arg.named_parameters["content"] = LogicalType::ANY;
	two_arg.named_parameters["context"] = LogicalType::INTEGER;
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

// Phase 3A: Multi-file processing implementation

std::vector<std::string> GetFilesFromPattern(ClientContext &context, const std::string &pattern) {
	auto &fs = FileSystem::GetFileSystem(context);
	std::vector<std::string> result;

	// Helper lambda to handle individual file paths (adapted from duckdb_yaml)
	auto processPath = [&](const std::string &file_path) {
		// First: check if we're dealing with just a single file that exists
		if (fs.FileExists(file_path)) {
			result.push_back(file_path);
			return;
		}

		// Second: attempt to use the path as a glob
		auto glob_files = GetGlobFiles(context, file_path);
		if (glob_files.size() > 0) {
			result.insert(result.end(), glob_files.begin(), glob_files.end());
			return;
		}

		// Third: if it looks like a directory, try to glob common test result files
		if (StringUtil::EndsWith(file_path, "/")) {
			// Common test result file patterns
			std::vector<std::string> patterns = {"*.xml", "*.json", "*.txt", "*.log", "*.out"};
			for (const auto &ext_pattern : patterns) {
				auto files = GetGlobFiles(context, fs.JoinPath(file_path, ext_pattern));
				result.insert(result.end(), files.begin(), files.end());
			}
			return;
		}

		// If file doesn't exist and isn't a valid glob, throw error
		throw IOException("File or directory does not exist: " + file_path);
	};

	processPath(pattern);
	return result;
}

std::vector<std::string> GetGlobFiles(ClientContext &context, const std::string &pattern) {
	auto &fs = FileSystem::GetFileSystem(context);
	std::vector<std::string> result;

	// Don't bother if we can't identify a glob pattern
	try {
		bool has_glob = fs.HasGlob(pattern);
		if (!has_glob) {
			// For remote URLs, still try GlobFiles as it has better support
			if (pattern.find("://") == std::string::npos || pattern.find("file://") == 0) {
				return result;
			}
		}
	} catch (const NotImplementedException &) {
		// If HasGlob is not implemented, still try GlobFiles for remote URLs
		if (pattern.find("://") == std::string::npos || pattern.find("file://") == 0) {
			return result;
		}
	}

	// Use GlobFiles which handles extension auto-loading and directory filtering
	try {
		auto glob_files = fs.GlobFiles(pattern, context, FileGlobOptions::ALLOW_EMPTY);
		for (auto &file : glob_files) {
			result.push_back(file.path);
		}
	} catch (const NotImplementedException &) {
		// No glob support available
		return result;
	} catch (const IOException &) {
		// Glob failed, return empty result
		return result;
	}

	return result;
}

void ProcessMultipleFiles(ClientContext &context, const std::vector<std::string> &files, TestResultFormat format,
                          const std::string &format_name, std::vector<ValidationEvent> &events, bool ignore_errors) {
	for (size_t file_idx = 0; file_idx < files.size(); file_idx++) {
		const auto &file_path = files[file_idx];

		try {
			// Read file content
			std::string content = ReadContentFromSource(context, file_path);

			// Detect format if AUTO, otherwise use provided format_name
			std::string effective_format_name = format_name;
			if (format == TestResultFormat::AUTO) {
				effective_format_name = DetectFormat(content);
				if (effective_format_name.empty()) {
					continue; // No parser found, skip file
				}
			}

			// Skip REGEXP format in multi-file mode (requires pattern)
			if (format == TestResultFormat::REGEXP) {
				continue;
			}

			// Parse content using core API
			auto file_events = ParseContent(context, content, effective_format_name);
			if (file_events.empty()) {
				continue; // No events parsed, skip file
			}

			// Set log_file on each event to track source file
			for (auto &event : file_events) {
				event.log_file = file_path;
			}

			// Add events to main collection
			events.insert(events.end(), file_events.begin(), file_events.end());

		} catch (const IOException &e) {
			// IOException (file not found, can't read, etc.) - always skip and continue
			continue;
		} catch (const std::exception &e) {
			// Parsing errors - skip if ignore_errors, otherwise rethrow
			if (ignore_errors) {
				continue;
			}
			throw;
		}
	}
}

std::string ExtractBuildIdFromPath(const std::string &file_path) {
	// Extract build ID from common patterns like:
	// - /builds/build-123/results.xml -> "build-123"
	// - /ci-logs/pipeline-456/test.log -> "pipeline-456"
	// - /artifacts/20231201-142323/output.txt -> "20231201-142323"

	std::regex build_patterns[] = {
	    std::regex(R"(/(?:build|pipeline|run|job)-([^/\s]+)/)"), // build-123, pipeline-456
	    std::regex(R"(/(\d{8}-\d{6})/)"),                        // 20231201-142323
	    std::regex(R"(/(?:builds?|ci|artifacts)/([^/\s]+)/)"),   // builds/abc123, ci/def456
	    std::regex(R"([_-](\w+\d+)[_-])"),                       // any_build123_ pattern
	};

	for (const auto &pattern : build_patterns) {
		std::smatch match;
		if (std::regex_search(file_path, match, pattern)) {
			return match[1].str();
		}
	}

	return ""; // No build ID found
}

std::string ExtractEnvironmentFromPath(const std::string &file_path) {
	// Extract environment from common patterns like:
	// - /environments/dev/results.xml -> "dev"
	// - /staging/ci-logs/test.log -> "staging"
	// - /prod/artifacts/output.txt -> "prod"

	std::vector<std::string> environments = {"dev",  "development", "staging", "stage",
	                                         "prod", "production",  "test",    "testing"};

	for (const auto &env : environments) {
		if (file_path.find("/" + env + "/") != std::string::npos ||
		    file_path.find("-" + env + "-") != std::string::npos ||
		    file_path.find("_" + env + "_") != std::string::npos) {
			return env;
		}
	}

	return ""; // No environment found
}

} // namespace duckdb
