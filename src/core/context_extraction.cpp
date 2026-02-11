#include "../include/read_duck_hunt_log_function.hpp"
#include "../include/validation_event_types.hpp"
#include <algorithm>
#include <sstream>

namespace duckdb {

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
		// For UNKNOWN event types (unparsed lines), output NULL for status and severity
		if (event.event_type == ValidationEventType::UNKNOWN) {
			output.SetValue(col++, i, Value()); // NULL status
			output.SetValue(col++, i, Value()); // NULL severity
		} else {
			output.SetValue(col++, i, Value(ValidationEventStatusToString(event.status)));
			output.SetValue(col++, i, Value(event.severity));
		}
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

} // namespace duckdb
