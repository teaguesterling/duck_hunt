# Duck Hunt - DuckDB Test Results & Build Output Parser Extension

A comprehensive DuckDB extension for parsing and analyzing test results, build outputs, and DevOps pipeline data from various tools and formats. Perfect for automated development workflows, CI/CD analysis, and agent-driven programming tasks.

## Features

Duck Hunt provides powerful SQL-based analysis of development tool outputs through table functions and utilities:

### Core Functions

#### Table Functions
- **`read_duck_hunt_log(file_path, format := 'AUTO')`** - Parse test results and build outputs from files
- **`parse_duck_hunt_log(content, format := 'AUTO')`** - Parse test results and build outputs from strings
- **`read_duck_hunt_workflow_log(file_path, format := 'AUTO')`** - Parse CI/CD workflow logs from files
- **`parse_duck_hunt_workflow_log(content, format := 'AUTO')`** - Parse CI/CD workflow logs from strings

#### Scalar Functions
- **`status_badge(status)`** - Convert status string to badge: `[ OK ]`, `[FAIL]`, `[WARN]`, `[ .. ]`, `[ ?? ]`
- **`status_badge(error_count, warning_count)`** - Compute badge from error/warning counts
- **`status_badge(error_count, warning_count, is_running)`** - Compute badge with running state

### Supported Formats

| Format String | Tool | Description |
|---------------|------|-------------|
| `auto` | Auto-detect | Automatically detect format from content |
| `regexp:<pattern>` | Dynamic | Custom regex with named capture groups |
| **Test Frameworks** | | |
| `pytest_json` | pytest | Python test framework JSON output |
| `pytest_text` | pytest | Python test framework text output |
| `gotest_json` | Go test | Go testing framework JSON output |
| `cargo_test_json` | Cargo test | Rust test framework JSON output |
| `junit_text` | JUnit | Java JUnit 4/5, TestNG, Surefire text output |
| `rspec_text` | RSpec | Ruby test framework text output |
| `mocha_chai_text` | Mocha/Chai | JavaScript test framework text output |
| `gtest_text` | Google Test | C++ test framework text output |
| `nunit_xunit_text` | NUnit/xUnit | .NET test frameworks text output |
| `duckdb_test` | DuckDB | DuckDB test runner output |
| **Linting & Static Analysis** | | |
| `eslint_json` | ESLint | JavaScript/TypeScript linter JSON |
| `rubocop_json` | RuboCop | Ruby linter JSON |
| `swiftlint_json` | SwiftLint | Swift linter JSON |
| `phpstan_json` | PHPStan | PHP static analyzer JSON |
| `shellcheck_json` | Shellcheck | Shell script linter JSON |
| `stylelint_json` | Stylelint | CSS/SCSS linter JSON |
| `clippy_json` | Clippy | Rust linter JSON |
| `markdownlint_json` | Markdownlint | Markdown linter JSON |
| `yamllint_json` | yamllint | YAML linter JSON |
| `bandit_json` | Bandit | Python security linter JSON |
| `bandit_text` | Bandit | Python security linter text |
| `spotbugs_json` | SpotBugs | Java bug finder JSON |
| `ktlint_json` | ktlint | Kotlin linter JSON |
| `hadolint_json` | Hadolint | Dockerfile linter JSON |
| `lintr_json` | lintr | R linter JSON |
| `sqlfluff_json` | sqlfluff | SQL linter JSON |
| `tflint_json` | tflint | Terraform linter JSON |
| `pylint_text` | Pylint | Python linter text output |
| `flake8_text` | Flake8 | Python style checker text |
| `black_text` | Black | Python formatter text |
| `mypy_text` | MyPy | Python type checker text |
| `isort_text` | isort | Python import sorter text |
| `autopep8_text` | autopep8 | Python formatter text |
| `yapf_text` | YAPF | Python formatter text |
| `clang_tidy_text` | clang-tidy | C/C++ linter text |
| `kube_score_json` | kube-score | Kubernetes manifest analyzer JSON |
| `generic_lint` | Generic | Fallback for `file:line:col: severity: msg` format |
| **Build Systems** | | |
| `cmake_build` | CMake | CMake build output |
| `make_error` | GNU Make | Make build errors and warnings |
| `python_build` | Python | pip, setuptools, wheel build output |
| `node_build` | Node.js | npm, yarn, webpack build output |
| `cargo_build` | Cargo | Rust build output |
| `maven_build` | Maven | Java Maven build output |
| `gradle_build` | Gradle | Java/Android Gradle build output |
| `msbuild` | MSBuild | .NET build output |
| `bazel_build` | Bazel | Bazel build output |
| `docker_build` | Docker | Docker build output |
| **Debugging & Coverage** | | |
| `valgrind` | Valgrind | Memory analyzer (Memcheck, Helgrind, etc.) |
| `gdb_lldb` | GDB/LLDB | Debugger output (crashes, stack traces) |
| `coverage_text` | Coverage | Code coverage text output |
| `pytest_cov_text` | pytest-cov | pytest coverage text output |
| **CI/CD & Infrastructure** | | |
| `github_actions_text` | GitHub Actions | Workflow log output |
| `github_cli` | GitHub CLI | gh command output |
| `gitlab_ci_text` | GitLab CI | Pipeline log output |
| `jenkins_text` | Jenkins | Build log output |
| `drone_ci_text` | Drone CI | Pipeline log output |
| `terraform_text` | Terraform | Infrastructure tool output |
| `ansible_text` | Ansible | Playbook execution output |

