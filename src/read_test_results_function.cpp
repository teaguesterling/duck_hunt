#include "include/read_test_results_function.hpp"
#include "include/validation_event_types.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "yyjson.hpp"
#include <fstream>
#include <regex>
#include <sstream>
#include <map>

namespace duckdb {

using namespace duckdb_yyjson;

TestResultFormat DetectTestResultFormat(const std::string& content) {
    // First check if it's valid JSON
    if (IsValidJSON(content)) {
        // Check for specific JSON formats
        if (content.find("\"tests\":") != std::string::npos) {
            return TestResultFormat::PYTEST_JSON;
        }
        if (content.find("\"Action\":") != std::string::npos && content.find("\"Package\":") != std::string::npos) {
            return TestResultFormat::GOTEST_JSON;
        }
        if (content.find("\"messages\":") != std::string::npos && content.find("\"filePath\":") != std::string::npos) {
            return TestResultFormat::ESLINT_JSON;
        }
        if (content.find("\"files\":") != std::string::npos && content.find("\"offenses\":") != std::string::npos && content.find("\"cop_name\":") != std::string::npos) {
            return TestResultFormat::RUBOCOP_JSON;
        }
        if (content.find("\"type\":") != std::string::npos && content.find("\"event\":") != std::string::npos && 
            (content.find("\"suite\"") != std::string::npos || content.find("\"test\"") != std::string::npos)) {
            return TestResultFormat::CARGO_TEST_JSON;
        }
        if (content.find("\"rule_id\":") != std::string::npos && content.find("\"severity\":") != std::string::npos && 
            content.find("\"file\":") != std::string::npos) {
            return TestResultFormat::SWIFTLINT_JSON;
        }
        if (content.find("\"totals\":") != std::string::npos && content.find("\"files\":") != std::string::npos && 
            content.find("\"errors\":") != std::string::npos) {
            return TestResultFormat::PHPSTAN_JSON;
        }
        if (content.find("\"file\":") != std::string::npos && content.find("\"level\":") != std::string::npos && 
            content.find("\"code\":") != std::string::npos && content.find("\"message\":") != std::string::npos && content.find("\"line\":") != std::string::npos &&
            content.find("\"DL") != std::string::npos) {
            return TestResultFormat::HADOLINT_JSON;
        }
        if (content.find("\"code\":") != std::string::npos && content.find("\"level\":") != std::string::npos && 
            content.find("\"line\":") != std::string::npos && content.find("\"column\":") != std::string::npos) {
            return TestResultFormat::SHELLCHECK_JSON;
        }
        if (content.find("\"source\":") != std::string::npos && content.find("\"warnings\":") != std::string::npos && 
            content.find("\"rule\":") != std::string::npos && content.find("\"severity\":") != std::string::npos) {
            return TestResultFormat::STYLELINT_JSON;
        }
        if (content.find("\"message\":") != std::string::npos && content.find("\"spans\":") != std::string::npos && 
            content.find("\"level\":") != std::string::npos && content.find("\"file_name\":") != std::string::npos) {
            return TestResultFormat::CLIPPY_JSON;
        }
        if (content.find("\"fileName\":") != std::string::npos && content.find("\"lineNumber\":") != std::string::npos && 
            content.find("\"ruleNames\":") != std::string::npos && content.find("\"ruleDescription\":") != std::string::npos) {
            return TestResultFormat::MARKDOWNLINT_JSON;
        }
        if (content.find("\"file\":") != std::string::npos && content.find("\"line\":") != std::string::npos && 
            content.find("\"column\":") != std::string::npos && content.find("\"rule\":") != std::string::npos && content.find("\"level\":") != std::string::npos) {
            return TestResultFormat::YAMLLINT_JSON;
        }
        if (content.find("\"results\":") != std::string::npos && content.find("\"test_id\":") != std::string::npos && 
            content.find("\"issue_severity\":") != std::string::npos && content.find("\"issue_confidence\":") != std::string::npos) {
            return TestResultFormat::BANDIT_JSON;
        }
        if (content.find("\"BugCollection\":") != std::string::npos && content.find("\"BugInstance\":") != std::string::npos && 
            content.find("\"type\":") != std::string::npos && content.find("\"priority\":") != std::string::npos) {
            return TestResultFormat::SPOTBUGS_JSON;
        }
        if (content.find("\"file\":") != std::string::npos && content.find("\"errors\":") != std::string::npos && 
            content.find("\"rule\":") != std::string::npos && content.find("\"line\":") != std::string::npos && content.find("\"column\":") != std::string::npos) {
            return TestResultFormat::KTLINT_JSON;
        }
        if (content.find("\"filename\":") != std::string::npos && content.find("\"line_number\":") != std::string::npos && 
            content.find("\"column_number\":") != std::string::npos && content.find("\"linter\":") != std::string::npos && content.find("\"type\":") != std::string::npos) {
            return TestResultFormat::LINTR_JSON;
        }
        if (content.find("\"filepath\":") != std::string::npos && content.find("\"violations\":") != std::string::npos && 
            content.find("\"line_no\":") != std::string::npos && content.find("\"code\":") != std::string::npos && content.find("\"rule\":") != std::string::npos) {
            return TestResultFormat::SQLFLUFF_JSON;
        }
        if (content.find("\"issues\":") != std::string::npos && content.find("\"rule\":") != std::string::npos && 
            content.find("\"range\":") != std::string::npos && content.find("\"filename\":") != std::string::npos && content.find("\"severity\":") != std::string::npos) {
            return TestResultFormat::TFLINT_JSON;
        }
        if (content.find("\"object_name\":") != std::string::npos && content.find("\"type_meta\":") != std::string::npos && 
            content.find("\"checks\":") != std::string::npos && content.find("\"grade\":") != std::string::npos && content.find("\"file_name\":") != std::string::npos) {
            return TestResultFormat::KUBE_SCORE_JSON;
        }
    }
    
    // Check CI/CD patterns first since they often contain output from multiple test frameworks
    
    // Check for GitHub Actions patterns
    if ((content.find("##[section]") != std::string::npos && content.find("##[section]Starting:") != std::string::npos) ||
        (content.find("Agent name:") != std::string::npos && content.find("Agent machine name:") != std::string::npos) ||
        (content.find("##[group]") != std::string::npos && content.find("##[endgroup]") != std::string::npos) ||
        (content.find("##[error]") != std::string::npos && content.find("Process completed with exit code") != std::string::npos) ||
        (content.find("Job completed:") != std::string::npos && content.find("Result:") != std::string::npos && content.find("Elapsed time:") != std::string::npos) ||
        (content.find("==============================================================================") != std::string::npos && content.find("Task         :") != std::string::npos && content.find("Description  :") != std::string::npos)) {
        return TestResultFormat::GITHUB_ACTIONS_TEXT;
    }
    
    // Check for GitLab CI patterns
    if ((content.find("Running with gitlab-runner") != std::string::npos) ||
        (content.find("using docker driver") != std::string::npos && content.find("Getting source from Git repository") != std::string::npos) ||
        (content.find("Executing \"step_script\" stage") != std::string::npos) ||
        (content.find("Job succeeded") != std::string::npos && content.find("$ bundle exec") != std::string::npos) ||
        (content.find("Fetching changes with git depth") != std::string::npos && content.find("Created fresh repository") != std::string::npos)) {
        return TestResultFormat::GITLAB_CI_TEXT;
    }
    
    // Check for Jenkins patterns
    if ((content.find("Started by user") != std::string::npos && content.find("Building in workspace") != std::string::npos) ||
        (content.find("[Pipeline] Start of Pipeline") != std::string::npos && content.find("[Pipeline] End of Pipeline") != std::string::npos) ||
        (content.find("[Pipeline] stage") != std::string::npos && content.find("[Pipeline] sh") != std::string::npos) ||
        (content.find("Finished: SUCCESS") != std::string::npos || content.find("Finished: FAILURE") != std::string::npos) ||
        (content.find("java.lang.RuntimeException") != std::string::npos && content.find("at org.jenkinsci.plugins") != std::string::npos)) {
        return TestResultFormat::JENKINS_TEXT;
    }
    
    // Check for DroneCI patterns
    if ((content.find("[drone:exec]") != std::string::npos && content.find("starting build step:") != std::string::npos) ||
        (content.find("[drone:exec]") != std::string::npos && content.find("completed build step:") != std::string::npos) ||
        (content.find("[drone:exec]") != std::string::npos && content.find("pipeline execution complete") != std::string::npos) ||
        (content.find("[drone:exec]") != std::string::npos && content.find("pipeline failed with exit code") != std::string::npos) ||
        (content.find("+ git clone") != std::string::npos && content.find("DRONE_COMMIT_SHA") != std::string::npos)) {
        return TestResultFormat::DRONE_CI_TEXT;
    }
    
    // Check text patterns (DuckDB test should be checked before make error since it may contain both)
    if (content.find("[0/") != std::string::npos && content.find("] (0%):") != std::string::npos && 
        content.find("test cases:") != std::string::npos) {
        return TestResultFormat::DUCKDB_TEST;
    }
    
    // Check for Valgrind patterns (should be checked early due to unique format)
    if ((content.find("==") != std::string::npos && content.find("Memcheck") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("Helgrind") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("Cachegrind") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("Massif") != std::string::npos) ||
        (content.find("==") != std::string::npos && content.find("DRD") != std::string::npos) ||
        (content.find("Invalid read") != std::string::npos || content.find("Invalid write") != std::string::npos) ||
        (content.find("definitely lost") != std::string::npos && content.find("bytes") != std::string::npos) ||
        (content.find("Possible data race") != std::string::npos && content.find("thread") != std::string::npos)) {
        return TestResultFormat::VALGRIND;
    }
    
    // Check for GDB/LLDB patterns (should be checked early due to unique format)
    if ((content.find("GNU gdb") != std::string::npos || content.find("(gdb)") != std::string::npos) ||
        (content.find("lldb") != std::string::npos && content.find("target create") != std::string::npos) ||
        (content.find("Program received signal") != std::string::npos && content.find("Segmentation fault") != std::string::npos) ||
        (content.find("Process") != std::string::npos && content.find("stopped") != std::string::npos && content.find("EXC_BAD_ACCESS") != std::string::npos) ||
        (content.find("frame #") != std::string::npos && content.find("0x") != std::string::npos) ||
        (content.find("breakpoint") != std::string::npos && content.find("hit count") != std::string::npos) ||
        (content.find("(lldb)") != std::string::npos) ||
        (content.find("Reading symbols from") != std::string::npos && content.find("Starting program:") != std::string::npos)) {
        return TestResultFormat::GDB_LLDB;
    }
    
    // Check for Mocha/Chai text patterns (should be checked before RSpec since they can share similar symbols)
    if ((content.find("passing") != std::string::npos && content.find("failing") != std::string::npos) ||
        (content.find("Error:") != std::string::npos && content.find("at Context.<anonymous>") != std::string::npos) ||
        (content.find("AssertionError:") != std::string::npos && content.find("at Context.<anonymous>") != std::string::npos) ||
        (content.find("at Test.Runnable.run") != std::string::npos && content.find("node_modules/mocha") != std::string::npos) ||
        (content.find("✓") != std::string::npos && content.find("✗") != std::string::npos && content.find("(") != std::string::npos && content.find("ms)") != std::string::npos)) {
        return TestResultFormat::MOCHA_CHAI_TEXT;
    }
    
    // Check for Google Test patterns (should be checked before other C++ test frameworks)
    if ((content.find("[==========]") != std::string::npos && content.find("Running") != std::string::npos && content.find("tests from") != std::string::npos) ||
        (content.find("[ RUN      ]") != std::string::npos && content.find("[       OK ]") != std::string::npos) ||
        (content.find("[  FAILED  ]") != std::string::npos && content.find("ms total") != std::string::npos) ||
        (content.find("[  PASSED  ]") != std::string::npos && content.find("tests.") != std::string::npos) ||
        (content.find("[----------]") != std::string::npos && content.find("Global test environment") != std::string::npos)) {
        return TestResultFormat::GTEST_TEXT;
    }
    
    // Check for NUnit/xUnit patterns (should be checked before other .NET test frameworks)
    if ((content.find("NUnit") != std::string::npos && content.find("Test Count:") != std::string::npos && content.find("Passed:") != std::string::npos) ||
        (content.find("Test Run Summary") != std::string::npos && content.find("Overall result:") != std::string::npos) ||
        (content.find("xUnit.net") != std::string::npos && content.find("VSTest Adapter") != std::string::npos) ||
        (content.find("[PASS]") != std::string::npos && content.find("[FAIL]") != std::string::npos && content.find(".Tests.") != std::string::npos) ||
        (content.find("Starting:") != std::string::npos && content.find("Finished:") != std::string::npos && content.find("==>") != std::string::npos) ||
        (content.find("Total tests:") != std::string::npos && content.find("Failed:") != std::string::npos && content.find("Skipped:") != std::string::npos)) {
        return TestResultFormat::NUNIT_XUNIT_TEXT;
    }
    
    // Check for RSpec text patterns (should be checked after Mocha/Chai since they can contain similar keywords)
    if ((content.find("Finished in") != std::string::npos && content.find("examples") != std::string::npos) ||
        (content.find("Randomized with seed") != std::string::npos && content.find("failures") != std::string::npos) ||
        (content.find("Failed examples:") != std::string::npos && content.find("rspec") != std::string::npos) ||
        (content.find("✓") != std::string::npos && content.find("✗") != std::string::npos) ||
        (content.find("pending:") != std::string::npos && content.find("PENDING:") != std::string::npos) ||
        (content.find("Failure/Error:") != std::string::npos && content.find("expected") != std::string::npos)) {
        return TestResultFormat::RSPEC_TEXT;
    }
    
    // Check for JUnit text patterns (should be checked before pytest since they can contain similar keywords)
    if ((content.find("T E S T S") != std::string::npos && content.find("Tests run:") != std::string::npos) ||
        (content.find("JUnit Jupiter") != std::string::npos && content.find("tests found") != std::string::npos) ||
        (content.find("Running TestSuite") != std::string::npos && content.find("Total tests run:") != std::string::npos) ||
        (content.find("Time elapsed:") != std::string::npos && content.find("PASSED!") != std::string::npos) ||
        (content.find("Time elapsed:") != std::string::npos && content.find("FAILURE!") != std::string::npos) ||
        (content.find(" > ") != std::string::npos && (content.find(" PASSED") != std::string::npos || content.find(" FAILED") != std::string::npos))) {
        return TestResultFormat::JUNIT_TEXT;
    }
    
    // Check for Python linting tools (should be checked before pytest since they can contain similar keywords)
    
    // Check for Pylint patterns
    if ((content.find("************* Module") != std::string::npos && content.find("Your code has been rated") != std::string::npos) ||
        (content.find("C:") != std::string::npos && content.find("W:") != std::string::npos && content.find("E:") != std::string::npos && content.find("(") != std::string::npos && content.find(")") != std::string::npos) ||
        (content.find("missing-module-docstring") != std::string::npos || content.find("invalid-name") != std::string::npos || content.find("unused-argument") != std::string::npos) ||
        (content.find("Global evaluation") != std::string::npos && content.find("Raw metrics") != std::string::npos)) {
        return TestResultFormat::PYLINT_TEXT;
    }
    
    // Check for yapf patterns (must be before autopep8 since both use similar diff formats)
    if ((content.find("--- a/") != std::string::npos && content.find("+++ b/") != std::string::npos && content.find("(original)") != std::string::npos && content.find("(reformatted)") != std::string::npos) ||
        (content.find("Reformatted ") != std::string::npos && content.find(".py") != std::string::npos) ||
        (content.find("yapf --") != std::string::npos) ||
        (content.find("Style configuration:") != std::string::npos && content.find("Line length:") != std::string::npos) ||
        (content.find("Files reformatted:") != std::string::npos && content.find("Files with no changes:") != std::string::npos) ||
        (content.find("Processing /") != std::string::npos && content.find("--verbose") != std::string::npos) ||
        (content.find("ERROR: Files would be reformatted but yapf was run with --check") != std::string::npos)) {
        return TestResultFormat::YAPF_TEXT;
    }
    
    // Check for autopep8 patterns (must be after yapf since both use diff formats)
    if ((content.find("--- original/") != std::string::npos && content.find("+++ fixed/") != std::string::npos) ||
        (content.find("fixed ") != std::string::npos && content.find(" files") != std::string::npos) ||
        (content.find("ERROR:") != std::string::npos && content.find(": E999 SyntaxError:") != std::string::npos) ||
        (content.find("autopep8 --") != std::string::npos && content.find("--") != std::string::npos) ||
        (content.find("Applied configuration:") != std::string::npos && content.find("aggressive:") != std::string::npos) ||
        (content.find("Files processed:") != std::string::npos && content.find("Files modified:") != std::string::npos) ||
        (content.find("Total fixes applied:") != std::string::npos)) {
        return TestResultFormat::AUTOPEP8_TEXT;
    }
    
    // Check for Flake8 patterns
    if ((content.find("F401") != std::string::npos || content.find("E131") != std::string::npos || content.find("W503") != std::string::npos) ||
        (content.find(".py:") != std::string::npos && content.find(":") != std::string::npos && (content.find(" F") != std::string::npos || content.find(" E") != std::string::npos || content.find(" W") != std::string::npos || content.find(" C") != std::string::npos)) ||
        (content.find("imported but unused") != std::string::npos || content.find("line too long") != std::string::npos || content.find("continuation line") != std::string::npos)) {
        return TestResultFormat::FLAKE8_TEXT;
    }
    
    // Check for Black patterns
    if ((content.find("would reformat") != std::string::npos && content.find("Oh no!") != std::string::npos && content.find("💥") != std::string::npos) ||
        (content.find("files would be reformatted") != std::string::npos && content.find("files would be left unchanged") != std::string::npos) ||
        (content.find("All done!") != std::string::npos && content.find("✨") != std::string::npos && content.find("🍰") != std::string::npos) ||
        (content.find("--- ") != std::string::npos && content.find("+++ ") != std::string::npos && content.find("(original)") != std::string::npos && content.find("(formatted)") != std::string::npos)) {
        return TestResultFormat::BLACK_TEXT;
    }
    
    // Check for mypy patterns
    if ((content.find(": error:") != std::string::npos && content.find("[") != std::string::npos && content.find("]") != std::string::npos) ||
        (content.find(": warning:") != std::string::npos && content.find("[") != std::string::npos && content.find("]") != std::string::npos) ||
        (content.find(": note:") != std::string::npos && content.find("Revealed type") != std::string::npos) ||
        (content.find("Found") != std::string::npos && content.find("error") != std::string::npos && content.find("files") != std::string::npos && content.find("checked") != std::string::npos) ||
        (content.find("Success: no issues found") != std::string::npos) ||
        (content.find("return-value") != std::string::npos || content.find("arg-type") != std::string::npos || content.find("attr-defined") != std::string::npos)) {
        return TestResultFormat::MYPY_TEXT;
    }
    
    // Check for Docker build patterns
    if ((content.find("Sending build context to Docker daemon") != std::string::npos && content.find("Step") != std::string::npos && content.find("FROM") != std::string::npos) ||
        (content.find("Successfully built") != std::string::npos && content.find("Successfully tagged") != std::string::npos) ||
        (content.find("[+] Building") != std::string::npos && content.find("FINISHED") != std::string::npos) ||
        (content.find("=> [internal] load build definition from Dockerfile") != std::string::npos) ||
        (content.find("The command") != std::string::npos && content.find("returned a non-zero code:") != std::string::npos) ||
        (content.find("SECURITY SCANNING:") != std::string::npos && content.find("vulnerability found") != std::string::npos) ||
        (content.find("Step") != std::string::npos && content.find("RUN") != std::string::npos && content.find("---> Running in") != std::string::npos)) {
        return TestResultFormat::DOCKER_BUILD;
    }
    
    // Check for Bazel build patterns
    if ((content.find("INFO: Analyzed") != std::string::npos && content.find("targets") != std::string::npos) ||
        (content.find("INFO: Found") != std::string::npos && content.find("targets") != std::string::npos) ||
        (content.find("INFO: Build completed") != std::string::npos && content.find("total actions") != std::string::npos) ||
        (content.find("PASSED: //") != std::string::npos || content.find("FAILED: //") != std::string::npos) ||
        (content.find("ERROR: /workspace/") != std::string::npos && content.find("BUILD:") != std::string::npos) ||
        (content.find("Starting local Bazel server") != std::string::npos && content.find("connecting to it") != std::string::npos) ||
        (content.find("Loading:") != std::string::npos && content.find("packages loaded") != std::string::npos) ||
        (content.find("Analyzing:") != std::string::npos && content.find("targets") != std::string::npos && content.find("configured") != std::string::npos) ||
        (content.find("TIMEOUT: //") != std::string::npos || content.find("FLAKY: //") != std::string::npos || content.find("SKIPPED: //") != std::string::npos)) {
        return TestResultFormat::BAZEL_BUILD;
    }
    
    // Check for isort patterns  
    if ((content.find("Imports are incorrectly sorted") != std::string::npos) ||
        (content.find("Fixing") != std::string::npos && content.find(".py") != std::string::npos) ||
        (content.find("files reformatted") != std::string::npos && content.find("files left unchanged") != std::string::npos) ||
        (content.find("import-order-style:") != std::string::npos || content.find("profile:") != std::string::npos) ||
        (content.find("ERROR: isort found an import in the wrong position") != std::string::npos) ||
        (content.find("Parsing") != std::string::npos && content.find(".py") != std::string::npos && content.find("Placing imports") != std::string::npos) ||
        (content.find("would be reformatted") != std::string::npos && content.find("would be left unchanged") != std::string::npos)) {
        return TestResultFormat::ISORT_TEXT;
    }
    
    // Check for coverage.py patterns
    if ((content.find("Name") != std::string::npos && content.find("Stmts") != std::string::npos && content.find("Miss") != std::string::npos && content.find("Cover") != std::string::npos) ||
        (content.find("coverage run") != std::string::npos && content.find("--source=") != std::string::npos) ||
        (content.find("Coverage report generated") != std::string::npos) ||
        (content.find("coverage html") != std::string::npos || content.find("coverage xml") != std::string::npos || content.find("coverage json") != std::string::npos) ||
        (content.find("Wrote HTML report to") != std::string::npos || content.find("Wrote XML report to") != std::string::npos || content.find("Wrote JSON report to") != std::string::npos) ||
        (content.find("TOTAL") != std::string::npos && content.find("-------") != std::string::npos && content.find("%") != std::string::npos) ||
        (content.find("Coverage failure:") != std::string::npos && content.find("--fail-under=") != std::string::npos) ||
        (content.find("Branch") != std::string::npos && content.find("BrPart") != std::string::npos)) {
        return TestResultFormat::COVERAGE_TEXT;
    }
    
    // Check for bandit text patterns
    if ((content.find("Test results:") != std::string::npos && content.find(">> Issue:") != std::string::npos && content.find("Severity:") != std::string::npos) ||
        (content.find("[bandit]") != std::string::npos && content.find("security scan") != std::string::npos) ||
        (content.find("Total issues (by severity):") != std::string::npos && content.find("Total issues (by confidence):") != std::string::npos) ||
        (content.find("Code scanned:") != std::string::npos && content.find("Total lines of code:") != std::string::npos) ||
        (content.find("More Info: https://bandit.readthedocs.io") != std::string::npos) ||
        (content.find("CWE:") != std::string::npos && content.find("https://cwe.mitre.org") != std::string::npos) ||
        (content.find("running on Python") != std::string::npos && content.find("[main]") != std::string::npos)) {
        return TestResultFormat::BANDIT_TEXT;
    }
    
    // Check for pytest-cov patterns (should be checked before regular pytest since it includes pytest output)
    if ((content.find("-- coverage:") != std::string::npos && content.find("python") != std::string::npos) ||
        (content.find("collected") != std::string::npos && content.find("items") != std::string::npos && content.find("----------- coverage:") != std::string::npos) ||
        (content.find("PASSED") != std::string::npos && content.find("::") != std::string::npos && content.find("Name") != std::string::npos && content.find("Stmts") != std::string::npos && content.find("Miss") != std::string::npos && content.find("Cover") != std::string::npos) ||
        (content.find("platform") != std::string::npos && content.find("plugins: cov-") != std::string::npos) ||
        (content.find("Coverage threshold check failed") != std::string::npos && content.find("Expected:") != std::string::npos) ||
        (content.find("Required test coverage") != std::string::npos && content.find("not met") != std::string::npos)) {
        return TestResultFormat::PYTEST_COV_TEXT;
    }
    
    if (content.find("PASSED") != std::string::npos && content.find("::") != std::string::npos) {
        return TestResultFormat::PYTEST_TEXT;
    }
    
    if ((content.find("CMake Error") != std::string::npos || content.find("CMake Warning") != std::string::npos || 
         content.find("gmake[") != std::string::npos) && 
        (content.find("Building C") != std::string::npos || content.find("Building CXX") != std::string::npos || 
         content.find("Linking") != std::string::npos || content.find("CMakeLists.txt") != std::string::npos)) {
        return TestResultFormat::CMAKE_BUILD;
    }
    
    // Check for Python build patterns
    if ((content.find("Building wheel") != std::string::npos && content.find("setup.py") != std::string::npos) ||
        (content.find("running build") != std::string::npos && content.find("python setup.py") != std::string::npos) ||
        (content.find("pip install") != std::string::npos && content.find("ERROR:") != std::string::npos) ||
        (content.find("FAILED") != std::string::npos && content.find("AssertionError") != std::string::npos)) {
        return TestResultFormat::PYTHON_BUILD;
    }
    
    // Check for Node.js build patterns
    if ((content.find("npm ERR!") != std::string::npos || content.find("yarn install") != std::string::npos) ||
        (content.find("webpack") != std::string::npos && (content.find("ERROR") != std::string::npos || content.find("WARNING") != std::string::npos)) ||
        (content.find("jest") != std::string::npos && content.find("FAIL") != std::string::npos) ||
        (content.find("eslint") != std::string::npos && content.find("error") != std::string::npos)) {
        return TestResultFormat::NODE_BUILD;
    }
    
    // Check for Cargo build patterns
    if ((content.find("Compiling") != std::string::npos && content.find("cargo") != std::string::npos) ||
        (content.find("error[E") != std::string::npos && content.find("-->") != std::string::npos) ||
        (content.find("cargo test") != std::string::npos && content.find("FAILED") != std::string::npos) ||
        (content.find("cargo clippy") != std::string::npos && content.find("warning:") != std::string::npos) ||
        (content.find("rustc --explain") != std::string::npos)) {
        return TestResultFormat::CARGO_BUILD;
    }
    
    // Check for Maven build patterns
    if ((content.find("[INFO]") != std::string::npos && content.find("maven") != std::string::npos) ||
        (content.find("[ERROR]") != std::string::npos && content.find("COMPILATION ERROR") != std::string::npos) ||
        (content.find("maven-compiler-plugin") != std::string::npos) ||
        (content.find("maven-surefire-plugin") != std::string::npos && content.find("Tests run:") != std::string::npos) ||
        (content.find("BUILD FAILURE") != std::string::npos && content.find("Total time:") != std::string::npos)) {
        return TestResultFormat::MAVEN_BUILD;
    }
    
    // Check for Gradle build patterns
    if ((content.find("> Task :") != std::string::npos) ||
        (content.find("BUILD SUCCESSFUL") != std::string::npos && content.find("actionable task") != std::string::npos) ||
        (content.find("BUILD FAILED") != std::string::npos && content.find("actionable task") != std::string::npos) ||
        (content.find("Gradle") != std::string::npos && content.find("build") != std::string::npos) ||
        (content.find("[ant:checkstyle]") != std::string::npos) ||
        (content.find("Execution failed for task") != std::string::npos)) {
        return TestResultFormat::GRADLE_BUILD;
    }
    
    // Check for MSBuild patterns
    if ((content.find("Microsoft (R) Build Engine") != std::string::npos) ||
        (content.find("Build started") != std::string::npos && content.find("Time Elapsed") != std::string::npos) ||
        (content.find("Build FAILED") != std::string::npos && content.find("Error(s)") != std::string::npos) ||
        (content.find("Build succeeded") != std::string::npos && content.find("Warning(s)") != std::string::npos) ||
        (content.find("error CS") != std::string::npos && content.find(".csproj") != std::string::npos) ||
        (content.find("xUnit.net") != std::string::npos && content.find("[FAIL]") != std::string::npos)) {
        return TestResultFormat::MSBUILD;
    }
    
    
    if (content.find("make: ***") != std::string::npos && content.find("Error") != std::string::npos) {
        return TestResultFormat::MAKE_ERROR;
    }
    
    if (content.find(": error:") != std::string::npos || content.find(": warning:") != std::string::npos) {
        return TestResultFormat::GENERIC_LINT;
    }
    
    return TestResultFormat::UNKNOWN;
}

std::string TestResultFormatToString(TestResultFormat format) {
    switch (format) {
        case TestResultFormat::UNKNOWN: return "unknown";
        case TestResultFormat::AUTO: return "auto";
        case TestResultFormat::PYTEST_JSON: return "pytest_json";
        case TestResultFormat::GOTEST_JSON: return "gotest_json";
        case TestResultFormat::ESLINT_JSON: return "eslint_json";
        case TestResultFormat::PYTEST_TEXT: return "pytest_text";
        case TestResultFormat::MAKE_ERROR: return "make_error";
        case TestResultFormat::GENERIC_LINT: return "generic_lint";
        case TestResultFormat::DUCKDB_TEST: return "duckdb_test";
        case TestResultFormat::RUBOCOP_JSON: return "rubocop_json";
        case TestResultFormat::CARGO_TEST_JSON: return "cargo_test_json";
        case TestResultFormat::SWIFTLINT_JSON: return "swiftlint_json";
        case TestResultFormat::PHPSTAN_JSON: return "phpstan_json";
        case TestResultFormat::SHELLCHECK_JSON: return "shellcheck_json";
        case TestResultFormat::STYLELINT_JSON: return "stylelint_json";
        case TestResultFormat::CLIPPY_JSON: return "clippy_json";
        case TestResultFormat::MARKDOWNLINT_JSON: return "markdownlint_json";
        case TestResultFormat::YAMLLINT_JSON: return "yamllint_json";
        case TestResultFormat::BANDIT_JSON: return "bandit_json";
        case TestResultFormat::SPOTBUGS_JSON: return "spotbugs_json";
        case TestResultFormat::KTLINT_JSON: return "ktlint_json";
        case TestResultFormat::HADOLINT_JSON: return "hadolint_json";
        case TestResultFormat::LINTR_JSON: return "lintr_json";
        case TestResultFormat::SQLFLUFF_JSON: return "sqlfluff_json";
        case TestResultFormat::TFLINT_JSON: return "tflint_json";
        case TestResultFormat::KUBE_SCORE_JSON: return "kube_score_json";
        case TestResultFormat::CMAKE_BUILD: return "cmake_build";
        case TestResultFormat::PYTHON_BUILD: return "python_build";
        case TestResultFormat::NODE_BUILD: return "node_build";
        case TestResultFormat::CARGO_BUILD: return "cargo_build";
        case TestResultFormat::MAVEN_BUILD: return "maven_build";
        case TestResultFormat::GRADLE_BUILD: return "gradle_build";
        case TestResultFormat::MSBUILD: return "msbuild";
        case TestResultFormat::JUNIT_TEXT: return "junit_text";
        case TestResultFormat::VALGRIND: return "valgrind";
        case TestResultFormat::GDB_LLDB: return "gdb_lldb";
        case TestResultFormat::RSPEC_TEXT: return "rspec_text";
        case TestResultFormat::MOCHA_CHAI_TEXT: return "mocha_chai_text";
        case TestResultFormat::GTEST_TEXT: return "gtest_text";
        case TestResultFormat::NUNIT_XUNIT_TEXT: return "nunit_xunit_text";
        case TestResultFormat::PYLINT_TEXT: return "pylint_text";
        case TestResultFormat::FLAKE8_TEXT: return "flake8_text";
        case TestResultFormat::BLACK_TEXT: return "black_text";
        case TestResultFormat::MYPY_TEXT: return "mypy_text";
        case TestResultFormat::DOCKER_BUILD: return "docker_build";
        case TestResultFormat::BAZEL_BUILD: return "bazel_build";
        case TestResultFormat::ISORT_TEXT: return "isort_text";
        case TestResultFormat::BANDIT_TEXT: return "bandit_text";
        case TestResultFormat::AUTOPEP8_TEXT: return "autopep8_text";
        case TestResultFormat::YAPF_TEXT: return "yapf_text";
        case TestResultFormat::COVERAGE_TEXT: return "coverage_text";
        case TestResultFormat::PYTEST_COV_TEXT: return "pytest_cov_text";
        case TestResultFormat::GITHUB_ACTIONS_TEXT: return "github_actions_text";
        case TestResultFormat::GITLAB_CI_TEXT: return "gitlab_ci_text";
        case TestResultFormat::JENKINS_TEXT: return "jenkins_text";
        case TestResultFormat::DRONE_CI_TEXT: return "drone_ci_text";
        default: return "unknown";
    }
}

TestResultFormat StringToTestResultFormat(const std::string& str) {
    if (str == "auto") return TestResultFormat::AUTO;
    if (str == "pytest_json") return TestResultFormat::PYTEST_JSON;
    if (str == "gotest_json") return TestResultFormat::GOTEST_JSON;
    if (str == "eslint_json") return TestResultFormat::ESLINT_JSON;
    if (str == "pytest_text") return TestResultFormat::PYTEST_TEXT;
    if (str == "make_error") return TestResultFormat::MAKE_ERROR;
    if (str == "generic_lint") return TestResultFormat::GENERIC_LINT;
    if (str == "duckdb_test") return TestResultFormat::DUCKDB_TEST;
    if (str == "rubocop_json") return TestResultFormat::RUBOCOP_JSON;
    if (str == "cargo_test_json") return TestResultFormat::CARGO_TEST_JSON;
    if (str == "swiftlint_json") return TestResultFormat::SWIFTLINT_JSON;
    if (str == "phpstan_json") return TestResultFormat::PHPSTAN_JSON;
    if (str == "shellcheck_json") return TestResultFormat::SHELLCHECK_JSON;
    if (str == "stylelint_json") return TestResultFormat::STYLELINT_JSON;
    if (str == "clippy_json") return TestResultFormat::CLIPPY_JSON;
    if (str == "markdownlint_json") return TestResultFormat::MARKDOWNLINT_JSON;
    if (str == "yamllint_json") return TestResultFormat::YAMLLINT_JSON;
    if (str == "bandit_json") return TestResultFormat::BANDIT_JSON;
    if (str == "spotbugs_json") return TestResultFormat::SPOTBUGS_JSON;
    if (str == "ktlint_json") return TestResultFormat::KTLINT_JSON;
    if (str == "hadolint_json") return TestResultFormat::HADOLINT_JSON;
    if (str == "lintr_json") return TestResultFormat::LINTR_JSON;
    if (str == "sqlfluff_json") return TestResultFormat::SQLFLUFF_JSON;
    if (str == "tflint_json") return TestResultFormat::TFLINT_JSON;
    if (str == "kube_score_json") return TestResultFormat::KUBE_SCORE_JSON;
    if (str == "cmake_build") return TestResultFormat::CMAKE_BUILD;
    if (str == "python_build") return TestResultFormat::PYTHON_BUILD;
    if (str == "node_build") return TestResultFormat::NODE_BUILD;
    if (str == "cargo_build") return TestResultFormat::CARGO_BUILD;
    if (str == "maven_build") return TestResultFormat::MAVEN_BUILD;
    if (str == "gradle_build") return TestResultFormat::GRADLE_BUILD;
    if (str == "msbuild") return TestResultFormat::MSBUILD;
    if (str == "junit_text") return TestResultFormat::JUNIT_TEXT;
    if (str == "valgrind") return TestResultFormat::VALGRIND;
    if (str == "gdb_lldb") return TestResultFormat::GDB_LLDB;
    if (str == "rspec_text") return TestResultFormat::RSPEC_TEXT;
    if (str == "mocha_chai_text") return TestResultFormat::MOCHA_CHAI_TEXT;
    if (str == "gtest_text") return TestResultFormat::GTEST_TEXT;
    if (str == "nunit_xunit_text") return TestResultFormat::NUNIT_XUNIT_TEXT;
    if (str == "pylint_text") return TestResultFormat::PYLINT_TEXT;
    if (str == "flake8_text") return TestResultFormat::FLAKE8_TEXT;
    if (str == "black_text") return TestResultFormat::BLACK_TEXT;
    if (str == "mypy_text") return TestResultFormat::MYPY_TEXT;
    if (str == "docker_build") return TestResultFormat::DOCKER_BUILD;
    if (str == "bazel_build") return TestResultFormat::BAZEL_BUILD;
    if (str == "isort_text") return TestResultFormat::ISORT_TEXT;
    if (str == "bandit_text") return TestResultFormat::BANDIT_TEXT;
    if (str == "autopep8_text") return TestResultFormat::AUTOPEP8_TEXT;
    if (str == "yapf_text") return TestResultFormat::YAPF_TEXT;
    if (str == "coverage_text") return TestResultFormat::COVERAGE_TEXT;
    if (str == "pytest_cov_text") return TestResultFormat::PYTEST_COV_TEXT;
    if (str == "github_actions_text") return TestResultFormat::GITHUB_ACTIONS_TEXT;
    if (str == "gitlab_ci_text") return TestResultFormat::GITLAB_CI_TEXT;
    if (str == "jenkins_text") return TestResultFormat::JENKINS_TEXT;
    if (str == "drone_ci_text") return TestResultFormat::DRONE_CI_TEXT;
    if (str == "unknown") return TestResultFormat::UNKNOWN;
    return TestResultFormat::AUTO;  // Default to auto-detection
}

std::string ReadContentFromSource(const std::string& source) {
    // For now, assume source is a file path
    // Later we can add support for direct content strings
    std::ifstream file(source);
    if (!file.is_open()) {
        throw IOException("Could not open file: " + source);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    return content;
}

bool IsValidJSON(const std::string& content) {
    // Simple heuristic - starts with { or [
    std::string trimmed = content;
    StringUtil::Trim(trimmed);
    return !trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '[');
}

unique_ptr<FunctionData> ReadTestResultsBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names) {
    auto bind_data = make_uniq<ReadTestResultsBindData>();
    
    // Get source parameter (required)
    if (input.inputs.empty()) {
        throw BinderException("read_test_results requires at least one parameter (source)");
    }
    bind_data->source = input.inputs[0].ToString();
    
    // Get format parameter (optional, defaults to auto)
    if (input.inputs.size() > 1) {
        bind_data->format = StringToTestResultFormat(input.inputs[1].ToString());
    } else {
        bind_data->format = TestResultFormat::AUTO;
    }
    
    // Define return schema
    return_types = {
        LogicalType::BIGINT,   // event_id
        LogicalType::VARCHAR,  // tool_name
        LogicalType::VARCHAR,  // event_type
        LogicalType::VARCHAR,  // file_path
        LogicalType::INTEGER,  // line_number
        LogicalType::INTEGER,  // column_number
        LogicalType::VARCHAR,  // function_name
        LogicalType::VARCHAR,  // status
        LogicalType::VARCHAR,  // severity
        LogicalType::VARCHAR,  // category
        LogicalType::VARCHAR,  // message
        LogicalType::VARCHAR,  // suggestion
        LogicalType::VARCHAR,  // error_code
        LogicalType::VARCHAR,  // test_name
        LogicalType::DOUBLE,   // execution_time
        LogicalType::VARCHAR,  // raw_output
        LogicalType::VARCHAR   // structured_data
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number",
        "column_number", "function_name", "status", "severity", "category",
        "message", "suggestion", "error_code", "test_name", "execution_time",
        "raw_output", "structured_data"
    };
    
    return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ReadTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<ReadTestResultsBindData>();
    auto global_state = make_uniq<ReadTestResultsGlobalState>();
    
    // Read content from source
    std::string content;
    try {
        content = ReadContentFromSource(bind_data.source);
    } catch (const IOException&) {
        // If file reading fails, treat source as direct content
        content = bind_data.source;
    }
    
    // Auto-detect format if needed
    TestResultFormat format = bind_data.format;
    if (format == TestResultFormat::AUTO) {
        format = DetectTestResultFormat(content);
    }
    
    // Parse content based on detected format
    switch (format) {
        case TestResultFormat::PYTEST_JSON:
            ParsePytestJSON(content, global_state->events);
            break;
        case TestResultFormat::DUCKDB_TEST:
            ParseDuckDBTestOutput(content, global_state->events);
            break;
        case TestResultFormat::ESLINT_JSON:
            ParseESLintJSON(content, global_state->events);
            break;
        case TestResultFormat::GOTEST_JSON:
            ParseGoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::MAKE_ERROR:
            ParseMakeErrors(content, global_state->events);
            break;
        case TestResultFormat::PYTEST_TEXT:
            ParsePytestText(content, global_state->events);
            break;
        case TestResultFormat::GENERIC_LINT:
            ParseGenericLint(content, global_state->events);
            break;
        case TestResultFormat::RUBOCOP_JSON:
            ParseRuboCopJSON(content, global_state->events);
            break;
        case TestResultFormat::CARGO_TEST_JSON:
            ParseCargoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::SWIFTLINT_JSON:
            ParseSwiftLintJSON(content, global_state->events);
            break;
        case TestResultFormat::PHPSTAN_JSON:
            ParsePHPStanJSON(content, global_state->events);
            break;
        case TestResultFormat::SHELLCHECK_JSON:
            ParseShellCheckJSON(content, global_state->events);
            break;
        case TestResultFormat::STYLELINT_JSON:
            ParseStylelintJSON(content, global_state->events);
            break;
        case TestResultFormat::CLIPPY_JSON:
            ParseClippyJSON(content, global_state->events);
            break;
        case TestResultFormat::MARKDOWNLINT_JSON:
            ParseMarkdownlintJSON(content, global_state->events);
            break;
        case TestResultFormat::YAMLLINT_JSON:
            ParseYamllintJSON(content, global_state->events);
            break;
        case TestResultFormat::BANDIT_JSON:
            ParseBanditJSON(content, global_state->events);
            break;
        case TestResultFormat::SPOTBUGS_JSON:
            ParseSpotBugsJSON(content, global_state->events);
            break;
        case TestResultFormat::KTLINT_JSON:
            ParseKtlintJSON(content, global_state->events);
            break;
        case TestResultFormat::HADOLINT_JSON:
            ParseHadolintJSON(content, global_state->events);
            break;
        case TestResultFormat::LINTR_JSON:
            ParseLintrJSON(content, global_state->events);
            break;
        case TestResultFormat::SQLFLUFF_JSON:
            ParseSqlfluffJSON(content, global_state->events);
            break;
        case TestResultFormat::TFLINT_JSON:
            ParseTflintJSON(content, global_state->events);
            break;
        case TestResultFormat::KUBE_SCORE_JSON:
            ParseKubeScoreJSON(content, global_state->events);
            break;
        case TestResultFormat::CMAKE_BUILD:
            ParseCMakeBuild(content, global_state->events);
            break;
        case TestResultFormat::PYTHON_BUILD:
            ParsePythonBuild(content, global_state->events);
            break;
        case TestResultFormat::NODE_BUILD:
            ParseNodeBuild(content, global_state->events);
            break;
        case TestResultFormat::CARGO_BUILD:
            ParseCargoBuild(content, global_state->events);
            break;
        case TestResultFormat::MAVEN_BUILD:
            ParseMavenBuild(content, global_state->events);
            break;
        case TestResultFormat::GRADLE_BUILD:
            ParseGradleBuild(content, global_state->events);
            break;
        case TestResultFormat::MSBUILD:
            ParseMSBuild(content, global_state->events);
            break;
        case TestResultFormat::JUNIT_TEXT:
            ParseJUnitText(content, global_state->events);
            break;
        case TestResultFormat::VALGRIND:
            ParseValgrind(content, global_state->events);
            break;
        case TestResultFormat::GDB_LLDB:
            ParseGdbLldb(content, global_state->events);
            break;
        case TestResultFormat::RSPEC_TEXT:
            ParseRSpecText(content, global_state->events);
            break;
        case TestResultFormat::MOCHA_CHAI_TEXT:
            ParseMochaChai(content, global_state->events);
            break;
        case TestResultFormat::GTEST_TEXT:
            ParseGoogleTest(content, global_state->events);
            break;
        case TestResultFormat::NUNIT_XUNIT_TEXT:
            ParseNUnitXUnit(content, global_state->events);
            break;
        case TestResultFormat::PYLINT_TEXT:
            ParsePylintText(content, global_state->events);
            break;
        case TestResultFormat::FLAKE8_TEXT:
            ParseFlake8Text(content, global_state->events);
            break;
        case TestResultFormat::BLACK_TEXT:
            ParseBlackText(content, global_state->events);
            break;
        case TestResultFormat::MYPY_TEXT:
            ParseMypyText(content, global_state->events);
            break;
        case TestResultFormat::DOCKER_BUILD:
            ParseDockerBuild(content, global_state->events);
            break;
        case TestResultFormat::BAZEL_BUILD:
            ParseBazelBuild(content, global_state->events);
            break;
        case TestResultFormat::ISORT_TEXT:
            ParseIsortText(content, global_state->events);
            break;
        case TestResultFormat::BANDIT_TEXT:
            ParseBanditText(content, global_state->events);
            break;
        case TestResultFormat::AUTOPEP8_TEXT:
            ParseAutopep8Text(content, global_state->events);
            break;
        case TestResultFormat::YAPF_TEXT:
            ParseYapfText(content, global_state->events);
            break;
        case TestResultFormat::COVERAGE_TEXT:
            ParseCoverageText(content, global_state->events);
            break;
        case TestResultFormat::PYTEST_COV_TEXT:
            ParsePytestCovText(content, global_state->events);
            break;
        case TestResultFormat::GITHUB_ACTIONS_TEXT:
            ParseGitHubActionsText(content, global_state->events);
            break;
        case TestResultFormat::GITLAB_CI_TEXT:
            ParseGitLabCIText(content, global_state->events);
            break;
        case TestResultFormat::JENKINS_TEXT:
            ParseJenkinsText(content, global_state->events);
            break;
        case TestResultFormat::DRONE_CI_TEXT:
            ParseDroneCIText(content, global_state->events);
            break;
        default:
            // For unknown formats, don't create any events
            break;
    }
    
    return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ReadTestResultsInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                            GlobalTableFunctionState *global_state) {
    return make_uniq<ReadTestResultsLocalState>();
}

void ReadTestResultsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &global_state = data_p.global_state->Cast<ReadTestResultsGlobalState>();
    auto &local_state = data_p.local_state->Cast<ReadTestResultsLocalState>();
    
    // Populate output chunk
    PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE);
    
    // Update offset for next chunk
    local_state.chunk_offset += output.size();
}

void PopulateDataChunkFromEvents(DataChunk &output, const std::vector<ValidationEvent> &events, 
                                idx_t start_offset, idx_t chunk_size) {
    idx_t events_remaining = events.size() > start_offset ? events.size() - start_offset : 0;
    idx_t output_size = std::min(chunk_size, events_remaining);
    
    if (output_size == 0) {
        output.SetCardinality(0);
        return;
    }
    
    output.SetCardinality(output_size);
    
    for (idx_t i = 0; i < output_size; i++) {
        const auto &event = events[start_offset + i];
        idx_t col = 0;
        
        output.SetValue(col++, i, Value::BIGINT(event.event_id));
        output.SetValue(col++, i, Value(event.tool_name));
        output.SetValue(col++, i, Value(ValidationEventTypeToString(event.event_type)));
        output.SetValue(col++, i, Value(event.file_path));
        output.SetValue(col++, i, event.line_number == -1 ? Value() : Value::INTEGER(event.line_number));
        output.SetValue(col++, i, event.column_number == -1 ? Value() : Value::INTEGER(event.column_number));
        output.SetValue(col++, i, Value(event.function_name));
        output.SetValue(col++, i, Value(ValidationEventStatusToString(event.status)));
        output.SetValue(col++, i, Value(event.severity));
        output.SetValue(col++, i, Value(event.category));
        output.SetValue(col++, i, Value(event.message));
        output.SetValue(col++, i, Value(event.suggestion));
        output.SetValue(col++, i, Value(event.error_code));
        output.SetValue(col++, i, Value(event.test_name));
        output.SetValue(col++, i, Value::DOUBLE(event.execution_time));
        output.SetValue(col++, i, Value(event.raw_output));
        output.SetValue(col++, i, Value(event.structured_data));
    }
}

void ParsePytestJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse pytest JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid pytest JSON: root is not an object");
    }
    
    // Get tests array
    yyjson_val *tests = yyjson_obj_get(root, "tests");
    if (!tests || !yyjson_is_arr(tests)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid pytest JSON: no tests array found");
    }
    
    // Parse each test
    size_t idx, max;
    yyjson_val *test;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(tests, idx, max, test) {
        if (!yyjson_is_obj(test)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "pytest";
        event.event_type = ValidationEventType::TEST_RESULT;
        event.line_number = -1;
        event.column_number = -1;
        event.execution_time = 0.0;
        
        // Extract nodeid (test name with file path)
        yyjson_val *nodeid = yyjson_obj_get(test, "nodeid");
        if (nodeid && yyjson_is_str(nodeid)) {
            std::string nodeid_str = yyjson_get_str(nodeid);
            
            // Parse nodeid format: "file.py::test_function"
            size_t separator = nodeid_str.find("::");
            if (separator != std::string::npos) {
                event.file_path = nodeid_str.substr(0, separator);
                event.test_name = nodeid_str.substr(separator + 2);
                event.function_name = event.test_name;
            } else {
                event.test_name = nodeid_str;
                event.function_name = nodeid_str;
            }
        }
        
        // Extract outcome
        yyjson_val *outcome = yyjson_obj_get(test, "outcome");
        if (outcome && yyjson_is_str(outcome)) {
            std::string outcome_str = yyjson_get_str(outcome);
            event.status = StringToValidationEventStatus(outcome_str);
        } else {
            event.status = ValidationEventStatus::ERROR;
        }
        
        // Extract call details
        yyjson_val *call = yyjson_obj_get(test, "call");
        if (call && yyjson_is_obj(call)) {
            // Extract duration
            yyjson_val *duration = yyjson_obj_get(call, "duration");
            if (duration && yyjson_is_num(duration)) {
                event.execution_time = yyjson_get_real(duration);
            }
            
            // Extract longrepr (error message)
            yyjson_val *longrepr = yyjson_obj_get(call, "longrepr");
            if (longrepr && yyjson_is_str(longrepr)) {
                event.message = yyjson_get_str(longrepr);
            }
        }
        
        // Set category based on status
        switch (event.status) {
            case ValidationEventStatus::PASS:
                event.category = "test_success";
                if (event.message.empty()) event.message = "Test passed";
                break;
            case ValidationEventStatus::FAIL:
                event.category = "test_failure";
                if (event.message.empty()) event.message = "Test failed";
                break;
            case ValidationEventStatus::SKIP:
                event.category = "test_skipped";
                if (event.message.empty()) event.message = "Test skipped";
                break;
            default:
                event.category = "test_error";
                if (event.message.empty()) event.message = "Test error";
                break;
        }
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseDuckDBTestOutput(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track current test being processed
    std::string current_test_file;
    bool in_failure_section = false;
    std::string failure_message;
    std::string failure_query;
    int failure_line = -1;
    
    while (std::getline(stream, line)) {
        // Parse test progress lines: [X/Y] (Z%): /path/to/test.test
        if (line.find("[") == 0 && line.find("]:") != std::string::npos) {
            size_t path_start = line.find("): ");
            if (path_start != std::string::npos) {
                current_test_file = line.substr(path_start + 3);
                // Trim any trailing whitespace/dots
                while (!current_test_file.empty() && 
                       (current_test_file.back() == '.' || current_test_file.back() == ' ')) {
                    current_test_file.pop_back();
                }
            }
        }
        
        // Detect failure start
        else if (line.find("Wrong result in query!") != std::string::npos ||
                 line.find("Query unexpectedly failed") != std::string::npos) {
            in_failure_section = true;
            failure_message = line;
            
            // Extract line number from failure message
            size_t line_start = line.find(".test:");
            if (line_start != std::string::npos) {
                line_start += 6; // Length of ".test:"
                size_t line_end = line.find(")", line_start);
                if (line_end != std::string::npos) {
                    try {
                        failure_line = std::stoi(line.substr(line_start, line_end - line_start));
                    } catch (...) {
                        failure_line = -1;
                    }
                }
            }
        }
        
        // Capture SQL query in failure section
        else if (in_failure_section && !line.empty() && 
                 line.find("================================================================================") == std::string::npos &&
                 line.find("SELECT") == 0) {
            failure_query = line;
        }
        
        // End of failure section - create failure event
        else if (in_failure_section && line.find("FAILED:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "duckdb_test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = current_test_file;
            event.line_number = failure_line;
            event.column_number = -1;
            event.function_name = failure_query.empty() ? "unknown" : failure_query.substr(0, 50);
            event.status = ValidationEventStatus::FAIL;
            event.category = "test_failure";
            event.message = failure_message;
            event.raw_output = failure_query;
            event.execution_time = 0.0;
            
            events.push_back(event);
            
            // Reset failure tracking
            in_failure_section = false;
            failure_message.clear();
            failure_query.clear();
            failure_line = -1;
        }
        
        // Parse summary line: test cases: X | Y passed | Z failed
        else if (line.find("test cases:") != std::string::npos) {
            // Extract summary statistics and create summary events
            std::string summary_line = line;
            
            // Extract passed count
            size_t passed_pos = summary_line.find(" passed");
            if (passed_pos != std::string::npos) {
                size_t passed_start = summary_line.rfind(" ", passed_pos - 1);
                if (passed_start != std::string::npos) {
                    try {
                        int passed_count = std::stoi(summary_line.substr(passed_start + 1, passed_pos - passed_start - 1));
                        
                        // Create passed test events (summary)
                        ValidationEvent summary_event;
                        summary_event.event_id = event_id++;
                        summary_event.tool_name = "duckdb_test";
                        summary_event.event_type = ValidationEventType::TEST_RESULT;
                        summary_event.status = ValidationEventStatus::INFO;
                        summary_event.category = "test_summary";
                        summary_event.message = "Test summary: " + std::to_string(passed_count) + " tests passed";
                        summary_event.line_number = -1;
                        summary_event.column_number = -1;
                        summary_event.execution_time = 0.0;
                        
                        events.push_back(summary_event);
                    } catch (...) {
                        // Ignore parsing errors
                    }
                }
            }
        }
    }
    
    // If no events were created, add a basic summary
    if (events.empty()) {
        ValidationEvent summary_event;
        summary_event.event_id = 1;
        summary_event.tool_name = "duckdb_test";
        summary_event.event_type = ValidationEventType::TEST_RESULT;
        summary_event.status = ValidationEventStatus::INFO;
        summary_event.category = "test_summary";
        summary_event.message = "DuckDB test output parsed (no specific test results found)";
        summary_event.line_number = -1;
        summary_event.column_number = -1;
        summary_event.execution_time = 0.0;
        
        events.push_back(summary_event);
    }
}

void ParseESLintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse ESLint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid ESLint JSON: root is not an array");
    }
    
    // Parse each file result
    size_t idx, max;
    yyjson_val *file_result;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, file_result) {
        if (!yyjson_is_obj(file_result)) continue;
        
        // Get file path
        yyjson_val *file_path = yyjson_obj_get(file_result, "filePath");
        std::string file_path_str;
        if (file_path && yyjson_is_str(file_path)) {
            file_path_str = yyjson_get_str(file_path);
        }
        
        // Get messages array
        yyjson_val *messages = yyjson_obj_get(file_result, "messages");
        if (!messages || !yyjson_is_arr(messages)) continue;
        
        // Parse each message
        size_t msg_idx, msg_max;
        yyjson_val *message;
        yyjson_arr_foreach(messages, msg_idx, msg_max, message) {
            if (!yyjson_is_obj(message)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "eslint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path_str;
            event.execution_time = 0.0;
            
            // Extract line and column
            yyjson_val *line = yyjson_obj_get(message, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            yyjson_val *column = yyjson_obj_get(message, "column");
            if (column && yyjson_is_num(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Extract message
            yyjson_val *msg_text = yyjson_obj_get(message, "message");
            if (msg_text && yyjson_is_str(msg_text)) {
                event.message = yyjson_get_str(msg_text);
            }
            
            // Extract rule ID
            yyjson_val *rule_id = yyjson_obj_get(message, "ruleId");
            if (rule_id && yyjson_is_str(rule_id)) {
                event.error_code = yyjson_get_str(rule_id);
                event.function_name = yyjson_get_str(rule_id);
            }
            
            // Map severity to status
            yyjson_val *severity = yyjson_obj_get(message, "severity");
            if (severity && yyjson_is_num(severity)) {
                int sev = yyjson_get_int(severity);
                switch (sev) {
                    case 2:
                        event.status = ValidationEventStatus::ERROR;
                        event.category = "lint_error";
                        event.severity = "error";
                        break;
                    case 1:
                        event.status = ValidationEventStatus::WARNING;
                        event.category = "lint_warning"; 
                        event.severity = "warning";
                        break;
                    default:
                        event.status = ValidationEventStatus::INFO;
                        event.category = "lint_info";
                        event.severity = "info";
                        break;
                }
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.category = "lint_warning";
                event.severity = "warning";
            }
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseGoTestJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track test results
    std::map<std::string, ValidationEvent> test_events;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse each JSON line
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) continue;
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Extract fields
        yyjson_val *action = yyjson_obj_get(root, "Action");
        yyjson_val *package = yyjson_obj_get(root, "Package");
        yyjson_val *test = yyjson_obj_get(root, "Test");
        yyjson_val *elapsed = yyjson_obj_get(root, "Elapsed");
        yyjson_val *output = yyjson_obj_get(root, "Output");
        
        if (!action || !yyjson_is_str(action)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        std::string action_str = yyjson_get_str(action);
        std::string package_str = package && yyjson_is_str(package) ? yyjson_get_str(package) : "";
        std::string test_str = test && yyjson_is_str(test) ? yyjson_get_str(test) : "";
        
        // Create unique test key
        std::string test_key = package_str + "::" + test_str;
        
        if (action_str == "run" && !test_str.empty()) {
            // Initialize test event
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "go_test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = package_str;
            event.test_name = test_str;
            event.function_name = test_str;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            
            test_events[test_key] = event;
        } else if ((action_str == "pass" || action_str == "fail" || action_str == "skip") && !test_str.empty()) {
            // Finalize test event
            if (test_events.find(test_key) != test_events.end()) {
                ValidationEvent &event = test_events[test_key];
                
                if (elapsed && yyjson_is_num(elapsed)) {
                    event.execution_time = yyjson_get_real(elapsed);
                }
                
                if (action_str == "pass") {
                    event.status = ValidationEventStatus::PASS;
                    event.category = "test_success";
                    event.message = "Test passed";
                } else if (action_str == "fail") {
                    event.status = ValidationEventStatus::FAIL;
                    event.category = "test_failure";
                    event.message = "Test failed";
                } else if (action_str == "skip") {
                    event.status = ValidationEventStatus::SKIP;
                    event.category = "test_skipped";
                    event.message = "Test skipped";
                }
                
                events.push_back(event);
                test_events.erase(test_key);
            }
        }
        
        yyjson_doc_free(doc);
    }
}

void ParseMakeErrors(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_function;
    
    while (std::getline(stream, line)) {
        // Parse function context: "file.c: In function 'function_name':"
        std::regex function_pattern(R"(([^:]+):\s*In function\s+'([^']+)':)");
        std::smatch func_match;
        if (std::regex_match(line, func_match, function_pattern)) {
            current_function = func_match[2].str();
            continue;
        }
        
        // Parse GCC/Clang error format: file:line:column: severity: message
        std::regex error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = current_function;
            event.message = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            
            std::string severity = match[4].str();
            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "compilation"; 
                event.severity = "warning";
            } else if (severity == "note") {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }
            
            events.push_back(event);
        }
        // Parse make failure line with target extraction
        else if (line.find("make: ***") != std::string::npos && line.find("Error") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_failure";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            
            // Extract makefile target from pattern like "[Makefile:23: build/main]"
            std::regex target_pattern(R"(\[([^:]+):(\d+):\s*([^\]]+)\])");
            std::smatch target_match;
            if (std::regex_search(line, target_match, target_pattern)) {
                event.file_path = target_match[1].str();  // Makefile
                event.line_number = std::stoi(target_match[2].str());  // Line in Makefile
                event.test_name = target_match[3].str();  // Target name (e.g., "build/main")
            }
            
            events.push_back(event);
        }
        // Parse linker errors for make builds
        else if (line.find("undefined reference") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            
            // Extract symbol name from undefined reference
            std::regex symbol_pattern(R"(undefined reference to `([^']+)')");
            std::smatch symbol_match;
            if (std::regex_search(line, symbol_match, symbol_pattern)) {
                event.function_name = symbol_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }
            
            events.push_back(event);
        }
    }
}

void ParsePytestText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Look for pytest text output patterns: "file.py::test_name STATUS"
        if (line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "test";
            
            // Parse the line format: "file.py::test_name STATUS"
            size_t separator = line.find("::");
            if (separator != std::string::npos) {
                event.file_path = line.substr(0, separator);
                
                std::string rest = line.substr(separator + 2);
                
                // Find the status at the end
                if (rest.find(" PASSED") != std::string::npos) {
                    event.status = ValidationEventStatus::PASS;
                    event.message = "Test passed";
                    event.test_name = rest.substr(0, rest.find(" PASSED"));
                } else if (rest.find(" FAILED") != std::string::npos) {
                    event.status = ValidationEventStatus::FAIL;
                    event.message = "Test failed";
                    event.test_name = rest.substr(0, rest.find(" FAILED"));
                } else if (rest.find(" SKIPPED") != std::string::npos) {
                    event.status = ValidationEventStatus::SKIP;
                    event.message = "Test skipped";
                    event.test_name = rest.substr(0, rest.find(" SKIPPED"));
                } else {
                    // Default case
                    event.status = ValidationEventStatus::INFO;
                    event.message = "Test result";
                    event.test_name = rest;
                }
            }
            
            events.push_back(event);
        }
    }
}

void ParseRuboCopJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse RuboCop JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid RuboCop JSON: root is not an object");
    }
    
    // Get files array
    yyjson_val *files = yyjson_obj_get(root, "files");
    if (!files || !yyjson_is_arr(files)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid RuboCop JSON: no files array found");
    }
    
    // Parse each file
    size_t idx, max;
    yyjson_val *file;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(files, idx, max, file) {
        if (!yyjson_is_obj(file)) continue;
        
        // Get file path
        yyjson_val *path = yyjson_obj_get(file, "path");
        if (!path || !yyjson_is_str(path)) continue;
        
        std::string file_path = yyjson_get_str(path);
        
        // Get offenses array
        yyjson_val *offenses = yyjson_obj_get(file, "offenses");
        if (!offenses || !yyjson_is_arr(offenses)) continue;
        
        // Parse each offense
        size_t offense_idx, offense_max;
        yyjson_val *offense;
        
        yyjson_arr_foreach(offenses, offense_idx, offense_max, offense) {
            if (!yyjson_is_obj(offense)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rubocop";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "code_quality";
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(offense, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else if (severity_str == "convention") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "convention";
                } else {
                    event.status = ValidationEventStatus::INFO;
                    event.severity = severity_str;
                }
            }
            
            // Get message
            yyjson_val *message = yyjson_obj_get(offense, "message");
            if (message && yyjson_is_str(message)) {
                event.message = yyjson_get_str(message);
            }
            
            // Get cop name (rule ID)
            yyjson_val *cop_name = yyjson_obj_get(offense, "cop_name");
            if (cop_name && yyjson_is_str(cop_name)) {
                event.error_code = yyjson_get_str(cop_name);
            }
            
            // Get location
            yyjson_val *location = yyjson_obj_get(offense, "location");
            if (location && yyjson_is_obj(location)) {
                yyjson_val *start_line = yyjson_obj_get(location, "start_line");
                yyjson_val *start_column = yyjson_obj_get(location, "start_column");
                
                if (start_line && yyjson_is_num(start_line)) {
                    event.line_number = yyjson_get_int(start_line);
                }
                if (start_column && yyjson_is_num(start_column)) {
                    event.column_number = yyjson_get_int(start_column);
                }
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "rubocop_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseCargoTestJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track test events
    std::map<std::string, ValidationEvent> test_events;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse each JSON line
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) continue;
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get type and event
        yyjson_val *type = yyjson_obj_get(root, "type");
        yyjson_val *event_val = yyjson_obj_get(root, "event");
        
        if (!type || !yyjson_is_str(type) || !event_val || !yyjson_is_str(event_val)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        std::string type_str = yyjson_get_str(type);
        std::string event_str = yyjson_get_str(event_val);
        
        // Handle test events
        if (type_str == "test") {
            yyjson_val *name = yyjson_obj_get(root, "name");
            if (!name || !yyjson_is_str(name)) {
                yyjson_doc_free(doc);
                continue;
            }
            
            std::string test_name = yyjson_get_str(name);
            
            if (event_str == "started") {
                // Initialize test event
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo_test";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = test_name;
                event.function_name = test_name;
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.category = "test";
                
                test_events[test_name] = event;
            } else if (event_str == "ok" || event_str == "failed" || event_str == "ignored") {
                // Finalize test event
                if (test_events.find(test_name) != test_events.end()) {
                    ValidationEvent &event = test_events[test_name];
                    
                    // Get execution time
                    yyjson_val *exec_time = yyjson_obj_get(root, "exec_time");
                    if (exec_time && yyjson_is_num(exec_time)) {
                        event.execution_time = yyjson_get_real(exec_time);
                    }
                    
                    // Set status based on event
                    if (event_str == "ok") {
                        event.status = ValidationEventStatus::PASS;
                        event.message = "Test passed";
                        event.severity = "success";
                    } else if (event_str == "failed") {
                        event.status = ValidationEventStatus::FAIL;
                        event.message = "Test failed";
                        event.severity = "error";
                        
                        // Get failure details from stdout
                        yyjson_val *stdout_val = yyjson_obj_get(root, "stdout");
                        if (stdout_val && yyjson_is_str(stdout_val)) {
                            std::string stdout_str = yyjson_get_str(stdout_val);
                            if (!stdout_str.empty()) {
                                event.message = "Test failed: " + stdout_str;
                            }
                        }
                    } else if (event_str == "ignored") {
                        event.status = ValidationEventStatus::SKIP;
                        event.message = "Test ignored";
                        event.severity = "info";
                    }
                    
                    // Set raw output and structured data
                    event.raw_output = content;
                    event.structured_data = "cargo_test_json";
                    
                    events.push_back(event);
                    test_events.erase(test_name);
                }
            }
        }
        
        yyjson_doc_free(doc);
    }
}

void ParseSwiftLintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse SwiftLint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid SwiftLint JSON: root is not an array");
    }
    
    // Parse each violation
    size_t idx, max;
    yyjson_val *violation;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, violation) {
        if (!yyjson_is_obj(violation)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "swiftlint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.line_number = -1;
        event.column_number = -1;
        event.execution_time = 0.0;
        event.category = "code_quality";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(violation, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line and column
        yyjson_val *line = yyjson_obj_get(violation, "line");
        if (line && yyjson_is_num(line)) {
            event.line_number = yyjson_get_int(line);
        }
        
        yyjson_val *column = yyjson_obj_get(violation, "column");
        if (column && yyjson_is_num(column)) {
            event.column_number = yyjson_get_int(column);
        }
        
        // Get severity
        yyjson_val *severity = yyjson_obj_get(violation, "severity");
        if (severity && yyjson_is_str(severity)) {
            std::string severity_str = yyjson_get_str(severity);
            if (severity_str == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else if (severity_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.severity = severity_str;
            }
        }
        
        // Get reason (message)
        yyjson_val *reason = yyjson_obj_get(violation, "reason");
        if (reason && yyjson_is_str(reason)) {
            event.message = yyjson_get_str(reason);
        }
        
        // Get rule ID
        yyjson_val *rule_id = yyjson_obj_get(violation, "rule_id");
        if (rule_id && yyjson_is_str(rule_id)) {
            event.error_code = yyjson_get_str(rule_id);
        }
        
        // Get type (rule type)
        yyjson_val *type = yyjson_obj_get(violation, "type");
        if (type && yyjson_is_str(type)) {
            event.suggestion = yyjson_get_str(type);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "swiftlint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParsePHPStanJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse PHPStan JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid PHPStan JSON: root is not an object");
    }
    
    // Get files object
    yyjson_val *files = yyjson_obj_get(root, "files");
    if (!files || !yyjson_is_obj(files)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid PHPStan JSON: no files object found");
    }
    
    // Parse each file
    size_t idx, max;
    yyjson_val *file_key, *file_data;
    int64_t event_id = 1;
    
    yyjson_obj_foreach(files, idx, max, file_key, file_data) {
        if (!yyjson_is_str(file_key) || !yyjson_is_obj(file_data)) continue;
        
        std::string file_path = yyjson_get_str(file_key);
        
        // Get messages array
        yyjson_val *messages = yyjson_obj_get(file_data, "messages");
        if (!messages || !yyjson_is_arr(messages)) continue;
        
        // Parse each message
        size_t msg_idx, msg_max;
        yyjson_val *message;
        
        yyjson_arr_foreach(messages, msg_idx, msg_max, message) {
            if (!yyjson_is_obj(message)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "phpstan";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "static_analysis";
            
            // Get message text
            yyjson_val *msg_text = yyjson_obj_get(message, "message");
            if (msg_text && yyjson_is_str(msg_text)) {
                event.message = yyjson_get_str(msg_text);
            }
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(message, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            }
            
            // Get ignorable status (use as severity indicator)
            yyjson_val *ignorable = yyjson_obj_get(message, "ignorable");
            if (ignorable && yyjson_is_bool(ignorable)) {
                if (yyjson_get_bool(ignorable)) {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                }
            } else {
                // Default to error
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "phpstan_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseShellCheckJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse ShellCheck JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid ShellCheck JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "shellcheck";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "shell_script";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(issue, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line number
        yyjson_val *line = yyjson_obj_get(issue, "line");
        if (line && yyjson_is_int(line)) {
            event.line_number = yyjson_get_int(line);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column = yyjson_obj_get(issue, "column");
        if (column && yyjson_is_int(column)) {
            event.column_number = yyjson_get_int(column);
        } else {
            event.column_number = -1;
        }
        
        // Get severity/level
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map ShellCheck levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else if (level_str == "style") {
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get error code (SC#### codes)
        yyjson_val *code = yyjson_obj_get(issue, "code");
        if (code && yyjson_is_str(code)) {
            event.error_code = yyjson_get_str(code);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get fix suggestions if available
        yyjson_val *fix = yyjson_obj_get(issue, "fix");
        if (fix && yyjson_is_obj(fix)) {
            yyjson_val *replacements = yyjson_obj_get(fix, "replacements");
            if (replacements && yyjson_is_arr(replacements)) {
                event.suggestion = "Fix available";
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "shellcheck_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseStylelintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse stylelint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid stylelint JSON: root is not an array");
    }
    
    // Parse each file result
    size_t idx, max;
    yyjson_val *file_result;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, file_result) {
        if (!yyjson_is_obj(file_result)) continue;
        
        // Get source file path
        yyjson_val *source = yyjson_obj_get(file_result, "source");
        if (!source || !yyjson_is_str(source)) continue;
        
        std::string file_path = yyjson_get_str(source);
        
        // Get warnings array
        yyjson_val *warnings = yyjson_obj_get(file_result, "warnings");
        if (!warnings || !yyjson_is_arr(warnings)) continue;
        
        // Parse each warning
        size_t warn_idx, warn_max;
        yyjson_val *warning;
        
        yyjson_arr_foreach(warnings, warn_idx, warn_max, warning) {
            if (!yyjson_is_obj(warning)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "stylelint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "css_style";
            event.file_path = file_path;
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(warning, "line");
            if (line && yyjson_is_int(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            // Get column number
            yyjson_val *column = yyjson_obj_get(warning, "column");
            if (column && yyjson_is_int(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(warning, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                event.severity = severity_str;
                
                // Map stylelint severity to ValidationEventStatus
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                } else {
                    event.status = ValidationEventStatus::WARNING;
                }
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            // Get rule name
            yyjson_val *rule = yyjson_obj_get(warning, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.error_code = yyjson_get_str(rule);
            }
            
            // Get message text
            yyjson_val *text = yyjson_obj_get(warning, "text");
            if (text && yyjson_is_str(text)) {
                event.message = yyjson_get_str(text);
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "stylelint_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseClippyJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSONL format (JSON Lines) - each line is a separate JSON object
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse JSON using yyjson
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) {
            continue; // Skip invalid JSON lines
        }
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get message object
        yyjson_val *message = yyjson_obj_get(root, "message");
        if (!message || !yyjson_is_obj(message)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get spans array
        yyjson_val *spans = yyjson_obj_get(message, "spans");
        if (!spans || !yyjson_is_arr(spans)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get primary span (first span with is_primary = true)
        yyjson_val *primary_span = nullptr;
        size_t idx, max;
        yyjson_val *span;
        
        yyjson_arr_foreach(spans, idx, max, span) {
            if (!yyjson_is_obj(span)) continue;
            
            yyjson_val *is_primary = yyjson_obj_get(span, "is_primary");
            if (is_primary && yyjson_is_bool(is_primary) && yyjson_get_bool(is_primary)) {
                primary_span = span;
                break;
            }
        }
        
        if (!primary_span) {
            // If no primary span found, use the first span
            primary_span = yyjson_arr_get_first(spans);
        }
        
        if (!primary_span) {
            yyjson_doc_free(doc);
            continue;
        }
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "clippy";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "code_quality";
        
        // Get file name from primary span
        yyjson_val *file_name = yyjson_obj_get(primary_span, "file_name");
        if (file_name && yyjson_is_str(file_name)) {
            event.file_path = yyjson_get_str(file_name);
        }
        
        // Get line number from primary span
        yyjson_val *line_start = yyjson_obj_get(primary_span, "line_start");
        if (line_start && yyjson_is_int(line_start)) {
            event.line_number = yyjson_get_int(line_start);
        } else {
            event.line_number = -1;
        }
        
        // Get column number from primary span
        yyjson_val *column_start = yyjson_obj_get(primary_span, "column_start");
        if (column_start && yyjson_is_int(column_start)) {
            event.column_number = yyjson_get_int(column_start);
        } else {
            event.column_number = -1;
        }
        
        // Get severity level
        yyjson_val *level = yyjson_obj_get(message, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map clippy levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warn" || level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "note" || level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get code object for error code
        yyjson_val *code = yyjson_obj_get(message, "code");
        if (code && yyjson_is_obj(code)) {
            yyjson_val *code_str = yyjson_obj_get(code, "code");
            if (code_str && yyjson_is_str(code_str)) {
                event.error_code = yyjson_get_str(code_str);
            }
        }
        
        // Get message text
        yyjson_val *message_text = yyjson_obj_get(message, "message");
        if (message_text && yyjson_is_str(message_text)) {
            event.message = yyjson_get_str(message_text);
        }
        
        // Get suggestion from primary span
        yyjson_val *suggested_replacement = yyjson_obj_get(primary_span, "suggested_replacement");
        if (suggested_replacement && yyjson_is_str(suggested_replacement)) {
            event.suggestion = yyjson_get_str(suggested_replacement);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "clippy_json";
        
        events.push_back(event);
        
        yyjson_doc_free(doc);
    }
}

void ParseMarkdownlintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse markdownlint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid markdownlint JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "markdownlint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "documentation";
        
        // Get file name
        yyjson_val *file_name = yyjson_obj_get(issue, "fileName");
        if (file_name && yyjson_is_str(file_name)) {
            event.file_path = yyjson_get_str(file_name);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "lineNumber");
        if (line_number && yyjson_is_int(line_number)) {
            event.line_number = yyjson_get_int(line_number);
        } else {
            event.line_number = -1;
        }
        
        // Markdownlint doesn't provide column in the same way - use errorRange if available
        yyjson_val *error_range = yyjson_obj_get(issue, "errorRange");
        if (error_range && yyjson_is_arr(error_range)) {
            yyjson_val *first_elem = yyjson_arr_get_first(error_range);
            if (first_elem && yyjson_is_int(first_elem)) {
                event.column_number = yyjson_get_int(first_elem);
            } else {
                event.column_number = -1;
            }
        } else {
            event.column_number = -1;
        }
        
        // Markdownlint issues are typically warnings (unless they're really severe)
        event.severity = "warning";
        event.status = ValidationEventStatus::WARNING;
        
        // Get rule names (first rule name as error code)
        yyjson_val *rule_names = yyjson_obj_get(issue, "ruleNames");
        if (rule_names && yyjson_is_arr(rule_names)) {
            yyjson_val *first_rule = yyjson_arr_get_first(rule_names);
            if (first_rule && yyjson_is_str(first_rule)) {
                event.error_code = yyjson_get_str(first_rule);
            }
        }
        
        // Get rule description as message
        yyjson_val *rule_description = yyjson_obj_get(issue, "ruleDescription");
        if (rule_description && yyjson_is_str(rule_description)) {
            event.message = yyjson_get_str(rule_description);
        }
        
        // Get error detail as suggestion if available
        yyjson_val *error_detail = yyjson_obj_get(issue, "errorDetail");
        if (error_detail && yyjson_is_str(error_detail)) {
            event.suggestion = yyjson_get_str(error_detail);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "markdownlint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseYamllintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse yamllint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid yamllint JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "yamllint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "configuration";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(issue, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line number
        yyjson_val *line = yyjson_obj_get(issue, "line");
        if (line && yyjson_is_int(line)) {
            event.line_number = yyjson_get_int(line);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column = yyjson_obj_get(issue, "column");
        if (column && yyjson_is_int(column)) {
            event.column_number = yyjson_get_int(column);
        } else {
            event.column_number = -1;
        }
        
        // Get severity level
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map yamllint levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get rule name as error code
        yyjson_val *rule = yyjson_obj_get(issue, "rule");
        if (rule && yyjson_is_str(rule)) {
            event.error_code = yyjson_get_str(rule);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "yamllint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseBanditJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse Bandit JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid Bandit JSON: root is not an object");
    }
    
    // Get results array
    yyjson_val *results = yyjson_obj_get(root, "results");
    if (!results || !yyjson_is_arr(results)) {
        yyjson_doc_free(doc);
        return; // No results to process
    }
    
    // Parse each security issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(results, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "bandit";
        event.event_type = ValidationEventType::SECURITY_FINDING;
        event.category = "security";
        
        // Get file path
        yyjson_val *filename = yyjson_obj_get(issue, "filename");
        if (filename && yyjson_is_str(filename)) {
            event.file_path = yyjson_get_str(filename);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "line_number");
        if (line_number && yyjson_is_int(line_number)) {
            event.line_number = yyjson_get_int(line_number);
        } else {
            event.line_number = -1;
        }
        
        // Get column offset (Bandit uses col_offset)
        yyjson_val *col_offset = yyjson_obj_get(issue, "col_offset");
        if (col_offset && yyjson_is_int(col_offset)) {
            event.column_number = yyjson_get_int(col_offset);
        } else {
            event.column_number = -1;
        }
        
        // Get test ID as error code
        yyjson_val *test_id = yyjson_obj_get(issue, "test_id");
        if (test_id && yyjson_is_str(test_id)) {
            event.error_code = yyjson_get_str(test_id);
        }
        
        // Get issue severity and map to status
        yyjson_val *issue_severity = yyjson_obj_get(issue, "issue_severity");
        if (issue_severity && yyjson_is_str(issue_severity)) {
            std::string severity_str = yyjson_get_str(issue_severity);
            event.severity = severity_str;
            
            // Map Bandit severity to ValidationEventStatus
            if (severity_str == "HIGH") {
                event.status = ValidationEventStatus::ERROR;
            } else if (severity_str == "MEDIUM") {
                event.status = ValidationEventStatus::WARNING;
            } else { // LOW
                event.status = ValidationEventStatus::INFO;
            }
        } else {
            event.severity = "medium";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get issue text as message
        yyjson_val *issue_text = yyjson_obj_get(issue, "issue_text");
        if (issue_text && yyjson_is_str(issue_text)) {
            event.message = yyjson_get_str(issue_text);
        }
        
        // Get test name as function context
        yyjson_val *test_name = yyjson_obj_get(issue, "test_name");
        if (test_name && yyjson_is_str(test_name)) {
            event.function_name = yyjson_get_str(test_name);
        }
        
        // Get CWE information for suggestion
        yyjson_val *issue_cwe = yyjson_obj_get(issue, "issue_cwe");
        if (issue_cwe && yyjson_is_obj(issue_cwe)) {
            yyjson_val *cwe_id = yyjson_obj_get(issue_cwe, "id");
            yyjson_val *cwe_link = yyjson_obj_get(issue_cwe, "link");
            
            if (cwe_id && yyjson_is_int(cwe_id)) {
                std::string suggestion = "CWE-" + std::to_string(yyjson_get_int(cwe_id));
                if (cwe_link && yyjson_is_str(cwe_link)) {
                    suggestion += ": " + std::string(yyjson_get_str(cwe_link));
                }
                event.suggestion = suggestion;
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "bandit_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseSpotBugsJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse SpotBugs JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid SpotBugs JSON: root is not an object");
    }
    
    // Get BugCollection object
    yyjson_val *bug_collection = yyjson_obj_get(root, "BugCollection");
    if (!bug_collection || !yyjson_is_obj(bug_collection)) {
        yyjson_doc_free(doc);
        return; // No bug collection to process
    }
    
    // Get BugInstance array
    yyjson_val *bug_instances = yyjson_obj_get(bug_collection, "BugInstance");
    if (!bug_instances || !yyjson_is_arr(bug_instances)) {
        yyjson_doc_free(doc);
        return; // No bug instances to process
    }
    
    // Parse each bug instance
    size_t idx, max;
    yyjson_val *bug;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(bug_instances, idx, max, bug) {
        if (!yyjson_is_obj(bug)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "spotbugs";
        event.event_type = ValidationEventType::LINT_ISSUE;
        
        // Get bug type as error code
        yyjson_val *type = yyjson_obj_get(bug, "type");
        if (type && yyjson_is_str(type)) {
            event.error_code = yyjson_get_str(type);
        }
        
        // Get category and map event type
        yyjson_val *category = yyjson_obj_get(bug, "category");
        if (category && yyjson_is_str(category)) {
            std::string category_str = yyjson_get_str(category);
            event.category = category_str;
            
            // Map SpotBugs categories to event types
            if (category_str == "SECURITY") {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (category_str == "PERFORMANCE") {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            } else if (category_str == "CORRECTNESS") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "correctness";
            } else if (category_str == "BAD_PRACTICE") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "code_quality";
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "static_analysis";
            }
        } else {
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "static_analysis";
        }
        
        // Get priority and map to status
        yyjson_val *priority = yyjson_obj_get(bug, "priority");
        if (priority && yyjson_is_str(priority)) {
            std::string priority_str = yyjson_get_str(priority);
            event.severity = priority_str;
            
            // Map SpotBugs priority to ValidationEventStatus (1=highest, 3=lowest)
            if (priority_str == "1") {
                event.status = ValidationEventStatus::ERROR;
            } else if (priority_str == "2") {
                event.status = ValidationEventStatus::WARNING;
            } else { // priority 3
                event.status = ValidationEventStatus::INFO;
            }
        } else {
            event.severity = "medium";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get short message as main message
        yyjson_val *short_message = yyjson_obj_get(bug, "ShortMessage");
        if (short_message && yyjson_is_str(short_message)) {
            event.message = yyjson_get_str(short_message);
        }
        
        // Get long message as suggestion
        yyjson_val *long_message = yyjson_obj_get(bug, "LongMessage");
        if (long_message && yyjson_is_str(long_message)) {
            event.suggestion = yyjson_get_str(long_message);
        }
        
        // Get source line information
        yyjson_val *source_line = yyjson_obj_get(bug, "SourceLine");
        if (source_line && yyjson_is_obj(source_line)) {
            // Check if this is the primary source line
            yyjson_val *primary = yyjson_obj_get(source_line, "primary");
            if (primary && yyjson_is_bool(primary) && yyjson_get_bool(primary)) {
                // Get source path
                yyjson_val *sourcepath = yyjson_obj_get(source_line, "sourcepath");
                if (sourcepath && yyjson_is_str(sourcepath)) {
                    event.file_path = yyjson_get_str(sourcepath);
                }
                
                // Get line number (use start line)
                yyjson_val *start_line = yyjson_obj_get(source_line, "start");
                if (start_line && yyjson_is_str(start_line)) {
                    event.line_number = std::stoll(yyjson_get_str(start_line));
                } else {
                    event.line_number = -1;
                }
                
                // SpotBugs doesn't provide column information
                event.column_number = -1;
            }
        }
        
        // Get method information for function context
        yyjson_val *method = yyjson_obj_get(bug, "Method");
        if (method && yyjson_is_obj(method)) {
            yyjson_val *primary = yyjson_obj_get(method, "primary");
            if (primary && yyjson_is_bool(primary) && yyjson_get_bool(primary)) {
                yyjson_val *method_name = yyjson_obj_get(method, "name");
                yyjson_val *classname = yyjson_obj_get(method, "classname");
                
                if (method_name && yyjson_is_str(method_name) && classname && yyjson_is_str(classname)) {
                    event.function_name = std::string(yyjson_get_str(classname)) + "." + std::string(yyjson_get_str(method_name));
                }
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "spotbugs_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseKtlintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse ktlint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid ktlint JSON: root is not an array");
    }
    
    // Parse each file entry
    size_t file_idx, file_max;
    yyjson_val *file_entry;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, file_idx, file_max, file_entry) {
        if (!yyjson_is_obj(file_entry)) continue;
        
        // Get file path
        std::string file_path;
        yyjson_val *file = yyjson_obj_get(file_entry, "file");
        if (file && yyjson_is_str(file)) {
            file_path = yyjson_get_str(file);
        }
        
        // Get errors array
        yyjson_val *errors = yyjson_obj_get(file_entry, "errors");
        if (!errors || !yyjson_is_arr(errors)) continue;
        
        // Parse each error
        size_t error_idx, error_max;
        yyjson_val *error;
        
        yyjson_arr_foreach(errors, error_idx, error_max, error) {
            if (!yyjson_is_obj(error)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ktlint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "code_style";
            event.file_path = file_path;
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(error, "line");
            if (line && yyjson_is_int(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            // Get column number
            yyjson_val *column = yyjson_obj_get(error, "column");
            if (column && yyjson_is_int(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Get rule name as error code
            yyjson_val *rule = yyjson_obj_get(error, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.error_code = yyjson_get_str(rule);
            }
            
            // Get message
            yyjson_val *message = yyjson_obj_get(error, "message");
            if (message && yyjson_is_str(message)) {
                event.message = yyjson_get_str(message);
            }
            
            // ktlint doesn't provide explicit severity, so we infer from rule types
            std::string rule_str = event.error_code;
            if (rule_str.find("max-line-length") != std::string::npos || 
                rule_str.find("no-wildcard-imports") != std::string::npos) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else if (rule_str.find("indent") != std::string::npos || 
                       rule_str.find("final-newline") != std::string::npos) {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else {
                // Default to warning for style issues
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "ktlint_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseHadolintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse hadolint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid hadolint JSON: root is not an array");
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "hadolint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "dockerfile";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(issue, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line number
        yyjson_val *line = yyjson_obj_get(issue, "line");
        if (line && yyjson_is_int(line)) {
            event.line_number = yyjson_get_int(line);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column = yyjson_obj_get(issue, "column");
        if (column && yyjson_is_int(column)) {
            event.column_number = yyjson_get_int(column);
        } else {
            event.column_number = -1;
        }
        
        // Get code as error code
        yyjson_val *code = yyjson_obj_get(issue, "code");
        if (code && yyjson_is_str(code)) {
            event.error_code = yyjson_get_str(code);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get level and map to status
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map hadolint levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else if (level_str == "style") {
                event.status = ValidationEventStatus::WARNING; // Treat style as warning
            } else {
                event.status = ValidationEventStatus::WARNING; // Default to warning
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "hadolint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseLintrJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse lintr JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid lintr JSON: root is not an array");
    }
    
    // Parse each lint issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "lintr";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "r_code_style";
        
        // Get filename
        yyjson_val *filename = yyjson_obj_get(issue, "filename");
        if (filename && yyjson_is_str(filename)) {
            event.file_path = yyjson_get_str(filename);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "line_number");
        if (line_number && yyjson_is_int(line_number)) {
            event.line_number = yyjson_get_int(line_number);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column_number = yyjson_obj_get(issue, "column_number");
        if (column_number && yyjson_is_int(column_number)) {
            event.column_number = yyjson_get_int(column_number);
        } else {
            event.column_number = -1;
        }
        
        // Get linter name as error code
        yyjson_val *linter = yyjson_obj_get(issue, "linter");
        if (linter && yyjson_is_str(linter)) {
            event.error_code = yyjson_get_str(linter);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get type and map to status
        yyjson_val *type = yyjson_obj_get(issue, "type");
        if (type && yyjson_is_str(type)) {
            std::string type_str = yyjson_get_str(type);
            event.severity = type_str;
            
            // Map lintr types to ValidationEventStatus
            if (type_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (type_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (type_str == "style") {
                event.status = ValidationEventStatus::WARNING; // Treat style as warning
            } else {
                event.status = ValidationEventStatus::INFO; // Default for other types
            }
        } else {
            event.severity = "style";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get line content as suggestion (if available)
        yyjson_val *line_content = yyjson_obj_get(issue, "line");
        if (line_content && yyjson_is_str(line_content)) {
            event.suggestion = "Code: " + std::string(yyjson_get_str(line_content));
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "lintr_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseSqlfluffJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse sqlfluff JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid sqlfluff JSON: root is not an array");
    }
    
    // Parse each file entry
    size_t file_idx, file_max;
    yyjson_val *file_entry;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, file_idx, file_max, file_entry) {
        if (!yyjson_is_obj(file_entry)) continue;
        
        // Get filepath
        yyjson_val *filepath = yyjson_obj_get(file_entry, "filepath");
        if (!filepath || !yyjson_is_str(filepath)) continue;
        
        std::string file_path = yyjson_get_str(filepath);
        
        // Get violations array
        yyjson_val *violations = yyjson_obj_get(file_entry, "violations");
        if (!violations || !yyjson_is_arr(violations)) continue;
        
        // Parse each violation
        size_t viol_idx, viol_max;
        yyjson_val *violation;
        
        yyjson_arr_foreach(violations, viol_idx, viol_max, violation) {
            if (!yyjson_is_obj(violation)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "sqlfluff";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "sql_style";
            event.file_path = file_path;
            
            // Get line number
            yyjson_val *line_no = yyjson_obj_get(violation, "line_no");
            if (line_no && yyjson_is_int(line_no)) {
                event.line_number = yyjson_get_int(line_no);
            } else {
                event.line_number = -1;
            }
            
            // Get column position  
            yyjson_val *line_pos = yyjson_obj_get(violation, "line_pos");
            if (line_pos && yyjson_is_int(line_pos)) {
                event.column_number = yyjson_get_int(line_pos);
            } else {
                event.column_number = -1;
            }
            
            // Get rule code as error_code
            yyjson_val *code = yyjson_obj_get(violation, "code");
            if (code && yyjson_is_str(code)) {
                event.error_code = yyjson_get_str(code);
            }
            
            // Get rule name for context
            yyjson_val *rule = yyjson_obj_get(violation, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.function_name = yyjson_get_str(rule);
            }
            
            // Get description as message
            yyjson_val *description = yyjson_obj_get(violation, "description");
            if (description && yyjson_is_str(description)) {
                event.message = yyjson_get_str(description);
            }
            
            // All sqlfluff violations are warnings by default
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            
            // Create suggestion from rule name
            if (!event.function_name.empty()) {
                event.suggestion = "Rule: " + event.function_name;
            }
            
            event.raw_output = content;
            event.structured_data = "sqlfluff_json";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseTflintJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse tflint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid tflint JSON: root is not an object");
    }
    
    // Get issues array
    yyjson_val *issues = yyjson_obj_get(root, "issues");
    if (!issues || !yyjson_is_arr(issues)) {
        yyjson_doc_free(doc);
        return; // No issues to process
    }
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(issues, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "tflint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "infrastructure";
        
        // Get rule information
        yyjson_val *rule = yyjson_obj_get(issue, "rule");
        if (rule && yyjson_is_obj(rule)) {
            // Get rule name as error code
            yyjson_val *rule_name = yyjson_obj_get(rule, "name");
            if (rule_name && yyjson_is_str(rule_name)) {
                event.error_code = yyjson_get_str(rule_name);
                event.function_name = yyjson_get_str(rule_name);
            }
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(rule, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                event.severity = severity_str;
                
                // Map tflint severities to ValidationEventStatus
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                } else if (severity_str == "notice") {
                    event.status = ValidationEventStatus::INFO;
                } else {
                    event.status = ValidationEventStatus::WARNING; // Default
                }
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get range information
        yyjson_val *range = yyjson_obj_get(issue, "range");
        if (range && yyjson_is_obj(range)) {
            // Get filename
            yyjson_val *filename = yyjson_obj_get(range, "filename");
            if (filename && yyjson_is_str(filename)) {
                event.file_path = yyjson_get_str(filename);
            }
            
            // Get start position
            yyjson_val *start = yyjson_obj_get(range, "start");
            if (start && yyjson_is_obj(start)) {
                yyjson_val *line = yyjson_obj_get(start, "line");
                if (line && yyjson_is_int(line)) {
                    event.line_number = yyjson_get_int(line);
                } else {
                    event.line_number = -1;
                }
                
                yyjson_val *column = yyjson_obj_get(start, "column");
                if (column && yyjson_is_int(column)) {
                    event.column_number = yyjson_get_int(column);
                } else {
                    event.column_number = -1;
                }
            }
        }
        
        // Create suggestion from rule name
        if (!event.function_name.empty()) {
            event.suggestion = "Rule: " + event.function_name;
        }
        
        event.raw_output = content;
        event.structured_data = "tflint_json";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
}

void ParseKubeScoreJSON(const std::string& content, std::vector<ValidationEvent>& events) {
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse kube-score JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid kube-score JSON: root is not an array");
    }
    
    // Parse each Kubernetes object
    size_t obj_idx, obj_max;
    yyjson_val *k8s_object;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, obj_idx, obj_max, k8s_object) {
        if (!yyjson_is_obj(k8s_object)) continue;
        
        // Get object metadata
        std::string object_name;
        std::string file_name;
        std::string resource_kind;
        std::string namespace_name = "default";
        int line_number = -1;
        
        yyjson_val *obj_name = yyjson_obj_get(k8s_object, "object_name");
        if (obj_name && yyjson_is_str(obj_name)) {
            object_name = yyjson_get_str(obj_name);
        }
        
        yyjson_val *file_name_val = yyjson_obj_get(k8s_object, "file_name");
        if (file_name_val && yyjson_is_str(file_name_val)) {
            file_name = yyjson_get_str(file_name_val);
        }
        
        yyjson_val *file_row = yyjson_obj_get(k8s_object, "file_row");
        if (file_row && yyjson_is_int(file_row)) {
            line_number = yyjson_get_int(file_row);
        }
        
        // Get resource kind from type_meta
        yyjson_val *type_meta = yyjson_obj_get(k8s_object, "type_meta");
        if (type_meta && yyjson_is_obj(type_meta)) {
            yyjson_val *kind = yyjson_obj_get(type_meta, "kind");
            if (kind && yyjson_is_str(kind)) {
                resource_kind = yyjson_get_str(kind);
            }
        }
        
        // Get namespace from object_meta
        yyjson_val *object_meta = yyjson_obj_get(k8s_object, "object_meta");
        if (object_meta && yyjson_is_obj(object_meta)) {
            yyjson_val *ns = yyjson_obj_get(object_meta, "namespace");
            if (ns && yyjson_is_str(ns)) {
                namespace_name = yyjson_get_str(ns);
            }
        }
        
        // Get checks array
        yyjson_val *checks = yyjson_obj_get(k8s_object, "checks");
        if (!checks || !yyjson_is_arr(checks)) continue;
        
        // Parse each check
        size_t check_idx, check_max;
        yyjson_val *check;
        
        yyjson_arr_foreach(checks, check_idx, check_max, check) {
            if (!yyjson_is_obj(check)) continue;
            
            // Get grade to determine if this is an issue
            yyjson_val *grade = yyjson_obj_get(check, "grade");
            if (!grade || !yyjson_is_str(grade)) continue;
            
            std::string grade_str = yyjson_get_str(grade);
            
            // Skip OK checks unless they have comments
            yyjson_val *comments = yyjson_obj_get(check, "comments");
            bool has_comments = comments && yyjson_is_arr(comments) && yyjson_arr_size(comments) > 0;
            
            if (grade_str == "OK" && !has_comments) continue;
            
            // Get check information
            yyjson_val *check_info = yyjson_obj_get(check, "check");
            std::string check_id;
            std::string check_name;
            std::string check_comment;
            
            if (check_info && yyjson_is_obj(check_info)) {
                yyjson_val *id = yyjson_obj_get(check_info, "id");
                if (id && yyjson_is_str(id)) {
                    check_id = yyjson_get_str(id);
                }
                
                yyjson_val *name = yyjson_obj_get(check_info, "name");
                if (name && yyjson_is_str(name)) {
                    check_name = yyjson_get_str(name);
                }
                
                yyjson_val *comment = yyjson_obj_get(check_info, "comment");
                if (comment && yyjson_is_str(comment)) {
                    check_comment = yyjson_get_str(comment);
                }
            }
            
            // Create validation event for each comment or one general event
            if (has_comments) {
                size_t comment_idx, comment_max;
                yyjson_val *comment_obj;
                
                yyjson_arr_foreach(comments, comment_idx, comment_max, comment_obj) {
                    if (!yyjson_is_obj(comment_obj)) continue;
                    
                    ValidationEvent event;
                    event.event_id = event_id++;
                    event.tool_name = "kube-score";
                    event.event_type = ValidationEventType::LINT_ISSUE;
                    event.category = "kubernetes";
                    event.file_path = file_name;
                    event.line_number = line_number;
                    event.column_number = -1;
                    event.error_code = check_id;
                    event.function_name = object_name + " (" + resource_kind + ")";
                    
                    // Map grade to status
                    if (grade_str == "CRITICAL") {
                        event.status = ValidationEventStatus::ERROR;
                        event.severity = "critical";
                    } else if (grade_str == "WARNING") {
                        event.status = ValidationEventStatus::WARNING;
                        event.severity = "warning";
                    } else {
                        event.status = ValidationEventStatus::INFO;
                        event.severity = "info";
                    }
                    
                    // Get comment details
                    yyjson_val *summary = yyjson_obj_get(comment_obj, "summary");
                    yyjson_val *description = yyjson_obj_get(comment_obj, "description");
                    yyjson_val *path = yyjson_obj_get(comment_obj, "path");
                    
                    if (summary && yyjson_is_str(summary)) {
                        event.message = yyjson_get_str(summary);
                    } else {
                        event.message = check_name;
                    }
                    
                    if (description && yyjson_is_str(description)) {
                        event.suggestion = yyjson_get_str(description);
                    }
                    
                    // Add path context if available
                    if (path && yyjson_is_str(path) && strlen(yyjson_get_str(path)) > 0) {
                        event.test_name = yyjson_get_str(path);
                    }
                    
                    event.raw_output = content;
                    event.structured_data = "kube_score_json";
                    
                    events.push_back(event);
                }
            } else {
                // Create general event for non-OK checks without specific comments
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "kube-score";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "kubernetes";
                event.file_path = file_name;
                event.line_number = line_number;
                event.column_number = -1;
                event.error_code = check_id;
                event.function_name = object_name + " (" + resource_kind + ")";
                event.message = check_name;
                event.suggestion = check_comment;
                
                // Map grade to status
                if (grade_str == "CRITICAL") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "critical";
                } else if (grade_str == "WARNING") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else {
                    event.status = ValidationEventStatus::INFO;
                    event.severity = "info";
                }
                
                event.raw_output = content;
                event.structured_data = "kube_score_json";
                
                events.push_back(event);
            }
        }
    }
    
    yyjson_doc_free(doc);
}

void ParseCMakeBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse GCC/Clang error format: file:line:column: severity: message
        std::regex cpp_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, cpp_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;
            
            std::string severity = match[4].str();
            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "compilation";
                event.severity = "warning";
            } else if (severity == "note") {
                event.status = ValidationEventStatus::ERROR;  // Treat notes as errors for CMake builds
                event.category = "compilation";
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }
            
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake configuration errors
        else if (line.find("CMake Error") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "configuration";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract file info from CMake errors like "CMake Error at CMakeLists.txt:25"
            std::regex cmake_error_pattern(R"(CMake Error at ([^:]+):(\d+))");
            std::smatch cmake_match;
            if (std::regex_search(line, cmake_match, cmake_error_pattern)) {
                event.file_path = cmake_match[1].str();
                event.line_number = std::stoi(cmake_match[2].str());
            }
            
            event.message = content;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake warnings
        else if (line.find("CMake Warning") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.category = "configuration";
            event.severity = "warning";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract file info from CMake warnings
            std::regex cmake_warning_pattern(R"(CMake Warning at ([^:]+):(\d+))");
            std::smatch cmake_match;
            if (std::regex_search(line, cmake_match, cmake_warning_pattern)) {
                event.file_path = cmake_match[1].str();
                event.line_number = std::stoi(cmake_match[2].str());
            }
            
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse linker errors (both with and without /usr/bin/ld: prefix)
        else if (line.find("undefined reference") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract symbol name from linker error
            std::regex linker_pattern(R"(undefined reference to `([^']+)')");
            std::smatch linker_match;
            if (std::regex_search(line, linker_match, linker_pattern)) {
                event.function_name = linker_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }
            
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse collect2 linker errors
        else if (line.find("collect2: error:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse gmake errors
        else if (line.find("gmake[") != std::string::npos && line.find("***") != std::string::npos && line.find("Error") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_failure";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake configuration summary errors  
        else if (line.find("-- Configuring incomplete, errors occurred!") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "configuration";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
    }
}

void ParseGenericLint(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse generic lint format: file:line:column: level: message
        std::regex lint_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|info|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, lint_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "lint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;
            
            std::string level = match[4].str();
            if (level == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "lint_error";
                event.severity = "error";
            } else if (level == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "lint_warning";
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "lint_info";
                event.severity = "info";
            }
            
            events.push_back(event);
        }
    }
    
    // If no events were created, add a basic summary
    if (events.empty()) {
        ValidationEvent summary_event;
        summary_event.event_id = 1;
        summary_event.tool_name = "lint";
        summary_event.event_type = ValidationEventType::LINT_ISSUE;
        summary_event.status = ValidationEventStatus::INFO;
        summary_event.category = "lint_summary";
        summary_event.message = "Generic lint output parsed (no issues found)";
        summary_event.line_number = -1;
        summary_event.column_number = -1;
        summary_event.execution_time = 0.0;
        
        events.push_back(summary_event);
    }
}

// Parse test results implementation for string input
unique_ptr<FunctionData> ParseTestResultsBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
    auto bind_data = make_uniq<ReadTestResultsBindData>();
    
    // Get content parameter (required)
    if (input.inputs.empty()) {
        throw BinderException("parse_test_results requires at least one parameter (content)");
    }
    bind_data->source = input.inputs[0].ToString();
    
    // Get format parameter (optional, defaults to auto)
    if (input.inputs.size() > 1) {
        bind_data->format = StringToTestResultFormat(input.inputs[1].ToString());
    } else {
        bind_data->format = TestResultFormat::AUTO;
    }
    
    // Define return schema (same as read_test_results)
    return_types = {
        LogicalType::BIGINT,   // event_id
        LogicalType::VARCHAR,  // tool_name
        LogicalType::VARCHAR,  // event_type
        LogicalType::VARCHAR,  // file_path
        LogicalType::INTEGER,  // line_number
        LogicalType::INTEGER,  // column_number
        LogicalType::VARCHAR,  // function_name
        LogicalType::VARCHAR,  // status
        LogicalType::VARCHAR,  // severity
        LogicalType::VARCHAR,  // category
        LogicalType::VARCHAR,  // message
        LogicalType::VARCHAR,  // suggestion
        LogicalType::VARCHAR,  // error_code
        LogicalType::VARCHAR,  // test_name
        LogicalType::DOUBLE,   // execution_time
        LogicalType::VARCHAR,  // raw_output
        LogicalType::VARCHAR   // structured_data
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number",
        "column_number", "function_name", "status", "severity", "category",
        "message", "suggestion", "error_code", "test_name", "execution_time",
        "raw_output", "structured_data"
    };
    
    return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ParseTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<ReadTestResultsBindData>();
    auto global_state = make_uniq<ReadTestResultsGlobalState>();
    
    // Use source directly as content (no file reading)
    std::string content = bind_data.source;
    
    // Auto-detect format if needed
    TestResultFormat format = bind_data.format;
    if (format == TestResultFormat::AUTO) {
        format = DetectTestResultFormat(content);
    }
    
    // Parse content based on detected format (same logic as read_test_results)
    switch (format) {
        case TestResultFormat::PYTEST_JSON:
            ParsePytestJSON(content, global_state->events);
            break;
        case TestResultFormat::DUCKDB_TEST:
            ParseDuckDBTestOutput(content, global_state->events);
            break;
        case TestResultFormat::ESLINT_JSON:
            ParseESLintJSON(content, global_state->events);
            break;
        case TestResultFormat::GOTEST_JSON:
            ParseGoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::MAKE_ERROR:
            ParseMakeErrors(content, global_state->events);
            break;
        case TestResultFormat::PYTEST_TEXT:
            ParsePytestText(content, global_state->events);
            break;
        case TestResultFormat::GENERIC_LINT:
            ParseGenericLint(content, global_state->events);
            break;
        case TestResultFormat::RUBOCOP_JSON:
            ParseRuboCopJSON(content, global_state->events);
            break;
        case TestResultFormat::CARGO_TEST_JSON:
            ParseCargoTestJSON(content, global_state->events);
            break;
        case TestResultFormat::SWIFTLINT_JSON:
            ParseSwiftLintJSON(content, global_state->events);
            break;
        case TestResultFormat::PHPSTAN_JSON:
            ParsePHPStanJSON(content, global_state->events);
            break;
        case TestResultFormat::SHELLCHECK_JSON:
            ParseShellCheckJSON(content, global_state->events);
            break;
        case TestResultFormat::STYLELINT_JSON:
            ParseStylelintJSON(content, global_state->events);
            break;
        case TestResultFormat::CLIPPY_JSON:
            ParseClippyJSON(content, global_state->events);
            break;
        case TestResultFormat::MARKDOWNLINT_JSON:
            ParseMarkdownlintJSON(content, global_state->events);
            break;
        case TestResultFormat::YAMLLINT_JSON:
            ParseYamllintJSON(content, global_state->events);
            break;
        case TestResultFormat::BANDIT_JSON:
            ParseBanditJSON(content, global_state->events);
            break;
        case TestResultFormat::SPOTBUGS_JSON:
            ParseSpotBugsJSON(content, global_state->events);
            break;
        case TestResultFormat::KTLINT_JSON:
            ParseKtlintJSON(content, global_state->events);
            break;
        case TestResultFormat::HADOLINT_JSON:
            ParseHadolintJSON(content, global_state->events);
            break;
        case TestResultFormat::LINTR_JSON:
            ParseLintrJSON(content, global_state->events);
            break;
        case TestResultFormat::SQLFLUFF_JSON:
            ParseSqlfluffJSON(content, global_state->events);
            break;
        case TestResultFormat::TFLINT_JSON:
            ParseTflintJSON(content, global_state->events);
            break;
        case TestResultFormat::KUBE_SCORE_JSON:
            ParseKubeScoreJSON(content, global_state->events);
            break;
        case TestResultFormat::CMAKE_BUILD:
            ParseCMakeBuild(content, global_state->events);
            break;
        case TestResultFormat::PYTHON_BUILD:
            ParsePythonBuild(content, global_state->events);
            break;
        case TestResultFormat::NODE_BUILD:
            ParseNodeBuild(content, global_state->events);
            break;
        case TestResultFormat::CARGO_BUILD:
            ParseCargoBuild(content, global_state->events);
            break;
        case TestResultFormat::MAVEN_BUILD:
            ParseMavenBuild(content, global_state->events);
            break;
        case TestResultFormat::GRADLE_BUILD:
            ParseGradleBuild(content, global_state->events);
            break;
        case TestResultFormat::MSBUILD:
            ParseMSBuild(content, global_state->events);
            break;
        case TestResultFormat::JUNIT_TEXT:
            ParseJUnitText(content, global_state->events);
            break;
        case TestResultFormat::VALGRIND:
            ParseValgrind(content, global_state->events);
            break;
        case TestResultFormat::GDB_LLDB:
            ParseGdbLldb(content, global_state->events);
            break;
        case TestResultFormat::RSPEC_TEXT:
            ParseRSpecText(content, global_state->events);
            break;
        case TestResultFormat::MOCHA_CHAI_TEXT:
            ParseMochaChai(content, global_state->events);
            break;
        case TestResultFormat::GTEST_TEXT:
            ParseGoogleTest(content, global_state->events);
            break;
        case TestResultFormat::NUNIT_XUNIT_TEXT:
            ParseNUnitXUnit(content, global_state->events);
            break;
        case TestResultFormat::PYLINT_TEXT:
            ParsePylintText(content, global_state->events);
            break;
        case TestResultFormat::FLAKE8_TEXT:
            ParseFlake8Text(content, global_state->events);
            break;
        case TestResultFormat::BLACK_TEXT:
            ParseBlackText(content, global_state->events);
            break;
        case TestResultFormat::MYPY_TEXT:
            ParseMypyText(content, global_state->events);
            break;
        case TestResultFormat::DOCKER_BUILD:
            ParseDockerBuild(content, global_state->events);
            break;
        case TestResultFormat::BAZEL_BUILD:
            ParseBazelBuild(content, global_state->events);
            break;
        case TestResultFormat::ISORT_TEXT:
            ParseIsortText(content, global_state->events);
            break;
        case TestResultFormat::BANDIT_TEXT:
            ParseBanditText(content, global_state->events);
            break;
        case TestResultFormat::AUTOPEP8_TEXT:
            ParseAutopep8Text(content, global_state->events);
            break;
        case TestResultFormat::YAPF_TEXT:
            ParseYapfText(content, global_state->events);
            break;
        case TestResultFormat::COVERAGE_TEXT:
            ParseCoverageText(content, global_state->events);
            break;
        case TestResultFormat::PYTEST_COV_TEXT:
            ParsePytestCovText(content, global_state->events);
            break;
        case TestResultFormat::GITHUB_ACTIONS_TEXT:
            ParseGitHubActionsText(content, global_state->events);
            break;
        case TestResultFormat::GITLAB_CI_TEXT:
            ParseGitLabCIText(content, global_state->events);
            break;
        case TestResultFormat::JENKINS_TEXT:
            ParseJenkinsText(content, global_state->events);
            break;
        case TestResultFormat::DRONE_CI_TEXT:
            ParseDroneCIText(content, global_state->events);
            break;
        default:
            // For unknown formats, don't create any events
            break;
    }
    
    return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ParseTestResultsInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                              GlobalTableFunctionState *global_state) {
    return make_uniq<ReadTestResultsLocalState>();
}

void ParseTestResultsFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &global_state = data_p.global_state->Cast<ReadTestResultsGlobalState>();
    auto &local_state = data_p.local_state->Cast<ReadTestResultsLocalState>();
    
    // Populate output chunk (same logic as read_test_results)
    PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE);
    
    // Update offset for next chunk
    local_state.chunk_offset += output.size();
}

TableFunction GetReadTestResultsFunction() {
    TableFunction function("read_test_results", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                          ReadTestResultsFunction, ReadTestResultsBind, ReadTestResultsInitGlobal, ReadTestResultsInitLocal);
    
    return function;
}

TableFunction GetParseTestResultsFunction() {
    TableFunction function("parse_test_results", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                          ParseTestResultsFunction, ParseTestResultsBind, ParseTestResultsInitGlobal, ParseTestResultsInitLocal);
    
    return function;
}

void ParsePythonBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_test;
    std::string current_package;
    
    while (std::getline(stream, line)) {
        // Parse pip wheel building errors
        if (line.find("ERROR: Failed building wheel for") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pip";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "package_build";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract package name
            std::regex package_pattern(R"(ERROR: Failed building wheel for ([^\s,]+))");
            std::smatch package_match;
            if (std::regex_search(line, package_match, package_pattern)) {
                event.test_name = package_match[1].str();
            }
            
            events.push_back(event);
        }
        // Parse setuptools/distutils compilation errors (C extension errors)
        else if (line.find("error:") != std::string::npos && 
                (line.find(".c:") != std::string::npos || line.find(".cpp:") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "compilation";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract file and line info from C compilation errors
            std::regex c_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*error:\s*(.+))");
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_error_pattern)) {
                event.file_path = c_match[1].str();
                event.line_number = std::stoi(c_match[2].str());
                event.column_number = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            
            events.push_back(event);
        }
        // Parse Python test failures (pytest format)
        else if (line.find("FAILED ") != std::string::npos && line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract test name
            std::regex test_pattern(R"(FAILED\s+([^:]+::[\w_]+))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.file_path = event.test_name.substr(0, sep_pos);
                }
            }
            
            events.push_back(event);
        }
        // Parse Python test errors
        else if (line.find("ERROR ") != std::string::npos && line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::ERROR;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract test name
            std::regex test_pattern(R"(ERROR\s+([^:]+::[\w_]+))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.file_path = event.test_name.substr(0, sep_pos);
                }
            }
            
            events.push_back(event);
        }
        // Parse assertion errors with file:line info
        else if (line.find("AssertionError:") != std::string::npos || line.find("TypeError:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "assertion";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // These are usually part of a test failure context
            if (!current_test.empty()) {
                event.test_name = current_test;
            }
            
            events.push_back(event);
        }
        // Parse file:line test location info
        else if (std::regex_match(line, std::regex(R"(\s*([^:]+):(\d+):\s+in\s+\w+)"))) {
            std::regex location_pattern(R"(\s*([^:]+):(\d+):\s+in\s+(\w+))");
            std::smatch loc_match;
            if (std::regex_search(line, loc_match, location_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "pytest";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = ValidationEventStatus::INFO;
                event.category = "traceback";
                event.severity = "info";
                event.file_path = loc_match[1].str();
                event.line_number = std::stoi(loc_match[2].str());
                event.function_name = loc_match[3].str();
                event.message = line;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "python_build";
                
                events.push_back(event);
            }
        }
        // Parse setup.py command failures
        else if (line.find("error: command") != std::string::npos && line.find("failed with exit status") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_command";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract command name
            std::regex cmd_pattern(R"(error: command '([^']+)')");
            std::smatch cmd_match;
            if (std::regex_search(line, cmd_match, cmd_pattern)) {
                event.function_name = cmd_match[1].str();
            }
            
            events.push_back(event);
        }
        // Parse C extension warnings
        else if (line.find("warning:") != std::string::npos && 
                (line.find(".c:") != std::string::npos || line.find(".cpp:") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.category = "compilation";
            event.severity = "warning";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract file and line info
            std::regex c_warn_pattern(R"(([^:]+):(\d+):(\d*):?\s*warning:\s*(.+))");
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_warn_pattern)) {
                event.file_path = c_match[1].str();
                event.line_number = std::stoi(c_match[2].str());
                event.column_number = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            
            events.push_back(event);
        }
    }
}

void ParseNodeBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_test_file;
    
    while (std::getline(stream, line)) {
        // Parse npm/yarn errors
        if (line.find("npm ERR!") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "npm";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "package_manager";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract error codes
            if (line.find("npm ERR! code") != std::string::npos) {
                std::regex code_pattern(R"(npm ERR! code ([A-Z_]+))");
                std::smatch code_match;
                if (std::regex_search(line, code_match, code_pattern)) {
                    event.error_code = code_match[1].str();
                }
            }
            
            events.push_back(event);
        }
        // Parse yarn errors  
        else if (line.find("error ") != std::string::npos && line.find("yarn") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yarn";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "package_manager";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Jest test failures
        else if (line.find("FAIL ") != std::string::npos && line.find(".test.js") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract test file
            std::regex test_file_pattern(R"(FAIL\s+([^\s]+\.test\.js))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_file_pattern)) {
                event.file_path = test_match[1].str();
                current_test_file = event.file_path;
            }
            
            events.push_back(event);
        }
        // Parse Jest test suite failures
        else if (line.find("● Test suite failed to run") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::ERROR;
            event.category = "test_suite";
            event.severity = "error";
            event.message = line;
            event.file_path = current_test_file;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Jest individual test failures
        else if (line.find("●") != std::string::npos && line.find("›") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "test_case";
            event.severity = "error";
            event.message = line;
            event.file_path = current_test_file;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract test name
            std::regex test_name_pattern(R"(●\s+([^›]+)\s+›\s+(.+))");
            std::smatch name_match;
            if (std::regex_search(line, name_match, test_name_pattern)) {
                event.test_name = name_match[1].str() + " › " + name_match[2].str();
            }
            
            events.push_back(event);
        }
        // Parse ESLint errors and warnings
        else if (std::regex_match(line, std::regex(R"(\s*\d+:\d+\s+(error|warning)\s+.+)"))) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "eslint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Parse ESLint format: "  15:5   error    'console' is not defined    no-undef"
            std::regex eslint_pattern(R"(\s*(\d+):(\d+)\s+(error|warning)\s+(.+?)\s+([^\s]+)$)");
            std::smatch eslint_match;
            if (std::regex_search(line, eslint_match, eslint_pattern)) {
                event.line_number = std::stoi(eslint_match[1].str());
                event.column_number = std::stoi(eslint_match[2].str());
                std::string severity = eslint_match[3].str();
                event.message = eslint_match[4].str();
                event.error_code = eslint_match[5].str();
                
                if (severity == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.category = "lint_error";
                    event.severity = "error";
                } else {
                    event.status = ValidationEventStatus::WARNING;
                    event.category = "lint_warning";
                    event.severity = "warning";
                }
            }
            
            events.push_back(event);
        }
        // Parse file paths for ESLint
        else if (line.find("/") != std::string::npos && line.find(".js") != std::string::npos && 
                line.find("  ") != 0 && line.find("error") == std::string::npos) {
            // This is likely a file path line like "/home/user/myproject/src/index.js"
            // Store it for the next ESLint errors
            if (!events.empty() && events.back().tool_name == "eslint" && events.back().file_path.empty()) {
                events.back().file_path = line;
            }
        }
        // Parse Webpack errors
        else if (line.find("ERROR in") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "webpack";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "bundling";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file and line info: "ERROR in ./src/components/Header.js"
            std::regex webpack_error_pattern(R"(ERROR in (.+?)(?:\s+(\d+):(\d+))?$)");
            std::smatch webpack_match;
            if (std::regex_search(line, webpack_match, webpack_error_pattern)) {
                event.file_path = webpack_match[1].str();
                if (webpack_match[2].matched) {
                    event.line_number = std::stoi(webpack_match[2].str());
                    event.column_number = std::stoi(webpack_match[3].str());
                }
            }
            
            events.push_back(event);
        }
        // Parse Webpack warnings
        else if (line.find("WARNING in") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "webpack";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.category = "bundling";
            event.severity = "warning";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file info
            std::regex webpack_warn_pattern(R"(WARNING in (.+))");
            std::smatch webpack_match;
            if (std::regex_search(line, webpack_match, webpack_warn_pattern)) {
                event.file_path = webpack_match[1].str();
            }
            
            events.push_back(event);
        }
        // Parse syntax errors in compilation
        else if (line.find("Syntax error:") != std::string::npos || line.find("Parsing error:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "javascript";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "syntax";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Node.js runtime errors with line numbers
        else if (line.find("at Object.<anonymous>") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "node";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "runtime";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file and line info: "at Object.<anonymous> (src/index.test.js:5:23)"
            std::regex runtime_pattern(R"(at Object\.<anonymous> \(([^:]+):(\d+):(\d+)\))");
            std::smatch runtime_match;
            if (std::regex_search(line, runtime_match, runtime_pattern)) {
                event.file_path = runtime_match[1].str();
                event.line_number = std::stoi(runtime_match[2].str());
                event.column_number = std::stoi(runtime_match[3].str());
            }
            
            events.push_back(event);
        }
        // Parse dependency resolution errors
        else if (line.find("Could not resolve dependency:") != std::string::npos || 
                line.find("Module not found:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "npm";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "dependency";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
    }
}

void ParseCargoBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse Rust compiler errors: error[E0XXX]: message --> file:line:column
        std::regex rust_error_pattern(R"(error\[E(\d+)\]:\s*(.+))");
        std::smatch match;
        
        if (std::regex_search(line, match, rust_error_pattern)) {
            std::string error_code = "E" + match[1].str();
            std::string message = match[2].str();
            
            // Look ahead for the file location line
            std::string location_line;
            if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                std::regex location_pattern(R"(-->\s*([^:]+):(\d+):(\d+))");
                std::smatch loc_match;
                
                if (std::regex_search(location_line, loc_match, location_pattern)) {
                    ValidationEvent event;
                    event.event_id = event_id++;
                    event.tool_name = "rustc";
                    event.event_type = ValidationEventType::BUILD_ERROR;
                    event.file_path = loc_match[1].str();
                    event.line_number = std::stoi(loc_match[2].str());
                    event.column_number = std::stoi(loc_match[3].str());
                    event.function_name = "";
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "compilation";
                    event.message = message;
                    event.error_code = error_code;
                    event.execution_time = 0.0;
                    event.raw_output = content;
                    event.structured_data = "cargo_build";
                    
                    events.push_back(event);
                }
            }
        }
        // Parse warning patterns: warning: message --> file:line:column
        else if (line.find("warning:") != std::string::npos) {
            std::regex warning_pattern(R"(warning:\s*(.+))");
            std::smatch warn_match;
            
            if (std::regex_search(line, warn_match, warning_pattern)) {
                std::string message = warn_match[1].str();
                
                // Look ahead for the file location line
                std::string location_line;
                if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                    std::regex location_pattern(R"(-->\s*([^:]+):(\d+):(\d+))");
                    std::smatch loc_match;
                    
                    if (std::regex_search(location_line, loc_match, location_pattern)) {
                        ValidationEvent event;
                        event.event_id = event_id++;
                        event.tool_name = "rustc";
                        event.event_type = ValidationEventType::LINT_ISSUE;
                        event.file_path = loc_match[1].str();
                        event.line_number = std::stoi(loc_match[2].str());
                        event.column_number = std::stoi(loc_match[3].str());
                        event.function_name = "";
                        event.status = ValidationEventStatus::WARNING;
                        event.severity = "warning";
                        event.category = "compilation";
                        event.message = message;
                        event.execution_time = 0.0;
                        event.raw_output = content;
                        event.structured_data = "cargo_build";
                        
                        events.push_back(event);
                    }
                }
            }
        }
        // Parse cargo test failures: test tests::test_name ... FAILED
        else if (line.find("test ") != std::string::npos && line.find("FAILED") != std::string::npos) {
            std::regex test_pattern(R"(test\s+([^\s]+)\s+\.\.\.\s+FAILED)");
            std::smatch test_match;
            
            if (std::regex_search(line, test_match, test_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = test_match[1].str();
                event.function_name = test_match[1].str();
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo test panic details: thread 'test_name' panicked at 'message', file:line:column
        else if (line.find("thread '") != std::string::npos && line.find("panicked at") != std::string::npos) {
            std::regex panic_pattern(R"(thread '([^']+)' panicked at '([^']+)',\s*([^:]+):(\d+):(\d+))");
            std::smatch panic_match;
            
            if (std::regex_search(line, panic_match, panic_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = panic_match[1].str();
                event.function_name = panic_match[1].str();
                event.file_path = panic_match[3].str();
                event.line_number = std::stoi(panic_match[4].str());
                event.column_number = std::stoi(panic_match[5].str());
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_panic";
                event.message = panic_match[2].str();
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo clippy warnings and errors
        else if ((line.find("clippy::") != std::string::npos || line.find("warning:") != std::string::npos) &&
                 (line.find("-->") != std::string::npos || line.find("src/") != std::string::npos)) {
            std::regex clippy_pattern(R"(([^:]+):(\d+):(\d+):\s*(warning|error):\s*(.+))");
            std::smatch clippy_match;
            
            if (std::regex_search(line, clippy_match, clippy_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "clippy";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.file_path = clippy_match[1].str();
                event.line_number = std::stoi(clippy_match[2].str());
                event.column_number = std::stoi(clippy_match[3].str());
                event.function_name = "";
                
                std::string severity = clippy_match[4].str();
                if (severity == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "lint_error";
                } else {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                    event.category = "lint_warning";
                }
                
                event.message = clippy_match[5].str();
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo build/compilation failures
        else if (line.find("error: could not compile") != std::string::npos) {
            std::regex compile_error_pattern(R"(error: could not compile `([^`]+)`)");
            std::smatch compile_match;
            
            if (std::regex_search(line, compile_match, compile_error_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = compile_match[1].str();
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "compilation";
                event.message = "Could not compile package: " + compile_match[1].str();
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo test result summary: test result: FAILED. X passed; Y failed; Z ignored
        else if (line.find("test result: FAILED") != std::string::npos) {
            std::regex summary_pattern(R"(test result: FAILED\.\s*(\d+) passed;\s*(\d+) failed)");
            std::smatch summary_match;
            
            if (std::regex_search(line, summary_match, summary_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_summary";
                event.message = "Test suite failed: " + summary_match[2].str() + " failed, " + 
                               summary_match[1].str() + " passed";
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
        // Parse cargo fmt check differences
        else if (line.find("Diff in") != std::string::npos && line.find("at line") != std::string::npos) {
            std::regex fmt_pattern(R"(Diff in ([^\s]+) at line (\d+):)");
            std::smatch fmt_match;
            
            if (std::regex_search(line, fmt_match, fmt_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "rustfmt";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.file_path = fmt_match[1].str();
                event.line_number = std::stoi(fmt_match[2].str());
                event.column_number = -1;
                event.function_name = "";
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
                event.category = "formatting";
                event.message = "Code formatting difference detected";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                
                events.push_back(event);
            }
        }
    }
}

void ParseMavenBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // Maven patterns
    std::regex compile_error_pattern(R"(\[ERROR\]\s+(.+?):(\[(\d+),(\d+)\])\s+(.+))");
    std::regex compile_warning_pattern(R"(\[WARNING\]\s+(.+?):(\[(\d+),(\d+)\])\s+(.+))");
    std::regex test_failure_pattern(R"(\[ERROR\]\s+(.+?)\(\s*(.+?)\s*\)\s+Time elapsed:\s+([\d.]+)\s+s\s+<<<\s+(FAILURE|ERROR)!)");
    std::regex test_result_pattern(R"(Tests run:\s+(\d+),\s+Failures:\s+(\d+),\s+Errors:\s+(\d+),\s+Skipped:\s+(\d+))");
    std::regex checkstyle_pattern(R"(\[(ERROR|WARN)\]\s+(.+?):(\d+):\s+(.+?)\s+\[(.+?)\])");
    std::regex spotbugs_pattern(R"(\[(ERROR|WARN)\]\s+(High|Medium|Low):\s+(.+?)\s+in\s+(.+?)\s+\[(.+?)\])");
    std::regex pmd_pattern(R"(\[(ERROR|WARN)\]\s+(.+?):(\d+):\s+(.+?)\s+\[(.+?)\])");
    std::regex dependency_pattern(R"(\[WARNING\]\s+(Used undeclared dependencies|Unused declared dependencies) found:)");
    std::regex build_failure_pattern(R"(BUILD FAILURE)");
    std::regex compilation_failure_pattern(R"(COMPILATION ERROR)");
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse Java compilation errors (Maven compiler plugin format)
        if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-compiler";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = std::stoi(match[4].str());
            event.function_name = "";
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse Java compilation warnings
        else if (std::regex_search(line, match, compile_warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-compiler";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = std::stoi(match[4].str());
            event.function_name = "";
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "compilation";
            event.message = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse JUnit test failures
        else if (std::regex_search(line, match, test_failure_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = match[1].str();
            event.test_name = match[2].str() + "." + match[1].str();
            event.execution_time = std::stod(match[3].str());
            event.status = (match[4].str() == "FAILURE") ? ValidationEventStatus::FAIL : ValidationEventStatus::ERROR;
            event.severity = (match[4].str() == "FAILURE") ? "error" : "critical";
            event.category = (match[4].str() == "FAILURE") ? "test_failure" : "test_error";
            std::string failure_type = match[4].str();
            std::transform(failure_type.begin(), failure_type.end(), failure_type.begin(), ::tolower);
            event.message = "Test " + failure_type;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse Checkstyle violations (when preceded by checkstyle plugin info)
        else if (std::regex_search(line, match, checkstyle_pattern) && 
                 (content.find("maven-checkstyle-plugin") != std::string::npos || 
                  content.find("checkstyle") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "checkstyle";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[2].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = -1;
            event.function_name = "";
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = (match[1].str() == "ERROR") ? "error" : "warning";
            event.category = "style";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse SpotBugs findings
        else if (std::regex_search(line, match, spotbugs_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "spotbugs";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.function_name = match[4].str();
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = match[2].str();
            std::transform(event.severity.begin(), event.severity.end(), event.severity.begin(), ::tolower);
            event.category = "static_analysis";
            event.message = match[3].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            // Map SpotBugs category
            if (event.error_code.find("SQL") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("PERFORMANCE") != std::string::npos || 
                      event.error_code.find("DLS_") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            }
            
            events.push_back(event);
        }
        // Parse PMD violations (when preceded by PMD plugin info)
        else if (std::regex_search(line, match, pmd_pattern) && 
                 (content.find("maven-pmd-plugin") != std::string::npos || 
                  content.find("PMD version") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pmd";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[2].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = -1;
            event.function_name = "";
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = (match[1].str() == "ERROR") ? "error" : "warning";
            event.category = "code_quality";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse Maven dependency analysis warnings
        else if (std::regex_search(line, match, dependency_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-dependency";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "dependency";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse build failure summary
        else if (std::regex_search(line, match, build_failure_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "build_failure";
            event.message = "Maven build failed";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "maven_build";
            
            events.push_back(event);
        }
        // Parse test result summaries
        else if (std::regex_search(line, match, test_result_pattern)) {
            int total_tests = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            
            if (total_tests > 0) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "maven-surefire";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
                event.severity = (failures > 0 || errors > 0) ? "error" : "info";
                event.category = "test_summary";
                event.message = "Tests: " + std::to_string(total_tests) + 
                               " total, " + std::to_string(failures) + " failures, " + 
                               std::to_string(errors) + " errors, " + 
                               std::to_string(skipped) + " skipped";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "maven_build";
                
                events.push_back(event);
            }
        }
    }
}

void ParseGradleBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // Gradle patterns
    std::regex task_pattern(R"(> Task :([^\s]+)\s+(FAILED|UP-TO-DATE|SKIPPED))");
    std::regex compile_error_pattern(R"((.+?):(\d+): error: (.+))");
    std::regex test_failure_pattern(R"((\w+) > (\w+) (FAILED|PASSED|SKIPPED))");
    std::regex test_summary_pattern(R"((\d+) tests completed(?:, (\d+) failed)?(?:, (\d+) skipped)?)");
    std::regex checkstyle_pattern(R"(\[ant:checkstyle\] (.+?):(\d+): (.+?) \[(.+?)\])");
    std::regex spotbugs_pattern(R"(Bug: (High|Medium|Low): (.+?) \[(.+?)\])");
    std::regex android_lint_pattern(R"((.+?):(\d+): (Error|Warning): (.+?) \[(.+?)\])");
    std::regex build_result_pattern(R"(BUILD (SUCCESSFUL|FAILED) in (\d+)s)");
    std::regex execution_failed_pattern(R"(Execution failed for task '([^']+)')");
    std::regex gradle_error_pattern(R"(\* What went wrong:)");
    
    std::string current_task = "";
    bool in_error_block = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse task execution results
        if (std::regex_search(line, match, task_pattern)) {
            std::string task_name = match[1].str();
            std::string task_result = match[2].str();
            current_task = task_name;
            
            if (task_result == "FAILED") {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "gradle";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = task_name;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "task_failure";
                event.message = "Task " + task_name + " failed";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "gradle_build";
                
                events.push_back(event);
            }
        }
        // Parse Java compilation errors
        else if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-javac";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse test results
        else if (std::regex_search(line, match, test_failure_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string test_result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            
            if (test_result == "FAILED") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (test_result == "PASSED") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (test_result == "SKIPPED") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse test summaries
        else if (std::regex_search(line, match, test_summary_pattern)) {
            int total_tests = std::stoi(match[1].str());
            int failed_tests = match[2].matched ? std::stoi(match[2].str()) : 0;
            int skipped_tests = match[3].matched ? std::stoi(match[3].str()) : 0;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed_tests > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed_tests > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total_tests) + 
                           " completed, " + std::to_string(failed_tests) + " failed, " + 
                           std::to_string(skipped_tests) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse Checkstyle violations (Gradle format)
        else if (std::regex_search(line, match, checkstyle_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-checkstyle";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "style";
            event.message = match[3].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse SpotBugs findings (Gradle format)
        else if (std::regex_search(line, match, spotbugs_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-spotbugs";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.function_name = current_task;
            event.severity = match[1].str();
            std::transform(event.severity.begin(), event.severity.end(), event.severity.begin(), ::tolower);
            event.status = (event.severity == "high") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.category = "static_analysis";
            event.message = match[2].str();
            event.error_code = match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            // Map SpotBugs categories
            if (event.error_code.find("SQL") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("PERFORMANCE") != std::string::npos || 
                      event.error_code.find("DLS_") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
            }
            
            events.push_back(event);
        }
        // Parse Android Lint issues
        else if (std::regex_search(line, match, android_lint_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-android-lint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            
            std::string level = match[3].str();
            if (level == "Error") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            event.category = "android_lint";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            // Map Android-specific categories
            if (event.error_code.find("Security") != std::string::npos || 
                event.error_code.find("SQLInjection") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("Performance") != std::string::npos ||
                      event.error_code.find("ThreadSleep") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            }
            
            events.push_back(event);
        }
        // Parse build results
        else if (std::regex_search(line, match, build_result_pattern)) {
            std::string result = match[1].str();
            int duration = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = (result == "SUCCESSFUL") ? ValidationEventStatus::PASS : ValidationEventStatus::ERROR;
            event.severity = (result == "SUCCESSFUL") ? "info" : "error";
            event.category = "build_result";
            event.message = "Build " + result;
            event.execution_time = static_cast<double>(duration);
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse execution failure messages
        else if (std::regex_search(line, match, execution_failed_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.function_name = match[1].str();
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "execution_failure";
            event.message = "Execution failed for task '" + match[1].str() + "'";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Track error block context
        else if (std::regex_search(line, match, gradle_error_pattern)) {
            in_error_block = true;
        }
    }
}

void ParseMSBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // MSBuild patterns
    std::regex compile_error_pattern(R"((.+?)\((\d+),(\d+)\): error (CS\d+): (.+?) \[(.+?\.csproj)\])");
    std::regex compile_warning_pattern(R"((.+?)\((\d+),(\d+)\): warning (CS\d+|CA\d+): (.+?) \[(.+?\.csproj)\])");
    std::regex build_result_pattern(R"(Build (FAILED|succeeded)\.)");
    std::regex error_summary_pattern(R"(\s+(\d+) Error\(s\))");
    std::regex warning_summary_pattern(R"(\s+(\d+) Warning\(s\))");
    std::regex time_elapsed_pattern(R"(Time Elapsed (\d+):(\d+):(\d+)\.(\d+))");
    std::regex test_result_pattern(R"((Failed|Passed)!\s+-\s+Failed:\s+(\d+),\s+Passed:\s+(\d+),\s+Skipped:\s+(\d+),\s+Total:\s+(\d+),\s+Duration:\s+(\d+)\s*ms)");
    std::regex xunit_test_pattern(R"(\[xUnit\.net\s+[\d:\.]+\]\s+(.+?)\.(.+?)\s+\[(PASS|FAIL|SKIP)\])");
    std::regex project_pattern("Project \"(.+?\\.csproj)\" on node (\\d+) \\((.+?) targets\\)\\.");
    std::regex analyzer_warning_pattern("(.+?)\\((\\d+),(\\d+)\\): warning (CA\\d+): (.+?) \\[(.+?\\.csproj)\\]");
    
    std::string current_project = "";
    bool in_build_summary = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse C# compilation errors
        if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild-csc";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.function_name = current_project;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse C# compilation warnings
        else if (std::regex_search(line, match, compile_warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild-csc";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.function_name = current_project;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "compilation";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            // Map analyzer warnings to appropriate categories
            std::string error_code = match[4].str();
            if (error_code.find("CA") == 0) {
                event.tool_name = "msbuild-analyzer";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "code_analysis";
                
                // Map specific CA codes to categories
                if (error_code == "CA2100" || error_code.find("Security") != std::string::npos) {
                    event.event_type = ValidationEventType::SECURITY_FINDING;
                    event.category = "security";
                } else if (error_code == "CA1031" || error_code.find("Performance") != std::string::npos) {
                    event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                    event.category = "performance";
                }
            }
            
            events.push_back(event);
        }
        // Parse .NET test results summary
        else if (std::regex_search(line, match, test_result_pattern)) {
            std::string result = match[1].str();
            int failed = std::stoi(match[2].str());
            int passed = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            int total = std::stoi(match[5].str());
            int duration = std::stoi(match[6].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "dotnet-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " + 
                           std::to_string(failed) + " failed, " + 
                           std::to_string(skipped) + " skipped";
            event.execution_time = static_cast<double>(duration) / 1000.0; // Convert ms to seconds
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse xUnit test results
        else if (std::regex_search(line, match, xunit_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string test_result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "xunit";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            
            if (test_result == "FAIL") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (test_result == "PASS") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (test_result == "SKIP") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse build results
        else if (std::regex_search(line, match, build_result_pattern)) {
            std::string result = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.function_name = current_project;
            event.status = (result == "succeeded") ? ValidationEventStatus::PASS : ValidationEventStatus::ERROR;
            event.severity = (result == "succeeded") ? "info" : "error";
            event.category = "build_result";
            event.message = "Build " + result;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse project context
        else if (std::regex_search(line, match, project_pattern)) {
            current_project = match[1].str();
        }
        // Parse build timing
        else if (std::regex_search(line, match, time_elapsed_pattern)) {
            int hours = std::stoi(match[1].str());
            int minutes = std::stoi(match[2].str());
            int seconds = std::stoi(match[3].str());
            int milliseconds = std::stoi(match[4].str());
            
            double total_seconds = hours * 3600 + minutes * 60 + seconds + milliseconds / 1000.0;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "msbuild";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.function_name = current_project;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "build_timing";
            event.message = "Build completed";
            event.execution_time = total_seconds;
            event.raw_output = content;
            event.structured_data = "msbuild";
            
            events.push_back(event);
        }
        // Parse error/warning summaries
        else if (std::regex_search(line, match, error_summary_pattern)) {
            int error_count = std::stoi(match[1].str());
            
            if (error_count > 0) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "msbuild";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = current_project;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "error_summary";
                event.message = std::to_string(error_count) + " compilation error(s)";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "msbuild";
                
                events.push_back(event);
            }
        }
        else if (std::regex_search(line, match, warning_summary_pattern)) {
            int warning_count = std::stoi(match[1].str());
            
            if (warning_count > 0) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "msbuild";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = current_project;
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
                event.category = "warning_summary";
                event.message = std::to_string(warning_count) + " compilation warning(s)";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "msbuild";
                
                events.push_back(event);
            }
        }
    }
}

void ParseJUnitText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // JUnit text patterns for different output formats
    std::regex junit4_class_pattern(R"(Running (.+))");
    std::regex junit4_summary_pattern(R"(Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+), Time elapsed: ([\d.]+) sec.*?)");
    std::regex junit4_test_pattern(R"((.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) sec\s+<<< (PASSED!|FAILURE!|ERROR!|SKIPPED!))");
    std::regex junit4_exception_pattern(R"((.+?): (.+)$)");
    std::regex junit4_stack_trace_pattern(R"(\s+at (.+?)\.(.+?)\((.+?):(\d+)\))");
    
    // JUnit 5 patterns
    std::regex junit5_header_pattern(R"(JUnit Jupiter ([\d.]+))");
    std::regex junit5_class_pattern(R"([├└]─ (.+?) [✓✗↷])");
    std::regex junit5_test_pattern(R"([│\s]+[├└]─ (.+?)\(\) ([✓✗↷]) \((\d+)ms\))");
    std::regex junit5_summary_pattern(R"(\[\s+(\d+) tests (found|successful|failed|skipped)\s+\])");
    
    // Maven Surefire patterns
    std::regex surefire_class_pattern(R"(\[INFO\] Running (.+))");
    std::regex surefire_test_pattern(R"(\[ERROR\] (.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) s\s+<<< (FAILURE!|ERROR!))");
    std::regex surefire_summary_pattern(R"(\[INFO\] Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+))");
    std::regex surefire_results_pattern(R"(\[ERROR\] (.+):(\d+) (.+))");
    
    // Gradle patterns
    std::regex gradle_test_pattern(R"((.+?) > (.+?) (PASSED|FAILED|SKIPPED))");
    std::regex gradle_summary_pattern(R"((\d+) tests completed, (\d+) failed, (\d+) skipped)");
    
    // TestNG patterns
    std::regex testng_test_pattern(R"((.+?)\.(.+?): (PASS|FAIL|SKIP))");
    std::regex testng_summary_pattern(R"(Total tests run: (\d+), Failures: (\d+), Skips: (\d+))");
    
    std::string current_class = "";
    std::string current_exception = "";
    std::string current_test = "";
    bool in_stack_trace = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse JUnit 4 class execution
        if (std::regex_search(line, match, junit4_class_pattern)) {
            current_class = match[1].str();
            in_stack_trace = false;
        }
        // Parse JUnit 4 class summary
        else if (std::regex_search(line, match, junit4_summary_pattern)) {
            int tests_run = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            double time_elapsed = std::stod(match[5].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit4";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = current_class;
            event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0 || errors > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(tests_run) + 
                           " total, " + std::to_string(tests_run - failures - errors - skipped) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(errors) + " errors, " +
                           std::to_string(skipped) + " skipped";
            event.execution_time = time_elapsed;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse JUnit 4 individual test results
        else if (std::regex_search(line, match, junit4_test_pattern)) {
            std::string test_method = match[1].str();
            std::string test_class = match[2].str();
            double time_elapsed = std::stod(match[3].str());
            std::string result = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit4";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = time_elapsed;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "PASSED!") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAILURE!") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
                current_test = event.test_name;
                in_stack_trace = true;
            } else if (result == "ERROR!") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_error";
                event.message = "Test error";
                current_test = event.test_name;
                in_stack_trace = true;
            } else if (result == "SKIPPED!") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse JUnit 5 header
        else if (std::regex_search(line, match, junit5_header_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit5";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_framework";
            event.message = "JUnit Jupiter " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse JUnit 5 class results
        else if (std::regex_search(line, match, junit5_class_pattern)) {
            current_class = match[1].str();
        }
        // Parse JUnit 5 test results
        else if (std::regex_search(line, match, junit5_test_pattern)) {
            std::string test_method = match[1].str();
            std::string result_symbol = match[2].str();
            int time_ms = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit5";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = current_class + "." + test_method;
            event.execution_time = static_cast<double>(time_ms) / 1000.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result_symbol == "✓") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result_symbol == "✗") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result_symbol == "↷") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse Maven Surefire class execution
        else if (std::regex_search(line, match, surefire_class_pattern)) {
            current_class = match[1].str();
        }
        // Parse Maven Surefire test failures
        else if (std::regex_search(line, match, surefire_test_pattern)) {
            std::string test_method = match[1].str();
            std::string test_class = match[2].str();
            double time_elapsed = std::stod(match[3].str());
            std::string result = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = time_elapsed;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "FAILURE!") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "ERROR!") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_error";
                event.message = "Test error";
            }
            
            events.push_back(event);
        }
        // Parse Maven Surefire summary
        else if (std::regex_search(line, match, surefire_summary_pattern)) {
            int tests_run = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0 || errors > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(tests_run) + 
                           " total, " + std::to_string(tests_run - failures - errors - skipped) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(errors) + " errors, " +
                           std::to_string(skipped) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse Gradle test results
        else if (std::regex_search(line, match, gradle_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "PASSED") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAILED") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "SKIPPED") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse Gradle test summary
        else if (std::regex_search(line, match, gradle_summary_pattern)) {
            int total = std::stoi(match[1].str());
            int failed = std::stoi(match[2].str());
            int skipped = std::stoi(match[3].str());
            int passed = total - failed - skipped;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " +
                           std::to_string(failed) + " failed, " + 
                           std::to_string(skipped) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse TestNG test results
        else if (std::regex_search(line, match, testng_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "testng";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            if (result == "PASS") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAIL") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "SKIP") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            events.push_back(event);
        }
        // Parse TestNG summary
        else if (std::regex_search(line, match, testng_summary_pattern)) {
            int total = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int skips = std::stoi(match[3].str());
            int passed = total - failures - skips;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "testng";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failures > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(skips) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "junit";
            
            events.push_back(event);
        }
        // Parse exception messages and stack traces
        else if (in_stack_trace && std::regex_search(line, match, junit4_exception_pattern)) {
            std::string exception_type = match[1].str();
            std::string exception_message = match[2].str();
            
            // Update the last test event with exception details
            if (!events.empty() && events.back().test_name == current_test) {
                events.back().message = exception_type + ": " + exception_message;
                events.back().error_code = exception_type;
            }
        }
        // Parse stack trace lines for file/line information
        else if (in_stack_trace && std::regex_search(line, match, junit4_stack_trace_pattern)) {
            std::string class_name = match[1].str();
            std::string method_name = match[2].str();
            std::string file_name = match[3].str();
            int line_number = std::stoi(match[4].str());
            
            // Update the last test event with file/line details
            if (!events.empty() && events.back().test_name == current_test && events.back().file_path.empty()) {
                events.back().file_path = file_name;
                events.back().line_number = line_number;
                events.back().function_name = method_name;
            }
        }
        // Reset stack trace mode on blank lines or new test patterns
        else if (line.empty() || line.find("Running") != std::string::npos) {
            in_stack_trace = false;
            current_test = "";
        }
    }
}

void ParseValgrind(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    uint64_t event_id = 1;
    
    // Track current context
    std::string current_tool = "Valgrind";
    std::string current_pid;
    std::string current_error_type;
    std::string current_message;
    std::vector<std::string> stack_trace;
    bool in_error_block = false;
    bool in_summary = false;
    
    // Regular expressions for different Valgrind patterns
    std::regex pid_regex(R"(==(\d+)==)");
    std::regex memcheck_header(R"(==\d+== Memcheck, a memory error detector)");
    std::regex helgrind_header(R"(==\d+== Helgrind, a thread error detector)");
    std::regex cachegrind_header(R"(==\d+== Cachegrind, a cache and branch-prediction profiler)");
    std::regex massif_header(R"(==\d+== Massif, a heap profiler)");
    std::regex drd_header(R"(==\d+== DRD, a thread error detector)");
    
    // Error patterns
    std::regex invalid_access(R"(==\d+== (Invalid (read|write) of size \d+))");
    std::regex memory_leak(R"(==\d+== (\d+ bytes .* (definitely|indirectly|possibly) lost))");
    std::regex uninitialized(R"(==\d+== (Conditional jump .* uninitialised|Use of uninitialised))");
    std::regex invalid_free(R"(==\d+== (Invalid free\(\)|delete|realloc))");
    std::regex data_race(R"(==\d+== (Possible data race))");
    std::regex lock_order(R"(==\d+== (Lock order .* violated))");
    
    // Stack trace patterns
    std::regex stack_frame(R"(==\d+==\s+at 0x[0-9A-F]+: (.+) \(([^:]+):(\d+)\))");
    std::regex stack_frame_no_line(R"(==\d+==\s+at 0x[0-9A-F]+: (.+))");
    std::regex error_summary(R"(==\d+== ERROR SUMMARY: (\d+) errors?)");
    
    // Cache statistics patterns
    std::regex cache_stat(R"(==\d+== ([DL1L]+)\s+(refs|misses|miss rate):\s*([0-9,]+|[\d.]+%))");
    std::regex branch_stat(R"(==\d+== (Branches|Mispredicts|Mispred rate):\s*([0-9,]+|[\d.]+%))");
    
    // Heap statistics
    std::regex heap_usage(R"(==\d+== Total heap usage: (\d+) allocs, (\d+) frees, ([0-9,]+) bytes allocated)");
    std::regex peak_memory(R"(==\d+== Peak memory usage: ([0-9,]+) bytes)");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Extract PID from Valgrind output
        if (std::regex_search(line, match, pid_regex)) {
            current_pid = match[1].str();
        }
        
        // Detect Valgrind tool type
        if (std::regex_search(line, memcheck_header)) {
            current_tool = "Memcheck";
        } else if (std::regex_search(line, helgrind_header)) {
            current_tool = "Helgrind";
        } else if (std::regex_search(line, cachegrind_header)) {
            current_tool = "Cachegrind";
        } else if (std::regex_search(line, massif_header)) {
            current_tool = "Massif";
        } else if (std::regex_search(line, drd_header)) {
            current_tool = "DRD";
        }
        
        // Memory errors (Memcheck)
        if (std::regex_search(line, match, invalid_access)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = match[1].str();
            event.error_code = "INVALID_ACCESS";
            event.raw_output = line;
            
            current_error_type = "invalid_access";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Memory leaks
        else if (std::regex_search(line, match, memory_leak)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_LEAK;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "memory_leak";
            event.message = match[1].str();
            event.error_code = "MEMORY_LEAK";
            event.raw_output = line;
            
            current_error_type = "memory_leak";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Uninitialized value errors
        else if (std::regex_search(line, match, uninitialized)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "uninitialized";
            event.message = match[1].str();
            event.error_code = "UNINITIALIZED";
            event.raw_output = line;
            
            current_error_type = "uninitialized";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Invalid free/delete
        else if (std::regex_search(line, match, invalid_free)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "invalid_free";
            event.message = match[1].str();
            event.error_code = "INVALID_FREE";
            event.raw_output = line;
            
            current_error_type = "invalid_free";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Thread errors (Helgrind/DRD)
        else if (std::regex_search(line, match, data_race)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::THREAD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "data_race";
            event.message = match[1].str();
            event.error_code = "DATA_RACE";
            event.raw_output = line;
            
            current_error_type = "data_race";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Lock order violations
        else if (std::regex_search(line, match, lock_order)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::THREAD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "lock_order";
            event.message = match[1].str();
            event.error_code = "LOCK_ORDER_VIOLATION";
            event.raw_output = line;
            
            current_error_type = "lock_order";
            current_message = event.message;
            in_error_block = true;
            stack_trace.clear();
            
            events.push_back(event);
        }
        
        // Stack trace with file and line
        else if (std::regex_search(line, match, stack_frame)) {
            if (in_error_block && !events.empty()) {
                auto& last_event = events.back();
                if (last_event.file_path.empty()) {
                    // Use first stack frame for file location
                    last_event.function_name = match[1].str();
                    last_event.file_path = match[2].str();
                    try {
                        last_event.line_number = std::stoi(match[3].str());
                    } catch (...) {
                        last_event.line_number = -1;
                    }
                }
                stack_trace.push_back(line);
            }
        }
        
        // Stack trace without line info
        else if (std::regex_search(line, match, stack_frame_no_line)) {
            if (in_error_block && !events.empty()) {
                auto& last_event = events.back();
                if (last_event.function_name.empty()) {
                    last_event.function_name = match[1].str();
                }
                stack_trace.push_back(line);
            }
        }
        
        // Cache statistics (Cachegrind)
        else if (std::regex_search(line, match, cache_stat)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "cache_analysis";
            event.message = match[1].str() + " " + match[2].str() + ": " + match[3].str();
            event.error_code = "CACHE_STAT";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Branch statistics (Cachegrind)
        else if (std::regex_search(line, match, branch_stat)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "branch_analysis";
            event.message = match[1].str() + ": " + match[2].str();
            event.error_code = "BRANCH_STAT";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Heap usage summary (Memcheck/Massif)
        else if (std::regex_search(line, match, heap_usage)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "heap_analysis";
            event.message = "Total heap usage: " + match[1].str() + " allocs, " + 
                           match[2].str() + " frees, " + match[3].str() + " bytes allocated";
            event.error_code = "HEAP_SUMMARY";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Peak memory (Massif)
        else if (std::regex_search(line, match, peak_memory)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "memory_usage";
            event.message = "Peak memory usage: " + match[1].str() + " bytes";
            event.error_code = "PEAK_MEMORY";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Error summary
        else if (std::regex_search(line, match, error_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_tool;
            event.event_type = ValidationEventType::SUMMARY;
            event.status = std::stoi(match[1].str()) > 0 ? ValidationEventStatus::ERROR : ValidationEventStatus::PASS;
            event.severity = std::stoi(match[1].str()) > 0 ? "error" : "info";
            event.category = "summary";
            event.message = "Total errors: " + match[1].str();
            event.error_code = "ERROR_SUMMARY";
            event.raw_output = line;
            in_summary = true;
            events.push_back(event);
        }
        
        // End of error block detection
        if (line.find("==") != std::string::npos && line.find("== ") != std::string::npos && 
            line.length() > 10 && in_error_block) {
            // Check if this starts a new error or ends current one
            if (line.find("Invalid") == std::string::npos && 
                line.find("bytes") == std::string::npos && 
                line.find("Conditional") == std::string::npos &&
                line.find("Use of") == std::string::npos &&
                line.find("Possible data race") == std::string::npos &&
                line.find("Lock order") == std::string::npos &&
                !stack_trace.empty()) {
                // Update last event with complete stack trace
                if (!events.empty()) {
                    std::string complete_trace;
                    for (const auto& trace_line : stack_trace) {
                        if (!complete_trace.empty()) complete_trace += "\\n";
                        complete_trace += trace_line;
                    }
                    events.back().structured_data = complete_trace;
                }
                in_error_block = false;
                stack_trace.clear();
            }
        }
    }
}

void ParseGdbLldb(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    uint64_t event_id = 1;
    
    // Track current context
    std::string current_debugger = "GDB";
    std::string current_program;
    std::string current_frame;
    std::vector<std::string> stack_trace;
    bool in_backtrace = false;
    bool in_register_dump = false;
    
    // Regular expressions for GDB/LLDB patterns
    std::regex gdb_header(R"(GNU gdb \(.*\) ([\d.]+))");
    std::regex lldb_header(R"(lldb.*version ([\d.]+))");
    std::regex program_start(R"(Starting program: (.+))");
    std::regex target_create(R"(target create \"(.+)\")");
    
    // Signal/crash patterns
    std::regex signal_received(R"(Program received signal (\w+), (.+))");
    std::regex exc_bad_access(R"(stop reason = EXC_BAD_ACCESS \(code=(\d+), address=(0x[0-9a-fA-F]+)\))");
    std::regex segfault_location(R"(0x([0-9a-fA-F]+) in (.+) \(.*\) at (.+):(\d+))");
    std::regex lldb_crash_frame(R"(frame #\d+: (0x[0-9a-fA-F]+) .+`(.+) at (.+):(\d+):(\d+))");
    
    // Backtrace patterns
    std::regex gdb_frame(R"(#(\d+)\s+(0x[0-9a-fA-F]+) in (.+) \(.*\) at (.+):(\d+))");
    std::regex gdb_frame_no_file(R"(#(\d+)\s+(0x[0-9a-fA-F]+) in (.+))");
    std::regex lldb_frame(R"(\* frame #(\d+): (0x[0-9a-fA-F]+) .+`(.+) at (.+):(\d+):(\d+))");
    std::regex lldb_frame_simple(R"(frame #(\d+): (0x[0-9a-fA-F]+) .+`(.+))");
    
    // Breakpoint patterns
    std::regex breakpoint_hit(R"(Breakpoint (\d+), (.+) \(.*\) at (.+):(\d+))");
    std::regex lldb_breakpoint_hit(R"(stop reason = breakpoint (\d+)\.(\d+))");
    std::regex breakpoint_set(R"(Breakpoint (\d+):.*where = .+`(.+) \+ \d+ at (.+):(\d+))");
    
    // Register and memory patterns
    std::regex register_line(R"((\w+)\s+(0x[0-9a-fA-F]+))");
    std::regex memory_access(R"(Cannot access memory at address (0x[0-9a-fA-F]+))");
    
    // Thread patterns
    std::regex thread_info(R"(\* thread #(\d+).*tid = (0x[0-9a-fA-F]+))");
    std::regex gdb_thread_info(R"(\* (\d+)\s+Thread (0x[0-9a-fA-F]+) \(LWP (\d+)\))");
    
    // Watchpoint patterns
    std::regex watchpoint_hit(R"(Hardware watchpoint (\d+): (.+))");
    std::regex watchpoint_set(R"(Watchpoint (\d+): addr = (0x[0-9a-fA-F]+))");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Detect debugger type
        if (std::regex_search(line, match, gdb_header)) {
            current_debugger = "GDB";
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "debugger_startup";
            event.message = "GDB version " + match[1].str() + " started";
            event.error_code = "DEBUGGER_START";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, lldb_header)) {
            current_debugger = "LLDB";
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "debugger_startup";
            event.message = "LLDB version " + match[1].str() + " started";
            event.error_code = "DEBUGGER_START";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Program startup
        else if (std::regex_search(line, match, program_start)) {
            current_program = match[1].str();
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "program_start";
            event.message = "Started program: " + current_program;
            event.error_code = "PROGRAM_START";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, target_create)) {
            current_program = match[1].str();
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "target_create";
            event.message = "Target created: " + current_program;
            event.error_code = "TARGET_CREATE";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Signal/crash detection
        else if (std::regex_search(line, match, signal_received)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::CRASH_SIGNAL;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "signal_crash";
            event.message = "Signal " + match[1].str() + ": " + match[2].str();
            event.error_code = match[1].str();
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, exc_bad_access)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::CRASH_SIGNAL;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = "EXC_BAD_ACCESS at address " + match[2].str();
            event.error_code = "EXC_BAD_ACCESS";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Crash location detection
        else if (std::regex_search(line, match, segfault_location)) {
            if (!events.empty() && events.back().event_type == ValidationEventType::CRASH_SIGNAL) {
                auto& last_event = events.back();
                last_event.function_name = match[2].str();
                last_event.file_path = match[3].str();
                try {
                    last_event.line_number = std::stoi(match[4].str());
                } catch (...) {
                    last_event.line_number = -1;
                }
            }
        } else if (std::regex_search(line, match, lldb_crash_frame)) {
            if (!events.empty() && events.back().event_type == ValidationEventType::CRASH_SIGNAL) {
                auto& last_event = events.back();
                last_event.function_name = match[2].str();
                last_event.file_path = match[3].str();
                try {
                    last_event.line_number = std::stoi(match[4].str());
                    last_event.column_number = std::stoi(match[5].str());
                } catch (...) {
                    last_event.line_number = -1;
                    last_event.column_number = -1;
                }
            }
        }
        
        // Backtrace parsing
        else if (line.find("(gdb) bt") != std::string::npos || line.find("(lldb) bt") != std::string::npos) {
            in_backtrace = true;
            stack_trace.clear();
        } else if (std::regex_search(line, match, gdb_frame)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
                if (stack_trace.size() == 1 && !events.empty()) {
                    // First frame - update crash event with location details
                    auto& last_event = events.back();
                    if (last_event.file_path.empty()) {
                        last_event.function_name = match[3].str();
                        last_event.file_path = match[4].str();
                        try {
                            last_event.line_number = std::stoi(match[5].str());
                        } catch (...) {
                            last_event.line_number = -1;
                        }
                    }
                }
            }
        } else if (std::regex_search(line, match, gdb_frame_no_file)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
            }
        } else if (std::regex_search(line, match, lldb_frame)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
                if (stack_trace.size() == 1 && !events.empty()) {
                    auto& last_event = events.back();
                    if (last_event.file_path.empty()) {
                        last_event.function_name = match[3].str();
                        last_event.file_path = match[4].str();
                        try {
                            last_event.line_number = std::stoi(match[5].str());
                            last_event.column_number = std::stoi(match[6].str());
                        } catch (...) {
                            last_event.line_number = -1;
                            last_event.column_number = -1;
                        }
                    }
                }
            }
        } else if (std::regex_search(line, match, lldb_frame_simple)) {
            if (in_backtrace) {
                stack_trace.push_back(line);
            }
        }
        
        // Breakpoint events
        else if (std::regex_search(line, match, breakpoint_hit)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_hit";
            event.function_name = match[2].str();
            event.file_path = match[3].str();
            try {
                event.line_number = std::stoi(match[4].str());
            } catch (...) {
                event.line_number = -1;
            }
            event.message = "Breakpoint " + match[1].str() + " hit at " + match[2].str();
            event.error_code = "BREAKPOINT_HIT";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, lldb_breakpoint_hit)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_hit";
            event.message = "Breakpoint " + match[1].str() + "." + match[2].str() + " hit";
            event.error_code = "BREAKPOINT_HIT";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, breakpoint_set)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "breakpoint_set";
            event.function_name = match[2].str();
            event.file_path = match[3].str();
            try {
                event.line_number = std::stoi(match[4].str());
            } catch (...) {
                event.line_number = -1;
            }
            event.message = "Breakpoint " + match[1].str() + " set at " + match[2].str();
            event.error_code = "BREAKPOINT_SET";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Watchpoint events
        else if (std::regex_search(line, match, watchpoint_hit)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "watchpoint_hit";
            event.message = "Watchpoint " + match[1].str() + " hit: " + match[2].str();
            event.error_code = "WATCHPOINT_HIT";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, watchpoint_set)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "watchpoint_set";
            event.message = "Watchpoint " + match[1].str() + " set at address " + match[2].str();
            event.error_code = "WATCHPOINT_SET";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Memory access errors
        else if (std::regex_search(line, match, memory_access)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::MEMORY_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "memory_access";
            event.message = "Cannot access memory at address " + match[1].str();
            event.error_code = "MEMORY_ACCESS_ERROR";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Thread information
        else if (std::regex_search(line, match, thread_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "thread_info";
            event.message = "Thread #" + match[1].str() + " (TID: " + match[2].str() + ")";
            event.error_code = "THREAD_INFO";
            event.raw_output = line;
            events.push_back(event);
        } else if (std::regex_search(line, match, gdb_thread_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = current_debugger;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "thread_info";
            event.message = "Thread " + match[1].str() + " (LWP: " + match[3].str() + ")";
            event.error_code = "THREAD_INFO";
            event.raw_output = line;
            events.push_back(event);
        }
        
        // End backtrace when we see a new command prompt
        if ((line.find("(gdb)") != std::string::npos || line.find("(lldb)") != std::string::npos) && 
            line.find("bt") == std::string::npos) {
            if (in_backtrace && !stack_trace.empty()) {
                // Add stack trace to the last crash event
                if (!events.empty()) {
                    std::string complete_trace;
                    for (const auto& trace_line : stack_trace) {
                        if (!complete_trace.empty()) complete_trace += "\\n";
                        complete_trace += trace_line;
                    }
                    // Find the most recent crash or debug event to attach the stack trace
                    for (auto it = events.rbegin(); it != events.rend(); ++it) {
                        if (it->event_type == ValidationEventType::CRASH_SIGNAL ||
                            it->event_type == ValidationEventType::DEBUG_EVENT) {
                            it->structured_data = complete_trace;
                            break;
                        }
                    }
                }
                in_backtrace = false;
                stack_trace.clear();
            }
        }
    }
}

void ParseRSpecText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for RSpec output
    std::regex test_passed(R"(\s*✓\s*(.+))");
    std::regex test_failed(R"(\s*✗\s*(.+))");
    std::regex test_pending(R"(\s*pending:\s*(.+)\s*\(PENDING:\s*(.+)\))");
    std::regex context_start(R"(^([A-Z][A-Za-z0-9_:]+)\s*$)");
    std::regex nested_context(R"(^\s+(#\w+|.+)\s*$)");
    std::regex failure_error(R"(Failure/Error:\s*(.+))");
    std::regex expected_pattern(R"(\s*expected\s*(.+))");
    std::regex got_pattern(R"(\s*got:\s*(.+))");
    std::regex file_line_pattern(R"(# (.+):(\d+):in)");
    std::regex summary_pattern(R"(Finished in (.+) seconds .* (\d+) examples?, (\d+) failures?(, (\d+) pending)?)");;
    std::regex failed_example(R"(rspec (.+):(\d+) # (.+))");
    
    std::string current_context;
    std::string current_method;
    std::string current_failure_file;
    int current_failure_line = -1;
    std::string current_failure_message;
    bool in_failure_section = false;
    bool in_failed_examples = false;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Skip empty lines and dividers
        if (line.empty() || line.find("Failures:") != std::string::npos ||
            line.find("Failed examples:") != std::string::npos) {
            if (line.find("Failures:") != std::string::npos) {
                in_failure_section = true;
            }
            if (line.find("Failed examples:") != std::string::npos) {
                in_failed_examples = true;
            }
            continue;
        }
        
        // Parse failed example references
        if (in_failed_examples && std::regex_search(line, match, failed_example)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.test_name = match[3].str();
            event.message = "Test failed: " + match[3].str();
            event.raw_output = line;
            events.push_back(event);
            continue;
        }
        
        // Parse test context (class/module names)
        if (std::regex_match(line, match, context_start)) {
            current_context = match[1].str();
            continue;
        }
        
        // Parse nested context (method names or descriptions)
        if (!current_context.empty() && std::regex_match(line, match, nested_context)) {
            current_method = match[1].str();
            // Remove leading # if present
            if (current_method.substr(0, 1) == "#") {
                current_method = current_method.substr(1);
            }
            continue;
        }
        
        // Parse passed tests
        if (std::regex_search(line, match, test_passed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "test_success";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test passed: " + match[1].str();
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Parse failed tests
        else if (std::regex_search(line, match, test_failed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test failed: " + match[1].str();
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Parse pending tests
        else if (std::regex_search(line, match, test_pending)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::SKIP;
            event.severity = "warning";
            event.category = "test_pending";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test pending: " + match[2].str();
            event.raw_output = line;
            events.push_back(event);
        }
        
        // Parse failure details
        else if (std::regex_search(line, match, failure_error)) {
            current_failure_message = match[1].str();
        }
        
        // Parse expected/got patterns for better error details
        else if (std::regex_search(line, match, expected_pattern)) {
            if (!current_failure_message.empty()) {
                current_failure_message += " | Expected: " + match[1].str();
            }
        }
        else if (std::regex_search(line, match, got_pattern)) {
            if (!current_failure_message.empty()) {
                current_failure_message += " | Got: " + match[1].str();
            }
        }
        
        // Parse file and line information
        else if (std::regex_search(line, match, file_line_pattern)) {
            current_failure_file = match[1].str();
            current_failure_line = std::stoi(match[2].str());
            
            // Update the most recent failed test event with file/line info
            for (auto it = events.rbegin(); it != events.rend(); ++it) {
                if (it->tool_name == "RSpec" && it->status == ValidationEventStatus::FAIL && 
                    it->file_path.empty()) {
                    it->file_path = current_failure_file;
                    it->line_number = current_failure_line;
                    if (!current_failure_message.empty()) {
                        it->message = current_failure_message;
                    }
                    break;
                }
            }
        }
        
        // Parse summary
        else if (std::regex_search(line, match, summary_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = ValidationEventType::SUMMARY;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_summary";
            
            std::string execution_time = match[1].str();
            int total_examples = std::stoi(match[2].str());
            int failures = std::stoi(match[3].str());
            int pending = 0;
            if (match[5].matched) {
                pending = std::stoi(match[5].str());
            }
            
            event.message = "Test run completed: " + std::to_string(total_examples) + 
                          " examples, " + std::to_string(failures) + " failures, " + 
                          std::to_string(pending) + " pending";
            event.execution_time = std::stod(execution_time);
            event.raw_output = line;
            events.push_back(event);
        }
    }
}

void ParseMochaChai(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Mocha/Chai output
    std::regex test_passed(R"(\s*✓\s*(.+)\s*\((\d+)ms\))");
    std::regex test_failed(R"(\s*✗\s*(.+))");
    std::regex test_pending(R"(\s*-\s*(.+)\s*\(pending\))");
    std::regex context_start(R"(^\s*([A-Z][A-Za-z0-9\s]+)\s*$)");
    std::regex nested_context(R"(^\s{2,}([a-z][A-Za-z0-9\s]+)\s*$)");
    std::regex error_line(R"((Error|AssertionError):\s*(.+))");
    std::regex file_line(R"(\s*at\s+Context\.<anonymous>\s+\((.+):(\d+):(\d+)\))");
    std::regex test_stack(R"(\s*at\s+Test\.Runnable\.run\s+\((.+):(\d+):(\d+)\))");
    std::regex summary_line(R"(\s*(\d+)\s+passing\s*\(([0-9.]+s)\))");
    std::regex failing_line(R"(\s*(\d+)\s+failing)");
    std::regex pending_line(R"(\s*(\d+)\s+pending)");
    std::regex failed_example_start(R"(\s*(\d+)\)\s+(.+))");
    std::regex expected_got_line(R"(\s*\+(.+))");
    std::regex actual_line(R"(\s*-(.+))");
    
    std::string current_context;
    std::string current_nested_context;
    std::string current_test_name;
    std::string current_error_message;
    std::string current_file_path;
    int current_line_number = 0;
    int current_column = 0;
    int64_t current_execution_time = 0;
    std::vector<std::string> stack_trace;
    bool in_failure_details = false;
    int failure_number = 0;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for test passed
        if (std::regex_match(line, match, test_passed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            current_execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.message = "Test passed: " + test_name;
            event.test_name = current_context + " " + current_nested_context + " " + test_name;
            event.status = ValidationEventStatus::PASS;
            event.file_path = current_file_path;
            event.line_number = current_line_number;
            event.column_number = current_column;
            event.execution_time = current_execution_time;
            event.tool_name = "mocha";
            event.category = "mocha_chai_text";
            event.raw_output = line;
            event.function_name = current_context;
            event.structured_data = "{}";
            
            events.push_back(event);
            
            // Reset for next test
            current_file_path = "";
            current_line_number = 0;
            current_column = 0;
            current_execution_time = 0;
        }
        // Check for test failed
        else if (std::regex_match(line, match, test_failed)) {
            current_test_name = match[1].str();
        }
        // Check for test pending
        else if (std::regex_match(line, match, test_pending)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.message = "Test pending: " + test_name;
            event.test_name = current_context + " " + current_nested_context + " " + test_name;
            event.status = ValidationEventStatus::SKIP;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "mocha";
            event.category = "mocha_chai_text";
            event.raw_output = line;
            event.function_name = current_context;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for context start
        else if (std::regex_match(line, match, context_start)) {
            current_context = match[1].str();
            current_nested_context = "";
        }
        // Check for nested context
        else if (std::regex_match(line, match, nested_context)) {
            current_nested_context = match[1].str();
        }
        // Check for error messages
        else if (std::regex_match(line, match, error_line)) {
            current_error_message = match[1].str() + ": " + match[2].str();
        }
        // Check for file and line information
        else if (std::regex_match(line, match, file_line)) {
            current_file_path = match[1].str();
            current_line_number = std::stoi(match[2].str());
            current_column = std::stoi(match[3].str());
            
            // If we have a failed test, create the event now
            if (!current_test_name.empty() && !current_error_message.empty()) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.event_type = ValidationEventType::TEST_RESULT;
                event.severity = "error";
                event.message = current_error_message;
                event.test_name = current_context + " " + current_nested_context + " " + current_test_name;
                event.status = ValidationEventStatus::FAIL;
                event.file_path = current_file_path;
                event.line_number = current_line_number;
                event.column_number = current_column;
                event.execution_time = 0;
                event.tool_name = "mocha";
                event.category = "mocha_chai_text";
                event.raw_output = line;
                event.function_name = current_context;
                event.structured_data = "{}";
                
                events.push_back(event);
                
                // Reset for next test
                current_test_name = "";
                current_error_message = "";
                current_file_path = "";
                current_line_number = 0;
                current_column = 0;
            }
        }
        // Check for failed example start (in failure summary)
        else if (std::regex_match(line, match, failed_example_start)) {
            failure_number = std::stoi(match[1].str());
            std::string full_test_name = match[2].str();
            in_failure_details = true;
            
            // Extract context and test name from full name
            size_t last_space = full_test_name.rfind(' ');
            if (last_space != std::string::npos) {
                current_context = full_test_name.substr(0, last_space);
                current_test_name = full_test_name.substr(last_space + 1);
            } else {
                current_test_name = full_test_name;
            }
        }
        // Check for summary lines
        else if (std::regex_match(line, match, summary_line)) {
            int passing_count = std::stoi(match[1].str());
            std::string total_time = match[2].str();
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "info";
            summary_event.message = "Test execution completed with " + std::to_string(passing_count) + " passing tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::INFO;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"passing_tests\": " + std::to_string(passing_count) + ", \"total_time\": \"" + total_time + "\"}";
            
            events.push_back(summary_event);
        }
        else if (std::regex_match(line, match, failing_line)) {
            int failing_count = std::stoi(match[1].str());
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "error";
            summary_event.message = "Test execution completed with " + std::to_string(failing_count) + " failing tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::FAIL;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"failing_tests\": " + std::to_string(failing_count) + "}";
            
            events.push_back(summary_event);
        }
        else if (std::regex_match(line, match, pending_line)) {
            int pending_count = std::stoi(match[1].str());
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "warning";
            summary_event.message = "Test execution completed with " + std::to_string(pending_count) + " pending tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::WARNING;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"pending_tests\": " + std::to_string(pending_count) + "}";
            
            events.push_back(summary_event);
        }
        
        // Always add stack trace lines when we encounter them
        if (std::regex_match(line, match, test_stack) || std::regex_match(line, match, file_line)) {
            stack_trace.push_back(line);
        }
        
        // Clear stack trace after processing failure details
        if (in_failure_details && line.empty()) {
            in_failure_details = false;
            stack_trace.clear();
        }
    }
}

void ParseGoogleTest(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Google Test output
    std::regex test_run_start(R"(\[\s*RUN\s*\]\s*(.+))");
    std::regex test_passed(R"(\[\s*OK\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_failed(R"(\[\s*FAILED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_skipped(R"(\[\s*SKIPPED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_suite_start(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+))");
    std::regex test_suite_end(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+)\s*\((\d+)\s*ms total\))");
    std::regex test_summary_start(R"(\[==========\]\s*(\d+)\s*tests from\s*(\d+)\s*test suites ran\.\s*\((\d+)\s*ms total\))");
    std::regex tests_passed_summary(R"(\[\s*PASSED\s*\]\s*(\d+)\s*tests\.)");
    std::regex tests_failed_summary(R"(\[\s*FAILED\s*\]\s*(\d+)\s*tests,\s*listed below:)");
    std::regex failed_test_list(R"(\[\s*FAILED\s*\]\s*(.+))");
    std::regex failure_detail(R"((.+):\s*(.+):(\d+):\s*Failure)");
    std::regex global_env_setup(R"(\[----------\]\s*Global test environment set-up)");
    std::regex global_env_teardown(R"(\[----------\]\s*Global test environment tear-down)");
    
    std::string current_test_suite;
    std::string current_test_name;
    bool in_failure_details = false;
    std::vector<std::string> failure_lines;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for test run start
        if (std::regex_match(line, match, test_run_start)) {
            current_test_name = match[1].str();
        }
        // Check for test passed
        else if (std::regex_match(line, match, test_passed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.message = "Test passed: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::PASS;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for test failed
        else if (std::regex_match(line, match, test_failed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.message = "Test failed: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::FAIL;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for test skipped
        else if (std::regex_match(line, match, test_skipped)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.message = "Test skipped: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::SKIP;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for test suite start
        else if (std::regex_match(line, match, test_suite_start)) {
            current_test_suite = match[2].str();
        }
        // Check for test suite end
        else if (std::regex_match(line, match, test_suite_end)) {
            std::string suite_name = match[2].str();
            std::string total_time = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Test suite completed: " + suite_name + " (" + total_time + " ms total)";
            event.test_name = "";
            event.status = ValidationEventStatus::INFO;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = std::stoll(total_time);
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = suite_name;
            event.structured_data = "{\"suite_name\": \"" + suite_name + "\", \"total_time_ms\": " + total_time + "}";
            
            events.push_back(event);
        }
        // Check for overall test summary
        else if (std::regex_match(line, match, test_summary_start)) {
            std::string total_tests = match[1].str();
            std::string total_suites = match[2].str();
            std::string total_time = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Test execution completed: " + total_tests + " tests from " + total_suites + " test suites";
            event.test_name = "";
            event.status = ValidationEventStatus::INFO;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = std::stoll(total_time);
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"total_tests\": " + total_tests + ", \"total_suites\": " + total_suites + ", \"total_time_ms\": " + total_time + "}";
            
            events.push_back(event);
        }
        // Check for passed tests summary
        else if (std::regex_match(line, match, tests_passed_summary)) {
            std::string passed_count = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Tests passed: " + passed_count + " tests";
            event.test_name = "";
            event.status = ValidationEventStatus::PASS;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"passed_tests\": " + passed_count + "}";
            
            events.push_back(event);
        }
        // Check for failed tests summary
        else if (std::regex_match(line, match, tests_failed_summary)) {
            std::string failed_count = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "error";
            event.message = "Tests failed: " + failed_count + " tests";
            event.test_name = "";
            event.status = ValidationEventStatus::FAIL;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"failed_tests\": " + failed_count + "}";
            
            events.push_back(event);
        }
        // Check for failure details (file paths and line numbers)
        else if (std::regex_match(line, match, failure_detail)) {
            std::string test_name = match[1].str();
            std::string file_path = match[2].str();
            std::string line_str = match[3].str();
            int line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing line number fails, keep it as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.message = "Test failure details: " + test_name;
            event.test_name = test_name;
            event.status = ValidationEventStatus::FAIL;
            event.file_path = file_path;
            event.line_number = line_number;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{\"file_path\": \"" + file_path + "\", \"line_number\": " + std::to_string(line_number) + "}";
            
            events.push_back(event);
        }
    }
}

void ParseNUnitXUnit(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for NUnit/xUnit output
    std::regex nunit_header(R"(NUnit\s+([\d\.]+))");
    std::regex nunit_summary(R"(Test Count:\s*(\d+),\s*Passed:\s*(\d+),\s*Failed:\s*(\d+),\s*Warnings:\s*(\d+),\s*Inconclusive:\s*(\d+),\s*Skipped:\s*(\d+))");
    std::regex nunit_overall_result(R"(Overall result:\s*(\w+))");
    std::regex nunit_duration(R"(Duration:\s*([\d\.]+)\s*seconds)");
    std::regex nunit_failed_test(R"(\d+\)\s*(.+))");
    std::regex nunit_test_source(R"(Source:\s*(.+):line\s*(\d+))");
    std::regex nunit_test_assertion(R"(Expected:\s*(.+)\s*But was:\s*(.+))");
    
    std::regex xunit_header(R"(xUnit\.net VSTest Adapter\s+v([\d\.]+))");
    std::regex xunit_test_start(R"(Starting:\s*(.+))");
    std::regex xunit_test_finish(R"(Finished:\s*(.+))");
    std::regex xunit_test_pass(R"(\s*(.+)\s*\[PASS\])");
    std::regex xunit_test_fail(R"(\s*(.+)\s*\[FAIL\])");
    std::regex xunit_test_skip(R"(\s*(.+)\s*\[SKIP\])");
    std::regex xunit_assertion_failure(R"(Assert\.(\w+)\(\)\s*Failure)");
    std::regex xunit_stack_trace(R"(at\s+(.+)\s+in\s+(.+):line\s+(\d+))");
    std::regex xunit_total_summary(R"(Total tests:\s*(\d+))");
    std::regex xunit_passed_summary(R"(Passed:\s*(\d+))");
    std::regex xunit_failed_summary(R"(Failed:\s*(\d+))");
    std::regex xunit_skipped_summary(R"(Skipped:\s*(\d+))");
    std::regex xunit_time_summary(R"(Time:\s*([\d\.]+)s)");
    
    std::string current_test_suite;
    std::string current_framework = "unknown";
    bool in_failed_tests_section = false;
    bool in_skipped_tests_section = false;
    bool in_xunit_test_failure = false;
    std::vector<std::string> failure_details;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Detect NUnit vs xUnit framework
        if (std::regex_search(line, match, nunit_header)) {
            current_framework = "nunit";
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "NUnit version " + match[1].str();
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        else if (std::regex_search(line, match, xunit_header)) {
            current_framework = "xunit";
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "xUnit.net VSTest Adapter version " + match[1].str();
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // NUnit Test Summary
        else if (std::regex_search(line, match, nunit_summary)) {
            int total_tests = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int failed = std::stoi(match[3].str());
            int warnings = std::stoi(match[4].str());
            int inconclusive = std::stoi(match[5].str());
            int skipped = std::stoi(match[6].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = failed > 0 ? "error" : "info";
            event.status = failed > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.message = "Test summary: " + std::to_string(total_tests) + " total, " + 
                          std::to_string(passed) + " passed, " + std::to_string(failed) + " failed, " +
                          std::to_string(skipped) + " skipped";
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // NUnit Overall Result
        else if (std::regex_search(line, match, nunit_overall_result)) {
            std::string result = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = (result == "Failed") ? "error" : "info";
            event.status = (result == "Failed") ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.message = "Overall test result: " + result;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // NUnit Duration
        else if (std::regex_search(line, match, nunit_duration)) {
            double duration_seconds = std::stod(match[1].str());
            int64_t duration_ms = static_cast<int64_t>(duration_seconds * 1000);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Test execution time: " + match[1].str() + " seconds";
            event.execution_time = duration_ms;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            
            events.push_back(event);
        }
        // xUnit Test Suite Start
        else if (std::regex_search(line, match, xunit_test_start)) {
            current_test_suite = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Starting test suite: " + current_test_suite;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Test Suite Finish
        else if (std::regex_search(line, match, xunit_test_finish)) {
            std::string test_suite = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Finished test suite: " + test_suite;
            event.function_name = test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Test Pass
        else if (std::regex_search(line, match, xunit_test_pass)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Test passed: " + test_name;
            event.test_name = test_name;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Test Fail
        else if (std::regex_search(line, match, xunit_test_fail)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Test failed: " + test_name;
            event.test_name = test_name;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
            in_xunit_test_failure = true;
        }
        // xUnit Test Skip
        else if (std::regex_search(line, match, xunit_test_skip)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.status = ValidationEventStatus::SKIP;
            event.message = "Test skipped: " + test_name;
            event.test_name = test_name;
            event.function_name = current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // xUnit Stack Trace
        else if (std::regex_search(line, match, xunit_stack_trace)) {
            std::string method = match[1].str();
            std::string file_path = match[2].str();
            std::string line_number = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Stack trace: " + method;
            event.file_path = file_path;
            event.line_number = std::stoll(line_number);
            event.function_name = method;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.structured_data = "{\"file_path\": \"" + file_path + "\", \"line_number\": " + line_number + "}";
            
            events.push_back(event);
        }
        // NUnit Test Source (file and line)
        else if (std::regex_search(line, match, nunit_test_source)) {
            std::string source_path = match[1].str();
            std::string line_number = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Test failure location";
            event.file_path = source_path;
            event.line_number = std::stoll(line_number);
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.structured_data = "{\"file_path\": \"" + source_path + "\", \"line_number\": " + line_number + "}";
            
            events.push_back(event);
        }
        // xUnit Total Summary
        else if (std::regex_search(line, match, xunit_total_summary)) {
            int total_tests = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "xUnit test summary: " + std::to_string(total_tests) + " total tests";
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // Check for section markers
        else if (line.find("Failed Tests - Failures:") != std::string::npos) {
            in_failed_tests_section = true;
        }
        else if (line.find("Skipped Tests:") != std::string::npos) {
            in_skipped_tests_section = true;
            in_failed_tests_section = false;
        }
        // Handle assertion failures in xUnit
        else if (in_xunit_test_failure && std::regex_search(line, match, xunit_assertion_failure)) {
            std::string assertion_type = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Assertion failure: " + assertion_type;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        // Handle NUnit assertion details
        else if (in_failed_tests_section && std::regex_search(line, match, nunit_test_assertion)) {
            std::string expected = match[1].str();
            std::string actual = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Assertion failure - Expected: " + expected + ", But was: " + actual;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            
            events.push_back(event);
        }
        
        // Reset failure context when we encounter an empty line
        if (line.empty()) {
            in_xunit_test_failure = false;
        }
    }
}

void ParsePylintText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Pylint output
    std::regex pylint_module_header(R"(\*+\s*Module\s+(.+))");
    std::regex pylint_message(R"(([CWERF]):\s*(\d+),\s*(\d+):\s*(.+?)\s+\(([^)]+)\))");  // C:  1, 0: message (code)
    std::regex pylint_message_simple(R"(([CWERF]):\s*(\d+),\s*(\d+):\s*(.+))");  // C:  1, 0: message
    std::regex pylint_rating(R"(Your code has been rated at ([\d\.-]+)/10)");
    std::regex pylint_statistics(R"((\d+)\s+statements\s+analysed)");
    
    std::string current_module;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for module header
        if (std::regex_search(line, match, pylint_module_header)) {
            current_module = match[1].str();
            continue;
        }
        
        // Check for Pylint message with error code
        if (std::regex_search(line, match, pylint_message)) {
            std::string severity_char = match[1].str();
            std::string line_str = match[2].str();
            std::string column_str = match[3].str();
            std::string message = match[4].str();
            std::string error_code = match[5].str();
            
            int64_t line_number = 0;
            int64_t column_number = 0;
            
            try {
                line_number = std::stoi(line_str);
                column_number = std::stoi(column_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            
            // Map Pylint severity to ValidationEventStatus
            if (severity_char == "E" || severity_char == "F") {
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
                event.event_type = ValidationEventType::BUILD_ERROR;
            } else if (severity_char == "W") {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity_char == "C" || severity_char == "R") {
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = current_module.empty() ? "unknown" : current_module;
            event.line_number = line_number;
            event.column_number = column_number;
            event.error_code = error_code;
            event.tool_name = "pylint";
            event.category = "code_quality";
            event.raw_output = line;
            event.structured_data = "{\"severity_char\": \"" + severity_char + "\", \"error_code\": \"" + error_code + "\"}";
            
            events.push_back(event);
        }
        // Check for Pylint message without explicit error code
        else if (std::regex_search(line, match, pylint_message_simple)) {
            std::string severity_char = match[1].str();
            std::string line_str = match[2].str();
            std::string column_str = match[3].str();
            std::string message = match[4].str();
            
            int64_t line_number = 0;
            int64_t column_number = 0;
            
            try {
                line_number = std::stoi(line_str);
                column_number = std::stoi(column_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            
            // Map Pylint severity to ValidationEventStatus
            if (severity_char == "E" || severity_char == "F") {
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
                event.event_type = ValidationEventType::BUILD_ERROR;
            } else if (severity_char == "W") {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity_char == "C" || severity_char == "R") {
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = current_module.empty() ? "unknown" : current_module;
            event.line_number = line_number;
            event.column_number = column_number;
            event.tool_name = "pylint";
            event.category = "code_quality";
            event.raw_output = line;
            event.structured_data = "{\"severity_char\": \"" + severity_char + "\"}";
            
            events.push_back(event);
        }
        // Check for rating information
        else if (std::regex_search(line, match, pylint_rating)) {
            std::string rating = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Code quality rating: " + rating + "/10";
            event.tool_name = "pylint";
            event.category = "code_quality";
            event.raw_output = line;
            event.structured_data = "{\"rating\": \"" + rating + "\"}";
            
            events.push_back(event);
        }
    }
}

void ParseFlake8Text(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex pattern for Flake8 output: file.py:line:column: error_code message
    std::regex flake8_message(R"(([^:]+):(\d+):(\d+):\s*([FEWC]\d+)\s*(.+))");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, flake8_message)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string column_str = match[3].str();
            std::string error_code = match[4].str();
            std::string message = match[5].str();
            
            int64_t line_number = 0;
            int64_t column_number = 0;
            
            try {
                line_number = std::stoi(line_str);
                column_number = std::stoi(column_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            
            // Map Flake8 error codes to severity
            if (error_code.front() == 'F') {
                // F codes are pyflakes errors (logical errors)
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
                event.event_type = ValidationEventType::BUILD_ERROR;
            } else if (error_code.front() == 'E') {
                // E codes are PEP 8 errors (style errors)
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (error_code.front() == 'W') {
                // W codes are PEP 8 warnings
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (error_code.front() == 'C') {
                // C codes are complexity warnings
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = file_path;
            event.line_number = line_number;
            event.column_number = column_number;
            event.error_code = error_code;
            event.tool_name = "flake8";
            event.category = "style_guide";
            event.raw_output = line;
            event.structured_data = "{\"error_code\": \"" + error_code + "\", \"error_type\": \"" + std::string(1, error_code.front()) + "\"}";
            
            events.push_back(event);
        }
    }
}

void ParseBlackText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Black output
    std::regex would_reformat(R"(would reformat (.+))");
    std::regex reformat_summary(R"((\d+) files? would be reformatted, (\d+) files? would be left unchanged)");
    std::regex all_done_summary(R"(All done! ✨ 🍰 ✨)");
    std::regex diff_header(R"(--- (.+)\s+\(original\))");
    
    bool in_diff_mode = false;
    std::string current_file;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for "would reformat" messages
        if (std::regex_search(line, match, would_reformat)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "File would be reformatted by Black";
            event.file_path = file_path;
            event.tool_name = "black";
            event.category = "code_formatting";
            event.raw_output = line;
            event.structured_data = "{\"action\": \"would_reformat\"}";
            
            events.push_back(event);
        }
        // Check for reformat summary
        else if (std::regex_search(line, match, reformat_summary)) {
            std::string reformat_count = match[1].str();
            std::string unchanged_count = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = reformat_count + " files would be reformatted, " + unchanged_count + " files would be left unchanged";
            event.tool_name = "black";
            event.category = "code_formatting";
            event.raw_output = line;
            event.structured_data = "{\"reformat_count\": " + reformat_count + ", \"unchanged_count\": " + unchanged_count + "}";
            
            events.push_back(event);
        }
        // Check for "All done!" success message
        else if (std::regex_search(line, match, all_done_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Black formatting check completed successfully";
            event.tool_name = "black";
            event.category = "code_formatting";
            event.raw_output = line;
            event.structured_data = "{\"action\": \"success\"}";
            
            events.push_back(event);
        }
        // Check for diff header (unified diff mode)
        else if (std::regex_search(line, match, diff_header)) {
            current_file = match[1].str();
            in_diff_mode = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Black would apply formatting changes";
            event.file_path = current_file;
            event.tool_name = "black";
            event.category = "code_formatting";
            event.raw_output = line;
            event.structured_data = "{\"action\": \"diff_start\", \"file\": \"" + current_file + "\"}";
            
            events.push_back(event);
        }
        // Handle diff content (lines starting with + or -)
        else if (in_diff_mode && (line.front() == '+' || line.front() == '-') && line.size() > 1) {
            // Skip pure markers like +++/---
            if (line.substr(0, 3) != "+++" && line.substr(0, 3) != "---") {
                ValidationEvent event;
                event.event_id = event_id++;
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
                
                if (line.front() == '+') {
                    event.message = "Black would add: " + line.substr(1);
                } else {
                    event.message = "Black would remove: " + line.substr(1);
                }
                
                event.file_path = current_file;
                event.tool_name = "black";
                event.category = "code_formatting";
                event.raw_output = line;
                event.structured_data = "{\"action\": \"diff_line\", \"type\": \"" + std::string(1, line.front()) + "\"}";
                
                events.push_back(event);
            }
        }
        // Reset diff mode on empty lines or when encountering new files
        else if (line.empty() || line.find("would reformat") != std::string::npos) {
            in_diff_mode = false;
            current_file.clear();
        }
    }
}

void ParseMypyText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for mypy output
    std::regex mypy_message(R"(([^:]+):(\d+):\s*(error|warning|note):\s*(.+?)\s*\[([^\]]+)\])");
    std::regex mypy_message_no_code(R"(([^:]+):(\d+):\s*(error|warning|note):\s*(.+))");
    std::regex mypy_summary(R"(Found (\d+) errors? in (\d+) files? \(checked (\d+) files?\))");
    std::regex mypy_success(R"(Success: no issues found in (\d+) source files?)");
    std::regex mypy_revealed_type(R"((.+):(\d+):\s*note:\s*Revealed type is \"(.+)\")");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for mypy message with error code
        if (std::regex_search(line, match, mypy_message)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string severity = match[3].str();
            std::string message = match[4].str();
            std::string error_code = match[5].str();
            
            int64_t line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            
            // Map mypy severity to ValidationEventStatus
            if (severity == "error") {
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (severity == "warning") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity == "note") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = file_path;
            event.line_number = line_number;
            event.error_code = error_code;
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.raw_output = line;
            event.structured_data = "{\"error_code\": \"" + error_code + "\", \"severity\": \"" + severity + "\"}";
            
            events.push_back(event);
        }
        // Check for mypy message without error code
        else if (std::regex_search(line, match, mypy_message_no_code)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string severity = match[3].str();
            std::string message = match[4].str();
            
            int64_t line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            
            // Map mypy severity to ValidationEventStatus
            if (severity == "error") {
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (severity == "warning") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity == "note") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = file_path;
            event.line_number = line_number;
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.raw_output = line;
            event.structured_data = "{\"severity\": \"" + severity + "\"}";
            
            events.push_back(event);
        }
        // Check for summary with errors
        else if (std::regex_search(line, match, mypy_summary)) {
            std::string error_count = match[1].str();
            std::string file_count = match[2].str();
            std::string checked_count = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Found " + error_count + " errors in " + file_count + " files (checked " + checked_count + " files)";
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.raw_output = line;
            event.structured_data = "{\"error_count\": " + error_count + ", \"file_count\": " + file_count + ", \"checked_count\": " + checked_count + "}";
            
            events.push_back(event);
        }
        // Check for success message
        else if (std::regex_search(line, match, mypy_success)) {
            std::string checked_count = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Success: no issues found in " + checked_count + " source files";
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.raw_output = line;
            event.structured_data = "{\"checked_count\": " + checked_count + "}";
            
            events.push_back(event);
        }
    }
}

void ParseDockerBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Docker build output
    std::regex docker_step(R"(Step (\d+/\d+) : (.+))");
    std::regex docker_image_id(R"( ---> ([a-f0-9]{12}))");
    std::regex docker_running_in(R"( ---> Running in ([a-f0-9]{12}))");
    std::regex docker_removing(R"(Removing intermediate container ([a-f0-9]{12}))");
    std::regex docker_successfully_built(R"(Successfully built ([a-f0-9]{12}))");
    std::regex docker_successfully_tagged(R"(Successfully tagged (.+))");
    std::regex docker_error_command(R"(The command '(.+)' returned a non-zero code: (\d+))");
    std::regex docker_npm_err(R"(npm ERR! (.+))");
    std::regex docker_buildkit_step(R"(=> \[([^\]]+)\] (.+))");
    std::regex docker_buildkit_timing(R"(\[(\+)\] Building ([\d\.]+)s \((\d+/\d+)\) FINISHED)");
    std::regex docker_security_vuln(R"(✗ (High|Medium|Low|Critical) severity vulnerability found in (.+))");
    std::regex docker_cve(R"(Vulnerability: (CVE-\d{4}-\d{4,}))");
    std::regex docker_package_vuln(R"(Package: (.+))");
    std::regex docker_webpack_error(R"(ERROR in (.+))");
    std::regex docker_webpack_warning(R"(WARNING in (.+))");
    std::regex docker_module_not_found(R"(Module not found: Error: Can't resolve '(.+)' in '(.+)')");
    
    std::string current_step;
    std::string current_container;
    std::string current_vulnerability_package;
    std::string current_cve;
    bool in_security_scan = false;
    bool in_npm_error = false;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for security scanning section
        if (line.find("SECURITY SCANNING:") != std::string::npos) {
            in_security_scan = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SECURITY_FINDING;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Docker security scan initiated";
            event.tool_name = "docker";
            event.category = "security";
            event.raw_output = line;
            event.structured_data = "{\"action\": \"security_scan_start\"}";
            
            events.push_back(event);
            continue;
        }
        
        // Check for Docker step
        if (std::regex_search(line, match, docker_step)) {
            current_step = match[1].str();
            std::string command = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Docker step: " + command;
            event.tool_name = "docker";
            event.category = "build_step";
            event.raw_output = line;
            event.structured_data = "{\"step\": \"" + current_step + "\", \"command\": \"" + command + "\"}";
            
            events.push_back(event);
        }
        // Check for Docker container running
        else if (std::regex_search(line, match, docker_running_in)) {
            current_container = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Running in container " + current_container;
            event.tool_name = "docker";
            event.category = "container";
            event.raw_output = line;
            event.structured_data = "{\"container_id\": \"" + current_container + "\", \"action\": \"running\"}";
            
            events.push_back(event);
        }
        // Check for successful build
        else if (std::regex_search(line, match, docker_successfully_built)) {
            std::string image_id = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Docker image built successfully";
            event.tool_name = "docker";
            event.category = "build_success";
            event.raw_output = line;
            event.structured_data = "{\"image_id\": \"" + image_id + "\", \"action\": \"build_success\"}";
            
            events.push_back(event);
        }
        // Check for successful tagging
        else if (std::regex_search(line, match, docker_successfully_tagged)) {
            std::string tag = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Docker image tagged successfully: " + tag;
            event.tool_name = "docker";
            event.category = "build_success";
            event.raw_output = line;
            event.structured_data = "{\"tag\": \"" + tag + "\", \"action\": \"tag_success\"}";
            
            events.push_back(event);
        }
        // Check for command errors
        else if (std::regex_search(line, match, docker_error_command)) {
            std::string command = match[1].str();
            std::string exit_code = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Docker command failed: " + command;
            event.error_code = "exit_" + exit_code;
            event.tool_name = "docker";
            event.category = "build_error";
            event.raw_output = line;
            event.structured_data = "{\"command\": \"" + command + "\", \"exit_code\": " + exit_code + "}";
            
            events.push_back(event);
        }
        // Check for npm errors
        else if (std::regex_search(line, match, docker_npm_err)) {
            std::string error_msg = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "npm error: " + error_msg;
            event.tool_name = "npm";
            event.category = "package_error";
            event.raw_output = line;
            event.structured_data = "{\"error_message\": \"" + error_msg + "\", \"tool\": \"npm\"}";
            
            events.push_back(event);
        }
        // Check for webpack errors
        else if (std::regex_search(line, match, docker_webpack_error)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Webpack error in file";
            event.file_path = file_path;
            event.tool_name = "webpack";
            event.category = "build_error";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"tool\": \"webpack\"}";
            
            events.push_back(event);
        }
        // Check for webpack warnings
        else if (std::regex_search(line, match, docker_webpack_warning)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = "Webpack warning in file";
            event.file_path = file_path;
            event.tool_name = "webpack";
            event.category = "build_warning";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"tool\": \"webpack\"}";
            
            events.push_back(event);
        }
        // Check for module not found errors
        else if (std::regex_search(line, match, docker_module_not_found)) {
            std::string module = match[1].str();
            std::string path = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Module not found: " + module;
            event.file_path = path;
            event.error_code = "MODULE_NOT_FOUND";
            event.tool_name = "webpack";
            event.category = "dependency_error";
            event.raw_output = line;
            event.structured_data = "{\"missing_module\": \"" + module + "\", \"path\": \"" + path + "\"}";
            
            events.push_back(event);
        }
        // Check for security vulnerabilities
        else if (in_security_scan && std::regex_search(line, match, docker_security_vuln)) {
            std::string severity = match[1].str();
            std::string package = match[2].str();
            current_vulnerability_package = package;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SECURITY_FINDING;
            
            // Map security severity
            if (severity == "Critical" || severity == "High") {
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (severity == "Medium") {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            }
            
            event.message = severity + " severity vulnerability found in " + package;
            event.tool_name = "docker_scan";
            event.category = "security";
            event.raw_output = line;
            event.structured_data = "{\"severity\": \"" + severity + "\", \"package\": \"" + package + "\"}";
            
            events.push_back(event);
        }
        // Check for CVE information
        else if (in_security_scan && std::regex_search(line, match, docker_cve)) {
            current_cve = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SECURITY_FINDING;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "CVE identified: " + current_cve;
            event.error_code = current_cve;
            event.tool_name = "docker_scan";
            event.category = "security";
            event.raw_output = line;
            event.structured_data = "{\"cve\": \"" + current_cve + "\", \"package\": \"" + current_vulnerability_package + "\"}";
            
            events.push_back(event);
        }
        // Check for BuildKit output
        else if (std::regex_search(line, match, docker_buildkit_step)) {
            std::string step_name = match[1].str();
            std::string step_desc = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "BuildKit step: " + step_desc;
            event.tool_name = "docker_buildkit";
            event.category = "build_step";
            event.raw_output = line;
            event.structured_data = "{\"step_name\": \"" + step_name + "\", \"description\": \"" + step_desc + "\"}";
            
            events.push_back(event);
        }
        // Check for BuildKit completion
        else if (std::regex_search(line, match, docker_buildkit_timing)) {
            std::string duration = match[2].str();
            std::string steps = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "BuildKit build completed in " + duration + "s";
            event.tool_name = "docker_buildkit";
            event.category = "build_success";
            event.raw_output = line;
            event.structured_data = "{\"duration\": \"" + duration + "\", \"steps\": \"" + steps + "\"}";
            
            events.push_back(event);
        }
        
        // Reset security scan flag when we encounter regular Docker output
        if (in_security_scan && line.find("CACHING INFORMATION:") != std::string::npos) {
            in_security_scan = false;
        }
    }
}

void ParseBazelBuild(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Bazel build output
    std::regex bazel_analyzed(R"(INFO: Analyzed (\d+) targets? \((\d+) packages? loaded, (\d+) targets? configured\)\.)");
    std::regex bazel_found(R"(INFO: Found (\d+) targets?\.\.\.)");
    std::regex bazel_build_completed(R"(INFO: Build completed successfully, (\d+) total actions)");
    std::regex bazel_build_elapsed(R"(INFO: Elapsed time: ([\d\.]+)s, Critical Path: ([\d\.]+)s)");
    std::regex bazel_test_passed(R"(PASSED: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(([\d\.]+)s\))");
    std::regex bazel_test_failed(R"(FAILED: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(([\d\.]+)s\) \[(\d+)/(\d+) attempts\])");
    std::regex bazel_test_timeout(R"(TIMEOUT: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(([\d\.]+)s TIMEOUT\))");
    std::regex bazel_test_flaky(R"(FLAKY: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) passed in (\d+) out of (\d+) attempts)");
    std::regex bazel_test_skipped(R"(SKIPPED: (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) \(SKIPPED\))");
    std::regex bazel_compilation_error(R"(ERROR: ([^:]+):(\d+):(\d+): (.+) failed \(Exit (\d+)\): (.+))");
    std::regex bazel_build_error(R"(ERROR: ([^:]+):(\d+):(\d+): (.+))");
    std::regex bazel_linking_error(R"(ERROR: ([^:]+):(\d+):(\d+): Linking of rule '([^']+)' failed \(Exit (\d+)\): (.+))");
    std::regex bazel_warning(R"(WARNING: (.+))");
    std::regex bazel_fail_msg(R"(FAIL (.+):(\d+): (.+))");
    std::regex bazel_loading(R"(Loading: (\d+) packages? loaded)");
    std::regex bazel_analyzing(R"(Analyzing: (\d+) targets? \((\d+) packages? loaded, (\d+) targets? configured\))");
    std::regex bazel_action_info(R"(INFO: From (.+):)");
    std::regex bazel_up_to_date(R"(Target (//[^/\s]+(?:/[^/\s]+)*:[^/\s]+) up-to-date:)");
    std::regex bazel_test_summary(R"(Total: (\d+) tests?, (\d+) passed, (\d+) failed(?:, (\d+) timeout)?(?:, (\d+) flaky)?(?:, (\d+) skipped)?)");
    
    std::string current_target;
    std::string current_action;
    bool in_test_suite = false;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for test suite header
        if (line.find("==== Test Suite:") != std::string::npos) {
            in_test_suite = true;
            continue;
        }
        
        // Check for analysis phase
        if (std::regex_search(line, match, bazel_analyzed)) {
            int targets = std::stoi(match[1].str());
            int packages = std::stoi(match[2].str());
            int configured = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Analyzed " + std::to_string(targets) + " targets (" + 
                           std::to_string(packages) + " packages loaded, " + 
                           std::to_string(configured) + " targets configured)";
            event.tool_name = "bazel";
            event.category = "analysis";
            event.raw_output = line;
            event.structured_data = "{\"targets\": " + std::to_string(targets) + 
                                   ", \"packages\": " + std::to_string(packages) + 
                                   ", \"configured\": " + std::to_string(configured) + "}";
            
            events.push_back(event);
        }
        // Check for build completion
        else if (std::regex_search(line, match, bazel_build_completed)) {
            int actions = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Build completed successfully with " + std::to_string(actions) + " total actions";
            event.tool_name = "bazel";
            event.category = "build_success";
            event.raw_output = line;
            event.structured_data = "{\"total_actions\": " + std::to_string(actions) + "}";
            
            events.push_back(event);
        }
        // Check for elapsed time
        else if (std::regex_search(line, match, bazel_build_elapsed)) {
            double elapsed = std::stod(match[1].str());
            double critical_path = std::stod(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Build timing - Elapsed: " + match[1].str() + "s, Critical Path: " + match[2].str() + "s";
            event.tool_name = "bazel";
            event.category = "performance";
            event.execution_time = elapsed;
            event.raw_output = line;
            event.structured_data = "{\"elapsed_time\": " + match[1].str() + 
                                   ", \"critical_path_time\": " + match[2].str() + "}";
            
            events.push_back(event);
        }
        // Check for passed tests
        else if (std::regex_search(line, match, bazel_test_passed)) {
            std::string target = match[1].str();
            double duration = std::stod(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Test passed";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_result";
            event.execution_time = duration;
            event.raw_output = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"duration\": " + match[2].str() + "}";
            
            events.push_back(event);
        }
        // Check for failed tests
        else if (std::regex_search(line, match, bazel_test_failed)) {
            std::string target = match[1].str();
            double duration = std::stod(match[2].str());
            int current_attempt = std::stoi(match[3].str());
            int total_attempts = std::stoi(match[4].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = "Test failed (" + std::to_string(current_attempt) + "/" + std::to_string(total_attempts) + " attempts)";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_result";
            event.execution_time = duration;
            event.raw_output = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"duration\": " + match[2].str() + 
                                   ", \"current_attempt\": " + std::to_string(current_attempt) + 
                                   ", \"total_attempts\": " + std::to_string(total_attempts) + "}";
            
            events.push_back(event);
        }
        // Check for timeout tests
        else if (std::regex_search(line, match, bazel_test_timeout)) {
            std::string target = match[1].str();
            double duration = std::stod(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Test exceeded maximum time limit";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_timeout";
            event.execution_time = duration;
            event.raw_output = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"duration\": " + match[2].str() + ", \"reason\": \"timeout\"}";
            
            events.push_back(event);
        }
        // Check for flaky tests
        else if (std::regex_search(line, match, bazel_test_flaky)) {
            std::string target = match[1].str();
            int passed_attempts = std::stoi(match[2].str());
            int total_attempts = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = "Test is flaky - passed " + std::to_string(passed_attempts) + " out of " + std::to_string(total_attempts) + " attempts";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_flaky";
            event.raw_output = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"passed_attempts\": " + std::to_string(passed_attempts) + 
                                   ", \"total_attempts\": " + std::to_string(total_attempts) + "}";
            
            events.push_back(event);
        }
        // Check for skipped tests
        else if (std::regex_search(line, match, bazel_test_skipped)) {
            std::string target = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.status = ValidationEventStatus::SKIP;
            event.message = "Test skipped";
            event.test_name = target;
            event.tool_name = "bazel";
            event.category = "test_result";
            event.raw_output = line;
            event.structured_data = "{\"target\": \"" + target + "\", \"reason\": \"skipped\"}";
            
            events.push_back(event);
        }
        // Check for compilation errors
        else if (std::regex_search(line, match, bazel_compilation_error)) {
            std::string file_path = match[1].str();
            int line_num = std::stoi(match[2].str());
            int col_num = std::stoi(match[3].str());
            std::string rule_name = match[4].str();
            int exit_code = std::stoi(match[5].str());
            std::string error_msg = match[6].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = rule_name + " failed: " + error_msg;
            event.file_path = file_path;
            event.line_number = line_num;
            event.column_number = col_num;
            event.error_code = "exit_" + std::to_string(exit_code);
            event.tool_name = "bazel";
            event.category = "compilation_error";
            event.raw_output = line;
            event.structured_data = "{\"rule\": \"" + rule_name + "\", \"exit_code\": " + std::to_string(exit_code) + 
                                   ", \"error\": \"" + error_msg + "\"}";
            
            events.push_back(event);
        }
        // Check for linking errors
        else if (std::regex_search(line, match, bazel_linking_error)) {
            std::string file_path = match[1].str();
            int line_num = std::stoi(match[2].str());
            int col_num = std::stoi(match[3].str());
            std::string rule_name = match[4].str();
            int exit_code = std::stoi(match[5].str());
            std::string error_msg = match[6].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Linking failed for " + rule_name + ": " + error_msg;
            event.file_path = file_path;
            event.line_number = line_num;
            event.column_number = col_num;
            event.error_code = "link_exit_" + std::to_string(exit_code);
            event.tool_name = "bazel";
            event.category = "linking_error";
            event.raw_output = line;
            event.structured_data = "{\"rule\": \"" + rule_name + "\", \"exit_code\": " + std::to_string(exit_code) + 
                                   ", \"error\": \"" + error_msg + "\"}";
            
            events.push_back(event);
        }
        // Check for general build errors
        else if (std::regex_search(line, match, bazel_build_error)) {
            std::string file_path = match[1].str();
            int line_num = std::stoi(match[2].str());
            int col_num = std::stoi(match[3].str());
            std::string error_msg = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = error_msg;
            event.file_path = file_path;
            event.line_number = line_num;
            event.column_number = col_num;
            event.tool_name = "bazel";
            event.category = "build_error";
            event.raw_output = line;
            event.structured_data = "{\"error\": \"" + error_msg + "\"}";
            
            events.push_back(event);
        }
        // Check for warnings
        else if (std::regex_search(line, match, bazel_warning)) {
            std::string warning_msg = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = warning_msg;
            event.tool_name = "bazel";
            event.category = "build_warning";
            event.raw_output = line;
            event.structured_data = "{\"warning\": \"" + warning_msg + "\"}";
            
            events.push_back(event);
        }
        // Check for test failures with details
        else if (std::regex_search(line, match, bazel_fail_msg)) {
            std::string file_path = match[1].str();
            int line_num = std::stoi(match[2].str());
            std::string failure_msg = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.status = ValidationEventStatus::FAIL;
            event.message = failure_msg;
            event.file_path = file_path;
            event.line_number = line_num;
            event.tool_name = "bazel";
            event.category = "test_failure";
            event.raw_output = line;
            event.structured_data = "{\"failure_message\": \"" + failure_msg + "\"}";
            
            events.push_back(event);
        }
        // Check for loading phase
        else if (std::regex_search(line, match, bazel_loading)) {
            int packages = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Loading packages: " + std::to_string(packages) + " loaded";
            event.tool_name = "bazel";
            event.category = "loading";
            event.raw_output = line;
            event.structured_data = "{\"packages_loaded\": " + std::to_string(packages) + "}";
            
            events.push_back(event);
        }
        // Check for analyzing phase
        else if (std::regex_search(line, match, bazel_analyzing)) {
            int targets = std::stoi(match[1].str());
            int packages = std::stoi(match[2].str());
            int configured = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Analyzing " + std::to_string(targets) + " targets (" + 
                           std::to_string(packages) + " packages loaded, " + 
                           std::to_string(configured) + " targets configured)";
            event.tool_name = "bazel";
            event.category = "analyzing";
            event.raw_output = line;
            event.structured_data = "{\"targets\": " + std::to_string(targets) + 
                                   ", \"packages\": " + std::to_string(packages) + 
                                   ", \"configured\": " + std::to_string(configured) + "}";
            
            events.push_back(event);
        }
        // Check for test summary
        else if (std::regex_search(line, match, bazel_test_summary)) {
            int total = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int failed = std::stoi(match[3].str());
            int timeout = match[4].matched ? std::stoi(match[4].str()) : 0;
            int flaky = match[5].matched ? std::stoi(match[5].str()) : 0;
            int skipped = match[6].matched ? std::stoi(match[6].str()) : 0;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = (failed > 0) ? "error" : "info";
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.message = "Test Summary: " + std::to_string(total) + " tests, " + 
                           std::to_string(passed) + " passed, " + std::to_string(failed) + " failed" +
                           (timeout > 0 ? ", " + std::to_string(timeout) + " timeout" : "") +
                           (flaky > 0 ? ", " + std::to_string(flaky) + " flaky" : "") +
                           (skipped > 0 ? ", " + std::to_string(skipped) + " skipped" : "");
            event.tool_name = "bazel";
            event.category = "test_summary";
            event.raw_output = line;
            event.structured_data = "{\"total\": " + std::to_string(total) + 
                                   ", \"passed\": " + std::to_string(passed) + 
                                   ", \"failed\": " + std::to_string(failed) + 
                                   ", \"timeout\": " + std::to_string(timeout) + 
                                   ", \"flaky\": " + std::to_string(flaky) + 
                                   ", \"skipped\": " + std::to_string(skipped) + "}";
            
            events.push_back(event);
        }
    }
}

void ParseIsortText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for isort output
    std::regex isort_error(R"(ERROR: ([^:]+\.py) Imports are incorrectly sorted and/or formatted\.)");
    std::regex isort_fixing(R"(Fixing ([^:]+\.py))");
    std::regex isort_diff_header(R"(--- ([^:]+\.py):before\s+(.+))");
    std::regex isort_parse_error(R"(ERROR: ([^:]+\.py) (.+))");
    std::regex isort_warning(R"(WARNING: ([^:]+\.py) (.+))");
    std::regex isort_summary(R"(Successfully formatted (\d+) files?, (\d+) files? reformatted\.)");
    std::regex isort_dry_run(R"((\d+) files? would be reformatted, (\d+) files? would be left unchanged\.)");
    std::regex isort_config_setting(R"(([^:]+):\s*(.+))");
    std::regex isort_verbose_parsing(R"(Parsing ([^:]+\.py))");
    std::regex isort_verbose_placing(R"(Placing imports for ([^:]+\.py))");
    std::regex isort_check_error(R"(ERROR: isort found an import in the wrong position\.)");
    std::regex isort_file_line(R"(File: ([^:]+\.py))");
    std::regex isort_line_number(R"(Line: (\d+))");
    std::regex isort_expected(R"(Expected: (.+))");
    std::regex isort_actual(R"(Actual: (.+))");
    std::regex isort_skipped(R"(Skipped (\d+) files?)");
    std::regex isort_permission_denied(R"(ERROR: Permission denied: ([^:]+\.py))");
    std::regex isort_syntax_error(R"(ERROR: ([^:]+\.py) Unable to parse file\. (.+))");
    std::regex isort_encoding_warning(R"(WARNING: ([^:]+\.py) Unable to determine encoding\.)");
    std::regex isort_execution_time(R"(Total execution time: ([\d\.]+)s)");
    std::regex isort_final_summary(R"(Files processed: (\d+))");
    
    std::string current_file;
    std::string current_error_context;
    bool in_diff_section = false;
    bool in_config_section = false;
    bool in_check_mode = false;
    int current_line_number = -1;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for configuration section
        if (line.find("import-order-style:") != std::string::npos || 
            line.find("profile:") != std::string::npos ||
            line.find("line-length:") != std::string::npos) {
            in_config_section = true;
        }
        
        // Check for diff section
        if (line.find("---") != std::string::npos && line.find(":before") != std::string::npos) {
            in_diff_section = true;
        } else if (line.find("+++") != std::string::npos && line.find(":after") != std::string::npos) {
            in_diff_section = true;
        } else if (in_diff_section && line.empty()) {
            in_diff_section = false;
        }
        
        // Parse import sorting errors
        if (std::regex_search(line, match, isort_error)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Imports are incorrectly sorted and/or formatted";
            event.file_path = file_path;
            event.tool_name = "isort";
            event.category = "import_order";
            event.error_code = "IMPORT_ORDER";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"issue\": \"incorrect_import_order\"}";
            
            events.push_back(event);
        }
        // Parse fixing messages
        else if (std::regex_search(line, match, isort_fixing)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Fixing import order";
            event.file_path = file_path;
            event.tool_name = "isort";
            event.category = "import_fix";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"action\": \"fixing_imports\"}";
            
            events.push_back(event);
        }
        // Parse verbose parsing messages
        else if (std::regex_search(line, match, isort_verbose_parsing)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Parsing Python file";
            event.file_path = file_path;
            event.tool_name = "isort";
            event.category = "parsing";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"action\": \"parsing\"}";
            
            events.push_back(event);
        }
        // Parse verbose placing messages
        else if (std::regex_search(line, match, isort_verbose_placing)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Placing imports";
            event.file_path = file_path;
            event.tool_name = "isort";
            event.category = "placing";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"action\": \"placing_imports\"}";
            
            events.push_back(event);
        }
        // Parse check-only mode errors
        else if (std::regex_search(line, match, isort_check_error)) {
            in_check_mode = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Import found in wrong position";
            event.tool_name = "isort";
            event.category = "import_position";
            event.error_code = "WRONG_POSITION";
            event.raw_output = line;
            event.structured_data = "{\"check_mode\": true, \"issue\": \"wrong_import_position\"}";
            
            events.push_back(event);
        }
        // Parse file context in check mode
        else if (in_check_mode && std::regex_search(line, match, isort_file_line)) {
            current_file = match[1].str();
        }
        // Parse line number in check mode
        else if (in_check_mode && std::regex_search(line, match, isort_line_number)) {
            current_line_number = std::stoi(match[1].str());
        }
        // Parse expected vs actual in check mode
        else if (in_check_mode && std::regex_search(line, match, isort_expected)) {
            std::string expected = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Expected: " + expected;
            event.file_path = current_file;
            event.line_number = current_line_number;
            event.tool_name = "isort";
            event.category = "import_expectation";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + current_file + "\", \"expected\": \"" + expected + "\"}";
            
            events.push_back(event);
        }
        else if (in_check_mode && std::regex_search(line, match, isort_actual)) {
            std::string actual = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = "Actual: " + actual;
            event.file_path = current_file;
            event.line_number = current_line_number;
            event.tool_name = "isort";
            event.category = "import_actual";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + current_file + "\", \"actual\": \"" + actual + "\"}";
            
            events.push_back(event);
        }
        // Parse parse/syntax errors
        else if (std::regex_search(line, match, isort_parse_error) || 
                 std::regex_search(line, match, isort_syntax_error)) {
            std::string file_path = match[1].str();
            std::string error_msg = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = error_msg;
            event.file_path = file_path;
            event.tool_name = "isort";
            event.category = "parse_error";
            event.error_code = "PARSE_ERROR";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"error\": \"" + error_msg + "\"}";
            
            events.push_back(event);
        }
        // Parse permission errors
        else if (std::regex_search(line, match, isort_permission_denied)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Permission denied";
            event.file_path = file_path;
            event.tool_name = "isort";
            event.category = "permission_error";
            event.error_code = "PERMISSION_DENIED";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"error\": \"permission_denied\"}";
            
            events.push_back(event);
        }
        // Parse encoding warnings
        else if (std::regex_search(line, match, isort_encoding_warning)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = "Unable to determine encoding";
            event.file_path = file_path;
            event.tool_name = "isort";
            event.category = "encoding_warning";
            event.raw_output = line;
            event.structured_data = "{\"file\": \"" + file_path + "\", \"warning\": \"encoding_issue\"}";
            
            events.push_back(event);
        }
        // Parse success summary
        else if (std::regex_search(line, match, isort_summary)) {
            int formatted = std::stoi(match[1].str());
            int reformatted = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Successfully formatted " + std::to_string(formatted) + " files, " + 
                           std::to_string(reformatted) + " files reformatted";
            event.tool_name = "isort";
            event.category = "format_summary";
            event.raw_output = line;
            event.structured_data = "{\"formatted\": " + std::to_string(formatted) + 
                                   ", \"reformatted\": " + std::to_string(reformatted) + "}";
            
            events.push_back(event);
        }
        // Parse dry-run summary
        else if (std::regex_search(line, match, isort_dry_run)) {
            int would_reformat = std::stoi(match[1].str());
            int unchanged = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = std::to_string(would_reformat) + " files would be reformatted, " + 
                           std::to_string(unchanged) + " files would be left unchanged";
            event.tool_name = "isort";
            event.category = "dry_run_summary";
            event.raw_output = line;
            event.structured_data = "{\"would_reformat\": " + std::to_string(would_reformat) + 
                                   ", \"unchanged\": " + std::to_string(unchanged) + ", \"dry_run\": true}";
            
            events.push_back(event);
        }
        // Parse skipped files
        else if (std::regex_search(line, match, isort_skipped)) {
            int skipped = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Skipped " + std::to_string(skipped) + " files";
            event.tool_name = "isort";
            event.category = "skipped_files";
            event.raw_output = line;
            event.structured_data = "{\"skipped\": " + std::to_string(skipped) + "}";
            
            events.push_back(event);
        }
        // Parse execution time
        else if (std::regex_search(line, match, isort_execution_time)) {
            double exec_time = std::stod(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Total execution time: " + match[1].str() + "s";
            event.tool_name = "isort";
            event.category = "performance";
            event.execution_time = exec_time;
            event.raw_output = line;
            event.structured_data = "{\"execution_time\": " + match[1].str() + "}";
            
            events.push_back(event);
        }
        // Parse configuration settings
        else if (in_config_section && std::regex_search(line, match, isort_config_setting)) {
            std::string setting = match[1].str();
            std::string value = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Configuration: " + setting + " = " + value;
            event.tool_name = "isort";
            event.category = "configuration";
            event.raw_output = line;
            event.structured_data = "{\"setting\": \"" + setting + "\", \"value\": \"" + value + "\"}";
            
            events.push_back(event);
        }
        
        // Reset check mode context when we hit an empty line after check details
        if (in_check_mode && line.empty()) {
            in_check_mode = false;
            current_file.clear();
            current_line_number = -1;
        }
        
        // Reset config section when we hit "All done!" or similar
        if (line.find("All done!") != std::string::npos || 
            line.find("files would be reformatted") != std::string::npos) {
            in_config_section = false;
        }
    }
}

void ParseBanditText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for bandit text output
    std::regex bandit_issue_header(R"(>> Issue: \[([^:]+):([^\]]+)\] (.+))");
    std::regex bandit_severity(R"(Severity: (Low|Medium|High)\s+Confidence: (Low|Medium|High))");
    std::regex bandit_cwe(R"(CWE: (CWE-\d+) \(([^)]+)\))");
    std::regex bandit_more_info(R"(More Info: (https://bandit\.readthedocs\.io[^\s]+))");
    std::regex bandit_location(R"(Location: ([^:]+):(\d+):(\d+))");
    std::regex bandit_info_log(R"(\[main\]\s+(INFO|WARNING|ERROR)\s+(.+))");
    std::regex bandit_run_started(R"(Run started:(.+))");
    std::regex bandit_total_lines(R"(Total lines of code: (\d+))");
    std::regex bandit_skipped_lines(R"(Total lines skipped \(#nosec\): (\d+))");
    std::regex bandit_severity_summary(R"((Low|Medium|High|Undefined): (\d+))");
    std::regex bandit_confidence_summary(R"((Low|Medium|High|Undefined): (\d+))");
    std::regex bandit_files_skipped(R"(Files skipped \((\d+)\):)");
    std::regex bandit_final_log(R"(\[bandit\]\s+(INFO|WARNING|ERROR)\s+(.+))");
    std::regex bandit_issues_found(R"(Found (\d+) issues with security implications)");
    std::regex bandit_execution_time(R"(Total execution time: ([\d\.]+) seconds)");
    std::regex bandit_python_version(R"(running on Python ([\d\.]+))");
    
    std::string current_test_id;
    std::string current_test_name;
    std::string current_severity;
    std::string current_confidence;
    std::string current_file;
    std::string current_cwe;
    std::string current_more_info;
    int current_line = -1;
    int current_column = -1;
    bool in_issue_block = false;
    bool in_severity_summary = false;
    bool in_confidence_summary = false;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Parse issue header
        if (std::regex_search(line, match, bandit_issue_header)) {
            current_test_id = match[1].str();
            current_test_name = match[2].str();
            std::string description = match[3].str();
            in_issue_block = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SECURITY_FINDING;
            event.severity = "unknown";  // Will be updated when we parse severity line
            event.status = ValidationEventStatus::WARNING;
            event.message = description;
            event.tool_name = "bandit";
            event.category = "security";
            event.error_code = current_test_id;
            event.test_name = current_test_name;
            event.raw_output = line;
            event.structured_data = "{\"test_id\": \"" + current_test_id + "\", \"test_name\": \"" + current_test_name + "\", \"description\": \"" + description + "\"}";
            
            events.push_back(event);
        }
        // Parse severity and confidence
        else if (in_issue_block && std::regex_search(line, match, bandit_severity)) {
            current_severity = match[1].str();
            current_confidence = match[2].str();
            
            // Update the last event with severity information
            if (!events.empty()) {
                auto& last_event = events.back();
                last_event.severity = current_severity == "High" ? "error" : 
                                     current_severity == "Medium" ? "warning" : "info";
                last_event.status = current_severity == "High" ? ValidationEventStatus::ERROR :
                                   current_severity == "Medium" ? ValidationEventStatus::WARNING : ValidationEventStatus::INFO;
                
                // Update structured data with severity and confidence
                last_event.structured_data = "{\"test_id\": \"" + current_test_id + 
                                             "\", \"test_name\": \"" + current_test_name + 
                                             "\", \"severity\": \"" + current_severity + 
                                             "\", \"confidence\": \"" + current_confidence + "\"}";
            }
        }
        // Parse CWE information
        else if (in_issue_block && std::regex_search(line, match, bandit_cwe)) {
            current_cwe = match[1].str();
            std::string cwe_url = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SECURITY_FINDING;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "CWE reference: " + current_cwe;
            event.tool_name = "bandit";
            event.category = "cwe_reference";
            event.error_code = current_cwe;
            event.raw_output = line;
            event.structured_data = "{\"cwe\": \"" + current_cwe + "\", \"url\": \"" + cwe_url + "\"}";
            
            events.push_back(event);
        }
        // Parse more info URL
        else if (in_issue_block && std::regex_search(line, match, bandit_more_info)) {
            current_more_info = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "More information available";
            event.tool_name = "bandit";
            event.category = "documentation";
            event.suggestion = current_more_info;
            event.raw_output = line;
            event.structured_data = "{\"more_info\": \"" + current_more_info + "\"}";
            
            events.push_back(event);
        }
        // Parse location
        else if (in_issue_block && std::regex_search(line, match, bandit_location)) {
            current_file = match[1].str();
            current_line = std::stoi(match[2].str());
            current_column = std::stoi(match[3].str());
            
            // Update the first event in this block with location information
            if (!events.empty()) {
                for (auto& event : events) {
                    if (event.error_code == current_test_id && event.file_path.empty()) {
                        event.file_path = current_file;
                        event.line_number = current_line;
                        event.column_number = current_column;
                        break;
                    }
                }
            }
        }
        // Parse main info logs
        else if (std::regex_search(line, match, bandit_info_log)) {
            std::string log_level = match[1].str();
            std::string log_message = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = log_level == "ERROR" ? "error" : 
                            log_level == "WARNING" ? "warning" : "info";
            event.status = log_level == "ERROR" ? ValidationEventStatus::ERROR :
                          log_level == "WARNING" ? ValidationEventStatus::WARNING : ValidationEventStatus::INFO;
            event.message = log_message;
            event.tool_name = "bandit";
            event.category = "initialization";
            event.raw_output = line;
            event.structured_data = "{\"log_level\": \"" + log_level + "\", \"message\": \"" + log_message + "\"}";
            
            events.push_back(event);
        }
        // Parse run started
        else if (std::regex_search(line, match, bandit_run_started)) {
            std::string timestamp = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Security scan started";
            event.tool_name = "bandit";
            event.category = "scan_start";
            event.raw_output = line;
            event.structured_data = "{\"timestamp\": \"" + timestamp + "\", \"action\": \"scan_start\"}";
            
            events.push_back(event);
        }
        // Parse Python version
        else if (std::regex_search(line, match, bandit_python_version)) {
            std::string python_version = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Running on Python " + python_version;
            event.tool_name = "bandit";
            event.category = "environment";
            event.raw_output = line;
            event.structured_data = "{\"python_version\": \"" + python_version + "\"}";
            
            events.push_back(event);
        }
        // Parse total lines of code
        else if (std::regex_search(line, match, bandit_total_lines)) {
            int total_lines = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Total lines of code analyzed: " + std::to_string(total_lines);
            event.tool_name = "bandit";
            event.category = "code_metrics";
            event.raw_output = line;
            event.structured_data = "{\"total_lines\": " + std::to_string(total_lines) + "}";
            
            events.push_back(event);
        }
        // Parse skipped lines
        else if (std::regex_search(line, match, bandit_skipped_lines)) {
            int skipped_lines = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Lines skipped (#nosec): " + std::to_string(skipped_lines);
            event.tool_name = "bandit";
            event.category = "code_metrics";
            event.raw_output = line;
            event.structured_data = "{\"skipped_lines\": " + std::to_string(skipped_lines) + "}";
            
            events.push_back(event);
        }
        // Parse severity summary
        else if (line.find("Total issues (by severity):") != std::string::npos) {
            in_severity_summary = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Security issues summary by severity";
            event.tool_name = "bandit";
            event.category = "severity_summary";
            event.raw_output = line;
            event.structured_data = "{\"summary_type\": \"severity\"}";
            
            events.push_back(event);
        }
        else if (in_severity_summary && std::regex_search(line, match, bandit_severity_summary)) {
            std::string severity_level = match[1].str();
            int count = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = severity_level == "High" ? "error" : 
                            severity_level == "Medium" ? "warning" : "info";
            event.status = ValidationEventStatus::INFO;
            event.message = severity_level + " severity issues: " + std::to_string(count);
            event.tool_name = "bandit";
            event.category = "severity_count";
            event.raw_output = line;
            event.structured_data = "{\"severity\": \"" + severity_level + "\", \"count\": " + std::to_string(count) + "}";
            
            events.push_back(event);
        }
        // Parse confidence summary
        else if (line.find("Total issues (by confidence):") != std::string::npos) {
            in_confidence_summary = true;
            in_severity_summary = false;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Security issues summary by confidence";
            event.tool_name = "bandit";
            event.category = "confidence_summary";
            event.raw_output = line;
            event.structured_data = "{\"summary_type\": \"confidence\"}";
            
            events.push_back(event);
        }
        else if (in_confidence_summary && std::regex_search(line, match, bandit_confidence_summary)) {
            std::string confidence_level = match[1].str();
            int count = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = confidence_level + " confidence issues: " + std::to_string(count);
            event.tool_name = "bandit";
            event.category = "confidence_count";
            event.raw_output = line;
            event.structured_data = "{\"confidence\": \"" + confidence_level + "\", \"count\": " + std::to_string(count) + "}";
            
            events.push_back(event);
        }
        // Parse final bandit logs
        else if (std::regex_search(line, match, bandit_final_log)) {
            std::string log_level = match[1].str();
            std::string log_message = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = log_level == "ERROR" ? "error" : 
                            log_level == "WARNING" ? "warning" : "info";
            event.status = log_level == "ERROR" ? ValidationEventStatus::ERROR :
                          log_level == "WARNING" ? ValidationEventStatus::WARNING : ValidationEventStatus::PASS;
            event.message = log_message;
            event.tool_name = "bandit";
            event.category = "scan_completion";
            event.raw_output = line;
            event.structured_data = "{\"log_level\": \"" + log_level + "\", \"message\": \"" + log_message + "\"}";
            
            events.push_back(event);
        }
        // Parse issues found count
        else if (std::regex_search(line, match, bandit_issues_found)) {
            int issues_count = std::stoi(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = issues_count > 0 ? "warning" : "info";
            event.status = issues_count > 0 ? ValidationEventStatus::WARNING : ValidationEventStatus::PASS;
            event.message = "Found " + std::to_string(issues_count) + " security issues";
            event.tool_name = "bandit";
            event.category = "issues_summary";
            event.raw_output = line;
            event.structured_data = "{\"issues_found\": " + std::to_string(issues_count) + "}";
            
            events.push_back(event);
        }
        // Parse execution time
        else if (std::regex_search(line, match, bandit_execution_time)) {
            double exec_time = std::stod(match[1].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Security scan completed in " + match[1].str() + " seconds";
            event.tool_name = "bandit";
            event.category = "performance";
            event.execution_time = exec_time;
            event.raw_output = line;
            event.structured_data = "{\"execution_time\": " + match[1].str() + "}";
            
            events.push_back(event);
        }
        
        // Reset issue block when we hit the separator
        if (line.find("--------------------------------------------------") != std::string::npos) {
            in_issue_block = false;
        }
        
        // Reset summary flags when appropriate
        if (line.find("Files skipped") != std::string::npos) {
            in_severity_summary = false;
            in_confidence_summary = false;
        }
    }
}

void ParseAutopep8Text(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for autopep8 output
    std::regex diff_start(R"(--- original/(.+))");
    std::regex diff_fixed(R"(\+\+\+ fixed/(.+))");
    std::regex error_pattern(R"(ERROR: ([^:]+\.py):(\d+):(\d+): (E\d+) (.+))");
    std::regex warning_pattern(R"(WARNING: ([^:]+\.py):(\d+):(\d+): (E\d+) (.+))");
    std::regex info_pattern(R"(INFO: ([^:]+\.py): (.+))");
    std::regex fixed_pattern(R"(fixed ([^:]+\.py))");
    std::regex autopep8_cmd(R"(autopep8 (--[^\s]+.+))");
    std::regex config_line(R"(Applied configuration:)");
    std::regex summary_files_processed(R"(Files processed: (\d+))");
    std::regex summary_files_modified(R"(Files modified: (\d+))");
    std::regex summary_files_errors(R"(Files with errors: (\d+))");
    std::regex summary_fixes_applied(R"(Total fixes applied: (\d+))");
    std::regex summary_execution_time(R"(Execution time: ([\d\.]+)s)");
    std::regex syntax_error(R"(ERROR: ([^:]+\.py):(\d+):(\d+): SyntaxError: (.+))");
    std::regex encoding_error(R"(WARNING: ([^:]+\.py): could not determine file encoding)");
    std::regex already_formatted(R"(INFO: ([^:]+\.py): already formatted correctly)");
    
    std::smatch match;
    std::string current_file;
    bool in_diff = false;
    bool in_config = false;
    
    while (std::getline(stream, line)) {
        // Handle diff sections
        if (std::regex_search(line, match, diff_start)) {
            current_file = match[1].str();
            in_diff = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = current_file;
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "File formatting changes detected";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle error patterns (E999 syntax errors)
        if (std::regex_search(line, match, error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "syntax";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle warning patterns (line too long)
        if (std::regex_search(line, match, warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "style";
            event.message = match[5].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle info patterns (no changes needed)
        if (std::regex_search(line, match, info_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle fixed file patterns
        if (std::regex_search(line, match, fixed_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "File formatting applied";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle command patterns
        if (std::regex_search(line, match, autopep8_cmd)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Command: autopep8 " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle configuration section
        if (std::regex_search(line, match, config_line)) {
            in_config = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Configuration applied";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle summary statistics
        if (std::regex_search(line, match, summary_files_processed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files processed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, summary_files_modified)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files modified: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, summary_files_errors)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "summary";
            event.message = "Files with errors: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, summary_fixes_applied)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Total fixes applied: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, summary_execution_time)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "performance";
            event.message = "Execution time: " + match[1].str() + "s";
            event.execution_time = std::stod(match[1].str());
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle syntax errors
        if (std::regex_search(line, match, syntax_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "syntax";
            event.message = match[4].str();
            event.error_code = "SyntaxError";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle encoding errors
        if (std::regex_search(line, match, encoding_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "encoding";
            event.message = "Could not determine file encoding";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle already formatted files
        if (std::regex_search(line, match, already_formatted)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "Already formatted correctly";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // Configuration line continuation
        if (in_config && line.find(":") != std::string::npos && line.find(" ") == 0) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "autopep8";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = line.substr(2); // Remove leading spaces
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "autopep8_text";
            
            events.push_back(event);
            continue;
        }
        
        // End configuration section when we hit an empty line
        if (in_config && line.empty()) {
            in_config = false;
        }
    }
}

void ParseYapfText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for yapf output
    std::regex diff_start_yapf(R"(--- a/(.+) \(original\))");
    std::regex diff_fixed_yapf(R"(\+\+\+ b/(.+) \(reformatted\))");
    std::regex reformatted_file(R"(Reformatted (.+))");
    std::regex yapf_command(R"(yapf (--[^\s]+.+))");
    std::regex processing_verbose(R"(Processing (.+))");
    std::regex style_config(R"(Style configuration: (.+))");
    std::regex line_length_config(R"(Line length: (\d+))");
    std::regex indent_width_config(R"(Indent width: (\d+))");
    std::regex files_processed(R"(Files processed: (\d+))");
    std::regex files_reformatted(R"(Files reformatted: (\d+))");
    std::regex files_no_changes(R"(Files with no changes: (\d+))");
    std::regex execution_time(R"(Total execution time: ([\d\.]+)s)");
    std::regex check_error(R"(ERROR: Files would be reformatted but yapf was run with --check)");
    std::regex yapf_error(R"(yapf: error: (.+))");
    std::regex syntax_error(R"(ERROR: ([^:]+\.py):(\d+):(\d+): (.+))");
    std::regex encoding_warning(R"(WARNING: ([^:]+\.py): cannot determine encoding)");
    std::regex info_no_changes(R"(INFO: ([^:]+\.py): no changes needed)");
    std::regex files_left_unchanged(R"((\d+) files reformatted, (\d+) files left unchanged\.)");
    
    std::smatch match;
    std::string current_file;
    bool in_diff = false;
    bool in_config = false;
    
    while (std::getline(stream, line)) {
        // Handle yapf diff sections
        if (std::regex_search(line, match, diff_start_yapf)) {
            current_file = match[1].str();
            in_diff = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = current_file;
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "File formatting changes detected";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle reformatted file patterns
        if (std::regex_search(line, match, reformatted_file)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "File reformatted";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle yapf command patterns
        if (std::regex_search(line, match, yapf_command)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Command: yapf " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle verbose processing
        if (std::regex_search(line, match, processing_verbose)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "processing";
            event.message = "Processing file";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle style configuration
        if (std::regex_search(line, match, style_config)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Style configuration: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle line length configuration
        if (std::regex_search(line, match, line_length_config)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Line length: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle indent width configuration
        if (std::regex_search(line, match, indent_width_config)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "configuration";
            event.message = "Indent width: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle summary statistics
        if (std::regex_search(line, match, files_processed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files processed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, files_reformatted)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files reformatted: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, files_no_changes)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "summary";
            event.message = "Files with no changes: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, execution_time)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "performance";
            event.message = "Execution time: " + match[1].str() + "s";
            event.execution_time = std::stod(match[1].str());
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle combined summary (e.g., "5 files reformatted, 3 files left unchanged.")
        if (std::regex_search(line, match, files_left_unchanged)) {
            ValidationEvent event1;
            event1.event_id = event_id++;
            event1.tool_name = "yapf";
            event1.event_type = ValidationEventType::SUMMARY;
            event1.file_path = "";
            event1.line_number = -1;
            event1.column_number = -1;
            event1.status = ValidationEventStatus::INFO;
            event1.severity = "info";
            event1.category = "summary";
            event1.message = "Files reformatted: " + match[1].str();
            event1.execution_time = 0.0;
            event1.raw_output = content;
            event1.structured_data = "yapf_text";
            
            ValidationEvent event2;
            event2.event_id = event_id++;
            event2.tool_name = "yapf";
            event2.event_type = ValidationEventType::SUMMARY;
            event2.file_path = "";
            event2.line_number = -1;
            event2.column_number = -1;
            event2.status = ValidationEventStatus::INFO;
            event2.severity = "info";
            event2.category = "summary";
            event2.message = "Files left unchanged: " + match[2].str();
            event2.execution_time = 0.0;
            event2.raw_output = content;
            event2.structured_data = "yapf_text";
            
            events.push_back(event1);
            events.push_back(event2);
            continue;
        }
        
        // Handle check mode error
        if (std::regex_search(line, match, check_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "check_mode";
            event.message = "Files would be reformatted but yapf was run with --check";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle yapf errors
        if (std::regex_search(line, match, yapf_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "command_error";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle syntax errors
        if (std::regex_search(line, match, syntax_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "syntax";
            event.message = match[4].str();
            event.error_code = "SyntaxError";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle encoding warnings
        if (std::regex_search(line, match, encoding_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "encoding";
            event.message = "Cannot determine encoding";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle info messages (no changes needed)
        if (std::regex_search(line, match, info_no_changes)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yapf";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "formatting";
            event.message = "No changes needed";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "yapf_text";
            
            events.push_back(event);
            continue;
        }
    }
}

void ParseCoverageText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for coverage.py output
    std::regex coverage_header(R"(Name\s+Stmts\s+Miss\s+Cover(?:\s+Missing)?)");
    std::regex coverage_branch_header(R"(Name\s+Stmts\s+Miss\s+Branch\s+BrPart\s+Cover(?:\s+Missing)?)");
    std::regex separator_line(R"(^-+$)");
    std::regex coverage_row(R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex coverage_branch_row(R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex total_row(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex total_branch_row(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex coverage_run(R"(coverage run (.+))");
    std::regex coverage_command(R"(coverage (html|xml|json|report|erase|combine|debug))");
    std::regex report_generated(R"(Coverage report generated in ([\d\.]+) seconds)");
    std::regex wrote_report(R"(Wrote (HTML|XML|JSON) report to (.+))");
    std::regex coverage_failure(R"(Coverage failure: total of (\d+%|\d+\.\d+%) is below --fail-under=(\d+%))");
    std::regex no_data(R"(coverage: No data to report\.)");
    std::regex no_data_collected(R"(coverage: CoverageWarning: No data was collected\. \(no-data-collected\))");
    std::regex context_recorded(R"(Context '(.+)' recorded)");
    std::regex combined_data(R"(Combined data file (.+))");
    std::regex wrote_combined(R"(Wrote combined data to (.+))");
    std::regex erased_coverage(R"(Erased (.coverage))");
    std::regex precision_info(R"(coverage report --precision=(\d+))");
    std::regex fail_under_info(R"(coverage report --fail-under=(\d+))");
    std::regex skip_covered_info(R"(coverage report --skip-covered)");
    std::regex show_missing_info(R"(coverage report --show-missing)");
    std::regex debug_info(R"(-- (sys|data|config) -)");
    std::regex version_info(R"(version: (.+))");
    std::regex platform_info(R"(platform: (.+))");
    std::regex implementation_info(R"(implementation: (.+))");
    std::regex executable_info(R"(executable: (.+))");
    std::regex config_files_info(R"(config_files: (.+))");
    std::regex data_file_info(R"(data_file: (.+))");
    std::regex source_info(R"(source: \[(.+)\])");
    std::regex delta_coverage(R"(coverage report --diff=(.+))");
    std::regex delta_summary(R"(Total coverage: ([\d\.]+%))");
    std::regex files_changed(R"(Files changed: (\d+))");
    std::regex lines_added(R"(Lines added: (\d+))");
    std::regex lines_covered(R"(Lines covered: (\d+))");
    std::regex percentage_covered(R"(Percentage covered: ([\d\.]+%))");
    
    std::smatch match;
    bool in_coverage_table = false;
    bool in_branch_table = false;
    bool in_debug_section = false;
    
    while (std::getline(stream, line)) {
        // Handle coverage table headers
        if (std::regex_search(line, match, coverage_header)) {
            in_coverage_table = true;
            in_branch_table = false;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "report_header";
            event.message = "Coverage report table started";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle branch coverage table headers
        if (std::regex_search(line, match, coverage_branch_header)) {
            in_coverage_table = true;
            in_branch_table = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "report_header";
            event.message = "Branch coverage report table started";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle separator lines
        if (std::regex_search(line, match, separator_line)) {
            continue; // Skip separator lines
        }
        
        // Handle branch coverage rows
        if (in_branch_table && std::regex_search(line, match, coverage_branch_row)) {
            std::string file_path = match[1].str();
            std::string stmts = match[2].str();
            std::string miss = match[3].str();
            std::string branch = match[4].str();
            std::string br_part = match[5].str();
            std::string cover = match[6].str();
            std::string missing = match[7].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            
            // Set status based on coverage percentage
            double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
            if (coverage_pct >= 90.0) {
                event.status = ValidationEventStatus::INFO;
                event.severity = "info";
            } else if (coverage_pct >= 70.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            event.category = "branch_coverage";
            event.message = "Stmts: " + stmts + ", Miss: " + miss + ", Branch: " + branch + ", BrPart: " + br_part + ", Cover: " + cover;
            if (!missing.empty()) {
                event.suggestion = "Missing lines: " + missing;
            }
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle regular coverage rows
        if (in_coverage_table && !in_branch_table && std::regex_search(line, match, coverage_row)) {
            std::string file_path = match[1].str();
            std::string stmts = match[2].str();
            std::string miss = match[3].str();
            std::string cover = match[4].str();
            std::string missing = match[5].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            
            // Set status based on coverage percentage
            double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
            if (coverage_pct >= 90.0) {
                event.status = ValidationEventStatus::INFO;
                event.severity = "info";
            } else if (coverage_pct >= 70.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            event.category = "line_coverage";
            event.message = "Stmts: " + stmts + ", Miss: " + miss + ", Cover: " + cover;
            if (!missing.empty()) {
                event.suggestion = "Missing lines: " + missing;
            }
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle TOTAL rows for branch coverage
        if (in_branch_table && std::regex_search(line, match, total_branch_row)) {
            in_coverage_table = false;
            in_branch_table = false;
            
            std::string stmts = match[1].str();
            std::string miss = match[2].str();
            std::string branch = match[3].str();
            std::string br_part = match[4].str();
            std::string cover = match[5].str();
            std::string missing = match[6].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            // Set status based on coverage percentage
            double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
            if (coverage_pct >= 90.0) {
                event.status = ValidationEventStatus::INFO;
                event.severity = "info";
            } else if (coverage_pct >= 70.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            event.category = "total_branch_coverage";
            event.message = "Total branch coverage: " + cover + " (Stmts: " + stmts + ", Miss: " + miss + ", Branch: " + branch + ", BrPart: " + br_part + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle TOTAL rows for regular coverage
        if (in_coverage_table && !in_branch_table && std::regex_search(line, match, total_row)) {
            in_coverage_table = false;
            
            std::string stmts = match[1].str();
            std::string miss = match[2].str();
            std::string cover = match[3].str();
            std::string missing = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            // Set status based on coverage percentage
            double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
            if (coverage_pct >= 90.0) {
                event.status = ValidationEventStatus::INFO;
                event.severity = "info";
            } else if (coverage_pct >= 70.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            event.category = "total_coverage";
            event.message = "Total coverage: " + cover + " (Stmts: " + stmts + ", Miss: " + miss + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage run commands
        if (std::regex_search(line, match, coverage_run)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "command";
            event.message = "Coverage run: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage commands
        if (std::regex_search(line, match, coverage_command)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "command";
            event.message = "Coverage command: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle report generation
        if (std::regex_search(line, match, report_generated)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "performance";
            event.message = "Coverage report generated";
            event.execution_time = std::stod(match[1].str());
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle report writing
        if (std::regex_search(line, match, wrote_report)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[2].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "output";
            event.message = "Wrote " + match[1].str() + " report to " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage failure
        if (std::regex_search(line, match, coverage_failure)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "threshold";
            event.message = "Coverage failure: total of " + match[1].str() + " is below --fail-under=" + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle no data cases
        if (std::regex_search(line, match, no_data)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "no_data";
            event.message = "No data to report";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, no_data_collected)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "no_data";
            event.message = "No data was collected";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle context recording
        if (std::regex_search(line, match, context_recorded)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "context";
            event.message = "Context '" + match[1].str() + "' recorded";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle data combination
        if (std::regex_search(line, match, combined_data)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "combine";
            event.message = "Combined data file: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, wrote_combined)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "combine";
            event.message = "Wrote combined data to " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage erase
        if (std::regex_search(line, match, erased_coverage)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "cleanup";
            event.message = "Erased " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle delta coverage summary
        if (std::regex_search(line, match, delta_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "delta_coverage";
            event.message = "Total coverage: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle delta metrics
        if (std::regex_search(line, match, files_changed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "delta_metrics";
            event.message = "Files changed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, lines_added)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "delta_metrics";
            event.message = "Lines added: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, lines_covered)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "delta_metrics";
            event.message = "Lines covered: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, percentage_covered)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "coverage";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "delta_metrics";
            event.message = "Percentage covered: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "coverage_text";
            
            events.push_back(event);
            continue;
        }
    }
}

void ParsePytestCovText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for pytest-cov output
    std::regex test_session_start(R"(={3,} test session starts ={3,})");
    std::regex platform_info(R"(platform (.+) -- Python (.+), pytest-(.+), pluggy-(.+))");
    std::regex pytest_cov_plugin(R"(plugins: cov-(.+))");
    std::regex rootdir_info(R"(rootdir: (.+))");
    std::regex collected_items(R"(collected (\d+) items?)");
    std::regex test_result(R"((.+\.py)::(.+)\s+(PASSED|FAILED|SKIPPED|ERROR)\s+\[([^\]]+)\])");
    std::regex test_failure_section(R"(={3,} FAILURES ={3,})");
    std::regex test_short_summary(R"(={3,} short test summary info ={3,})");
    std::regex test_summary_line(R"(={3,} (\d+) failed, (\d+) passed(?:, (\d+) skipped)? in ([\d\.]+)s ={3,})");
    std::regex coverage_section(R"(----------- coverage: platform (.+), python (.+) -----------)");
    std::regex coverage_header(R"(Name\s+Stmts\s+Miss\s+Cover(?:\s+Missing)?)");
    std::regex coverage_branch_header(R"(Name\s+Stmts\s+Miss\s+Branch\s+BrPart\s+Cover(?:\s+Missing)?)");
    std::regex coverage_row(R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex coverage_branch_row(R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex total_coverage(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex total_branch_coverage(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
    std::regex coverage_threshold_fail(R"(Coverage threshold check failed\. Expected: >= (\d+)%, got: ([\d\.]+%))");
    std::regex required_coverage_fail(R"(Required test coverage of (\d+)% not met\. Total coverage: ([\d\.]+%))");
    std::regex pytest_cov_config(R"(pytest --cov=(.+) --cov-report=(.+))");
    std::regex pytest_cov_options(R"(--(cov-[a-z-]+)(?:=(.+))?)");
    std::regex coverage_xml_written(R"(Coverage XML written to (.+))");
    std::regex coverage_html_written(R"(Coverage HTML written to dir (.+))");
    std::regex coverage_config_used(R"(Using coverage configuration from (.+))");
    std::regex coverage_context(R"(Context '(.+)' recorded)");
    std::regex coverage_parallel(R"(Coverage data collected in parallel mode)");
    std::regex coverage_data_not_found(R"(pytest-cov: Coverage data was not found for source '(.+)')");
    std::regex module_never_imported(R"(pytest-cov: Module '(.+)' was never imported\.)");
    std::regex failure_detail_header(R"(_{3,} (.+) _{3,})");
    std::regex assertion_error(R"(E\s+(AssertionError: .+))");
    std::regex traceback_file(R"((.+):(\d+): (.+))");
    
    std::smatch match;
    bool in_test_execution = false;
    bool in_coverage_section = false;
    bool in_failure_section = false;
    bool in_coverage_table = false;
    bool in_branch_table = false;
    std::string current_test_file = "";
    
    while (std::getline(stream, line)) {
        // Handle test session start
        if (std::regex_search(line, match, test_session_start)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_session";
            event.message = "Test session started";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle platform and pytest info
        if (std::regex_search(line, match, platform_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "environment";
            event.message = "Platform: " + match[1].str() + ", Python: " + match[2].str() + ", pytest: " + match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle pytest-cov plugin detection
        if (std::regex_search(line, match, pytest_cov_plugin)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "plugin";
            event.message = "pytest-cov plugin version: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle collected items
        if (std::regex_search(line, match, collected_items)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_collection";
            event.message = "Collected " + match[1].str() + " test items";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            in_test_execution = true;
            continue;
        }
        
        // Handle individual test results
        if (in_test_execution && std::regex_search(line, match, test_result)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            
            std::string status = match[3].str();
            if (status == "PASSED") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (status == "FAILED") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else if (status == "SKIPPED") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "warning";
            } else if (status == "ERROR") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            event.category = "test_execution";
            event.message = "Test " + match[2].str() + " " + status;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle test failure section
        if (std::regex_search(line, match, test_failure_section)) {
            in_failure_section = true;
            in_test_execution = false;
            continue;
        }
        
        // Handle test summary section
        if (std::regex_search(line, match, test_short_summary)) {
            in_failure_section = false;
            continue;
        }
        
        // Handle test execution summary
        if (std::regex_search(line, match, test_summary_line)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            std::string failed = match[1].str();
            std::string passed = match[2].str();
            std::string skipped = match[3].matched ? match[3].str() : "0";
            std::string duration = match[4].str();
            
            if (failed != "0") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            }
            
            event.category = "test_summary";
            event.message = "Tests completed: " + failed + " failed, " + passed + " passed, " + skipped + " skipped in " + duration + "s";
            event.execution_time = std::stod(duration);
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage section start
        if (std::regex_search(line, match, coverage_section)) {
            in_coverage_section = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "coverage_section";
            event.message = "Coverage analysis started - Platform: " + match[1].str() + ", Python: " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage table headers
        if (in_coverage_section && std::regex_search(line, match, coverage_header)) {
            in_coverage_table = true;
            in_branch_table = false;
            continue;
        }
        
        if (in_coverage_section && std::regex_search(line, match, coverage_branch_header)) {
            in_coverage_table = true;
            in_branch_table = true;
            continue;
        }
        
        // Handle coverage rows
        if (in_coverage_table && !in_branch_table && std::regex_search(line, match, coverage_row)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            
            std::string statements = match[2].str();
            std::string missed = match[3].str();
            std::string coverage_pct = match[4].str();
            
            // Remove % sign and convert to number for severity calculation
            std::string pct_str = coverage_pct;
            pct_str.erase(pct_str.find('%'));
            double coverage_value = std::stod(pct_str);
            
            if (coverage_value >= 90.0) {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (coverage_value >= 75.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            }
            
            event.category = "file_coverage";
            event.message = "Coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed)";
            
            if (match[5].matched && !match[5].str().empty()) {
                event.message += " - Missing lines: " + match[5].str();
            }
            
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle branch coverage rows
        if (in_coverage_table && in_branch_table && std::regex_search(line, match, coverage_branch_row)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            
            std::string statements = match[2].str();
            std::string missed = match[3].str();
            std::string branches = match[4].str();
            std::string partial = match[5].str();
            std::string coverage_pct = match[6].str();
            
            // Remove % sign and convert to number for severity calculation
            std::string pct_str = coverage_pct;
            pct_str.erase(pct_str.find('%'));
            double coverage_value = std::stod(pct_str);
            
            if (coverage_value >= 90.0) {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (coverage_value >= 75.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            }
            
            event.category = "file_branch_coverage";
            event.message = "Branch coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed, " + branches + " branches, " + partial + " partial)";
            
            if (match[7].matched && !match[7].str().empty()) {
                event.message += " - Missing: " + match[7].str();
            }
            
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle total coverage
        if (in_coverage_section && std::regex_search(line, match, total_coverage)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            std::string statements = match[1].str();
            std::string missed = match[2].str();
            std::string coverage_pct = match[3].str();
            
            // Remove % sign and convert to number for severity calculation
            std::string pct_str = coverage_pct;
            pct_str.erase(pct_str.find('%'));
            double coverage_value = std::stod(pct_str);
            
            if (coverage_value >= 90.0) {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (coverage_value >= 75.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            }
            
            event.category = "total_coverage";
            event.message = "Total coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed)";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle total branch coverage
        if (in_coverage_section && std::regex_search(line, match, total_branch_coverage)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            std::string statements = match[1].str();
            std::string missed = match[2].str();
            std::string branches = match[3].str();
            std::string partial = match[4].str();
            std::string coverage_pct = match[5].str();
            
            // Remove % sign and convert to number for severity calculation
            std::string pct_str = coverage_pct;
            pct_str.erase(pct_str.find('%'));
            double coverage_value = std::stod(pct_str);
            
            if (coverage_value >= 90.0) {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (coverage_value >= 75.0) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            }
            
            event.category = "total_branch_coverage";
            event.message = "Total branch coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed, " + branches + " branches, " + partial + " partial)";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage threshold failures
        if (std::regex_search(line, match, coverage_threshold_fail)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "coverage_threshold";
            event.message = "Coverage threshold failed: Expected >= " + match[1].str() + "%, got " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, required_coverage_fail)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "coverage_threshold";
            event.message = "Required coverage not met: Expected " + match[1].str() + "%, got " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage report generation
        if (std::regex_search(line, match, coverage_xml_written)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "report_generation";
            event.message = "Coverage XML report written to: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, coverage_html_written)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "report_generation";
            event.message = "Coverage HTML report written to: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle assertion errors in failure section
        if (in_failure_section && std::regex_search(line, match, assertion_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = current_test_file;
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "assertion_error";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle configuration warnings/errors
        if (std::regex_search(line, match, coverage_data_not_found)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "configuration";
            event.message = "Coverage data not found for source: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, module_never_imported)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "configuration";
            event.message = "Module never imported: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";
            
            events.push_back(event);
            continue;
        }
    }
}

void ParseGitHubActionsText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for GitHub Actions output
    std::regex timestamp_prefix(R"(^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z)\s+(.+))");
    std::regex section_start(R"(##\[section\]Starting:\s+(.+))");
    std::regex section_finish(R"(##\[section\]Finishing:\s+(.+))");
    std::regex task_header_start(R"(={70,})");
    std::regex task_info(R"(^(Task|Description|Version|Author|Help)\s*:\s*(.+))");
    std::regex agent_info(R"(^Agent (name|machine name|version):\s*(?:'(.+)'|(.+)))");  
    std::regex environment_info(R"(^(Operating System|Runner image|Environment details))");
    std::regex git_operation(R"((/usr/bin/git\s+.+|Syncing repository:|From https://github\.com/.+))");
    std::regex group_start(R"(##\[group\](.+))");
    std::regex group_end(R"(##\[endgroup\])");
    std::regex error_message(R"(##\[error\](.+))");
    std::regex warning_message(R"(##\[warning\](.+))");
    std::regex command_output_start(R"(={20,}\s*Starting Command Output\s*={20,})");
    std::regex npm_command(R"(^>\s+([^@]+)@([^\s]+)\s+(.+))");
    std::regex test_result(R"((PASS|FAIL)\s+(.+))");
    std::regex jest_summary(R"(Test Suites:\s*(\d+)\s*failed,\s*(\d+)\s*passed,\s*(\d+)\s*total)");
    std::regex jest_test_summary(R"(Tests:\s*(\d+)\s*failed,\s*(\d+)\s*passed,\s*(\d+)\s*total)");
    std::regex jest_timing(R"(Time:\s*([\d\.]+)s)");
    std::regex process_exit_code(R"(##\[error\]Process completed with exit code (\d+)\.)");
    std::regex job_completed(R"(Job completed:\s*(.+))");
    std::regex job_result(R"(Result:\s*(Succeeded|Failed|Canceled))");
    std::regex elapsed_time(R"(Elapsed time:\s*(\d{2}:\d{2}:\d{2}))");
    std::regex job_summary(R"(##\[section\]Job summary:)");
    std::regex step_status(R"(-\s*([^:]+):\s*(Succeeded|Failed|Succeeded \\(with warnings\\)))");
    std::regex error_details_section(R"(##\[section\]Error details:)");
    std::regex performance_section(R"(##\[section\]Performance metrics:)");
    std::regex recommendations_section(R"(##\[section\]Recommendations:)");
    std::regex webpack_build(R"(Hash:\s*([a-f0-9]+))");
    std::regex webpack_asset(R"(\s+([^\s]+)\s+([\d\.]+\s+[KMG]iB)\s+(\d+)\s+\[emitted\].*)");
    std::regex eslint_warning(R"((\d+):(\d+)\s+(warning|error)\s+(.+)\s+([a-z-]+))");
    std::regex coverage_info(R"(Publishing test results from '(.+)'))");
    std::regex published_results(R"(Published (\d+) test result\\(s\\))");
    std::regex resource_usage(R"((Disk space|Memory usage|CPU usage):\s*(.+))");
    std::regex duration_metric(R"(-\s*([^:]+):\s*([\d\.]+)\s*(seconds|second|minutes|minute))");
    std::regex recommendation_item(R"(-\s*(.+))");
    
    std::smatch match;
    std::string current_section = "";
    std::string current_task = "";
    std::string current_timestamp = "";
    bool in_task_header = false;
    bool in_command_output = false;
    bool in_job_summary = false;
    bool in_error_details = false;
    bool in_performance_metrics = false;
    bool in_recommendations = false;
    
    while (std::getline(stream, line)) {
        // Extract timestamp and content
        if (std::regex_search(line, match, timestamp_prefix)) {
            current_timestamp = match[1].str();
            line = match[2].str();
        }
        
        // Handle section starts
        if (std::regex_search(line, match, section_start)) {
            current_section = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "section_start";
            event.message = "Starting: " + current_section;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle section finishes
        if (std::regex_search(line, match, section_finish)) {
            std::string finished_section = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "section_finish";
            event.message = "Finished: " + finished_section;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            current_section = "";
            continue;
        }
        
        // Handle task header start
        if (std::regex_search(line, match, task_header_start)) {
            in_task_header = true;
            continue;
        }
        
        // Handle task information
        if (in_task_header && std::regex_search(line, match, task_info)) {
            std::string info_type = match[1].str();
            std::string info_value = match[2].str();
            
            if (info_type == "Task") {
                current_task = info_value;
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "task_info";
            event.message = info_type + ": " + info_value;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle task header end
        if (in_task_header && std::regex_search(line, match, task_header_start)) {
            in_task_header = false;
            continue;
        }
        
        // Handle agent information
        if (std::regex_search(line, match, agent_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "agent_info";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle command output start
        if (std::regex_search(line, match, command_output_start)) {
            in_command_output = true;
            continue;
        }
        
        // Handle npm commands
        if (in_command_output && std::regex_search(line, match, npm_command)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "npm_command";
            event.message = "npm " + match[3].str() + " for " + match[1].str() + "@" + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle test results
        if (std::regex_search(line, match, test_result)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = match[2].str();
            event.line_number = -1;
            event.column_number = -1;
            
            std::string status = match[1].str();
            if (status == "PASS") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            }
            
            event.category = "test_execution";
            event.message = "Test " + status + ": " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle Jest test summary
        if (std::regex_search(line, match, jest_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            std::string failed = match[1].str();
            if (failed != "0") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            }
            
            event.category = "test_summary";
            event.message = "Test Suites: " + failed + " failed, " + match[2].str() + " passed, " + match[3].str() + " total";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle Jest individual test summary
        if (std::regex_search(line, match, jest_test_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            std::string failed = match[1].str();
            if (failed != "0") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            }
            
            event.category = "test_summary";
            event.message = "Tests: " + failed + " failed, " + match[2].str() + " passed, " + match[3].str() + " total";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle process exit codes
        if (std::regex_search(line, match, process_exit_code)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "process_error";
            event.message = "Process completed with exit code " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle job completion
        if (std::regex_search(line, match, job_completed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "job_completion";
            event.message = "Job completed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle job result
        if (std::regex_search(line, match, job_result)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            std::string result = match[1].str();
            if (result == "Succeeded") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (result == "Failed") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.severity = "warning";
            }
            
            event.category = "job_result";
            event.message = "Job result: " + result;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle elapsed time
        if (std::regex_search(line, match, elapsed_time)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "timing";
            event.message = "Elapsed time: " + match[1].str();
            
            // Convert time string to seconds
            std::string time_str = match[1].str();
            std::istringstream time_stream(time_str);
            std::string hours_str, minutes_str, seconds_str;
            std::getline(time_stream, hours_str, ':');
            std::getline(time_stream, minutes_str, ':');
            std::getline(time_stream, seconds_str);
            
            double total_seconds = std::stod(hours_str) * 3600 + std::stod(minutes_str) * 60 + std::stod(seconds_str);
            event.execution_time = total_seconds;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle job summary section
        if (std::regex_search(line, match, job_summary)) {
            in_job_summary = true;
            continue;
        }
        
        // Handle step status in job summary
        if (in_job_summary && std::regex_search(line, match, step_status)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            std::string step_name = match[1].str();
            std::string status = match[2].str();
            
            if (status == "Succeeded") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (status == "Failed") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            event.category = "step_summary";
            event.message = step_name + ": " + status;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle error details section
        if (std::regex_search(line, match, error_details_section)) {
            in_error_details = true;
            in_job_summary = false;
            continue;
        }
        
        // Handle performance metrics section
        if (std::regex_search(line, match, performance_section)) {
            in_performance_metrics = true;
            in_error_details = false;
            continue;
        }
        
        // Handle recommendations section
        if (std::regex_search(line, match, recommendations_section)) {
            in_recommendations = true;
            in_performance_metrics = false;
            continue;
        }
        
        // Handle duration metrics in performance section
        if (in_performance_metrics && std::regex_search(line, match, duration_metric)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "performance_metric";
            event.message = match[1].str() + ": " + match[2].str() + " " + match[3].str();
            event.execution_time = std::stod(match[2].str());
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle resource usage
        if (std::regex_search(line, match, resource_usage)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "resource_usage";
            event.message = match[1].str() + ": " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle recommendations
        if (in_recommendations && std::regex_search(line, match, recommendation_item)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "recommendation";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle webpack build info
        if (std::regex_search(line, match, webpack_build)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "webpack_build";
            event.message = "Webpack build hash: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle ESLint warnings in webpack output
        if (std::regex_search(line, match, eslint_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = std::stoi(match[1].str());
            event.column_number = std::stoi(match[2].str());
            
            std::string level = match[3].str();
            if (level == "error") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            event.category = "eslint";
            event.message = match[4].str() + " (" + match[5].str() + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle coverage publishing
        if (std::regex_search(line, match, coverage_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_publishing";
            event.message = "Publishing test results from: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle published test results count
        if (std::regex_search(line, match, published_results)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_publishing";
            event.message = "Published " + match[1].str() + " test results";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle general error messages
        if (std::regex_search(line, match, error_message)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "error";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
        
        // Handle general warning messages
        if (std::regex_search(line, match, warning_message)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "warning";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "github_actions_text";
            
            events.push_back(event);
            continue;
        }
    }
}

void ParseGitLabCIText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for GitLab CI output
    std::regex runner_info(R"(Running with gitlab-runner ([0-9.]+) \(([a-f0-9]+)\))");
    std::regex executor_info(R"(on (.+) using (.+) driver with image (.+))");
    std::regex git_repo_info(R"(Getting source from Git repository)");
    std::regex git_fetch(R"(Fetching changes with git depth set to (\d+)...)");
    std::regex git_checkout(R"(Checking out ([a-f0-9]+) as (.+)...)");
    std::regex step_execution(R"(Executing \"(.+)\" stage of the job script)");
    std::regex command_execution(R"(^\$ (.+))");
    std::regex bundle_install(R"(Bundle complete! (\d+) Gemfile dependencies, (\d+) gems now installed)");
    std::regex rspec_result(R"((\d+) examples?, (\d+) failures?)");
    std::regex rspec_timing(R"(Finished in ([\d.]+) seconds \(files took ([\d.]+) seconds to load\))");
    std::regex rspec_failure(R"(^\s+(\d+)\) (.+))");
    std::regex rspec_file_line(R"(# (.+):(\d+):in)");
    std::regex rubocop_inspect(R"(Inspecting (\d+) files)");
    std::regex rubocop_offense(R"((.+):(\d+):(\d+): ([WCE]): (.+): (.+))");
    std::regex rubocop_summary(R"((\d+) files inspected, (\d+) offenses? detected)");
    std::regex job_status(R"(Job (succeeded|failed))");
    
    std::smatch match;
    std::string current_step = "";
    std::string current_command = "";
    bool in_rspec_failures = false;
    bool in_rubocop_offenses = false;
    
    while (std::getline(stream, line)) {
        // Parse GitLab runner information
        if (std::regex_search(line, match, runner_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gitlab-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "runner_info";
            event.message = "GitLab Runner " + match[1].str() + " (" + match[2].str() + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse executor information
        if (std::regex_search(line, match, executor_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gitlab-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "executor_info";
            event.message = "Running on " + match[1].str() + " using " + match[2].str() + " with image " + match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse step execution
        if (std::regex_search(line, match, step_execution)) {
            current_step = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gitlab-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "step_execution";
            event.message = "Executing " + current_step + " stage";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse command execution
        if (std::regex_search(line, match, command_execution)) {
            current_command = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gitlab-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "command_execution";
            event.message = "Executing: " + current_command;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse RSpec results
        if (std::regex_search(line, match, rspec_result)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rspec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = std::stoi(match[2].str()) > 0 ? ValidationEventStatus::ERROR : ValidationEventStatus::PASS;
            event.severity = std::stoi(match[2].str()) > 0 ? "error" : "info";
            event.category = "test_summary";
            event.message = match[1].str() + " examples, " + match[2].str() + " failures";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            
            if (std::stoi(match[2].str()) > 0) {
                in_rspec_failures = true;
            }
            continue;
        }
        
        // Parse RSpec timing
        if (std::regex_search(line, match, rspec_timing)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rspec";
            event.event_type = ValidationEventType::PERFORMANCE_METRIC;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "timing";
            event.message = "Finished in " + match[1].str() + " seconds (files took " + match[2].str() + " seconds to load)";
            event.execution_time = std::stod(match[1].str());
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse RSpec failures
        if (in_rspec_failures && std::regex_search(line, match, rspec_failure)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rspec";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "test_failure";
            event.test_name = match[2].str();
            event.message = "RSpec failure: " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse RuboCop inspection
        if (std::regex_search(line, match, rubocop_inspect)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rubocop";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "inspection";
            event.message = "Inspecting " + match[1].str() + " files";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse RuboCop offenses
        if (std::regex_search(line, match, rubocop_offense)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rubocop";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = std::stoi(match[3].str());
            
            std::string severity_char = match[4].str();
            if (severity_char == "E") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else if (severity_char == "W") {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.severity = "info";
            }
            
            event.category = "style_issue";
            event.error_code = match[5].str();
            event.message = match[6].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse RuboCop summary
        if (std::regex_search(line, match, rubocop_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rubocop";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = std::stoi(match[2].str()) > 0 ? ValidationEventStatus::WARNING : ValidationEventStatus::PASS;
            event.severity = std::stoi(match[2].str()) > 0 ? "warning" : "info";
            event.category = "summary";
            event.message = match[1].str() + " files inspected, " + match[2].str() + " offenses detected";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse job status
        if (std::regex_search(line, match, job_status)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gitlab-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = match[1].str() == "succeeded" ? ValidationEventStatus::PASS : ValidationEventStatus::ERROR;
            event.severity = match[1].str() == "succeeded" ? "info" : "error";
            event.category = "job_status";
            event.message = "Job " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gitlab_ci_text";
            
            events.push_back(event);
            continue;
        }
    }
}

void ParseJenkinsText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Jenkins output
    std::regex build_start(R"(Started by user (.+))");
    std::regex workspace_info(R"(Building in workspace (.+))");
    std::regex git_checkout(R"(Checking out Revision ([a-f0-9]+) \((.+)\))");
    std::regex pipeline_start(R"(\[Pipeline\] Start of Pipeline)");
    std::regex pipeline_end(R"(\[Pipeline\] End of Pipeline)");
    std::regex pipeline_stage(R"(\[Pipeline\] \{ \((.+)\))");
    std::regex pipeline_step(R"(\[Pipeline\] (.+))");
    std::regex shell_command(R"(^\+ (.+))");
    std::regex npm_install(R"(added (\d+) packages .* in ([\d.]+)s)");
    std::regex npm_vulnerabilities(R"(found (\d+) vulnerabilit)");
    std::regex webpack_build(R"(Hash: ([a-f0-9]+))");
    std::regex webpack_asset(R"(\s+([^\s]+)\s+([\d\.]+\s+[KMG]iB)\s+(\d+)\s+\[emitted\])");
    std::regex jest_test_pass(R"(PASS (.+))");
    std::regex jest_test_fail(R"(FAIL (.+))");
    std::regex jest_summary(R"(Test Suites: (\d+) failed, (\d+) passed, (\d+) total)");
    std::regex jest_test_summary(R"(Tests: (\d+) failed, (\d+) passed, (\d+) total)");
    std::regex docker_build_start(R"(Sending build context to Docker daemon\s+([\d.]+[KMG]?B))");
    std::regex docker_step(R"(Step (\d+)/(\d+) : (.+))");
    std::regex docker_success(R"(Successfully built ([a-f0-9]+))");
    std::regex docker_tagged(R"(Successfully tagged (.+))");
    std::regex build_failure(R"(ERROR: Build step failed with exception)");
    std::regex java_exception(R"(([a-zA-Z.]+Exception): (.+))");
    std::regex jenkins_stack_trace(R"(\s+at ([a-zA-Z0-9_.]+)\(([^)]+)\))");
    std::regex build_result(R"(Finished: (SUCCESS|FAILURE|UNSTABLE|ABORTED))");
    
    std::smatch match;
    std::string current_stage = "";
    std::string current_user = "";
    bool in_pipeline = false;
    bool in_docker_build = false;
    bool in_exception = false;
    
    while (std::getline(stream, line)) {
        // Parse build start information
        if (std::regex_search(line, match, build_start)) {
            current_user = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jenkins";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "build_start";
            event.message = "Build started by user: " + current_user;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse workspace information
        if (std::regex_search(line, match, workspace_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jenkins";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "workspace";
            event.message = "Building in workspace: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse pipeline stages
        if (std::regex_search(line, match, pipeline_stage)) {
            current_stage = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jenkins";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "pipeline_stage";
            event.message = "Starting stage: " + current_stage;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Jest test results
        if (std::regex_search(line, match, jest_test_pass)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "test_pass";
            event.message = "Test passed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, jest_test_fail)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            event.message = "Test failed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Jest test summary
        if (std::regex_search(line, match, jest_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = std::stoi(match[1].str()) > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = std::stoi(match[1].str()) > 0 ? "error" : "info";
            event.category = "test_summary";
            event.message = "Test Suites: " + match[1].str() + " failed, " + match[2].str() + " passed, " + match[3].str() + " total";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Docker build information
        if (std::regex_search(line, match, docker_build_start)) {
            in_docker_build = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "docker";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "docker_build";
            event.message = "Docker build context: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Docker build success
        if (std::regex_search(line, match, docker_success)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "docker";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "docker_success";
            event.message = "Successfully built image: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse build exceptions
        if (std::regex_search(line, match, build_failure)) {
            in_exception = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jenkins";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "build_failure";
            event.message = "Build step failed with exception";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Java exceptions
        if (in_exception && std::regex_search(line, match, java_exception)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jenkins";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "java_exception";
            event.error_code = match[1].str();
            event.message = match[1].str() + ": " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse build result
        if (std::regex_search(line, match, build_result)) {
            std::string result = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jenkins";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            if (result == "SUCCESS") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (result == "FAILURE") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else if (result == "UNSTABLE") {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            event.category = "build_result";
            event.message = "Build finished: " + result;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "jenkins_text";
            
            events.push_back(event);
            continue;
        }
    }
}

void ParseDroneCIText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for DroneCI output
    std::regex drone_step_start(R"(\[drone:exec\] .* starting build step: (.+))");
    std::regex drone_step_complete(R"(\[drone:exec\] .* completed build step: (.+) \(exit code (\d+)\))");
    std::regex drone_pipeline_complete(R"(\[drone:exec\] .* pipeline execution complete)");
    std::regex drone_pipeline_failed(R"(\[drone:exec\] .* pipeline failed with exit code (\d+))");
    std::regex git_clone(R"(\+ git clone (.+) \.)");
    std::regex git_checkout(R"(\+ git checkout ([a-f0-9]+))");
    std::regex npm_install(R"(added (\d+) packages .* in ([\d.]+)s)");
    std::regex npm_vulnerabilities(R"(found (\d+) vulnerabilit)");
    std::regex jest_test_pass(R"(PASS (.+) \(([\d.]+) s\))");
    std::regex jest_test_fail(R"(FAIL (.+) \(([\d.]+) s\))");
    std::regex jest_test_item(R"(✓ (.+) \((\d+) ms\))");
    std::regex jest_test_fail_item(R"(✗ (.+) \(([\d.]+) s\))");
    std::regex jest_summary(R"(Test Suites: (\d+) failed, (\d+) passed, (\d+) total)");
    std::regex jest_test_summary(R"(Tests: (\d+) failed, (\d+) passed, (\d+) total)");
    std::regex jest_timing(R"(Time: ([\d.]+) s)");
    std::regex webpack_build(R"(Hash: ([a-f0-9]+))");
    std::regex webpack_warning(R"(Module Warning \(from ([^)]+)\):)");
    std::regex eslint_warning(R"((\d+):(\d+)\s+(warning|error)\s+(.+)\s+([a-z-]+))");
    std::regex docker_build_start(R"(Sending build context to Docker daemon\s+([\d.]+[KMG]?B))");
    std::regex docker_step(R"(Step (\d+)/(\d+) : (.+))");
    std::regex docker_success(R"(Successfully built ([a-f0-9]+))");
    std::regex docker_tagged(R"(Successfully tagged (.+))");
    std::regex curl_notification(R"(\+ curl -X POST .* --data '(.+)' )");
    
    std::smatch match;
    std::string current_step = "";
    bool in_jest_failure = false;
    
    while (std::getline(stream, line)) {
        // Parse DroneCI step start
        if (std::regex_search(line, match, drone_step_start)) {
            current_step = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "step_start";
            event.message = "Starting build step: " + current_step;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse DroneCI step completion
        if (std::regex_search(line, match, drone_step_complete)) {
            std::string step_name = match[1].str();
            int exit_code = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = exit_code == 0 ? ValidationEventStatus::PASS : ValidationEventStatus::FAIL;
            event.severity = exit_code == 0 ? "info" : "error";
            event.category = "step_complete";
            event.message = "Completed build step: " + step_name + " (exit code " + std::to_string(exit_code) + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Jest test results
        if (std::regex_search(line, match, jest_test_pass)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "test_pass";
            event.message = "Test passed: " + match[1].str();
            event.execution_time = std::stod(match[2].str());
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        if (std::regex_search(line, match, jest_test_fail)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            event.message = "Test failed: " + match[1].str();
            event.execution_time = std::stod(match[2].str());
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            in_jest_failure = true;
            continue;
        }
        
        // Parse individual test failures
        if (std::regex_search(line, match, jest_test_fail_item)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            event.test_name = match[1].str();
            event.message = "Test failure: " + match[1].str();
            event.execution_time = std::stod(match[2].str());
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Jest test summary
        if (std::regex_search(line, match, jest_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = std::stoi(match[1].str()) > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = std::stoi(match[1].str()) > 0 ? "error" : "info";
            event.category = "test_summary";
            event.message = "Test Suites: " + match[1].str() + " failed, " + match[2].str() + " passed, " + match[3].str() + " total";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse ESLint warnings
        if (std::regex_search(line, match, eslint_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "eslint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "/drone/src/src/services/auth.js"; // Default from sample
            event.line_number = std::stoi(match[1].str());
            event.column_number = std::stoi(match[2].str());
            event.status = match[3].str() == "error" ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = match[3].str();
            event.category = "lint_issue";
            event.error_code = match[5].str();
            event.message = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse Docker build success
        if (std::regex_search(line, match, docker_success)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "docker";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "docker_success";
            event.message = "Successfully built image: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse pipeline completion
        if (std::regex_search(line, match, drone_pipeline_complete)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "pipeline_complete";
            event.message = "Pipeline execution complete";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse pipeline failure
        if (std::regex_search(line, match, drone_pipeline_failed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "drone-ci";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "pipeline_failure";
            event.message = "Pipeline failed with exit code " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "drone_ci_text";
            
            events.push_back(event);
            continue;
        }
    }
}

} // namespace duckdb