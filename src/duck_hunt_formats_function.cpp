#include "include/duck_hunt_formats_function.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/exception.hpp"
#include <vector>

namespace duckdb {

// Bind data - pulls from actual registry
struct DuckHuntFormatsBindData : public TableFunctionData {
	std::vector<ParserInfo> formats;

	DuckHuntFormatsBindData() {
		// Get formats from the actual parser registry
		auto &registry = ParserRegistry::getInstance();
		formats = registry.getAllFormats();

		// Add meta formats that aren't parsers
		ParserInfo auto_format;
		auto_format.format_name = "auto";
		auto_format.description = "Automatic format detection";
		auto_format.category = "meta";
		auto_format.required_extension = "";
		auto_format.priority = 0;
		formats.insert(formats.begin(), auto_format);
	}
};

// Global state
struct DuckHuntFormatsGlobalState : public GlobalTableFunctionState {
	idx_t current_idx;

	DuckHuntFormatsGlobalState() : current_idx(0) {
	}
};

// Determine if a category supports workflow parsing
static bool CategorySupportsWorkflow(const std::string &category) {
	return category == "ci_system" || category == "workflow" || category.find("ci") != std::string::npos;
}

static unique_ptr<FunctionData> DuckHuntFormatsBind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<string> &names) {
	// Define struct type for command patterns: STRUCT(pattern VARCHAR, pattern_type VARCHAR)
	child_list_t<LogicalType> pattern_struct_children;
	pattern_struct_children.push_back(make_pair("pattern", LogicalType::VARCHAR));
	pattern_struct_children.push_back(make_pair("pattern_type", LogicalType::VARCHAR));
	auto pattern_struct_type = LogicalType::STRUCT(std::move(pattern_struct_children));

	// Define return schema
	return_types = {
	    LogicalType::VARCHAR,                              // format
	    LogicalType::VARCHAR,                              // description
	    LogicalType::VARCHAR,                              // category
	    LogicalType::INTEGER,                              // priority
	    LogicalType::VARCHAR,                              // requires_extension
	    LogicalType::BOOLEAN,                              // supports_workflow
	    LogicalType::LIST(std::move(pattern_struct_type)), // command_patterns
	    LogicalType::LIST(LogicalType::VARCHAR)            // groups
	};

	names = {"format",           "description", "category", "priority", "requires_extension", "supports_workflow",
	         "command_patterns", "groups"};

	return make_uniq<DuckHuntFormatsBindData>();
}

static unique_ptr<GlobalTableFunctionState> DuckHuntFormatsInitGlobal(ClientContext &context,
                                                                      TableFunctionInitInput &input) {
	return make_uniq<DuckHuntFormatsGlobalState>();
}

static void DuckHuntFormatsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<DuckHuntFormatsBindData>();
	auto &state = data_p.global_state->Cast<DuckHuntFormatsGlobalState>();

	idx_t count = 0;
	idx_t max_count = STANDARD_VECTOR_SIZE;

	while (state.current_idx < bind_data.formats.size() && count < max_count) {
		const auto &fmt = bind_data.formats[state.current_idx];

		output.SetValue(0, count, Value(fmt.format_name));
		output.SetValue(1, count, Value(fmt.description));
		output.SetValue(2, count, Value(fmt.category));
		output.SetValue(3, count, Value::INTEGER(fmt.priority));
		output.SetValue(4, count, fmt.required_extension.empty() ? Value() : Value(fmt.required_extension));
		output.SetValue(5, count, Value::BOOLEAN(CategorySupportsWorkflow(fmt.category)));

		// Build command_patterns list
		vector<Value> pattern_list;
		for (const auto &cp : fmt.command_patterns) {
			child_list_t<Value> struct_values;
			struct_values.push_back(make_pair("pattern", Value(cp.pattern)));
			struct_values.push_back(make_pair("pattern_type", Value(cp.pattern_type)));
			pattern_list.push_back(Value::STRUCT(std::move(struct_values)));
		}
		output.SetValue(6, count,
		                Value::LIST(LogicalType::STRUCT(
		                                {{"pattern", LogicalType::VARCHAR}, {"pattern_type", LogicalType::VARCHAR}}),
		                            std::move(pattern_list)));

		// Build groups list
		vector<Value> groups_list;
		for (const auto &group : fmt.groups) {
			groups_list.push_back(Value(group));
		}
		output.SetValue(7, count, Value::LIST(LogicalType::VARCHAR, std::move(groups_list)));

		state.current_idx++;
		count++;
	}

	output.SetCardinality(count);
}

TableFunction GetDuckHuntFormatsFunction() {
	TableFunction func("duck_hunt_formats", {}, DuckHuntFormatsFunction, DuckHuntFormatsBind,
	                   DuckHuntFormatsInitGlobal);
	return func;
}

} // namespace duckdb
