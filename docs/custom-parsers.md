# Custom Parsers

Duck Hunt allows you to define custom log parsers using JSON configuration. This enables parsing proprietary log formats, internal tools, or any custom output without writing C++ code.

## Quick Start: Inline Config Files

The simplest way to use a custom parser is to reference a JSON config file directly as the format:

```sql
-- Use config: prefix with file path
SELECT * FROM parse_duck_hunt_log(content, 'config:parsers/my_format.json');

-- Or just use .json file path directly
SELECT * FROM parse_duck_hunt_log(content, 'parsers/my_format.json');

-- Works with URLs too
SELECT * FROM parse_duck_hunt_log(content, 'https://example.com/parsers/custom.json');
```

This approach:
- Does NOT register the parser permanently
- Loads the config for each query
- Ideal for one-off parsing or ad-hoc formats
- Supports any DuckDB-accessible path (local, HTTP, S3, etc.)

## Loading a Parser

Use `duck_hunt_load_parser_config()` to register a custom parser from a JSON string:

```sql
SELECT duck_hunt_load_parser_config('{
  "name": "my_app_log",
  "aliases": ["myapp"],
  "priority": 75,
  "category": "app_logging",
  "groups": ["custom", "logging"],
  "description": "My application log format",

  "detection": {
    "contains": ["[MyApp]"]
  },

  "patterns": [
    {
      "regex": "\\[MyApp\\] \\[(?P<level>ERROR|WARNING|INFO)\\] (?P<message>.*)",
      "event_type": "LINT_ISSUE",
      "severity_map": {"ERROR": "error", "WARNING": "warning", "INFO": "info"}
    }
  ]
}');
```

### Loading from Files

Parser configs can also be loaded from files using DuckDB's filesystem interface:

```sql
-- Load from local file
SELECT duck_hunt_load_parser_config(content)
FROM read_text('parsers/my_parser.json');

-- Load from HTTP URL
SELECT duck_hunt_load_parser_config(content)
FROM read_text('https://example.com/parsers/custom.json');

-- Load multiple parsers from a directory
SELECT duck_hunt_load_parser_config(content)
FROM read_text('parsers/*.json');
```

## Unloading a Parser

Remove a custom parser with `duck_hunt_unload_parser()`:

```sql
SELECT duck_hunt_unload_parser('my_app_log');
```

**Note:** Built-in parsers cannot be unloaded.

## Configuration Schema

### Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Unique identifier for the parser (used as format string) |
| `patterns` | array | One or more patterns to match (see [Patterns](#patterns)) |

### Optional Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `aliases` | string[] | `[]` | Alternative names for the format |
| `priority` | integer | `50` | Detection priority (higher = checked first) |
| `category` | string | `"tool_output"` | Parser category |
| `tool_name` | string | `name` | Tool name in output (defaults to `name`) |
| `groups` | string[] | `["custom"]` | Format groups for group-based selection |
| `description` | string | `""` | Human-readable description |
| `detection` | object | `{}` | Auto-detection configuration (see [Detection](#detection)) |

## Detection

The `detection` object controls how the parser is matched during auto-detection. If no detection is specified, the parser will only be used when explicitly requested.

### Detection Methods

| Method | Description |
|--------|-------------|
| `contains` | Match if content contains ANY of the specified strings |
| `contains_all` | Match if content contains ALL of the specified strings |
| `regex` | Match if content matches the specified regex |

Multiple methods can be combined (all must match):

```json
{
  "detection": {
    "contains": ["[MyApp]"],
    "regex": "^\\d{4}-\\d{2}-\\d{2}"
  }
}
```

### Examples

```json
// Match if content contains any marker
{"detection": {"contains": ["[ERROR]", "[WARN]", "[INFO]"]}}

// Match if content contains all required markers
{"detection": {"contains_all": ["START:", "END:"]}}

// Match using regex pattern
{"detection": {"regex": "^\\[v\\d+\\.\\d+\\]"}}
```

## Patterns

Each pattern defines a regex and how to interpret matches. Patterns are tried in order; the first match wins.

### Pattern Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `regex` | string | **Yes** | Regex pattern with named capture groups |
| `event_type` | string | **Yes** | Event type (see [Event Types](#event-types)) |
| `name` | string | No | Pattern name for documentation |
| `severity` | string | No | Fixed severity (`debug`, `info`, `warning`, `error`, `critical`) |
| `severity_map` | object | No | Map captured value to severity |
| `status_map` | object | No | Map captured value to status (`PASS`, `FAIL`, `SKIP`) |

### Event Types

| Event Type | Description | Default Severity |
|------------|-------------|------------------|
| `BUILD_ERROR` | Compiler/build errors | `error` |
| `LINT_ISSUE` | Linting/style issues | `info` |
| `TEST_RESULT` | Test pass/fail | varies by status |
| `DIAGNOSTIC` | General diagnostic message | `info` |

### Named Capture Groups

Use Python-style `(?P<name>...)` capture groups to extract fields:

| Group Name | Output Field | Description |
|------------|--------------|-------------|
| `message`, `msg` | message | Error/warning message |
| `severity`, `level` | severity | Severity level |
| `file`, `file_path` | ref_file | File path |
| `line`, `lineno` | ref_line | Line number |
| `column`, `col` | ref_column | Column number |
| `error_code`, `code`, `rule` | error_code | Error/rule code |
| `function_name`, `func` | function_name | Function name |
| `test_name`, `test` | test_name | Test name |
| `scope` | scope | Hierarchical scope |
| `group` | group | Hierarchical group |
| `unit` | unit | Hierarchical unit |

## Examples

### Simple Error Parser

```sql
SELECT duck_hunt_load_parser_config('{
  "name": "simple_errors",
  "detection": {"contains": ["ERROR:", "WARN:"]},
  "patterns": [
    {
      "regex": "ERROR: (?P<message>.*)",
      "event_type": "BUILD_ERROR",
      "severity": "error"
    },
    {
      "regex": "WARN: (?P<message>.*)",
      "event_type": "LINT_ISSUE",
      "severity": "warning"
    }
  ]
}');

-- Use the parser
SELECT severity, message
FROM parse_duck_hunt_log('ERROR: disk full\nWARN: low memory', 'simple_errors');
```

### File Location Parser

```sql
SELECT duck_hunt_load_parser_config('{
  "name": "compiler_output",
  "detection": {"regex": "\\w+\\.\\w+:\\d+"},
  "patterns": [
    {
      "regex": "(?P<file>[^:]+):(?P<line>\\d+):(?P<column>\\d+): (?P<severity>error|warning): (?P<message>.*)",
      "event_type": "BUILD_ERROR"
    },
    {
      "regex": "(?P<file>[^:]+):(?P<line>\\d+): (?P<severity>error|warning): (?P<message>.*)",
      "event_type": "BUILD_ERROR"
    }
  ]
}');

SELECT ref_file, ref_line, ref_column, severity, message
FROM parse_duck_hunt_log('main.c:42:10: error: undefined reference', 'compiler_output');
-- Result: main.c, 42, 10, error, undefined reference
```

### Test Result Parser

```sql
SELECT duck_hunt_load_parser_config('{
  "name": "test_runner",
  "detection": {"contains": ["TEST:"]},
  "patterns": [
    {
      "regex": "TEST: (?P<result>ok|FAILED|skip) - (?P<test_name>.*)",
      "event_type": "TEST_RESULT",
      "status_map": {
        "ok": "PASS",
        "FAILED": "FAIL",
        "skip": "SKIP"
      }
    }
  ]
}');

SELECT status, test_name, severity
FROM parse_duck_hunt_log('TEST: ok - test_math\nTEST: FAILED - test_io', 'test_runner');
-- Results:
--   PASS, test_math, info
--   FAIL, test_io, error
```

### Dynamic Severity from Capture

```sql
SELECT duck_hunt_load_parser_config('{
  "name": "app_log",
  "detection": {"contains": ["[LOG]"]},
  "patterns": [
    {
      "regex": "\\[LOG\\] (?P<level>DEBUG|INFO|WARNING|ERROR|CRITICAL): (?P<message>.*)",
      "event_type": "LINT_ISSUE",
      "severity_map": {
        "DEBUG": "debug",
        "INFO": "info",
        "WARNING": "warning",
        "ERROR": "error",
        "CRITICAL": "critical"
      }
    }
  ]
}');

SELECT severity, message
FROM parse_duck_hunt_log('[LOG] WARNING: deprecated API', 'app_log');
-- Result: warning, deprecated API
```

### CI/CD Context Parser

```sql
SELECT duck_hunt_load_parser_config('{
  "name": "ci_context",
  "detection": {"contains": ["::"]},
  "patterns": [
    {
      "regex": "(?P<scope>[^:]+)::(?P<group>[^:]+)::(?P<unit>[^:]+): (?P<status>PASS|FAIL) - (?P<message>.*)",
      "event_type": "TEST_RESULT"
    }
  ]
}');

SELECT scope, "group", unit, status, message
FROM parse_duck_hunt_log('workflow-1::job-build::step-compile: PASS - compiled successfully', 'ci_context');
-- Result: workflow-1, job-build, step-compile, PASS, compiled successfully
```

## Integration with Format Groups

Custom parsers participate in format group selection:

```sql
-- Register a custom Python tool
SELECT duck_hunt_load_parser_config('{
  "name": "my_python_tool",
  "groups": ["python", "custom"],
  "detection": {"contains": ["[MyPyTool]"]},
  "patterns": [
    {"regex": "\\[MyPyTool\\] (?P<message>.*)", "event_type": "LINT_ISSUE"}
  ]
}');

-- Now 'python' group will include this parser
SELECT * FROM parse_duck_hunt_log('[MyPyTool] Issue found', 'python');
```

## Inline vs Registered Parsers

| Feature | Inline (`config:path.json`) | Registered (`duck_hunt_load_parser_config`) |
|---------|-----------------------------|--------------------------------------------|
| Persistence | None - loaded per query | Session-scoped |
| Auto-detection | No | Yes (participates in `format='auto'`) |
| Format groups | No | Yes (appears in group searches) |
| Performance | Loads config each time | Loaded once |
| Use case | Ad-hoc, one-off parsing | Repeated use, integration with auto-detect |

**Use inline config files when:**
- Parsing a one-off log file with a custom format
- Testing a new parser configuration
- The config file is stored alongside your data

**Use registered parsers when:**
- Parsing multiple files with the same format
- You want auto-detection to find your format
- You want your parser to be part of a format group

## Session Scope

Registered custom parsers are session-scoped: they persist within a database connection but are not saved across sessions. To persist parsers:

1. Store configurations in a table
2. Load them on connection start
3. Or use a startup script

```sql
-- Create a table for parser configs
CREATE TABLE IF NOT EXISTS parser_configs (
    name VARCHAR PRIMARY KEY,
    config JSON NOT NULL
);

-- Load all saved parsers
SELECT duck_hunt_load_parser_config(config::VARCHAR)
FROM parser_configs;
```

## Error Handling

Invalid configurations will raise errors:

```sql
-- Invalid JSON
SELECT duck_hunt_load_parser_config('not valid json');
-- Error: Invalid JSON

-- Missing required field
SELECT duck_hunt_load_parser_config('{"patterns": []}');
-- Error: Missing required field: name

-- Invalid regex
SELECT duck_hunt_load_parser_config('{
  "name": "bad",
  "patterns": [{"regex": "[invalid(", "event_type": "LINT_ISSUE"}]
}');
-- Error: Invalid regex pattern

-- Invalid event type
SELECT duck_hunt_load_parser_config('{
  "name": "bad",
  "patterns": [{"regex": ".*", "event_type": "NOT_REAL"}]
}');
-- Error: Invalid event_type: NOT_REAL
```

## Discovering Parsers

List all registered parsers including custom ones:

```sql
-- List all formats
SELECT format_name, category, priority, description
FROM duck_hunt_formats()
WHERE 'custom' = ANY(groups)
ORDER BY priority DESC;

-- Check if a custom parser exists
SELECT COUNT(*) > 0 AS exists
FROM duck_hunt_formats()
WHERE format_name = 'my_app_log';
```
