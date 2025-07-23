#include "include/read_test_results_function.hpp"
#include "include/validation_event_types.hpp"
#include "core/parser_registry.hpp"
#include "parsers/test_frameworks/junit_text_parser.hpp"
#include "parsers/test_frameworks/rspec_text_parser.hpp"
#include "parsers/test_frameworks/mocha_chai_text_parser.hpp"
#include "parsers/test_frameworks/gtest_text_parser.hpp"
#include "parsers/test_frameworks/nunit_xunit_text_parser.hpp"
#include "parsers/specialized/valgrind_parser.hpp"
#include "parsers/specialized/gdb_lldb_parser.hpp"
#include "parsers/specialized/coverage_parser.hpp"
#include "parsers/build_systems/maven_parser.hpp"
#include "parsers/build_systems/gradle_parser.hpp"
#include "parsers/build_systems/msbuild_parser.hpp"
#include "parsers/build_systems/node_parser.hpp"
#include "parsers/build_systems/python_parser.hpp"
#include "parsers/build_systems/cargo_parser.hpp"
#include "parsers/build_systems/cmake_parser.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/file_glob_options.hpp"
#include "yyjson.hpp"
#include <fstream>
#include <regex>
#include <sstream>
#include <map>
#include <algorithm>
#include <cctype>
#include <functional>

namespace duckdb {

using namespace duckdb_yyjson;

// Phase 3B: Error Pattern Analysis Functions

// Normalize error message for fingerprinting by removing variable content
std::string NormalizeErrorMessage(const std::string& message) {
    std::string normalized = message;
    
    // Convert to lowercase for case-insensitive comparison
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Remove file paths (anything that looks like a path)
    normalized = std::regex_replace(normalized, std::regex(R"([/\\][\w/\\.-]+\.(cpp|hpp|py|js|java|go|rs|rb|php|c|h)[:\s])"), " <file> ");
    normalized = std::regex_replace(normalized, std::regex(R"(/[\w/.-]+/)"), "/<path>/");
    normalized = std::regex_replace(normalized, std::regex(R"(\\[\w\\.-]+\\)"), "\\<path>\\");
    
    // Remove timestamps
    normalized = std::regex_replace(normalized, std::regex(R"(\d{4}-\d{2}-\d{2}[T\s]\d{2}:\d{2}:\d{2})"), "<timestamp>");
    normalized = std::regex_replace(normalized, std::regex(R"(\d{2}:\d{2}:\d{2})"), "<time>");
    
    // Remove line and column numbers
    normalized = std::regex_replace(normalized, std::regex(R"(:(\d+):(\d+):)"), ":<line>:<col>:");
    normalized = std::regex_replace(normalized, std::regex(R"(line\s+\d+)"), "line <num>");
    normalized = std::regex_replace(normalized, std::regex(R"(column\s+\d+)"), "column <num>");
    
    // Remove numeric IDs and memory addresses
    normalized = std::regex_replace(normalized, std::regex(R"(0x[0-9a-fA-F]+)"), "<addr>");
    normalized = std::regex_replace(normalized, std::regex(R"(\b\d{6,}\b)"), "<id>");
    
    // Remove variable names in quotes
    normalized = std::regex_replace(normalized, std::regex(R"('[\w.-]+')"), "'<var>'");
    normalized = std::regex_replace(normalized, std::regex(R"("[\w.-]+")"), "\"<var>\"");
    
    // Remove specific values but keep structure
    normalized = std::regex_replace(normalized, std::regex(R"(\b\d+\.\d+\b)"), "<decimal>");
    normalized = std::regex_replace(normalized, std::regex(R"(\b\d+\b)"), "<num>");
    
    // Normalize whitespace
    normalized = std::regex_replace(normalized, std::regex(R"(\s+)"), " ");
    
    // Trim whitespace
    auto start = normalized.find_first_not_of(" \t");
    auto end = normalized.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        normalized = normalized.substr(start, end - start + 1);
    }
    
    return normalized;
}

// Generate a fingerprint for an error based on normalized message and context
std::string GenerateErrorFingerprint(const ValidationEvent& event) {
    std::string normalized = NormalizeErrorMessage(event.message);
    
    // Create a composite fingerprint including tool and category context
    std::string fingerprint_source = event.tool_name + ":" + event.category + ":" + normalized;
    
    // Simple hash - in production, consider using a proper hash function
    std::hash<std::string> hasher;
    size_t hash_value = hasher(fingerprint_source);
    
    // Convert to hex string for readability
    std::stringstream ss;
    ss << std::hex << hash_value;
    
    return event.tool_name + "_" + event.category + "_" + ss.str();
}

// Calculate similarity between two error messages using simple edit distance approach
double CalculateMessageSimilarity(const std::string& msg1, const std::string& msg2) {
    std::string norm1 = NormalizeErrorMessage(msg1);
    std::string norm2 = NormalizeErrorMessage(msg2);
    
    if (norm1.empty() && norm2.empty()) return 1.0;
    if (norm1.empty() || norm2.empty()) return 0.0;
    if (norm1 == norm2) return 1.0;
    
    // Simple Levenshtein distance approximation
    size_t len1 = norm1.length();
    size_t len2 = norm2.length();
    size_t max_len = std::max(len1, len2);
    
    // Count common substrings for a simple similarity metric
    size_t common_chars = 0;
    size_t min_len = std::min(len1, len2);
    
    for (size_t i = 0; i < min_len; i++) {
        if (norm1[i] == norm2[i]) {
            common_chars++;
        }
    }
    
    // Add bonus for common keywords
    std::vector<std::string> keywords = {"error", "warning", "failed", "exception", "timeout", "permission", "not found"};
    size_t keyword_matches = 0;
    
    for (const auto& keyword : keywords) {
        if (norm1.find(keyword) != std::string::npos && norm2.find(keyword) != std::string::npos) {
            keyword_matches++;
        }
    }
    
    double base_similarity = static_cast<double>(common_chars) / max_len;
    double keyword_bonus = static_cast<double>(keyword_matches) * 0.1;
    
    return std::min(1.0, base_similarity + keyword_bonus);
}

