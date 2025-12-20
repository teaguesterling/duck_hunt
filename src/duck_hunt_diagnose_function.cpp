#include "include/duck_hunt_diagnose_function.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/file_system.hpp"
#include <vector>

namespace duckdb {

// Forward declaration from read_duck_hunt_log_function.cpp
std::string ReadContentFromSource(ClientContext& context, const std::string& source);

// Diagnosis result for a single parser
struct DiagnosisEntry {
    std::string format_name;
    int priority;
    bool can_parse;
    int64_t events_produced;
    bool is_selected;
};

// Bind data - stores content and diagnosis results
struct DuckHuntDiagnoseBindData : public TableFunctionData {
    std::string content;
    std::string source_path;  // Only for diagnose_read
    std::vector<DiagnosisEntry> results;
    bool is_file_mode;

    DuckHuntDiagnoseBindData() : is_file_mode(false) {}
};

// Global state for iteration
struct DuckHuntDiagnoseGlobalState : public GlobalTableFunctionState {
    idx_t current_idx;

    DuckHuntDiagnoseGlobalState() : current_idx(0) {}
};

// Run diagnosis on content against all registered parsers
static void RunDiagnosis(const std::string& content, std::vector<DiagnosisEntry>& results) {
    auto& registry = ParserRegistry::getInstance();
    auto all_formats = registry.getAllFormats();

    // First pass: find which parser would be selected by auto-detect
    IParser* selected_parser = registry.findParser(content);
    std::string selected_format = selected_parser ? selected_parser->getFormatName() : "";

    // Test each parser
    for (const auto& info : all_formats) {
        IParser* parser = registry.getParser(info.format_name);
        if (!parser) {
            continue;  // Skip meta formats like "auto"
        }

        DiagnosisEntry entry;
        entry.format_name = info.format_name;
        entry.priority = info.priority;
        entry.can_parse = false;
        entry.events_produced = 0;
        entry.is_selected = (info.format_name == selected_format);

        try {
            entry.can_parse = parser->canParse(content);

            // Only run parse() if canParse is true
            if (entry.can_parse) {
                auto events = parser->parse(content);
                entry.events_produced = static_cast<int64_t>(events.size());
            }
        } catch (...) {
            // Parser threw an exception - mark as can't parse
            entry.can_parse = false;
            entry.events_produced = 0;
        }

        results.push_back(entry);
    }

    // Sort by priority descending (same order as auto-detect)
    std::stable_sort(results.begin(), results.end(),
                     [](const DiagnosisEntry& a, const DiagnosisEntry& b) {
                         return a.priority > b.priority;
                     });
}

// Bind function for diagnose_parse (string content)
static unique_ptr<FunctionData> DuckHuntDiagnoseParseBindFunc(ClientContext &context,
                                                               TableFunctionBindInput &input,
                                                               vector<LogicalType> &return_types,
                                                               vector<string> &names) {
    // Define return schema
    return_types = {
        LogicalType::VARCHAR,   // format
        LogicalType::INTEGER,   // priority
        LogicalType::BOOLEAN,   // can_parse
        LogicalType::BIGINT,    // events_produced
        LogicalType::BOOLEAN    // is_selected
    };

    names = {
        "format",
        "priority",
        "can_parse",
        "events_produced",
        "is_selected"
    };

    if (input.inputs.empty()) {
        throw BinderException("duck_hunt_diagnose_parse requires a content parameter");
    }

    auto bind_data = make_uniq<DuckHuntDiagnoseBindData>();
    bind_data->content = input.inputs[0].ToString();
    bind_data->is_file_mode = false;

    // Run diagnosis during bind
    RunDiagnosis(bind_data->content, bind_data->results);

    return std::move(bind_data);
}

// Bind function for diagnose_read (file path)
static unique_ptr<FunctionData> DuckHuntDiagnoseReadBindFunc(ClientContext &context,
                                                              TableFunctionBindInput &input,
                                                              vector<LogicalType> &return_types,
                                                              vector<string> &names) {
    // Define return schema
    return_types = {
        LogicalType::VARCHAR,   // format
        LogicalType::INTEGER,   // priority
        LogicalType::BOOLEAN,   // can_parse
        LogicalType::BIGINT,    // events_produced
        LogicalType::BOOLEAN    // is_selected
    };

    names = {
        "format",
        "priority",
        "can_parse",
        "events_produced",
        "is_selected"
    };

    if (input.inputs.empty()) {
        throw BinderException("duck_hunt_diagnose_read requires a file path parameter");
    }

    auto bind_data = make_uniq<DuckHuntDiagnoseBindData>();
    bind_data->source_path = input.inputs[0].ToString();
    bind_data->is_file_mode = true;

    // Read file content
    bind_data->content = ReadContentFromSource(context, bind_data->source_path);

    // Run diagnosis
    RunDiagnosis(bind_data->content, bind_data->results);

    return std::move(bind_data);
}

// Init global state
static unique_ptr<GlobalTableFunctionState> DuckHuntDiagnoseInitGlobal(ClientContext &context,
                                                                        TableFunctionInitInput &input) {
    return make_uniq<DuckHuntDiagnoseGlobalState>();
}

// Execute function (shared by both)
static void DuckHuntDiagnoseFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &bind_data = data_p.bind_data->Cast<DuckHuntDiagnoseBindData>();
    auto &state = data_p.global_state->Cast<DuckHuntDiagnoseGlobalState>();

    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE;

    while (state.current_idx < bind_data.results.size() && count < max_count) {
        const auto &entry = bind_data.results[state.current_idx];

        output.SetValue(0, count, Value(entry.format_name));
        output.SetValue(1, count, Value::INTEGER(entry.priority));
        output.SetValue(2, count, Value::BOOLEAN(entry.can_parse));
        output.SetValue(3, count, Value::BIGINT(entry.events_produced));
        output.SetValue(4, count, Value::BOOLEAN(entry.is_selected));

        state.current_idx++;
        count++;
    }

    output.SetCardinality(count);
}

TableFunction GetDuckHuntDiagnoseParseFunction() {
    TableFunction func("duck_hunt_diagnose_parse", {LogicalType::VARCHAR},
                       DuckHuntDiagnoseFunction, DuckHuntDiagnoseParseBindFunc,
                       DuckHuntDiagnoseInitGlobal);
    return func;
}

TableFunction GetDuckHuntDiagnoseReadFunction() {
    TableFunction func("duck_hunt_diagnose_read", {LogicalType::VARCHAR},
                       DuckHuntDiagnoseFunction, DuckHuntDiagnoseReadBindFunc,
                       DuckHuntDiagnoseInitGlobal);
    return func;
}

} // namespace duckdb