## Schema

All parsers output a standardized schema optimized for agent analysis:

### Core Fields (17)
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

### Advanced Error Analysis Fields (Phase 3B)
| Field | Type | Description |
|-------|------|-------------|
| `error_fingerprint` | VARCHAR | Normalized error signature for pattern detection |
| `similarity_score` | DOUBLE | Similarity to pattern cluster centroid (0.0-1.0) |
| `pattern_id` | BIGINT | Assigned error pattern group ID (-1 if unassigned) |
| `root_cause_category` | VARCHAR | Detected root cause type (network, permission, config, etc.) |

### Enhanced Metadata Fields
| Field | Type | Description |
|-------|------|-------------|
| `source_file` | VARCHAR | Original source file path |
| `build_id` | VARCHAR | Build identifier extracted from file paths |
| `environment` | VARCHAR | Environment detected from file paths (prod, staging, dev) |
| `file_index` | INTEGER | Processing order index for multi-file operations |

### Workflow CI/CD Fields (Phase 3C)
| Field | Type | Description |
|-------|------|-------------|
| `workflow_name` | VARCHAR | Name of the workflow/pipeline |
| `job_name` | VARCHAR | Name of the current job/stage |
| `step_name` | VARCHAR | Name of the current step |
| `workflow_run_id` | VARCHAR | Unique identifier for the workflow run |
| `job_id` | VARCHAR | Unique identifier for the job |
| `step_id` | VARCHAR | Unique identifier for the step |
| `workflow_status` | VARCHAR | Overall workflow status (running, success, failure, cancelled) |
| `job_status` | VARCHAR | Job status (pending, running, success, failure, skipped) |
| `step_status` | VARCHAR | Step status (pending, running, success, failure, skipped) |
| `started_at` | VARCHAR | When the workflow/job/step started (ISO timestamp) |
| `completed_at` | VARCHAR | When it completed (ISO timestamp) |
| `duration` | DOUBLE | Duration in seconds |
| `workflow_type` | VARCHAR | Type of workflow system (github_actions, gitlab_ci, jenkins, docker_build) |
| `hierarchy_level` | INTEGER | Hierarchy level (0=workflow, 1=job, 2=step, 3=tool_output) |
| `parent_id` | VARCHAR | ID of the parent element in hierarchy |

## Usage Examples