// Detect root cause category based on error content and context
std::string DetectRootCauseCategory(const ValidationEvent& event) {
    std::string message_lower = event.message;
    std::transform(message_lower.begin(), message_lower.end(), message_lower.begin(), ::tolower);
    
    // Network-related errors
    if (message_lower.find("connection") != std::string::npos ||
        message_lower.find("timeout") != std::string::npos ||
        message_lower.find("unreachable") != std::string::npos ||
        message_lower.find("network") != std::string::npos ||
        message_lower.find("dns") != std::string::npos) {
        return "network";
    }
    
    // Permission and access errors
    if (message_lower.find("permission") != std::string::npos ||
        message_lower.find("access denied") != std::string::npos ||
        message_lower.find("unauthorized") != std::string::npos ||
        message_lower.find("forbidden") != std::string::npos ||
        message_lower.find("authentication") != std::string::npos) {
        return "permission";
    }
    
    // Configuration errors
    if (message_lower.find("config") != std::string::npos ||
        message_lower.find("invalid resource") != std::string::npos ||
        message_lower.find("not found") != std::string::npos ||
        message_lower.find("does not exist") != std::string::npos ||
        message_lower.find("missing") != std::string::npos) {
        return "configuration";
    }
    
    // Resource errors (memory, disk, etc.)
    if (message_lower.find("memory") != std::string::npos ||
        message_lower.find("disk") != std::string::npos ||
        message_lower.find("space") != std::string::npos ||
        message_lower.find("quota") != std::string::npos ||
        message_lower.find("limit") != std::string::npos) {
        return "resource";
    }
    
    // Syntax and validation errors
    if (message_lower.find("syntax") != std::string::npos ||
        message_lower.find("parse") != std::string::npos ||
        message_lower.find("invalid") != std::string::npos ||
        message_lower.find("format") != std::string::npos ||
        event.event_type == ValidationEventType::LINT_ISSUE ||
        event.event_type == ValidationEventType::TYPE_ERROR) {
        return "syntax";
    }
    
    // Build and dependency errors
    if (message_lower.find("build") != std::string::npos ||
        message_lower.find("compile") != std::string::npos ||
        message_lower.find("dependency") != std::string::npos ||
        message_lower.find("package") != std::string::npos ||
        event.event_type == ValidationEventType::BUILD_ERROR) {
        return "build";
    }
    
    // Test-specific errors
    if (event.event_type == ValidationEventType::TEST_RESULT) {
        return "test_logic";
    }
    
    // Default category
    return "unknown";
}

