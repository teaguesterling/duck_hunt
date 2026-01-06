#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/function/table_macro_function.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/main/extension/extension_loader.hpp"

namespace duckdb {

/**
 * Create the duck_hunt_match_command_patterns table macro.
 *
 * This macro matches a command string against command patterns from duck_hunt_formats().
 *
 * Usage:
 *   SELECT * FROM duck_hunt_match_command_patterns('pytest tests/');
 *   SELECT * FROM duck_hunt_match_command_patterns('cargo clippy --message-format=json');
 *
 * Returns matching formats with the pattern that matched.
 */
static unique_ptr<CreateMacroInfo> CreateMatchCommandPatternsMacro() {
	// The SQL for the table macro
	// Returns one row per format with matched patterns as a nested list
	// Orders by priority (higher priority first)
	const char *macro_sql = R"(
WITH patterns AS (
    SELECT format, priority, unnest(command_patterns) AS cp
    FROM duck_hunt_formats()
    WHERE len(command_patterns) > 0
),
matches AS (
    SELECT format, priority, cp.pattern AS matched_pattern, cp.pattern_type
    FROM patterns
    WHERE
        CASE cp.pattern_type
            WHEN 'literal' THEN cmd = cp.pattern
            WHEN 'like' THEN cmd LIKE cp.pattern
            WHEN 'regexp' THEN regexp_matches(cmd, cp.pattern)
            ELSE false
        END
)
SELECT
    format,
    max(priority) AS priority,
    list({matched_pattern: matched_pattern, pattern_type: pattern_type}) AS matched_patterns
FROM matches
GROUP BY format
ORDER BY priority DESC, format
)";

	// Parse the SQL
	Parser parser;
	parser.ParseQuery(macro_sql);
	if (parser.statements.size() != 1 || parser.statements[0]->type != StatementType::SELECT_STATEMENT) {
		throw InternalException("Failed to parse duck_hunt_match_command_patterns macro SQL");
	}
	auto node = std::move(parser.statements[0]->Cast<SelectStatement>().node);

	// Create the table macro function
	auto macro_func = make_uniq<TableMacroFunction>(std::move(node));

	// Add parameters
	// Required: cmd (the command to match)
	macro_func->parameters.push_back(make_uniq<ColumnRefExpression>("cmd"));

	// Create the macro info
	auto macro_info = make_uniq<CreateMacroInfo>(CatalogType::TABLE_MACRO_ENTRY);
	macro_info->schema = DEFAULT_SCHEMA;
	macro_info->name = "duck_hunt_match_command_patterns";
	macro_info->temporary = true;
	macro_info->internal = true;
	macro_info->macros.push_back(std::move(macro_func));

	return macro_info;
}

void RegisterDuckHuntMacros(ExtensionLoader &loader) {
	auto match_patterns_macro = CreateMatchCommandPatternsMacro();
	loader.RegisterFunction(*match_patterns_macro);
}

} // namespace duckdb
