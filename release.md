# Duck Hunt v1.0.0 Release Notes

**Initial Release** - December 2024

Duck Hunt is a DuckDB extension for parsing and analyzing test results, build outputs, and CI/CD pipeline logs. It provides a unified SQL interface to query development tool outputs from 45+ formats.

## Core Functions

### Table Functions
- `read_duck_hunt_log(file, format)` - Parse tool outputs from files
- `parse_duck_hunt_log(content, format)` - Parse tool outputs from strings
- `read_duck_hunt_workflow_log(file, format)` - Parse CI/CD workflow logs from files
- `parse_duck_hunt_workflow_log(content, format)` - Parse CI/CD workflow logs from strings

### Scalar Functions
- `status_badge(status)` - Convert status string to badge (`[ OK ]`, `[FAIL]`, `[WARN]`, `[ .. ]`, `[ ?? ]`)
- `status_badge(errors, warnings)` - Compute badge from error/warning counts
- `status_badge(errors, warnings, is_running)` - Compute badge with running state

## Supported Formats (45+)

### Dynamic Pattern Matching
- `regexp:<pattern>` - Custom regex patterns with named capture groups for parsing any log format

### Test Frameworks (9)
- pytest (JSON & text)
- Go test (JSON)
- Cargo test (Rust JSON)
- JUnit (Java text - JUnit 4/5, TestNG, Surefire)
- RSpec (Ruby text)
- Mocha/Chai (JavaScript text)
- Google Test (C++ text)
- NUnit/xUnit (.NET text)
- DuckDB test output

### Linting & Static Analysis (20+)
- ESLint, RuboCop, SwiftLint, PHPStan, Shellcheck, Stylelint
- Clippy, Markdownlint, yamllint, Bandit, SpotBugs, ktlint
- Hadolint, lintr, sqlfluff, tflint, kube-score
- Pylint, Flake8, Black, MyPy, clang-tidy

### Build Systems (10)
- CMake, GNU Make, Python (pip/setuptools)
- Node.js (npm/yarn/webpack), Cargo (Rust)
- Maven, Gradle, MSBuild, Bazel, Docker

### CI/CD Workflow Engines (4)
- GitHub Actions
- GitLab CI
- Jenkins
- Docker Build

### Debugging & Analysis (2)
- Valgrind (Memcheck, Helgrind, Cachegrind, Massif, DRD)
- GDB/LLDB

## Schema

### Core Fields (17)
- `event_id`, `tool_name`, `event_type`, `file_path`, `line_number`, `column_number`
- `function_name`, `status`, `severity`, `category`, `message`, `suggestion`
- `error_code`, `test_name`, `execution_time`, `raw_output`, `structured_data`

### Error Analysis Fields (4)
- `error_fingerprint` - Normalized error signature for pattern detection
- `similarity_score` - Similarity to pattern cluster (0.0-1.0)
- `pattern_id` - Assigned error pattern group ID
- `root_cause_category` - Detected root cause (network, permission, config, syntax, build, resource)

### Multi-file Metadata (4)
- `source_file`, `build_id`, `environment`, `file_index`

### Workflow Hierarchy Fields (13)
- `workflow_name`, `job_name`, `step_name`
- `workflow_run_id`, `job_id`, `step_id`
- `workflow_status`, `job_status`, `step_status`
- `started_at`, `completed_at`, `duration`
- `workflow_type`, `hierarchy_level`, `parent_id`

## Key Features

- **Automatic Format Detection** - Pass `'auto'` to detect format from content
- **Dynamic Regexp Parser** - Parse any log format with custom patterns
- **Error Pattern Clustering** - Group similar errors with fingerprinting
- **Root Cause Analysis** - Automatic categorization of error causes
- **Multi-file Processing** - Glob patterns with metadata extraction
- **Pipeline Integration** - Read from stdin for real-time analysis
- **Hierarchical Workflow Parsing** - Full CI/CD job/step structure

## Quick Start

```sql
-- Load extension
LOAD duck_hunt;

-- Parse build errors
SELECT file_path, line_number, message
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR';

-- Custom pattern matching
SELECT severity, message
FROM parse_duck_hunt_log(
    'ERROR: Connection failed\nWARNING: Retrying...',
    'regexp:(?P<severity>ERROR|WARNING):\s+(?P<message>.+)'
);

-- CI/CD workflow analysis
SELECT job_name, step_name, step_status
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE step_status = 'failure';

-- Build health badge
SELECT status_badge(
    COUNT(CASE WHEN status = 'ERROR' THEN 1 END),
    COUNT(CASE WHEN status = 'WARNING' THEN 1 END)
) as build_health
FROM read_duck_hunt_log('build.log', 'auto');
```

## Requirements

- DuckDB v1.4.0+

## License

MIT License
