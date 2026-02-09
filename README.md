# Duck Hunt

[![Documentation Status](https://readthedocs.org/projects/duck-hunt/badge/?version=latest)](https://duck-hunt.readthedocs.io/en/latest/?badge=latest)

A DuckDB extension for parsing test results, build outputs, and CI/CD logs from 100 development tools.

## Functions

### Table Functions

```sql
read_duck_hunt_log(source, format, severity_threshold, ignore_errors, content, context)
parse_duck_hunt_log(text, format, severity_threshold, content, context)
read_duck_hunt_workflow_log(source, format, severity_threshold, ignore_errors)
parse_duck_hunt_workflow_log(text, format, severity_threshold)
duck_hunt_formats()                               -- List all supported formats
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `format` | VARCHAR | `'auto'` | Parser format or `'auto'` for auto-detection |
| `severity_threshold` | VARCHAR | `'all'` | Minimum severity: `'all'`, `'info'`, `'warning'`, `'error'`, `'critical'` |
| `ignore_errors` | BOOLEAN | `false` | Continue processing when files fail to parse |
| `content` | ANY | `'full'` | Control `log_content` size (see below) |
| `context` | INTEGER | `0` | Include N surrounding log lines (adds `context` column) |

**Content modes** for memory optimization:

| Mode | Description |
|------|-------------|
| `content := 200` | Limit to 200 characters |
| `content := 'smart'` | Intelligent truncation around event |
| `content := 'none'` or `0` | Omit entirely (NULL) |
| `content := 'full'` | Full content (default) |

**Context extraction** for viewing surrounding log lines:

```sql
-- Include 3 lines before/after each event
SELECT ref_file, message, context
FROM read_duck_hunt_log('build.log', 'make_error', context := 3);

-- Access context line details
SELECT
    context[1].line_number,    -- Line number in original file
    context[1].content,        -- Line content
    context[1].is_event        -- TRUE if this line is part of the event
FROM parse_duck_hunt_log(content, 'make_error', context := 2);

-- Filter to just event lines within context
SELECT list_filter(context, x -> x.is_event) as event_lines
FROM read_duck_hunt_log('build.log', context := 3);
```

The `context` column is a `LIST(STRUCT(line_number INT, content VARCHAR, is_event BOOL))`.

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

Returns a table showing which parsers match the content:
- `format` - Parser name
- `priority` - Parser priority (higher = checked first)
- `can_parse` - Whether parser can handle the content
- `events_produced` - Number of events if parsed
- `is_selected` - Whether auto-detect would choose this parser

## Quick Start

```sql
-- Parse build errors
SELECT ref_file, ref_line, message
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR';

-- Parse test results (filter to warnings and above)
SELECT test_name, status, execution_time
FROM read_duck_hunt_log('pytest.json', 'pytest_json', severity_threshold := 'warning')
WHERE status = 'FAIL';

-- Process multiple files, skip failures
SELECT * FROM read_duck_hunt_log('logs/*.json', ignore_errors := true);

-- Limit log_content for memory efficiency
SELECT * FROM read_duck_hunt_log('huge.log', content := 200);

-- Debug format detection
SELECT format, can_parse, events_produced, is_selected
FROM duck_hunt_diagnose_read('mystery.log');

-- Build health badge
SELECT status_badge(
  COUNT(*) FILTER (WHERE status = 'ERROR'),
  COUNT(*) FILTER (WHERE status = 'WARNING')
) FROM read_duck_hunt_log('build.log', 'auto');
```

## Format Groups

Instead of specifying an exact format, use a **format group** to auto-detect among related tools:

```sql
-- Try all Python parsers (pytest, mypy, pylint, flake8, etc.)
SELECT * FROM parse_duck_hunt_log(content, 'python');

-- Try all linting tools (eslint, pylint, clippy, etc.)
SELECT * FROM parse_duck_hunt_log(content, 'lint');

-- Try all test frameworks (pytest, junit, gtest, etc.)
SELECT * FROM parse_duck_hunt_log(content, 'test');
```

**Language Groups:**

| Group | Tools |
|-------|-------|
| `python` | pytest, mypy, pylint, flake8, black, isort, bandit, coverage |
| `java` | junit, maven, gradle, spotbugs, ktlint, hdfs, spark, zookeeper |
| `c_cpp` | gtest, make, cmake, valgrind, gdb, strace, clang-tidy |
| `javascript` | eslint, stylelint, mocha, winston, pino, bunyan, node |
| `rust` | cargo, clippy, cargo_test |
| `go` | gotest, logrus |
| `dotnet` | nunit, msbuild, serilog, nlog |
| `ruby` | rspec, rubocop, ruby_logger, rails |

**Tool-Type Groups:**

| Group | Tools |
|-------|-------|
| `lint` | eslint, pylint, mypy, flake8, clippy, rubocop, hadolint, tflint, etc. (26 tools) |
| `test` | pytest, junit, gtest, rspec, mocha, gotest, cargo_test, etc. (12 tools) |
| `build` | make, cmake, maven, gradle, cargo, msbuild, bazel, etc. (9 tools) |
| `security` | bandit, trivy, tfsec, spotbugs, cloudtrail, etc. (10 tools) |
| `infrastructure` | kubernetes, terraform, ansible, hadolint, etc. (15 tools) |
| `logging` | log4j, winston, serilog, logrus, pino, jsonl, etc. (13 tools) |

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

**[See all 100 formats →](docs/formats.md)** | **[Workflow formats →](docs/workflow-formats.md)**

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

**[See full schema →](docs/schema.md)** | **[Field mappings by domain →](docs/field_mappings.md)**

## Compression Support

Duck Hunt transparently reads compressed files based on file extension:

```sql
-- GZIP compression (.gz) - built-in, always available
SELECT * FROM read_duck_hunt_log('build.log.gz', 'auto');
SELECT * FROM read_duck_hunt_workflow_log('actions.log.gz', 'github_actions');

-- ZSTD compression (.zst) - requires parquet extension
LOAD parquet;
SELECT * FROM read_duck_hunt_log('build.log.zst', 'auto');

-- Glob patterns work with compressed files
SELECT * FROM read_duck_hunt_log('logs/**/*.log.gz', 'auto');
```

| Extension | Format | Support |
|-----------|--------|---------|
| `.gz`, `.gzip` | GZIP | Built-in (always available) |
| `.zst`, `.zstd` | ZSTD | Requires `LOAD parquet` first |

## ZIP Archive Support

Read GitHub Actions workflow logs directly from downloaded ZIP archives:

```sql
-- Install and load the zipfs extension
INSTALL zipfs FROM community;
LOAD zipfs;

-- Parse all jobs from a GitHub Actions workflow run ZIP
SELECT job_name, job_order, severity, message
FROM read_duck_hunt_workflow_log('workflow_run.zip', 'github_actions_zip')
WHERE severity = 'error';

-- Read a single file from inside the ZIP
SELECT * FROM read_duck_hunt_workflow_log(
    'zip://workflow_run.zip/0_Build.txt',
    'github_actions'
);
```

The `github_actions_zip` format:
- Automatically extracts all job logs (`{N}_{job_name}.txt` files)
- Adds `job_order` and `job_name` columns from filenames
- Delegates parsing to the standard `github_actions` parser

## Documentation

- **[Format Reference](docs/formats.md)** - All 100 supported formats with examples
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
