# Duck Hunt - DuckDB Test Results & Build Output Parser Extension

A comprehensive DuckDB extension for parsing and analyzing test results, build outputs, and DevOps pipeline data from various tools and formats. Perfect for automated development workflows, CI/CD analysis, and agent-driven programming tasks.

## Features

Duck Hunt provides powerful SQL-based analysis of development tool outputs through two main table functions:

### Core Functions

- **`read_test_results(file_path, format := 'AUTO')`** - Parse test results and build outputs from files
- **`parse_test_results(content, format := 'AUTO')`** - Parse test results and build outputs from strings

### Supported Formats (29 Total)

#### Test Frameworks
- **pytest** (JSON & text output)
- **Go test** (JSON output) 
- **Cargo test** (Rust JSON output)
- **DuckDB test output** (custom format)

#### Linting & Static Analysis Tools
- **ESLint** (JavaScript/TypeScript)
- **RuboCop** (Ruby)
- **SwiftLint** (Swift)
- **PHPStan** (PHP)
- **Shellcheck** (Shell scripts)
- **Stylelint** (CSS/SCSS)
- **Clippy** (Rust)
- **Markdownlint** (Markdown)
- **yamllint** (YAML)
- **Bandit** (Python security)
- **SpotBugs** (Java)
- **ktlint** (Kotlin)
- **Hadolint** (Dockerfile)
- **lintr** (R)
- **sqlfluff** (SQL)
- **tflint** (Terraform)

#### Build Systems & DevOps Tools
- **CMake** build output (errors, warnings, configuration issues)
- **GNU Make** build output (compilation errors, linking issues, target failures)
- **Python builds** (pip, setuptools, pytest, wheel building)
- **Node.js builds** (npm, yarn, webpack, jest, eslint)
- **kube-score** (Kubernetes manifests)
- **Generic lint** (fallback parser for common error formats)

## Schema

All parsers output a standardized 17-field schema optimized for agent analysis:

| Field | Type | Description |
|-------|------|-------------|
| `event_id` | BIGINT | Unique identifier for each event |
| `tool_name` | VARCHAR | Name of the tool that generated the output |
| `event_type` | VARCHAR | Category of event (TEST_RESULT, BUILD_ERROR, LINT_ISSUE) |
| `file_path` | VARCHAR | Path to the file with the issue |
| `line_number` | INTEGER | Line number (-1 if not applicable) |
| `column_number` | INTEGER | Column number (-1 if not applicable) |
| `function_name` | VARCHAR | Function/method/test name |
| `status` | VARCHAR | Status (PASSED, FAILED, ERROR, WARNING, INFO, SKIPPED) |
| `severity` | VARCHAR | Severity level (error, warning, info, critical) |
| `category` | VARCHAR | Specific category (compilation, test_failure, lint_error, etc.) |
| `message` | VARCHAR | Detailed error/warning message |
| `suggestion` | VARCHAR | Suggested fix or improvement |
| `error_code` | VARCHAR | Tool-specific error code or rule ID |
| `test_name` | VARCHAR | Full test identifier |
| `execution_time` | DOUBLE | Test execution time in seconds |
| `raw_output` | VARCHAR | Original raw output |
| `structured_data` | VARCHAR | Parser-specific metadata |

## Usage Examples

### Analyze Build Failures
```sql
-- Parse CMake build output to find compilation errors
SELECT file_path, line_number, message, category
FROM read_test_results('build.log', 'cmake_build')
WHERE status = 'ERROR' AND category = 'compilation'
ORDER BY file_path, line_number;
```

### Test Results Analysis
```sql
-- Analyze Python test failures
SELECT test_name, message, execution_time
FROM read_test_results('pytest_output.json', 'pytest_json')
WHERE status = 'FAILED'
ORDER BY execution_time DESC;
```

### Build System Comparison
```sql
-- Compare error rates across different build systems
SELECT tool_name, category, COUNT(*) as error_count
FROM read_test_results('all_builds.log', 'AUTO')
WHERE status = 'ERROR'
GROUP BY tool_name, category
ORDER BY error_count DESC;
```

### Linting Issue Summary
```sql
-- Get ESLint rule violation summary
SELECT error_code, COUNT(*) as violations, 
       AVG(line_number) as avg_line
FROM read_test_results('eslint_output.json', 'eslint_json')
WHERE status IN ('ERROR', 'WARNING')
GROUP BY error_code
ORDER BY violations DESC;
```

### Agent Workflow Analysis
```sql
-- Find all compilation errors for agent remediation
SELECT file_path, line_number, column_number, message, suggestion
FROM parse_test_results(?, 'AUTO')  -- ? = build output from agent
WHERE event_type = 'BUILD_ERROR' AND category = 'compilation'
ORDER BY file_path, line_number;
```

## Auto-Detection

Duck Hunt automatically detects formats based on content patterns when `format := 'AUTO'` (default):

- **JSON formats**: Detected by specific field patterns
- **Build outputs**: Detected by tool-specific error patterns  
- **Test outputs**: Detected by result format patterns

## Building

### Prerequisites
- DuckDB development environment
- VCPKG for dependency management (optional)

### Build Steps
```bash
# Clone and build
git clone <repository-url>
cd duck_hunt
make release

# Run tests
make test
```

### Build Output
- `./build/release/duckdb` - DuckDB shell with extension loaded
- `./build/release/extension/duck_hunt/duck_hunt.duckdb_extension` - Loadable extension
- `./build/release/test/unittest` - Test runner

## Installation

### From Source
```sql
-- After building
LOAD './build/release/extension/duck_hunt/duck_hunt.duckdb_extension';
```

### Using Extension
```sql
-- Enable unsigned extensions if needed
SET allow_unsigned_extensions = true;

-- Install and load
INSTALL duck_hunt FROM '<repository-url>';
LOAD duck_hunt;
```

## Use Cases

### DevOps & CI/CD
- **Build failure analysis**: Parse compilation errors across multiple languages
- **Test result aggregation**: Combine test outputs from different frameworks
- **Quality gate enforcement**: SQL queries to check error thresholds
- **Pipeline debugging**: Analyze build logs to identify bottlenecks

### Agent-Driven Development
- **Error parsing**: Extract actionable error information for AI agents
- **Build health monitoring**: Track error trends over time
- **Automated remediation**: Identify patterns in build failures
- **Tool integration**: Unified interface for multiple development tools

### Development Analytics
- **Code quality metrics**: Analyze linting results across projects
- **Test performance tracking**: Monitor test execution times
- **Error categorization**: Group and analyze different types of issues
- **Cross-platform analysis**: Compare build results across environments

## Development

This extension follows DuckDB extension conventions and includes:

- Comprehensive test suite in `test/sql/`
- Sample data files in `workspace/`
- Standardized ValidationEvent schema
- Monadic API design principles
- Extensive format auto-detection

For contributing guidelines, see `CONTRIBUTING.md`.

## License

[Add your license information here]