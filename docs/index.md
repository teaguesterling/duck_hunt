# Duck Hunt

A DuckDB extension for parsing test results, build outputs, and CI/CD logs from 110 development tools.

## Installation

```sql
-- Install from DuckDB Community Extensions
INSTALL duck_hunt FROM community;
LOAD duck_hunt;
```

## Functions

### Table Functions

```sql
read_duck_hunt_log(file_path, format := 'auto', severity_threshold := 'all', context := 0)
parse_duck_hunt_log(content, format := 'auto', severity_threshold := 'all', context := 0)
read_duck_hunt_workflow_log(file_path, format := 'auto', severity_threshold := 'all')
parse_duck_hunt_workflow_log(content, format := 'auto', severity_threshold := 'all')
duck_hunt_formats()                               -- List all supported formats
duck_hunt_load_parser_config(json)                -- Load custom parser from JSON
duck_hunt_unload_parser(name)                     -- Unload a custom parser
```

**Parameters:**

- `format` - Parser format or `'auto'` for auto-detection
- `severity_threshold` - Minimum severity to include: `'all'`, `'info'`, `'warning'`, `'error'`, `'critical'`
- `context` - Number of surrounding log lines to include (adds `context` column when > 0)

### Scalar Functions

```sql
duck_hunt_detect_format(content)                  -- Auto-detect format from content
status_badge(status)                              -- 'error' → '[FAIL]'
status_badge(error_count, warning_count)          -- Compute from counts
status_badge(error_count, warning_count, running) -- With running state
```

- `duck_hunt_detect_format()` - Returns format string like `'pytest_json'`, `'make_error'`, or `NULL`
- `status_badge()` - Returns badges: `[ OK ]` `[FAIL]` `[WARN]` `[ .. ]` `[ ?? ]`

### Diagnostic Functions

```sql
duck_hunt_diagnose_read(file_path)                -- Debug format detection for a file
duck_hunt_diagnose_parse(content)                 -- Debug format detection for content
```

Returns a table showing which parsers match: `format`, `priority`, `can_parse`, `events_produced`, `is_selected`

## Quick Start

### Parse Build Errors

```sql
SELECT ref_file, ref_line, message
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR';
```

### Parse Test Results

```sql
SELECT test_name, status, execution_time
FROM read_duck_hunt_log('pytest.json', 'pytest_json')
WHERE status = 'FAIL';
```

### Custom Regex Pattern

```sql
SELECT severity, message
FROM parse_duck_hunt_log(
  'ERROR: Connection failed\nWARNING: Retrying...',
  'regexp:(?P<severity>ERROR|WARNING):\s+(?P<message>.+)'
);
```

### Build Health Badge

```sql
SELECT status_badge(
  COUNT(*) FILTER (WHERE status = 'ERROR'),
  COUNT(*) FILTER (WHERE status = 'WARNING')
) FROM read_duck_hunt_log('build.log', 'auto');
```

## Common Formats

| Format | Tool | Example |
|--------|------|---------|
| `auto` | Auto-detect | `read_duck_hunt_log('file.log', 'auto')` |
| `regexp:<pattern>` | Custom regex | `'regexp:(?P<severity>ERROR):\s+(?P<message>.*)'` |
| `pytest_json` | pytest | JSON output with `--json-report` |
| `eslint_json` | ESLint | JSON output with `--format json` |
| `mypy_text` | MyPy | Type checker text output |
| `make_error` | GNU Make | Build errors with GCC/Clang |
| `gotest_json` | Go test | JSON output with `-json` |
| `valgrind` | Valgrind | Memory checker output |
| `generic_lint` | Generic | `file:line:col: severity: message` |

See [Supported Formats](formats.md) for the complete list of 110 formats.

## Compression Support

Duck Hunt transparently reads compressed files based on file extension:

| Extension | Format | Support |
|-----------|--------|---------|
| `.gz`, `.gzip` | GZIP | Built-in (always available) |
| `.zst`, `.zstd` | ZSTD | Requires `LOAD parquet` first |

```sql
-- Read compressed log files directly
SELECT * FROM read_duck_hunt_log('build.log.gz', 'auto');
SELECT * FROM read_duck_hunt_workflow_log('actions.log.gz', 'github_actions');

-- Glob patterns work with compression
SELECT * FROM read_duck_hunt_log('logs/*.log.gz', 'auto');
```

## Output Schema

All parsers produce a standardized 39-field schema:

| Field | Description |
|-------|-------------|
| `event_id` | Unique event identifier |
| `tool_name` | Tool name (pytest, eslint, make, etc.) |
| `status` | PASS, FAIL, ERROR, WARNING, INFO, SKIP |
| `ref_file` | Referenced source file path |
| `ref_line` | Line number in referenced file |
| `log_file` | Path to the log file being parsed |
| `message` | Error/warning message |
| `scope` | Hierarchy level 1 (workflow, cluster, suite) |
| `group` | Hierarchy level 2 (job, namespace, class) |
| `unit` | Hierarchy level 3 (step, pod, method) |
| `fingerprint` | Error pattern signature for clustering |

See [Schema Reference](schema.md) for complete field documentation.

## Next Steps

- **[Supported Formats](formats.md)** - All 110 supported formats with examples
- **[Custom Parsers](custom-parsers.md)** - Define your own parsers with JSON configuration
- **[Workflow Formats](workflow-formats.md)** - CI/CD workflow parsing (GitHub Actions, GitLab CI, Jenkins)
- **[Usage Examples](examples.md)** - Detailed examples for common scenarios
- **[Schema Reference](schema.md)** - Complete field documentation

## License

MIT License - see [LICENSE](https://github.com/teaguesterling/duck_hunt/blob/main/LICENSE)