// Process events to generate Phase 3B error pattern metadata
void ProcessErrorPatterns(std::vector<ValidationEvent>& events) {
    // Step 1: Generate fingerprints and root cause categories for each event
    for (auto& event : events) {
        event.error_fingerprint = GenerateErrorFingerprint(event);
        event.root_cause_category = DetectRootCauseCategory(event);
    }
    
    // Step 2: Assign pattern IDs based on fingerprint clustering
    std::map<std::string, int64_t> fingerprint_to_pattern_id;
    int64_t next_pattern_id = 1;
    
    for (auto& event : events) {
        if (fingerprint_to_pattern_id.find(event.error_fingerprint) == fingerprint_to_pattern_id.end()) {
            fingerprint_to_pattern_id[event.error_fingerprint] = next_pattern_id++;
        }
        event.pattern_id = fingerprint_to_pattern_id[event.error_fingerprint];
    }
    
    // Step 3: Calculate similarity scores within pattern groups
    for (auto& event : events) {
        if (event.pattern_id == -1) continue;
        
        // Find the representative message for this pattern (first occurrence)
        std::string representative_message;
        for (const auto& other_event : events) {
            if (other_event.pattern_id == event.pattern_id) {
                representative_message = other_event.message;
                break;
            }
        }
        
        // Calculate similarity to representative message
        if (!representative_message.empty()) {
            event.similarity_score = CalculateMessageSimilarity(event.message, representative_message);
        }
    }
}

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
    
    // GitHub CLI detection moved to GitHubCliParser::canParse()
    
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
    
    // Check for Terraform patterns (check before general build patterns since terraform can contain similar keywords)
    if ((content.find("Terraform v") != std::string::npos && content.find("provider registry.terraform.io") != std::string::npos) ||
        (content.find("Terraform will perform the following actions:") != std::string::npos) ||
        (content.find("Resource actions are indicated with the following symbols:") != std::string::npos) ||
        (content.find("Plan:") != std::string::npos && content.find("to add,") != std::string::npos && content.find("to change,") != std::string::npos && content.find("to destroy") != std::string::npos) ||
        (content.find("Apply complete! Resources:") != std::string::npos && content.find("added,") != std::string::npos) ||
        (content.find("# ") != std::string::npos && content.find(" will be created") != std::string::npos) ||
        (content.find("# ") != std::string::npos && content.find(" will be updated in-place") != std::string::npos) ||
        (content.find("# ") != std::string::npos && content.find(" will be destroyed") != std::string::npos) ||
        (content.find("terraform apply") != std::string::npos && content.find("Enter a value:") != std::string::npos)) {
        return TestResultFormat::TERRAFORM_TEXT;
    }
    
    // Check for Ansible patterns (check before general build patterns since ansible can contain similar keywords)
    if ((content.find("PLAY [") != std::string::npos && content.find("] ****") != std::string::npos) ||
        (content.find("TASK [") != std::string::npos && content.find("] ****") != std::string::npos) ||
        (content.find("PLAY RECAP") != std::string::npos && content.find("ok=") != std::string::npos && content.find("changed=") != std::string::npos) ||
        (content.find("fatal: [") != std::string::npos && content.find("]: FAILED!") != std::string::npos) ||
        (content.find("fatal: [") != std::string::npos && content.find("]: UNREACHABLE!") != std::string::npos) ||
        (content.find("changed: [") != std::string::npos && content.find("]") != std::string::npos) ||
        (content.find("ok: [") != std::string::npos && content.find("]") != std::string::npos) ||
        (content.find("skipping: [") != std::string::npos && content.find("]") != std::string::npos) ||
        (content.find("RUNNING HANDLER [") != std::string::npos && content.find("] ****") != std::string::npos) ||
        (content.find("FAILED - RETRYING:") != std::string::npos && content.find("retries left") != std::string::npos)) {
        return TestResultFormat::ANSIBLE_TEXT;
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
        (content.find(".py:") != std::string::npos && content.find(":") != std::string::npos && 
         (content.find(": F") != std::string::npos || content.find(": E") != std::string::npos || 
          content.find(": W") != std::string::npos || content.find(": C") != std::string::npos)) ||
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
    
    // Check for make error patterns (must be before mypy since make errors can contain ": error:" and brackets)
    if (content.find("make: ***") != std::string::npos && content.find("Error") != std::string::npos) {
        return TestResultFormat::MAKE_ERROR;
    }
    
    // Clang-tidy detection moved to ClangTidyParser::canParse()

    // Check for mypy patterns (more specific to avoid conflicts with clang-tidy)
    // First exclude clang-tidy patterns
    if (content.find("readability-") == std::string::npos && 
        content.find("bugprone-") == std::string::npos && 
        content.find("cppcoreguidelines-") == std::string::npos &&
        content.find("google-build") == std::string::npos && 
        content.find("performance-") == std::string::npos && 
        content.find("modernize-") == std::string::npos &&
        content.find("warnings generated") == std::string::npos &&
        content.find("errors generated") == std::string::npos) {
        
        if (((content.find(": error:") != std::string::npos || content.find(": warning:") != std::string::npos) && 
             content.find("[") != std::string::npos && content.find("]") != std::string::npos) ||
            (content.find(": note:") != std::string::npos && content.find("Revealed type") != std::string::npos) ||
            (content.find("Found") != std::string::npos && content.find("error") != std::string::npos && content.find("files") != std::string::npos && content.find("checked") != std::string::npos) ||
            (content.find("Success: no issues found") != std::string::npos) ||
            (content.find("return-value") != std::string::npos || content.find("arg-type") != std::string::npos || content.find("attr-defined") != std::string::npos)) {
            return TestResultFormat::MYPY_TEXT;
        }
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
    
    // Check for pytest text patterns (file.py::test_name with PASSED/FAILED/SKIPPED)
    if (content.find("::") != std::string::npos && 
        (content.find("PASSED") != std::string::npos || 
         content.find("FAILED") != std::string::npos || 
         content.find("SKIPPED") != std::string::npos)) {
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
    
    if (content.find(": error:") != std::string::npos || content.find(": warning:") != std::string::npos) {
        return TestResultFormat::GENERIC_LINT;
    }
    
    // Fallback: try the modular parser registry for newly added parsers
    auto& registry = ParserRegistry::getInstance();
    auto parser = registry.findParser(content);
    if (parser) {
        return parser->getFormat();
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
        case TestResultFormat::GITHUB_CLI: return "github_cli";
        case TestResultFormat::CLANG_TIDY_TEXT: return "clang_tidy_text";
        case TestResultFormat::GITLAB_CI_TEXT: return "gitlab_ci_text";
        case TestResultFormat::JENKINS_TEXT: return "jenkins_text";
        case TestResultFormat::DRONE_CI_TEXT: return "drone_ci_text";
        case TestResultFormat::TERRAFORM_TEXT: return "terraform_text";
        case TestResultFormat::ANSIBLE_TEXT: return "ansible_text";
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
    if (str == "github_cli") return TestResultFormat::GITHUB_CLI;
    if (str == "clang_tidy_text") return TestResultFormat::CLANG_TIDY_TEXT;
    if (str == "gitlab_ci_text") return TestResultFormat::GITLAB_CI_TEXT;
    if (str == "jenkins_text") return TestResultFormat::JENKINS_TEXT;
    if (str == "drone_ci_text") return TestResultFormat::DRONE_CI_TEXT;
    if (str == "terraform_text") return TestResultFormat::TERRAFORM_TEXT;
    if (str == "ansible_text") return TestResultFormat::ANSIBLE_TEXT;
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
        LogicalType::VARCHAR,  // structured_data
        // Phase 3A: Multi-file processing metadata
        LogicalType::VARCHAR,  // source_file
        LogicalType::VARCHAR,  // build_id
        LogicalType::VARCHAR,  // environment
        LogicalType::BIGINT,   // file_index
        // Phase 3B: Error pattern analysis
        LogicalType::VARCHAR,  // error_fingerprint
        LogicalType::DOUBLE,   // similarity_score
        LogicalType::BIGINT,   // pattern_id
        LogicalType::VARCHAR   // root_cause_category
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number",
        "column_number", "function_name", "status", "severity", "category",
        "message", "suggestion", "error_code", "test_name", "execution_time",
        "raw_output", "structured_data",
        // Phase 3A: Multi-file processing metadata
        "source_file", "build_id", "environment", "file_index",
        // Phase 3B: Error pattern analysis
        "error_fingerprint", "similarity_score", "pattern_id", "root_cause_category"
    };
    
    return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ReadTestResultsInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<ReadTestResultsBindData>();
    auto global_state = make_uniq<ReadTestResultsGlobalState>();
    
    // Phase 3A: Check if source contains glob patterns or multiple files
    auto &fs = FileSystem::GetFileSystem(context);
    std::vector<std::string> files;
    
    try {
        // Try to expand the source as a glob pattern or file list
        files = GetFilesFromPattern(context, bind_data.source);
    } catch (const IOException&) {
        // If glob expansion fails, treat as single file or direct content
        files.clear();
    }
    
    if (files.size() > 1) {
        // Multi-file processing path
        ProcessMultipleFiles(context, files, bind_data.format, global_state->events);
    } else {
        // Single file processing path (original behavior)
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
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                    // Note: Legacy ParsePytestJSON removed - fully replaced by modular PytestJSONParser
                }
                break;
            case TestResultFormat::DUCKDB_TEST:
                ParseDuckDBTestOutput(content, global_state->events);
                break;
            case TestResultFormat::ESLINT_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        // Insert parsed events into the global state
                        // (removed debugging throws - parser registration fix worked!)
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("ESLint modular parser NOT found in registry");
                    }
                }
                break;
            case TestResultFormat::GOTEST_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                    // Note: Legacy ParseGoTestJSON removed - fully replaced by modular GoTestJSONParser
                }
                break;
            case TestResultFormat::MAKE_ERROR:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                    // Note: Legacy ParseMakeErrors removed - fully replaced by modular MakeParser
                }
                break;
            case TestResultFormat::PYTEST_TEXT:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        // Fallback to legacy parser if modular parser not found
                        ParsePytestText(content, global_state->events);
                    }
                }
                break;
            case TestResultFormat::GENERIC_LINT:
                ParseGenericLint(content, global_state->events);
                break;
            case TestResultFormat::RUBOCOP_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                    // Note: Legacy ParseRuboCopJSON removed - fully replaced by modular RuboCopJSONParser
                }
                break;
            case TestResultFormat::CARGO_TEST_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                    // Note: Legacy ParseCargoTestJSON removed - fully replaced by modular CargoTestJSONParser
                }
                break;
            case TestResultFormat::SWIFTLINT_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                    // Note: Legacy ParseSwiftLintJSON removed - fully replaced by modular SwiftLintJSONParser
                }
                break;
            case TestResultFormat::PHPSTAN_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                    // Note: Legacy ParsePHPStanJSON removed - fully replaced by modular PHPStanJSONParser
                }
                break;
            case TestResultFormat::SHELLCHECK_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("ShellCheck JSON modular parser not found in registry");
                    }
                }
                break;
            case TestResultFormat::STYLELINT_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("Stylelint JSON modular parser not found in registry");
                    }
                }
                break;
            case TestResultFormat::CLIPPY_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("Clippy JSON modular parser not found in registry");
                    }
                }
                break;
            case TestResultFormat::MARKDOWNLINT_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("Markdownlint JSON modular parser not found in registry");
                    }
                }
                break;
            case TestResultFormat::YAMLLINT_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("Yamllint JSON modular parser not found in registry");
                    }
                }
                break;
            case TestResultFormat::BANDIT_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("Bandit JSON modular parser not found in registry");
                    }
                }
                break;
            case TestResultFormat::SPOTBUGS_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("SpotBugs JSON modular parser not found in registry");
                    }
                }
                break;
            case TestResultFormat::KTLINT_JSON:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    } else {
                        throw std::runtime_error("Ktlint JSON modular parser not found in registry");
                    }
                }
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
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                }
                break;
            case TestResultFormat::CMAKE_BUILD:
                duck_hunt::CMakeParser::ParseCMakeBuild(content, global_state->events);
                break;
            case TestResultFormat::PYTHON_BUILD:
                duck_hunt::PythonParser::ParsePythonBuild(content, global_state->events);
                break;
            case TestResultFormat::NODE_BUILD:
                duck_hunt::NodeParser::ParseNodeBuild(content, global_state->events);
                break;
            case TestResultFormat::CARGO_BUILD:
                duck_hunt::CargoParser::ParseCargoBuild(content, global_state->events);
                break;
            case TestResultFormat::MAVEN_BUILD:
                duck_hunt::MavenParser::ParseMavenBuild(content, global_state->events);
                break;
            case TestResultFormat::GRADLE_BUILD:
                duck_hunt::GradleParser::ParseGradleBuild(content, global_state->events);
                break;
            case TestResultFormat::MSBUILD:
                duck_hunt::MSBuildParser::ParseMSBuild(content, global_state->events);
                break;
            case TestResultFormat::JUNIT_TEXT:
                duck_hunt::JUnitTextParser::ParseJUnitText(content, global_state->events);
                break;
            case TestResultFormat::VALGRIND:
                duck_hunt::ValgrindParser::ParseValgrind(content, global_state->events);
                break;
            case TestResultFormat::GDB_LLDB:
                duck_hunt::GdbLldbParser::ParseGdbLldb(content, global_state->events);
                break;
            case TestResultFormat::RSPEC_TEXT:
                duck_hunt::RSpecTextParser::ParseRSpecText(content, global_state->events);
                break;
            case TestResultFormat::MOCHA_CHAI_TEXT:
                duck_hunt::MochaChaiTextParser::ParseMochaChai(content, global_state->events);
                break;
            case TestResultFormat::GTEST_TEXT:
                duck_hunt::GTestTextParser::ParseGoogleTest(content, global_state->events);
                break;
            case TestResultFormat::NUNIT_XUNIT_TEXT:
                duck_hunt::NUnitXUnitTextParser::ParseNUnitXUnit(content, global_state->events);
                break;
            case TestResultFormat::PYLINT_TEXT:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                }
                break;
            case TestResultFormat::FLAKE8_TEXT:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                }
                break;
            case TestResultFormat::BLACK_TEXT:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                }
                break;
            case TestResultFormat::MYPY_TEXT:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                }
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
                duck_hunt::CoverageParser::ParseCoverageText(content, global_state->events);
                break;
            case TestResultFormat::PYTEST_COV_TEXT:
                duck_hunt::CoverageParser::ParsePytestCovText(content, global_state->events);
                break;
            case TestResultFormat::GITHUB_ACTIONS_TEXT:
                ParseGitHubActionsText(content, global_state->events);
                break;
            case TestResultFormat::GITHUB_CLI:
            case TestResultFormat::CLANG_TIDY_TEXT:
                // These formats are handled by the modular parser registry at the end
                break;
            case TestResultFormat::GITLAB_CI_TEXT:
                ParseGitLabCIText(content, global_state->events);
                break;
            case TestResultFormat::JENKINS_TEXT:
                ParseJenkinsText(content, global_state->events);
                break;
            case TestResultFormat::DRONE_CI_TEXT:
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                }
                break;
            case TestResultFormat::TERRAFORM_TEXT:
                ParseTerraformText(content, global_state->events);
                break;
            case TestResultFormat::ANSIBLE_TEXT:
                ParseAnsibleText(content, global_state->events);
                break;
            default:
                // Try the modular parser registry for new parsers
                {
                    auto& registry = ParserRegistry::getInstance();
                    auto parser = registry.getParser(format);
                    if (parser) {
                        auto events = parser->parse(content);
                        global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                    }
                }
                break;
        }
        
        // For single files, populate basic metadata
        if (!global_state->events.empty()) {
            for (auto& event : global_state->events) {
                if (event.source_file.empty()) {
                    event.source_file = bind_data.source;
                    event.build_id = ExtractBuildIdFromPath(bind_data.source);
                    event.environment = ExtractEnvironmentFromPath(bind_data.source);
                    event.file_index = 0;
                }
            }
        }
    }
    
    // Phase 3B: Process error patterns for intelligent categorization
    ProcessErrorPatterns(global_state->events);
    
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
        // Phase 3A: Multi-file processing metadata
        output.SetValue(col++, i, Value(event.source_file));
        output.SetValue(col++, i, event.build_id.empty() ? Value() : Value(event.build_id));
        output.SetValue(col++, i, event.environment.empty() ? Value() : Value(event.environment));
        output.SetValue(col++, i, event.file_index == -1 ? Value() : Value::BIGINT(event.file_index));
        // Phase 3B: Error pattern analysis
        output.SetValue(col++, i, event.error_fingerprint.empty() ? Value() : Value(event.error_fingerprint));
        output.SetValue(col++, i, event.similarity_score == 0.0 ? Value() : Value::DOUBLE(event.similarity_score));
        output.SetValue(col++, i, event.pattern_id == -1 ? Value() : Value::BIGINT(event.pattern_id));
        output.SetValue(col++, i, event.root_cause_category.empty() ? Value() : Value(event.root_cause_category));
    }
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
        LogicalType::VARCHAR,  // structured_data
        // Phase 3A: Multi-file processing metadata
        LogicalType::VARCHAR,  // source_file
        LogicalType::VARCHAR,  // build_id
        LogicalType::VARCHAR,  // environment
        LogicalType::BIGINT,   // file_index
        // Phase 3B: Error pattern analysis
        LogicalType::VARCHAR,  // error_fingerprint
        LogicalType::DOUBLE,   // similarity_score
        LogicalType::BIGINT,   // pattern_id
        LogicalType::VARCHAR   // root_cause_category
    };
    
    names = {
        "event_id", "tool_name", "event_type", "file_path", "line_number",
        "column_number", "function_name", "status", "severity", "category",
        "message", "suggestion", "error_code", "test_name", "execution_time",
        "raw_output", "structured_data",
        // Phase 3A: Multi-file processing metadata
        "source_file", "build_id", "environment", "file_index",
        // Phase 3B: Error pattern analysis
        "error_fingerprint", "similarity_score", "pattern_id", "root_cause_category"
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
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
                // Note: Legacy ParsePytestJSON removed - fully replaced by modular PytestJSONParser
            }
            break;
        case TestResultFormat::DUCKDB_TEST:
            ParseDuckDBTestOutput(content, global_state->events);
            break;
        case TestResultFormat::ESLINT_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    // Insert parsed events into the global state
                    // (removed debugging throws - parser registration fix worked!)
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: ESLint modular parser NOT found in registry");
                }
            }
            break;
        case TestResultFormat::GOTEST_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
                // Note: Legacy ParseGoTestJSON removed - fully replaced by modular GoTestJSONParser
            }
            break;
        case TestResultFormat::MAKE_ERROR:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
                // Note: Legacy ParseMakeErrors removed - fully replaced by modular MakeParser
            }
            break;
        case TestResultFormat::PYTEST_TEXT:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    // Fallback to legacy parser if modular parser not found
                    ParsePytestText(content, global_state->events);
                }
            }
            break;
        case TestResultFormat::GENERIC_LINT:
            ParseGenericLint(content, global_state->events);
            break;
        case TestResultFormat::RUBOCOP_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
                // Note: Legacy ParseRuboCopJSON removed - fully replaced by modular RuboCopJSONParser
            }
            break;
        case TestResultFormat::CARGO_TEST_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
                // Note: Legacy ParseCargoTestJSON removed - fully replaced by modular CargoTestJSONParser
            }
            break;
        case TestResultFormat::SWIFTLINT_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
                // Note: Legacy ParseSwiftLintJSON removed - fully replaced by modular SwiftLintJSONParser
            }
            break;
        case TestResultFormat::PHPSTAN_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
                // Note: Legacy ParsePHPStanJSON removed - fully replaced by modular PHPStanJSONParser
            }
            break;
        case TestResultFormat::SHELLCHECK_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: ShellCheck JSON modular parser not found in registry");
                }
            }
            break;
        case TestResultFormat::STYLELINT_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: Stylelint JSON modular parser not found in registry");
                }
            }
            break;
        case TestResultFormat::CLIPPY_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: Clippy JSON modular parser not found in registry");
                }
            }
            break;
        case TestResultFormat::MARKDOWNLINT_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: Markdownlint JSON modular parser not found in registry");
                }
            }
            break;
        case TestResultFormat::YAMLLINT_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: Yamllint JSON modular parser not found in registry");
                }
            }
            break;
        case TestResultFormat::BANDIT_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: Bandit JSON modular parser not found in registry");
                }
            }
            break;
        case TestResultFormat::SPOTBUGS_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: SpotBugs JSON modular parser not found in registry");
                }
            }
            break;
        case TestResultFormat::KTLINT_JSON:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                } else {
                    throw std::runtime_error("parse_test_results: Ktlint JSON modular parser not found in registry");
                }
            }
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
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
            }
            break;
        case TestResultFormat::CMAKE_BUILD:
            duck_hunt::CMakeParser::ParseCMakeBuild(content, global_state->events);
            break;
        case TestResultFormat::PYTHON_BUILD:
            duck_hunt::PythonParser::ParsePythonBuild(content, global_state->events);
            break;
        case TestResultFormat::NODE_BUILD:
            duck_hunt::NodeParser::ParseNodeBuild(content, global_state->events);
            break;
        case TestResultFormat::CARGO_BUILD:
            duck_hunt::CargoParser::ParseCargoBuild(content, global_state->events);
            break;
        case TestResultFormat::MAVEN_BUILD:
            duck_hunt::MavenParser::ParseMavenBuild(content, global_state->events);
            break;
        case TestResultFormat::GRADLE_BUILD:
            duck_hunt::GradleParser::ParseGradleBuild(content, global_state->events);
            break;
        case TestResultFormat::MSBUILD:
            duck_hunt::MSBuildParser::ParseMSBuild(content, global_state->events);
            break;
        case TestResultFormat::JUNIT_TEXT:
            duck_hunt::JUnitTextParser::ParseJUnitText(content, global_state->events);
            break;
        case TestResultFormat::VALGRIND:
            duck_hunt::ValgrindParser::ParseValgrind(content, global_state->events);
            break;
        case TestResultFormat::GDB_LLDB:
            duck_hunt::GdbLldbParser::ParseGdbLldb(content, global_state->events);
            break;
        case TestResultFormat::RSPEC_TEXT:
            duck_hunt::RSpecTextParser::ParseRSpecText(content, global_state->events);
            break;
        case TestResultFormat::MOCHA_CHAI_TEXT:
            duck_hunt::MochaChaiTextParser::ParseMochaChai(content, global_state->events);
            break;
        case TestResultFormat::GTEST_TEXT:
            duck_hunt::GTestTextParser::ParseGoogleTest(content, global_state->events);
            break;
        case TestResultFormat::NUNIT_XUNIT_TEXT:
            duck_hunt::NUnitXUnitTextParser::ParseNUnitXUnit(content, global_state->events);
            break;
        case TestResultFormat::PYLINT_TEXT:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
            }
            break;
        case TestResultFormat::FLAKE8_TEXT:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
            }
            break;
        case TestResultFormat::BLACK_TEXT:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
            }
            break;
        case TestResultFormat::MYPY_TEXT:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
            }
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
            duck_hunt::CoverageParser::ParseCoverageText(content, global_state->events);
            break;
        case TestResultFormat::PYTEST_COV_TEXT:
            duck_hunt::CoverageParser::ParsePytestCovText(content, global_state->events);
            break;
        case TestResultFormat::GITHUB_ACTIONS_TEXT:
            ParseGitHubActionsText(content, global_state->events);
            break;
        case TestResultFormat::GITHUB_CLI:
        case TestResultFormat::CLANG_TIDY_TEXT:
            // These formats are handled by the modular parser registry at the end
            break;
        case TestResultFormat::GITLAB_CI_TEXT:
            ParseGitLabCIText(content, global_state->events);
            break;
        case TestResultFormat::JENKINS_TEXT:
            ParseJenkinsText(content, global_state->events);
            break;
        case TestResultFormat::DRONE_CI_TEXT:
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
            }
            break;
        case TestResultFormat::TERRAFORM_TEXT:
            ParseTerraformText(content, global_state->events);
            break;
        case TestResultFormat::ANSIBLE_TEXT:
            ParseAnsibleText(content, global_state->events);
            break;
        default:
            // Try the modular parser registry for new parsers
            {
                auto& registry = ParserRegistry::getInstance();
                auto parser = registry.getParser(format);
                if (parser) {
                    auto events = parser->parse(content);
                    global_state->events.insert(global_state->events.end(), events.begin(), events.end());
                }
            }
            break;
    }
    
    // Phase 3B: Process error patterns for intelligent categorization
    ProcessErrorPatterns(global_state->events);
    
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


