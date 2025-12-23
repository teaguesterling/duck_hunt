# Duck Hunt

A DuckDB extension for parsing test results, build outputs, and CI/CD logs from 80+ development tools.

## Functions

### Table Functions

```sql
read_duck_hunt_log(file_path, format := 'auto', severity_threshold := 'all')
parse_duck_hunt_log(content, format := 'auto', severity_threshold := 'all')
read_duck_hunt_workflow_log(file_path, format := 'auto', severity_threshold := 'all')
parse_duck_hunt_workflow_log(content, format := 'auto', severity_threshold := 'all')
```

- `format` - Parser format or `'auto'` for auto-detection
- `severity_threshold` - Minimum severity to include: `'all'`, `'info'`, `'warning'`, `'error'`, `'critical'`

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
| `pytest_json` | pytest | [sample](test/samples/test_frameworks/pytest_json_failures.json) |
| `eslint_json` | ESLint | [sample](test/samples/linting_tools/eslint_output.json) |
| `mypy_text` | MyPy | [sample](test/samples/linting_tools/mypy_output.txt) |
| `make_error` | GNU Make | [sample](test/samples/build_systems/make_errors.txt) |
| `gotest_json` | Go test | [sample](test/samples/test_frameworks/gotest_failures.json) |
| `valgrind` | Valgrind | [sample](test/samples/debugging_tools/valgrind_memcheck.txt) |
| `generic_lint` | Generic | `file:line:col: severity: message` |

**[See all 80+ formats →](docs/formats.md)** | **[Workflow formats →](docs/workflow-formats.md)**

## Output Schema

All parsers produce a standardized 39-field schema:

| Field | Description |
|-------|-------------|
| `event_id` | Unique event identifier |
| `tool_name` | Tool name (pytest, eslint, make, etc.) |
| `status` | PASS, FAIL, ERROR, WARNING, INFO, SKIP |
| `file_path` | Source file path |
| `line_number` | Line number |
| `message` | Error/warning message |
| `scope` | Hierarchy level 1 (workflow, cluster, suite) |
| `group` | Hierarchy level 2 (job, namespace, class) |
| `unit` | Hierarchy level 3 (step, pod, method) |
| `fingerprint` | Error pattern signature for clustering |

**[See full schema →](docs/schema.md)** | **[Field mappings by domain →](docs/field_mappings.md)**

## Documentation

- **[Format Reference](docs/formats.md)** - All 80+ supported formats with examples
- **[Workflow Formats](docs/workflow-formats.md)** - CI/CD workflow parsing (GitHub Actions, GitLab CI, Jenkins)
- **[Format Maturity](docs/format-maturity.md)** - Stability ratings and test coverage
- **[Schema Reference](docs/schema.md)** - Complete field documentation
- **[Field Mappings](docs/field_mappings.md)** - How fields map to each domain
- **[Migration Guide](docs/migration-guide.md)** - Upgrading from previous schema versions
- **[Usage Examples](docs/examples.md)** - Detailed examples for common scenarios

## Installation

```sql
-- From source build
INSTALL duck_hunt FROM community;
LOAD duck_hunt;
```

## Building

```bash
git clone https://github.com/teaguesterling/duck_hunt
cd duck_hunt
make release
make test
```

## Acknowledgments

Test data includes samples from [loghub](https://github.com/logpai/loghub), a collection of system logs for AI-based log analysis research:

> Jieming Zhu, Shilin He, Jinyang Liu, Pinjia He, Qi Xie, Zibin Zheng, Michael R. Lyu. [Tools and Benchmarks for Automated Log Parsing](https://arxiv.org/abs/1811.03509). *International Conference on Software Engineering (ICSE)*, 2019.

## License

MIT License - see [LICENSE](LICENSE)
