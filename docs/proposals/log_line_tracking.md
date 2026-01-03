# Proposal: Log Line Position Tracking

## Summary

Add `log_line_start` and `log_line_end` fields to the duck_hunt output schema to track where each parsed event originated within the source log file.

## Motivation

When duck_hunt parses a log file, it extracts structured events but loses the connection to the original log file location. This makes it impossible to:

1. **Retrieve context**: "Show me 5 lines before and after this error"
2. **Create stable references**: Link back to specific positions in raw logs
3. **Support drill-down workflows**: Agent gets summary → wants more detail → needs to locate original content

### Current State

```sql
SELECT event_id, ref_line, message
FROM read_duck_hunt_log('build.log', 'make_error');
```

| event_id | ref_line | message |
|----------|----------|---------|
| 1 | 15 | 'undefined_var' undeclared |
| 2 | 28 | unused variable 'temp' |

Here, `ref_line` is the **source code line** (line 15 in `main.c`), not the position in `build.log` where this error message appears.

### Desired State

```sql
SELECT event_id, log_line_start, log_line_end, ref_line, message
FROM read_duck_hunt_log('build.log', 'make_error');
```

| event_id | log_line_start | log_line_end | ref_line | message |
|----------|----------------|--------------|----------|---------|
| 1 | 2 | 4 | 15 | 'undefined_var' undeclared |
| 2 | 6 | 6 | 28 | unused variable 'temp' |

Now we know event 1 spans lines 2-4 of `build.log`, enabling context retrieval.

## Use Case: lq Integration

The `lq` tool (Log Query) captures build/test output and stores it as parquet files. It needs to support:

```bash
# Get structured summary
lq run make --json
# Returns: {"errors": [{"ref": "5:1", "message": "..."}]}

# Drill into specific error with context
lq context 5:1 --lines 3
# Returns: 3 lines before/after error in original log
```

Without `log_line_start`/`log_line_end`, context retrieval requires re-parsing or heuristic matching.

## Proposed Schema Additions

| Field | Type | Description |
|-------|------|-------------|
| `log_line_start` | `INTEGER` | 1-indexed line number where event starts in log file |
| `log_line_end` | `INTEGER` | 1-indexed line number where event ends in log file |
| `log_byte_start` | `BIGINT` | (Optional) Byte offset for large file seeking |
| `log_byte_end` | `BIGINT` | (Optional) Byte offset for large file seeking |

## Implementation Notes

### For Line-Based Parsers

Most parsers process line-by-line. Track the current line number during iteration:

```cpp
int current_line = 0;
for (const auto& line : lines) {
    current_line++;
    if (auto event = parse_line(line)) {
        event->log_line_start = current_line;
        event->log_line_end = current_line;
        emit(event);
    }
}
```

### For Multi-Line Events

Some events span multiple lines (stack traces, multi-line error messages):

```
src/main.c:15:5: error: 'undefined_var' undeclared
   15 |     undefined_var = 42;
      |     ^~~~~~~~~~~~~
```

The parser should track the start line when the event begins and update `log_line_end` as continuation lines are consumed:

```cpp
if (is_continuation(line)) {
    current_event->log_line_end = current_line;
    current_event->message += "\n" + line;
} else {
    emit(current_event);
    current_event = parse_new_event(line);
    current_event->log_line_start = current_line;
    current_event->log_line_end = current_line;
}
```

### For Regex-Based Parsers

The `regexp:` format already processes line-by-line. Add line tracking to the regex scanner.

### For JSON/Structured Input

For formats like `pytest_json` or `eslint_json`, the source isn't line-based. Options:

1. Set `log_line_start = log_line_end = NULL` (no meaningful line position)
2. Track the line in the JSON where the object appears (less useful but possible)

Recommendation: Use NULL for structured formats.

## SQL Macro Support

Once implemented, lq can add context retrieval macros:

```sql
-- Get raw lines around an event
CREATE MACRO lq_context(run_id, event_id, context_lines := 3) AS TABLE
WITH event AS (
    SELECT log_line_start, log_line_end
    FROM lq_events
    WHERE run_id = run_id AND event_id = event_id
),
raw AS (
    SELECT unnest(string_split(content, E'\n')) AS line,
           generate_subscripts(string_split(content, E'\n'), 1) AS line_num
    FROM read_text('.lq/raw/' || run_id || '.log')
)
SELECT line_num, line
FROM raw, event
WHERE line_num BETWEEN (event.log_line_start - context_lines)
                    AND (event.log_line_end + context_lines);
```

## Testing

Add test cases for:

1. Single-line events have `log_line_start = log_line_end`
2. Multi-line events have correct range
3. Events at file boundaries (first/last line)
4. Large files (verify no off-by-one errors)
5. Structured formats return NULL

Example test:

```sql
-- test/sql/log_line_tracking.test
query IIII
SELECT event_id, log_line_start, log_line_end, status
FROM read_duck_hunt_log('test/samples/make.out', 'make_error')
ORDER BY event_id;
----
1	2	4	ERROR
2	5	5	INFO
3	6	8	WARNING
4	10	12	ERROR
5	13	13	ERROR
```

## Backward Compatibility

- New fields are nullable, so existing queries continue to work
- Parsers that haven't been updated will return NULL for these fields
- No breaking changes to existing schema

## Priority

Medium-high. This is a foundational feature for context-aware tooling. Without it, tools like lq must either:
- Store full raw output per event (wasteful)
- Re-parse logs for context (slow)
- Skip context features entirely (poor UX)

## Related

- lq project: `../lq/`
- Current schema: `docs/schema.md`
- Error fingerprint (already implemented): Enables cross-run matching