void ParseTerraformText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::smatch match;
    
    // Terraform regex patterns
    std::regex terraform_version(R"(Terraform v(\d+\.\d+\.\d+))");
    std::regex provider_info(R"(\+ provider registry\.terraform\.io/hashicorp/(\w+) v([\d\.]+))");
    std::regex resource_create(R"(# (\S+) will be created)");
    std::regex resource_update(R"(# (\S+) will be updated in-place)");
    std::regex resource_destroy(R"(# (\S+) will be destroyed)");
    std::regex plan_summary(R"(Plan: (\d+) to add, (\d+) to change, (\d+) to destroy)");
    std::regex resource_creating(R"((\S+): Creating\.\.\.)");
    std::regex resource_creation_complete(R"((\S+): Creation complete after (\d+)s \[id=(.+)\])");
    std::regex resource_modifying(R"((\S+): Modifying\.\.\. \[id=(.+)\])");
    std::regex resource_modification_complete(R"((\S+): Modifications complete after (\d+)s \[id=(.+)\])");
    std::regex resource_destroying(R"((\S+): Destroying\.\.\. \[id=(.+)\])");
    std::regex resource_destruction_complete(R"((\S+): Destruction complete after (\d+)s)");
    std::regex apply_complete(R"(Apply complete! Resources: (\d+) added, (\d+) changed, (\d+) destroyed)");
    std::regex terraform_error(R"(Error: (.+))");
    std::regex terraform_warning(R"(Warning: (.+))");
    std::regex terraform_output(R"((\w+) = (.+))");
    std::regex version_warning(R"(Your version of Terraform is out of date!)");
    
    while (std::getline(stream, line)) {
        // Parse Terraform version
        if (std::regex_search(line, match, terraform_version)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "version";
            event.message = "Terraform version: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse provider information
        if (std::regex_search(line, match, provider_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "provider";
            event.message = "Provider " + match[1].str() + " version " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse resource creation plan
        if (std::regex_search(line, match, resource_create)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "plan_create";
            event.message = "Will create resource: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse resource update plan
        if (std::regex_search(line, match, resource_update)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "plan_update";
            event.message = "Will update resource: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse resource destroy plan
        if (std::regex_search(line, match, resource_destroy)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "plan_destroy";
            event.message = "Will destroy resource: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse plan summary
        if (std::regex_search(line, match, plan_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "plan_summary";
            event.message = "Plan: " + match[1].str() + " to add, " + match[2].str() + " to change, " + match[3].str() + " to destroy";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse resource creation
        if (std::regex_search(line, match, resource_creating)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "resource_creating";
            event.message = "Creating resource: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse resource creation complete
        if (std::regex_search(line, match, resource_creation_complete)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "resource_created";
            event.message = "Created resource: " + match[1].str() + " in " + match[2].str() + "s";
            event.execution_time = std::stod(match[2].str());
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse apply complete
        if (std::regex_search(line, match, apply_complete)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "apply_complete";
            event.message = "Apply complete: " + match[1].str() + " added, " + match[2].str() + " changed, " + match[3].str() + " destroyed";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse errors
        if (std::regex_search(line, match, terraform_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "terraform_error";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse warnings
        if (std::regex_search(line, match, terraform_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "terraform_warning";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse outputs
        if (std::regex_search(line, match, terraform_output)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "output";
            event.message = "Output " + match[1].str() + " = " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            
            events.push_back(event);
            continue;
        }
    }
}

void ParseAnsibleText(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::smatch match;
    
    // Ansible regex patterns
    std::regex play_start(R"(PLAY \[([^\]]+)\] \*+)");
    std::regex task_start(R"(TASK \[([^\]]+)\] \*+)");
    std::regex task_ok(R"(ok: \[([^\]]+)\]( => \((.+)\))?)");
    std::regex task_changed(R"(changed: \[([^\]]+)\]( => \((.+)\))?)");
    std::regex task_skipping(R"(skipping: \[([^\]]+)\]( => \((.+)\))?)");
    std::regex task_failed(R"(fatal: \[([^\]]+)\]: FAILED! => (.+))");
    std::regex task_unreachable(R"(fatal: \[([^\]]+)\]: UNREACHABLE! => (.+))");
    std::regex handler_running(R"(RUNNING HANDLER \[([^\]]+)\] \*+)");
    std::regex play_recap_start(R"(PLAY RECAP \*+)");
    std::regex play_recap_host(R"((\S+)\s+:\s+ok=(\d+)\s+changed=(\d+)\s+unreachable=(\d+)\s+failed=(\d+)\s+skipped=(\d+)\s+rescued=(\d+)\s+ignored=(\d+))");
    std::regex ansible_error(R"(ERROR! (.+))");
    std::regex ansible_warning(R"(\[WARNING\]: (.+))");
    std::regex deprecation_warning(R"(\[DEPRECATION WARNING\]: (.+))");
    std::regex retry_failed(R"(FAILED - RETRYING: (.+) \((\d+) retries left\))");
    std::regex task_retry_exhausted(R"(fatal: \[([^\]]+)\]: FAILED! => \{\"attempts\": (\d+), .+\"msg\": \"(.+)\"\})");
    std::regex config_diff(R"(--- (.+))");
    std::regex ansible_notified(R"(NOTIFIED: \[([^\]]+)\] \*+)");
    
    bool in_play_recap = false;
    std::string current_play = "";
    std::string current_task = "";
    
    while (std::getline(stream, line)) {
        // Parse play start
        if (std::regex_search(line, match, play_start)) {
            current_play = match[1].str();
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "play_start";
            event.message = "Starting play: " + current_play;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse task start
        if (std::regex_search(line, match, task_start)) {
            current_task = match[1].str();
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "task_start";
            event.message = "Starting task: " + current_task;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse task OK
        if (std::regex_search(line, match, task_ok)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "task_ok";
            event.message = "Task OK on host: " + match[1].str() + " (" + current_task + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse task changed
        if (std::regex_search(line, match, task_changed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "info";
            event.category = "task_changed";
            event.message = "Task changed on host: " + match[1].str() + " (" + current_task + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse task skipped
        if (std::regex_search(line, match, task_skipping)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::SKIP;
            event.severity = "info";
            event.category = "task_skipped";
            event.message = "Task skipped on host: " + match[1].str() + " (" + current_task + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse task failed
        if (std::regex_search(line, match, task_failed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "task_failed";
            event.message = "Task failed on host: " + match[1].str() + " (" + current_task + ")";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse task unreachable
        if (std::regex_search(line, match, task_unreachable)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "host_unreachable";
            event.message = "Host unreachable: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse running handler
        if (std::regex_search(line, match, handler_running)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::DEBUG_INFO;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "handler_running";
            event.message = "Running handler: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse play recap start
        if (std::regex_search(line, match, play_recap_start)) {
            in_play_recap = true;
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "play_recap_start";
            event.message = "Playbook execution summary";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse play recap host stats
        if (in_play_recap && std::regex_search(line, match, play_recap_host)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            
            int failed = std::stoi(match[5].str());
            int unreachable = std::stoi(match[4].str());
            
            if (failed > 0 || unreachable > 0) {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            }
            
            event.category = "host_summary";
            event.message = "Host " + match[1].str() + ": ok=" + match[2].str() + " changed=" + match[3].str() + 
                           " unreachable=" + match[4].str() + " failed=" + match[5].str() + " skipped=" + match[6].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse errors
        if (std::regex_search(line, match, ansible_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "ansible_error";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse warnings
        if (std::regex_search(line, match, ansible_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "ansible_warning";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse deprecation warnings
        if (std::regex_search(line, match, deprecation_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "deprecation_warning";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
        
        // Parse retry failures
        if (std::regex_search(line, match, retry_failed)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ansible";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "retry_failed";
            event.message = "Retrying: " + match[1].str() + " (" + match[2].str() + " retries left)";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "ansible_text";
            
            events.push_back(event);
            continue;
        }
    }
}

// Phase 3A: Multi-file processing implementation

std::vector<std::string> GetFilesFromPattern(ClientContext& context, const std::string& pattern) {
    auto &fs = FileSystem::GetFileSystem(context);
    std::vector<std::string> result;
    
    // Helper lambda to handle individual file paths (adapted from duckdb_yaml)
    auto processPath = [&](const std::string &file_path) {
        // First: check if we're dealing with just a single file that exists
        if (fs.FileExists(file_path)) {
            result.push_back(file_path);
            return;
        }
        
        // Second: attempt to use the path as a glob
        auto glob_files = GetGlobFiles(context, file_path);
        if (glob_files.size() > 0) {
            result.insert(result.end(), glob_files.begin(), glob_files.end());
            return;
        }
        
        // Third: if it looks like a directory, try to glob common test result files
        if (StringUtil::EndsWith(file_path, "/")) {
            // Common test result file patterns
            std::vector<std::string> patterns = {
                "*.xml", "*.json", "*.txt", "*.log", "*.out"
            };
            for (const auto& ext_pattern : patterns) {
                auto files = GetGlobFiles(context, fs.JoinPath(file_path, ext_pattern));
                result.insert(result.end(), files.begin(), files.end());
            }
            return;
        }
        
        // If file doesn't exist and isn't a valid glob, throw error
        throw IOException("File or directory does not exist: " + file_path);
    };
    
    processPath(pattern);
    return result;
}

std::vector<std::string> GetGlobFiles(ClientContext& context, const std::string& pattern) {
    auto &fs = FileSystem::GetFileSystem(context);
    std::vector<std::string> result;
    
    // Don't bother if we can't identify a glob pattern
    try {
        bool has_glob = fs.HasGlob(pattern);
        if (!has_glob) {
            // For remote URLs, still try GlobFiles as it has better support
            if (pattern.find("://") == std::string::npos || pattern.find("file://") == 0) {
                return result;
            }
        }
    } catch (const NotImplementedException &) {
        // If HasGlob is not implemented, still try GlobFiles for remote URLs
        if (pattern.find("://") == std::string::npos || pattern.find("file://") == 0) {
            return result;
        }
    }
    
    // Use GlobFiles which handles extension auto-loading and directory filtering
    try {
        auto glob_files = fs.GlobFiles(pattern, context, FileGlobOptions::ALLOW_EMPTY);
        for (auto &file : glob_files) {
            result.push_back(file.path);
        }
    } catch (const NotImplementedException &) {
        // No glob support available
        return result;
    } catch (const IOException &) {
        // Glob failed, return empty result
        return result;
    }
    
    return result;
}

void ProcessMultipleFiles(ClientContext& context, const std::vector<std::string>& files, 
                         TestResultFormat format, std::vector<ValidationEvent>& events) {
    for (size_t file_idx = 0; file_idx < files.size(); file_idx++) {
        const auto& file_path = files[file_idx];
        
        try {
            // Read file content
            std::string content = ReadContentFromSource(file_path);
            
            // Detect format if AUTO
            TestResultFormat detected_format = format;
            if (format == TestResultFormat::AUTO) {
                detected_format = DetectTestResultFormat(content);
            }
            
            // Parse content and get events for this file
            std::vector<ValidationEvent> file_events;
            
            // Dispatch to appropriate parser based on format
            switch (detected_format) {
                case TestResultFormat::PYTEST_JSON:
                    {
                        auto& registry = ParserRegistry::getInstance();
                        auto parser = registry.getParser(detected_format);
                        if (parser) {
                            file_events = parser->parse(content);
                        }
                        // Note: Legacy ParsePytestJSON removed - fully replaced by modular PytestJSONParser
                    }
                    break;
                case TestResultFormat::GOTEST_JSON:
                    {
                        auto& registry = ParserRegistry::getInstance();
                        auto parser = registry.getParser(detected_format);
                        if (parser) {
                            file_events = parser->parse(content);
                        }
                        // Note: Legacy ParseGoTestJSON removed - fully replaced by modular GoTestJSONParser
                    }
                    break;
                case TestResultFormat::ESLINT_JSON:
                    {
                        auto& registry = ParserRegistry::getInstance();
                        auto parser = registry.getParser(detected_format);
                        if (parser) {
                            file_events = parser->parse(content);
                        }
                        // Note: Legacy ParseESLintJSON removed - fully replaced by modular ESLintJSONParser
                    }
                    break;
                case TestResultFormat::PYTEST_TEXT:
                    ParsePytestText(content, file_events);
                    break;
                case TestResultFormat::GITHUB_ACTIONS_TEXT:
                    ParseGitHubActionsText(content, file_events);
                    break;
                case TestResultFormat::GITHUB_CLI:
                case TestResultFormat::CLANG_TIDY_TEXT:
                    // These formats are handled by the modular parser registry at the end
                    break;
                case TestResultFormat::GITLAB_CI_TEXT:
                    ParseGitLabCIText(content, file_events);
                    break;
                case TestResultFormat::JENKINS_TEXT:
                    ParseJenkinsText(content, file_events);
                    break;
                case TestResultFormat::DRONE_CI_TEXT:
                    {
                        auto& registry = ParserRegistry::getInstance();
                        auto parser = registry.getParser(format);
                        if (parser) {
                            auto events = parser->parse(content);
                            file_events.insert(file_events.end(), events.begin(), events.end());
                        }
                    }
                    break;
                case TestResultFormat::TERRAFORM_TEXT:
                    ParseTerraformText(content, file_events);
                    break;
                case TestResultFormat::ANSIBLE_TEXT:
                    ParseAnsibleText(content, file_events);
                    break;
                // Add other formats as needed...
                default:
                    // Try the modular parser registry for new parsers
                    {
                        auto& registry = ParserRegistry::getInstance();
                        auto parser = registry.getParser(detected_format);
                        if (parser) {
                            file_events = parser->parse(content);
                        } else {
                            // For truly unsupported formats, continue to next file
                            continue;
                        }
                    }
                    break;
            }
            
            // Enrich events with multi-file metadata
            for (auto& event : file_events) {
                event.source_file = file_path;
                event.build_id = ExtractBuildIdFromPath(file_path);
                event.environment = ExtractEnvironmentFromPath(file_path);
                event.file_index = static_cast<int64_t>(file_idx);
            }
            
            // Add events to main collection
            events.insert(events.end(), file_events.begin(), file_events.end());
            
        } catch (const IOException& e) {
            // Continue processing other files if one fails
            continue;
        }
    }
}

std::string ExtractBuildIdFromPath(const std::string& file_path) {
    // Extract build ID from common patterns like:
    // - /builds/build-123/results.xml -> "build-123"
    // - /ci-logs/pipeline-456/test.log -> "pipeline-456"
    // - /artifacts/20231201-142323/output.txt -> "20231201-142323"
    
    std::regex build_patterns[] = {
        std::regex(R"(/(?:build|pipeline|run|job)-([^/\s]+)/)"),  // build-123, pipeline-456
        std::regex(R"(/(\d{8}-\d{6})/)"),                         // 20231201-142323 
        std::regex(R"(/(?:builds?|ci|artifacts)/([^/\s]+)/)"),    // builds/abc123, ci/def456
        std::regex(R"([_-](\w+\d+)[_-])"),                        // any_build123_ pattern
    };
    
    for (const auto& pattern : build_patterns) {
        std::smatch match;
        if (std::regex_search(file_path, match, pattern)) {
            return match[1].str();
        }
    }
    
    return ""; // No build ID found
}

std::string ExtractEnvironmentFromPath(const std::string& file_path) {
    // Extract environment from common patterns like:
    // - /environments/dev/results.xml -> "dev"
    // - /staging/ci-logs/test.log -> "staging"
    // - /prod/artifacts/output.txt -> "prod"
    
    std::vector<std::string> environments = {"dev", "development", "staging", "stage", "prod", "production", "test", "testing"};
    
    for (const auto& env : environments) {
        if (file_path.find("/" + env + "/") != std::string::npos ||
            file_path.find("-" + env + "-") != std::string::npos ||
            file_path.find("_" + env + "_") != std::string::npos) {
            return env;
        }
    }
    
    return ""; // No environment found
}

} // namespace duckdb