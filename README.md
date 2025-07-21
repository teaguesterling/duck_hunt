# Duck Hunt - DuckDB Test Results & Build Output Parser Extension

A comprehensive DuckDB extension for parsing and analyzing test results, build outputs, and DevOps pipeline data from various tools and formats. Perfect for automated development workflows, CI/CD analysis, and agent-driven programming tasks.

## Features

Duck Hunt provides powerful SQL-based analysis of development tool outputs through two main table functions:

### Core Functions

- **`read_test_results(file_path, format := 'AUTO')`** - Parse test results and build outputs from files
- **`parse_test_results(content, format := 'AUTO')`** - Parse test results and build outputs from strings

### Supported Formats (40 Total)

#### Test Frameworks (9 Total)
- **pytest** (JSON & text output)
- **Go test** (JSON output) 
- **Cargo test** (Rust JSON output)
- **JUnit** (Java text output - JUnit 4/5, TestNG, Surefire, Gradle test results)
- **RSpec** (Ruby text output - examples, failures, pending tests, execution times)
- **Mocha/Chai** (JavaScript text output - context parsing, assertion failures, summary statistics)
- **Google Test** (C++ console output - test suites, individual results, failure details)
- **NUnit/xUnit** (C#/.NET console output - dual framework support, test summaries, stack traces)
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
- **Cargo builds** (Rust compilation, testing, clippy, formatting)
- **Maven builds** (Java compilation, JUnit testing, checkstyle, spotbugs, PMD)
- **Gradle builds** (Java/Android compilation, testing, checkstyle, spotbugs, android lint)
- **MSBuild** (C#/.NET compilation, xUnit testing, code analyzers)
- **kube-score** (Kubernetes manifests)
- **Generic lint** (fallback parser for common error formats)

#### Debugging & Analysis Tools
- **Valgrind** (Memcheck, Helgrind, Cachegrind, Massif, DRD - memory analysis and thread debugging)
- **GDB/LLDB** (Debugger output - crashes, breakpoints, stack traces, memory errors)

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

## Usage Examples

### Pipeline Integration & Live Analysis

**Capture build logs and analyze in real-time:**
```bash
# Capture make output to both stderr and DuckDB for analysis
make | tee /dev/stderr | ./build/release/duckdb -s "COPY ( SELECT * EXCLUDE (raw_output) FROM read_test_results('/dev/stdin', 'auto') ) TO '.build-logs/$(git rev-parse --short HEAD).json' (APPEND true);"

# Analyze build errors from stdin pipe
cat make.out | ./build/release/duckdb -json -s "SELECT * EXCLUDE (raw_output) FROM read_test_results('/dev/stdin', 'auto') WHERE severity='error';"

# Real-time error monitoring during builds
./build.sh | ./build/release/duckdb -json -s "SELECT tool_name, COUNT(*) as error_count, GROUP_CONCAT(message, '\n') as errors FROM read_test_results('/dev/stdin', 'auto') WHERE status='ERROR' GROUP BY tool_name;"
```

### Advanced Error Pattern Analysis

**Leverage Phase 3B error clustering capabilities:**
```sql
-- Find error patterns and group similar failures
SELECT pattern_id, COUNT(*) as occurrence_count, 
       MIN(similarity_score) as min_similarity,
       MAX(similarity_score) as max_similarity,
       ANY_VALUE(message) as representative_message
FROM read_test_results('build_logs/**/*.log', 'auto')
WHERE pattern_id != -1
GROUP BY pattern_id
ORDER BY occurrence_count DESC;

-- Analyze error root causes
SELECT root_cause_category, COUNT(*) as error_count,
       GROUP_CONCAT(DISTINCT tool_name) as affected_tools
FROM read_test_results('ci_logs/**/*.txt', 'auto') 
WHERE root_cause_category IS NOT NULL
GROUP BY root_cause_category
ORDER BY error_count DESC;

-- Find anomalous errors (low similarity to known patterns)
SELECT file_path, line_number, message, similarity_score
FROM read_test_results('logs/**/*.log', 'auto')
WHERE similarity_score < 0.3 AND pattern_id != -1
ORDER BY similarity_score;
```

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

-- Analyze RSpec test failures across Ruby test suites
SELECT test_name, message, file_path, line_number
FROM read_test_results('rspec_output.txt', 'rspec_text')
WHERE status = 'FAIL'
ORDER BY file_path, line_number;

-- Monitor Google Test performance across C++ test suites
SELECT function_name, AVG(execution_time) as avg_time, COUNT(*) as test_count
FROM read_test_results('gtest_output.txt', 'gtest_text')
WHERE status = 'PASS'
GROUP BY function_name
ORDER BY avg_time DESC;

-- Analyze .NET test results from NUnit and xUnit frameworks
SELECT tool_name, status, COUNT(*) as test_count, 
       AVG(execution_time) as avg_execution_time
FROM read_test_results('nunit_xunit_output.txt', 'nunit_xunit_text')
GROUP BY tool_name, status
ORDER BY tool_name, status;
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

### Multi-Framework Test Analysis
```sql
-- Compare test performance across different frameworks and languages
SELECT tool_name, 
       COUNT(*) as total_tests,
       SUM(CASE WHEN status = 'PASS' THEN 1 ELSE 0 END) as passed,
       SUM(CASE WHEN status = 'FAIL' THEN 1 ELSE 0 END) as failed,
       AVG(execution_time) as avg_execution_time
FROM read_test_results('all_test_outputs/*.txt', 'AUTO')
WHERE event_type = 'TEST_RESULT'
GROUP BY tool_name
ORDER BY total_tests DESC;

-- Analyze JavaScript test suites (Mocha/Chai) for performance bottlenecks
SELECT function_name, test_name, execution_time, message
FROM read_test_results('mocha_output.txt', 'mocha_chai_text')
WHERE status = 'PASS' AND execution_time > 1000  -- Tests taking > 1 second
ORDER BY execution_time DESC;
```

### Agent Workflow Analysis
```sql
-- Find all compilation errors for agent remediation
SELECT file_path, line_number, column_number, message, suggestion
FROM parse_test_results(?, 'AUTO')  -- ? = build output from agent
WHERE event_type = 'BUILD_ERROR' AND category = 'compilation'
ORDER BY file_path, line_number;
```

### Memory Debugging Analysis
```sql
-- Analyze Valgrind memory errors for debugging
SELECT tool_name, category, COUNT(*) as error_count, 
       GROUP_CONCAT(DISTINCT file_path) as affected_files
FROM read_test_results('valgrind_output.txt', 'valgrind')
WHERE event_type IN ('memory_error', 'memory_leak')
GROUP BY tool_name, category
ORDER BY error_count DESC;

-- Extract memory leak details with stack traces
SELECT file_path, function_name, message, structured_data
FROM read_test_results('valgrind_output.txt', 'valgrind')
WHERE event_type = 'memory_leak' AND category = 'memory_leak'
ORDER BY file_path;
```

### Debugger Output Analysis
```sql
-- Analyze GDB/LLDB crashes and debugging events
SELECT tool_name, event_type, COUNT(*) as event_count,
       GROUP_CONCAT(DISTINCT category) as categories
FROM read_test_results('gdb_output.txt', 'gdb_lldb')
WHERE event_type IN ('crash_signal', 'debug_event')
GROUP BY tool_name, event_type
ORDER BY event_count DESC;

-- Extract crash details with stack traces
SELECT file_path, line_number, function_name, message, error_code
FROM read_test_results('debugger.log', 'gdb_lldb')
WHERE event_type = 'crash_signal'
ORDER BY file_path, line_number;

-- Find all breakpoint hits for debugging workflow
SELECT function_name, file_path, line_number, message
FROM read_test_results('debug_session.txt', 'auto')
WHERE category = 'breakpoint_hit'
ORDER BY event_id;
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

## Future Work

Duck Hunt currently supports 40 formats across test frameworks, linting tools, build systems, and debugging tools. The following expansions would provide comprehensive coverage of the entire development ecosystem:

### 🔥 High Priority Build Systems
- **Bazel** (Google's system) - Large-scale projects

### 🐛 Debugging & Analysis Tools
- **AddressSanitizer/ThreadSanitizer** - Fast sanitizer output
- **Clang Static Analyzer** - Static analysis reports
- **perf** - Linux performance profiling output

### 🔍 Linting & Code Quality
- **SonarQube** - Multi-language analysis (XML/JSON)
- **Pylint/Flake8** (Python) - Text/JSON output
- **golangci-lint** (Go) - Text/JSON output  
- **Black/Prettier** (Formatters) - Text output
- **CodeClimate** - Multi-language JSON output

### 🔄 CI/CD & DevOps
- **GitHub Actions** - Workflow log parsing
- **GitLab CI** - Pipeline log parsing
- **Jenkins** - Console output parsing
- **Docker** - Build output parsing
- **Ansible** - Playbook execution logs

### 📦 Package Managers
- **apt/dpkg** (Debian/Ubuntu) - Package management logs
- **yum/rpm** (RedHat/CentOS) - Package management logs
- **brew** (macOS) - Package installation logs
- **composer** (PHP) - Dependency management

### 🛠️ Specialized Build Systems
- **SBT** (Scala Build Tool)
- **Buck** (Facebook's build system) 
- **Pants** (Twitter's build system)
- **Ninja** (Low-level build system)

### 📊 Performance & Monitoring
- **Intel VTune** - CPU profiling reports
- **gperftools** - Google performance tools
- **Coverity** - Enterprise static analysis
- **PVS-Studio** - Cross-platform analysis

**Target**: ~70+ total formats covering 95% of development tool ecosystem
**Current**: 40 formats (57% coverage)

With comprehensive test framework coverage now complete across Ruby, JavaScript, C++, and C#/.NET ecosystems, Duck Hunt provides robust parsing for 9 major test frameworks. This roadmap would establish Duck Hunt as the definitive tool for agent-driven development workflow analysis across all major programming languages, build systems, and development tools.

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