### Pipeline Integration & Live Analysis

**Capture build logs and analyze in real-time:**
```bash
# Capture make output to both stderr and DuckDB for analysis
make | tee /dev/stderr | ./build/release/duckdb -s "COPY ( SELECT * EXCLUDE (raw_output) FROM read_duck_hunt_log('/dev/stdin', 'auto') ) TO '.build-logs/$(git rev-parse --short HEAD).json' (APPEND true);"

# Analyze build errors from stdin pipe
cat make.out | ./build/release/duckdb -json -s "SELECT * EXCLUDE (raw_output) FROM read_duck_hunt_log('/dev/stdin', 'auto') WHERE severity='error';"

# Real-time error monitoring during builds
./build.sh | ./build/release/duckdb -json -s "SELECT tool_name, COUNT(*) as error_count, GROUP_CONCAT(message, '\n') as errors FROM read_duck_hunt_log('/dev/stdin', 'auto') WHERE status='ERROR' GROUP BY tool_name;"
```

### Advanced Error Pattern Analysis

**Leverage Phase 3B error clustering capabilities:**
```sql
-- Find error patterns and group similar failures
SELECT pattern_id, COUNT(*) as occurrence_count, 
       MIN(similarity_score) as min_similarity,
       MAX(similarity_score) as max_similarity,
       ANY_VALUE(message) as representative_message
FROM read_duck_hunt_log('build_logs/**/*.log', 'auto')
WHERE pattern_id != -1
GROUP BY pattern_id
ORDER BY occurrence_count DESC;

-- Analyze error root causes
SELECT root_cause_category, COUNT(*) as error_count,
       GROUP_CONCAT(DISTINCT tool_name) as affected_tools
FROM read_duck_hunt_log('ci_logs/**/*.txt', 'auto') 
WHERE root_cause_category IS NOT NULL
GROUP BY root_cause_category
ORDER BY error_count DESC;

-- Find anomalous errors (low similarity to known patterns)
SELECT file_path, line_number, message, similarity_score
FROM read_duck_hunt_log('logs/**/*.log', 'auto')
WHERE similarity_score < 0.3 AND pattern_id != -1
ORDER BY similarity_score;
```

### Analyze Build Failures
```sql
-- Parse CMake build output to find compilation errors
SELECT file_path, line_number, message, category
FROM read_duck_hunt_log('build.log', 'cmake_build')
WHERE status = 'ERROR' AND category = 'compilation'
ORDER BY file_path, line_number;
```

### Test Results Analysis
```sql
-- Analyze Python test failures
SELECT test_name, message, execution_time
FROM read_duck_hunt_log('pytest_output.json', 'pytest_json')
WHERE status = 'FAILED'
ORDER BY execution_time DESC;

-- Analyze RSpec test failures across Ruby test suites
SELECT test_name, message, file_path, line_number
FROM read_duck_hunt_log('rspec_output.txt', 'rspec_text')
WHERE status = 'FAIL'
ORDER BY file_path, line_number;

-- Monitor Google Test performance across C++ test suites
SELECT function_name, AVG(execution_time) as avg_time, COUNT(*) as test_count
FROM read_duck_hunt_log('gtest_output.txt', 'gtest_text')
WHERE status = 'PASS'
GROUP BY function_name
ORDER BY avg_time DESC;

-- Analyze .NET test results from NUnit and xUnit frameworks
SELECT tool_name, status, COUNT(*) as test_count, 
       AVG(execution_time) as avg_execution_time
FROM read_duck_hunt_log('nunit_xunit_output.txt', 'nunit_xunit_text')
GROUP BY tool_name, status
ORDER BY tool_name, status;
```

### Build System Comparison
```sql
-- Compare error rates across different build systems
SELECT tool_name, category, COUNT(*) as error_count
FROM read_duck_hunt_log('all_builds.log', 'AUTO')
WHERE status = 'ERROR'
GROUP BY tool_name, category
ORDER BY error_count DESC;
```

