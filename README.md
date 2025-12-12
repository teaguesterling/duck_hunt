# Duck Hunt

A DuckDB extension for parsing test results, build outputs, and CI/CD logs from 60+ development tools.

## Functions

### Table Functions

```sql
read_duck_hunt_log(file_path, format := 'auto')           -- Parse from file
parse_duck_hunt_log(content, format := 'auto')            -- Parse from string
read_duck_hunt_workflow_log(file_path, format := 'auto')  -- Parse CI/CD workflows
parse_duck_hunt_workflow_log(content, format := 'auto')   -- Parse CI/CD from string
```

### Scalar Functions

```sql
status_badge(status)                              -- 'error' → '[FAIL]'
status_badge(error_count, warning_count)          -- Compute from counts
status_badge(error_count, warning_count, running) -- With running state
```

Badges: `[ OK ]` `[FAIL]` `[WARN]` `[ .. ]` `[ ?? ]`

## Quick Start

```sql
-- Parse build errors
SELECT file_path, line_number, message
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR';

-- Parse test results
SELECT test_name, status, execution_time
FROM read_duck_hunt_log('pytest.json', 'pytest_json')
WHERE status = 'FAIL';

-- Custom regex pattern
SELECT severity, message
FROM parse_duck_hunt_log(
  'ERROR: Connection failed\nWARNING: Retrying...',
  'regexp:(?P<severity>ERROR|WARNING):\s+(?P<message>.+)'
);

-- Build health badge
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
| `pytest_json` | pytest | [sample](test/samples/pytest.json) |
| `eslint_json` | ESLint | [sample](test/samples/eslint.json) |
| `mypy_text` | MyPy | [sample](test/samples/mypy.txt) |
| `make_error` | GNU Make | [sample](test/samples/make.out) |
| `gotest_json` | Go test | [sample](test/samples/gotest.json) |
| `github_actions_text` | GitHub Actions | [sample](test/samples/github_actions.log) |
| `valgrind` | Valgrind | Memory analysis |
| `generic_lint` | Generic | `file:line:col: severity: message` |

**[See all 60+ formats →](docs/formats.md)**

## Output Schema

All parsers produce a standardized schema:

| Field | Description |
|-------|-------------|
| `event_id` | Unique event identifier |
| `tool_name` | Tool name (pytest, eslint, make, etc.) |
| `status` | PASS, FAIL, ERROR, WARNING, INFO, SKIP |
| `file_path` | Source file path |
| `line_number` | Line number |
| `message` | Error/warning message |
| `error_code` | Rule ID or error code |

**[See full schema (38 fields) →](docs/schema.md)**

## Documentation

- **[Format Reference](docs/formats.md)** - All 60+ supported formats with examples
- **[Schema Reference](docs/schema.md)** - Complete field documentation
- **[Usage Examples](docs/examples.md)** - Detailed examples for common scenarios

## Installation

```sql
-- From source build
LOAD './build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- Or with unsigned extensions enabled
SET allow_unsigned_extensions = true;
LOAD duck_hunt;
```

## Building

```bash
git clone https://github.com/teaguesterling/duck_hunt
cd duck_hunt
make release
make test
```

## License

MIT License - see [LICENSE](LICENSE)