### Linting Issue Summary
```sql
-- Get ESLint rule violation summary
SELECT error_code, COUNT(*) as violations, 
       AVG(line_number) as avg_line
FROM read_duck_hunt_log('eslint_output.json', 'eslint_json')
WHERE status IN ('ERROR', 'WARNING')
GROUP BY error_code
ORDER BY violations DESC;
```

### Multi-Framework Test Analysis
```sql
-- Compare test performance across different frameworks and languages
SELECT tool_name, 
       COUNT(*) as total_tests,
       SUM(CASE WHEN status = 'PASS' THEN 1 ELSE 0 END) as passed,
       SUM(CASE WHEN status = 'FAIL' THEN 1 ELSE 0 END) as failed,
       AVG(execution_time) as avg_execution_time
FROM read_duck_hunt_log('all_test_outputs/*.txt', 'AUTO')
WHERE event_type = 'TEST_RESULT'
GROUP BY tool_name
ORDER BY total_tests DESC;

-- Analyze JavaScript test suites (Mocha/Chai) for performance bottlenecks
SELECT function_name, test_name, execution_time, message
FROM read_duck_hunt_log('mocha_output.txt', 'mocha_chai_text')
WHERE status = 'PASS' AND execution_time > 1000  -- Tests taking > 1 second
ORDER BY execution_time DESC;
```

### Agent Workflow Analysis
```sql
-- Find all compilation errors for agent remediation
SELECT file_path, line_number, column_number, message, suggestion
FROM parse_duck_hunt_log(?, 'AUTO')  -- ? = build output from agent
WHERE event_type = 'BUILD_ERROR' AND category = 'compilation'
ORDER BY file_path, line_number;
```

### Status Badges
```sql
-- Add status badges to build summaries
SELECT status_badge(status) as badge, tool_name, message
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status IN ('ERROR', 'WARNING', 'PASS');
-- Returns: [FAIL] make  undefined reference to 'foo'
--          [WARN] gcc   unused variable 'x'
--          [ OK ] test  All tests passed

-- Compute badge from aggregated counts
SELECT status_badge(
    COUNT(CASE WHEN status = 'ERROR' THEN 1 END),
    COUNT(CASE WHEN status = 'WARNING' THEN 1 END)
) as overall_status
FROM read_duck_hunt_log('build.log', 'auto');
-- Returns: [FAIL] if any errors, [WARN] if warnings only, [ OK ] otherwise

-- Show running status for live builds
SELECT status_badge(0, 0, true) as status;  -- Returns: [ .. ]
```

### CI/CD Workflow Log Analysis

**Parse and analyze workflow execution logs:**
```sql
-- Analyze GitHub Actions workflow failures
SELECT workflow_name, job_name, step_name, message, step_status
FROM read_duck_hunt_workflow_log('github_actions.log', 'github_actions')
WHERE step_status IN ('failure', 'error')
ORDER BY event_id;

-- Track workflow hierarchy and execution flow
SELECT hierarchy_level, workflow_type, job_name, step_name,
       COUNT(*) as event_count,
       SUM(CASE WHEN status = 'ERROR' THEN 1 ELSE 0 END) as errors
FROM read_duck_hunt_workflow_log('workflow_log/*.log', 'auto')
GROUP BY hierarchy_level, workflow_type, job_name, step_name
ORDER BY hierarchy_level, event_count DESC;

-- Compare workflow performance across CI systems
SELECT workflow_type,
       COUNT(DISTINCT workflow_run_id) as runs,
       AVG(duration) as avg_duration,
       SUM(CASE WHEN workflow_status = 'failure' THEN 1 ELSE 0 END) as failures
FROM read_duck_hunt_workflow_log('ci_logs/**/*.log', 'auto')
WHERE hierarchy_level = 0
GROUP BY workflow_type
ORDER BY avg_duration DESC;

-- Analyze Jenkins build stages
SELECT job_name, step_name, step_status, duration, message
FROM read_duck_hunt_workflow_log('jenkins_console.log', 'jenkins')
WHERE hierarchy_level >= 2
ORDER BY event_id;

-- Find Docker build layer failures
SELECT step_name, message, severity
FROM read_duck_hunt_workflow_log('docker_build.log', 'docker_build')
WHERE status = 'ERROR' AND hierarchy_level = 2
ORDER BY event_id;
```

### Memory Debugging Analysis
```sql
-- Analyze Valgrind memory errors for debugging
SELECT tool_name, category, COUNT(*) as error_count, 
       GROUP_CONCAT(DISTINCT file_path) as affected_files
FROM read_duck_hunt_log('valgrind_output.txt', 'valgrind')
WHERE event_type IN ('memory_error', 'memory_leak')
GROUP BY tool_name, category
ORDER BY error_count DESC;

-- Extract memory leak details with stack traces
SELECT file_path, function_name, message, structured_data
FROM read_duck_hunt_log('valgrind_output.txt', 'valgrind')
WHERE event_type = 'memory_leak' AND category = 'memory_leak'
ORDER BY file_path;
```

### Debugger Output Analysis
```sql
-- Analyze GDB/LLDB crashes and debugging events
SELECT tool_name, event_type, COUNT(*) as event_count,
       GROUP_CONCAT(DISTINCT category) as categories
FROM read_duck_hunt_log('gdb_output.txt', 'gdb_lldb')
WHERE event_type IN ('crash_signal', 'debug_event')
GROUP BY tool_name, event_type
ORDER BY event_count DESC;

-- Extract crash details with stack traces
SELECT file_path, line_number, function_name, message, error_code
FROM read_duck_hunt_log('debugger.log', 'gdb_lldb')
WHERE event_type = 'crash_signal'
ORDER BY file_path, line_number;

-- Find all breakpoint hits for debugging workflow
SELECT function_name, file_path, line_number, message
FROM read_duck_hunt_log('debug_session.txt', 'auto')
WHERE category = 'breakpoint_hit'
ORDER BY event_id;
```

### Dynamic Regexp Parser

Parse any log format using custom regex patterns with named capture groups. Named groups are automatically mapped to the ValidationEvent schema:

| Named Group | Maps To | Description |
|-------------|---------|-------------|
| `severity`, `level` | status, severity | ERROR, WARNING, INFO, etc. |
| `message`, `msg`, `description` | message | The main error/warning message |
| `file`, `file_path`, `path` | file_path | Path to the file |
| `line`, `line_number`, `lineno` | line_number | Line number |
| `column`, `col` | column_number | Column number |
| `code`, `error_code`, `rule` | error_code | Error code or rule ID |
| `category`, `type` | category | Category of the issue |
| `test_name`, `test`, `name` | test_name | Test name |
| `suggestion`, `fix`, `hint` | suggestion | Suggested fix |
| `tool`, `tool_name` | tool_name | Override the tool name |

**Examples:**

```sql
-- Parse custom application logs
SELECT severity, message, file_path, line_number
FROM parse_duck_hunt_log('
myapp.ERROR.database: Connection failed at line 42
myapp.WARNING.cache: Cache miss for key abc123
myapp.ERROR.api: Timeout after 30s',
'regexp:myapp\.(?P<severity>ERROR|WARNING)\.(?P<category>\w+):\s+(?P<message>.+)');

-- Parse GCC-style compiler output with custom pattern
SELECT file_path, line_number, severity, message
FROM read_duck_hunt_log('build.log',
'regexp:(?P<file>[^:]+):(?P<line>\d+):\s+(?P<severity>error|warning):\s+(?P<message>.+)');

-- Parse test output with test names and results
SELECT test_name, status, message
FROM parse_duck_hunt_log('
FAIL test_user_auth: Expected 200, got 401
PASS test_data_fetch: All assertions passed
FAIL test_validation: Invalid input format',
'regexp:(?P<severity>FAIL|PASS)\s+(?P<test_name>\w+):\s+(?P<message>.+)');

-- Parse custom CI/CD logs with error codes
SELECT error_code, severity, message
FROM parse_duck_hunt_log('
[E001] Build failed: missing dependency
[W002] Deprecated API usage detected
[E003] Test coverage below threshold',
'regexp:\[(?P<code>[EW]\d+)\]\s+(?P<message>.+)');
```

**Notes:**
- Supports both Python-style `(?P<name>...)` and ECMAScript-style `(?<name>...)` named groups
- Lines not matching the pattern are skipped
- If no matches found, returns a single summary event
- Multi-file glob patterns are not supported with regexp format (use single files)

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

## Future Work

Duck Hunt currently supports 40+ formats across test frameworks, linting tools, build systems, and debugging tools. The following expansions would provide comprehensive coverage of the entire development ecosystem:

### 🔥 High Priority Build Systems
- **Bazel** (Google's system) - Large-scale projects
- **SBT** (Scala Build Tool)
- **Buck** (Facebook's build system) 
- **Pants** (Twitter's build system)
- **Ninja** (Low-level build system)

### 🐛 Debugging & Analysis Tools
- **AddressSanitizer/ThreadSanitizer** - Fast sanitizer output
- **Clang Static Analyzer** - Static analysis reports
- **perf** - Linux performance profiling output
- **Intel VTune** - CPU profiling reports
- **gperftools** - Google performance tools

### 🔍 Linting & Code Quality
- **SonarQube** - Multi-language analysis (XML/JSON)
- **golangci-lint** (Go) - Text/JSON output  
- **Prettier** (JavaScript/TypeScript formatter) - Text output
- **CodeClimate** - Multi-language JSON output
- **Coverity** - Enterprise static analysis
- **PVS-Studio** - Cross-platform analysis

### 🔄 CI/CD & DevOps (Additional)
- **Ansible** - Playbook execution logs
- **Terraform** - Apply/plan output parsing
- **CircleCI** - Pipeline log parsing
- **Travis CI** - Build log parsing

### 📦 Package Managers
- **apt/dpkg** (Debian/Ubuntu) - Package management logs
- **yum/rpm** (RedHat/CentOS) - Package management logs
- **brew** (macOS) - Package installation logs
- **composer** (PHP) - Dependency management

**Target**: ~60+ total formats covering 90% of development tool ecosystem
**Current**: 45+ formats (75% coverage) + dynamic regexp parser for unlimited custom formats

### ✅ Already Implemented (Comprehensive Coverage)
- **Test Frameworks**: pytest, Go test, Cargo test, JUnit, RSpec, Mocha/Chai, Google Test, NUnit/xUnit, DuckDB test
- **Python Linting**: Pylint, Flake8, Black, MyPy, Bandit
- **Build Systems**: CMake, Make, Python, Node.js, Cargo, Maven, Gradle, MSBuild
- **JSON Linters**: ESLint, RuboCop, SwiftLint, PHPStan, Shellcheck, Stylelint, Clippy, etc.
- **Debugging Tools**: Valgrind (full suite), GDB/LLDB
- **CI/CD Workflow Engines**: GitHub Actions, GitLab CI, Jenkins, Docker Build
- **Advanced Features**: Error pattern clustering, similarity scoring, root cause analysis, workflow hierarchy parsing

Duck Hunt provides robust parsing across 9 major test frameworks, 4 CI/CD workflow engines, and comprehensive coverage of Python, JavaScript, Java, C/C++, Rust, and other ecosystems. The remaining roadmap focuses on enterprise tools, specialized build systems, and additional CI/CD platform integration.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**MIT License**

Copyright (c) 2024 Duck Hunt Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

**Duck Hunt** | MIT License | Built with ❤️ for agent-driven development

*Comprehensive test results & build output parser extension for DuckDB*