#include "include/read_duck_hunt_log_function.hpp"
#include "include/validation_event_types.hpp"
#include "core/parser_registry.hpp" // Modular parser registry
#include "parsers/test_frameworks/duckdb_test_parser.hpp"
#include "parsers/test_frameworks/pytest_cov_text_parser.hpp"
#include "parsers/tool_outputs/generic_lint_parser.hpp"
#include "parsers/tool_outputs/regexp_parser.hpp"
#include "parsers/test_frameworks/junit_text_parser.hpp"
#include "parsers/test_frameworks/rspec_text_parser.hpp"
#include "parsers/test_frameworks/mocha_chai_text_parser.hpp"
#include "parsers/test_frameworks/gtest_text_parser.hpp"
#include "parsers/test_frameworks/nunit_xunit_text_parser.hpp"
#include "parsers/specialized/valgrind_parser.hpp"
#include "parsers/specialized/gdb_lldb_parser.hpp"
#include "parsers/specialized/strace_parser.hpp"
#include "parsers/specialized/coverage_parser.hpp"
#include "parsers/build_systems/maven_parser.hpp"
#include "parsers/build_systems/gradle_parser.hpp"
#include "parsers/build_systems/msbuild_parser.hpp"
#include "parsers/build_systems/node_parser.hpp"
#include "parsers/build_systems/python_parser.hpp"
#include "parsers/build_systems/cargo_parser.hpp"
#include "parsers/build_systems/cmake_parser.hpp"
#include "parsers/test_frameworks/junit_xml_parser.hpp"
#include "core/webbed_integration.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/file_glob_options.hpp"
#include "duckdb/common/enums/file_compression_type.hpp"
#include "yyjson.hpp"
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <cctype>
#include <functional>

namespace duckdb {

using namespace duckdb_yyjson;

// Forward declaration for auto-detection fallback
IParser *TryAutoDetectNewRegistry(const std::string &content);

// Phase 3B: Error Pattern Analysis Functions

// Pre-compiled regexes for error message normalization (compiled once, reused)
namespace {
// File paths
const std::regex RE_FILE_EXT(R"([/\\][\w/\\.-]+\.(cpp|hpp|py|js|java|go|rs|rb|php|c|h)[:\s])");
const std::regex RE_UNIX_PATH(R"(/[\w/.-]+/)");
const std::regex RE_WIN_PATH(R"(\\[\w\\.-]+\\)");
// Timestamps
const std::regex RE_DATETIME(R"(\d{4}-\d{2}-\d{2}[T\s]\d{2}:\d{2}:\d{2})");
const std::regex RE_TIME(R"(\d{2}:\d{2}:\d{2})");
// Line/column numbers
const std::regex RE_LINE_COL(R"(:(\d+):(\d+):)");
const std::regex RE_LINE_NUM(R"(line\s+\d+)");
const std::regex RE_COL_NUM(R"(column\s+\d+)");
// IDs and addresses
const std::regex RE_HEX_ADDR(R"(0x[0-9a-fA-F]+)");
const std::regex RE_LONG_ID(R"(\b\d{6,}\b)");
// Quoted variables
const std::regex RE_SINGLE_QUOTED(R"('[\w.-]+')");
const std::regex RE_DOUBLE_QUOTED(R"("[\w.-]+")");
// Numbers
const std::regex RE_DECIMAL(R"(\b\d+\.\d+\b)");
const std::regex RE_INTEGER(R"(\b\d+\b)");
// Whitespace
const std::regex RE_WHITESPACE(R"(\s+)");
} // namespace

// Normalize error message for fingerprinting by removing variable content
std::string NormalizeErrorMessage(const std::string &message) {
	std::string normalized = message;

	// Convert to lowercase for case-insensitive comparison
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

	// Remove file paths (anything that looks like a path)
	normalized = std::regex_replace(normalized, RE_FILE_EXT, " <file> ");
	normalized = std::regex_replace(normalized, RE_UNIX_PATH, "/<path>/");
	normalized = std::regex_replace(normalized, RE_WIN_PATH, "\\<path>\\");

	// Remove timestamps
	normalized = std::regex_replace(normalized, RE_DATETIME, "<timestamp>");
	normalized = std::regex_replace(normalized, RE_TIME, "<time>");

	// Remove line and column numbers
	normalized = std::regex_replace(normalized, RE_LINE_COL, ":<line>:<col>:");
	normalized = std::regex_replace(normalized, RE_LINE_NUM, "line <num>");
	normalized = std::regex_replace(normalized, RE_COL_NUM, "column <num>");

	// Remove numeric IDs and memory addresses
	normalized = std::regex_replace(normalized, RE_HEX_ADDR, "<addr>");
	normalized = std::regex_replace(normalized, RE_LONG_ID, "<id>");

	// Remove variable names in quotes
	normalized = std::regex_replace(normalized, RE_SINGLE_QUOTED, "'<var>'");
	normalized = std::regex_replace(normalized, RE_DOUBLE_QUOTED, "\"<var>\"");

	// Remove specific values but keep structure
	normalized = std::regex_replace(normalized, RE_DECIMAL, "<decimal>");
	normalized = std::regex_replace(normalized, RE_INTEGER, "<num>");

	// Normalize whitespace
	normalized = std::regex_replace(normalized, RE_WHITESPACE, " ");

	// Trim whitespace
	auto start = normalized.find_first_not_of(" \t");
	auto end = normalized.find_last_not_of(" \t");
	if (start != std::string::npos && end != std::string::npos) {
		normalized = normalized.substr(start, end - start + 1);
	}

	return normalized;
}

// Generate a fingerprint for an error based on normalized message and context
std::string GenerateErrorFingerprint(const ValidationEvent &event) {
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
double CalculateMessageSimilarity(const std::string &msg1, const std::string &msg2) {
	std::string norm1 = NormalizeErrorMessage(msg1);
	std::string norm2 = NormalizeErrorMessage(msg2);

	if (norm1.empty() && norm2.empty())
		return 1.0;
	if (norm1.empty() || norm2.empty())
		return 0.0;
	if (norm1 == norm2)
		return 1.0;

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
	std::vector<std::string> keywords = {"error",   "warning",    "failed",   "exception",
	                                     "timeout", "permission", "not found"};
	size_t keyword_matches = 0;

	for (const auto &keyword : keywords) {
		if (norm1.find(keyword) != std::string::npos && norm2.find(keyword) != std::string::npos) {
			keyword_matches++;
		}
	}

	double base_similarity = static_cast<double>(common_chars) / max_len;
	double keyword_bonus = static_cast<double>(keyword_matches) * 0.1;

	return std::min(1.0, base_similarity + keyword_bonus);
}

// Detect root cause category based on error content and context
std::string DetectRootCauseCategory(const ValidationEvent &event) {
	std::string message_lower = event.message;
	std::transform(message_lower.begin(), message_lower.end(), message_lower.begin(), ::tolower);

	// Network-related errors
	if (message_lower.find("connection") != std::string::npos || message_lower.find("timeout") != std::string::npos ||
	    message_lower.find("unreachable") != std::string::npos || message_lower.find("network") != std::string::npos ||
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
	if (message_lower.find("memory") != std::string::npos || message_lower.find("disk") != std::string::npos ||
	    message_lower.find("space") != std::string::npos || message_lower.find("quota") != std::string::npos ||
	    message_lower.find("limit") != std::string::npos) {
		return "resource";
	}

	// Syntax and validation errors
	if (message_lower.find("syntax") != std::string::npos || message_lower.find("parse") != std::string::npos ||
	    message_lower.find("invalid") != std::string::npos || message_lower.find("format") != std::string::npos ||
	    event.event_type == ValidationEventType::LINT_ISSUE || event.event_type == ValidationEventType::TYPE_ERROR) {
		return "syntax";
	}

	// Build and dependency errors
	if (message_lower.find("build") != std::string::npos || message_lower.find("compile") != std::string::npos ||
	    message_lower.find("dependency") != std::string::npos || message_lower.find("package") != std::string::npos ||
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

// Process events to generate error pattern metadata
void ProcessErrorPatterns(std::vector<ValidationEvent> &events) {
	// Step 1: Generate fingerprints for each event
	for (auto &event : events) {
		event.fingerprint = GenerateErrorFingerprint(event);
	}

	// Step 2: Assign pattern IDs based on fingerprint clustering
	std::map<std::string, int64_t> fingerprint_to_pattern_id;
	int64_t next_pattern_id = 1;

	for (auto &event : events) {
		if (fingerprint_to_pattern_id.find(event.fingerprint) == fingerprint_to_pattern_id.end()) {
			fingerprint_to_pattern_id[event.fingerprint] = next_pattern_id++;
		}
		event.pattern_id = fingerprint_to_pattern_id[event.fingerprint];
	}

	// Step 3: Calculate similarity scores within pattern groups
	for (auto &event : events) {
		if (event.pattern_id == -1)
			continue;

		// Find the representative message for this pattern (first occurrence)
		std::string representative_message;
		for (const auto &other_event : events) {
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

TestResultFormat DetectTestResultFormat(const std::string &content) {
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
		if (content.find("\"files\":") != std::string::npos && content.find("\"offenses\":") != std::string::npos &&
		    content.find("\"cop_name\":") != std::string::npos) {
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
		    content.find("\"code\":") != std::string::npos && content.find("\"message\":") != std::string::npos &&
		    content.find("\"line\":") != std::string::npos && content.find("\"DL") != std::string::npos) {
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
		if (content.find("\"fileName\":") != std::string::npos &&
		    content.find("\"lineNumber\":") != std::string::npos &&
		    content.find("\"ruleNames\":") != std::string::npos &&
		    content.find("\"ruleDescription\":") != std::string::npos) {
			return TestResultFormat::MARKDOWNLINT_JSON;
		}
		if (content.find("\"file\":") != std::string::npos && content.find("\"line\":") != std::string::npos &&
		    content.find("\"column\":") != std::string::npos && content.find("\"rule\":") != std::string::npos &&
		    content.find("\"level\":") != std::string::npos) {
			return TestResultFormat::YAMLLINT_JSON;
		}
		if (content.find("\"results\":") != std::string::npos && content.find("\"test_id\":") != std::string::npos &&
		    content.find("\"issue_severity\":") != std::string::npos &&
		    content.find("\"issue_confidence\":") != std::string::npos) {
			return TestResultFormat::BANDIT_JSON;
		}
		if (content.find("\"BugCollection\":") != std::string::npos &&
		    content.find("\"BugInstance\":") != std::string::npos && content.find("\"type\":") != std::string::npos &&
		    content.find("\"priority\":") != std::string::npos) {
			return TestResultFormat::SPOTBUGS_JSON;
		}
		if (content.find("\"file\":") != std::string::npos && content.find("\"errors\":") != std::string::npos &&
		    content.find("\"rule\":") != std::string::npos && content.find("\"line\":") != std::string::npos &&
		    content.find("\"column\":") != std::string::npos) {
			return TestResultFormat::KTLINT_JSON;
		}
		if (content.find("\"filename\":") != std::string::npos &&
		    content.find("\"line_number\":") != std::string::npos &&
		    content.find("\"column_number\":") != std::string::npos &&
		    content.find("\"linter\":") != std::string::npos && content.find("\"type\":") != std::string::npos) {
			return TestResultFormat::LINTR_JSON;
		}
		if (content.find("\"filepath\":") != std::string::npos &&
		    content.find("\"violations\":") != std::string::npos && content.find("\"line_no\":") != std::string::npos &&
		    content.find("\"code\":") != std::string::npos && content.find("\"rule\":") != std::string::npos) {
			return TestResultFormat::SQLFLUFF_JSON;
		}
		if (content.find("\"issues\":") != std::string::npos && content.find("\"rule\":") != std::string::npos &&
		    content.find("\"range\":") != std::string::npos && content.find("\"filename\":") != std::string::npos &&
		    content.find("\"severity\":") != std::string::npos) {
			return TestResultFormat::TFLINT_JSON;
		}
		if (content.find("\"object_name\":") != std::string::npos &&
		    content.find("\"type_meta\":") != std::string::npos && content.find("\"checks\":") != std::string::npos &&
		    content.find("\"grade\":") != std::string::npos && content.find("\"file_name\":") != std::string::npos) {
			return TestResultFormat::KUBE_SCORE_JSON;
		}
	}

	// Check CI/CD patterns first since they often contain output from multiple test frameworks

	// Check for GitHub Actions patterns
	if ((content.find("##[section]") != std::string::npos &&
	     content.find("##[section]Starting:") != std::string::npos) ||
	    (content.find("Agent name:") != std::string::npos &&
	     content.find("Agent machine name:") != std::string::npos) ||
	    (content.find("##[group]") != std::string::npos && content.find("##[endgroup]") != std::string::npos) ||
	    (content.find("##[error]") != std::string::npos &&
	     content.find("Process completed with exit code") != std::string::npos) ||
	    (content.find("Job completed:") != std::string::npos && content.find("Result:") != std::string::npos &&
	     content.find("Elapsed time:") != std::string::npos) ||
	    (content.find("==============================================================================") !=
	         std::string::npos &&
	     content.find("Task         :") != std::string::npos && content.find("Description  :") != std::string::npos)) {
		return TestResultFormat::GITHUB_ACTIONS_TEXT;
	}

	// GitHub CLI detection moved to GitHubCliParser::canParse()

	// Check for GitLab CI patterns
	if ((content.find("Running with gitlab-runner") != std::string::npos) ||
	    (content.find("using docker driver") != std::string::npos &&
	     content.find("Getting source from Git repository") != std::string::npos) ||
	    (content.find("Executing \"step_script\" stage") != std::string::npos) ||
	    (content.find("Job succeeded") != std::string::npos && content.find("$ bundle exec") != std::string::npos) ||
	    (content.find("Fetching changes with git depth") != std::string::npos &&
	     content.find("Created fresh repository") != std::string::npos)) {
		return TestResultFormat::GITLAB_CI_TEXT;
	}

	// Check for Jenkins patterns
	if ((content.find("Started by user") != std::string::npos &&
	     content.find("Building in workspace") != std::string::npos) ||
	    (content.find("[Pipeline] Start of Pipeline") != std::string::npos &&
	     content.find("[Pipeline] End of Pipeline") != std::string::npos) ||
	    (content.find("[Pipeline] stage") != std::string::npos && content.find("[Pipeline] sh") != std::string::npos) ||
	    (content.find("Finished: SUCCESS") != std::string::npos ||
	     content.find("Finished: FAILURE") != std::string::npos) ||
	    (content.find("java.lang.RuntimeException") != std::string::npos &&
	     content.find("at org.jenkinsci.plugins") != std::string::npos)) {
		return TestResultFormat::JENKINS_TEXT;
	}

	// Check for DroneCI patterns
	if ((content.find("[drone:exec]") != std::string::npos &&
	     content.find("starting build step:") != std::string::npos) ||
	    (content.find("[drone:exec]") != std::string::npos &&
	     content.find("completed build step:") != std::string::npos) ||
	    (content.find("[drone:exec]") != std::string::npos &&
	     content.find("pipeline execution complete") != std::string::npos) ||
	    (content.find("[drone:exec]") != std::string::npos &&
	     content.find("pipeline failed with exit code") != std::string::npos) ||
	    (content.find("+ git clone") != std::string::npos && content.find("DRONE_COMMIT_SHA") != std::string::npos)) {
		return TestResultFormat::DRONE_CI_TEXT;
	}

	// Check for Terraform patterns (check before general build patterns since terraform can contain similar keywords)
	if ((content.find("Terraform v") != std::string::npos &&
	     content.find("provider registry.terraform.io") != std::string::npos) ||
	    (content.find("Terraform will perform the following actions:") != std::string::npos) ||
	    (content.find("Resource actions are indicated with the following symbols:") != std::string::npos) ||
	    (content.find("Plan:") != std::string::npos && content.find("to add,") != std::string::npos &&
	     content.find("to change,") != std::string::npos && content.find("to destroy") != std::string::npos) ||
	    (content.find("Apply complete! Resources:") != std::string::npos &&
	     content.find("added,") != std::string::npos) ||
	    (content.find("# ") != std::string::npos && content.find(" will be created") != std::string::npos) ||
	    (content.find("# ") != std::string::npos && content.find(" will be updated in-place") != std::string::npos) ||
	    (content.find("# ") != std::string::npos && content.find(" will be destroyed") != std::string::npos) ||
	    (content.find("terraform apply") != std::string::npos && content.find("Enter a value:") != std::string::npos)) {
		return TestResultFormat::TERRAFORM_TEXT;
	}

	// Check for Ansible patterns (check before general build patterns since ansible can contain similar keywords)
	if ((content.find("PLAY [") != std::string::npos && content.find("] ****") != std::string::npos) ||
	    (content.find("TASK [") != std::string::npos && content.find("] ****") != std::string::npos) ||
	    (content.find("PLAY RECAP") != std::string::npos && content.find("ok=") != std::string::npos &&
	     content.find("changed=") != std::string::npos) ||
	    (content.find("fatal: [") != std::string::npos && content.find("]: FAILED!") != std::string::npos) ||
	    (content.find("fatal: [") != std::string::npos && content.find("]: UNREACHABLE!") != std::string::npos) ||
	    (content.find("changed: [") != std::string::npos && content.find("]") != std::string::npos) ||
	    (content.find("ok: [") != std::string::npos && content.find("]") != std::string::npos) ||
	    (content.find("skipping: [") != std::string::npos && content.find("]") != std::string::npos) ||
	    (content.find("RUNNING HANDLER [") != std::string::npos && content.find("] ****") != std::string::npos) ||
	    (content.find("FAILED - RETRYING:") != std::string::npos &&
	     content.find("retries left") != std::string::npos)) {
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
	    (content.find("Program received signal") != std::string::npos &&
	     content.find("Segmentation fault") != std::string::npos) ||
	    (content.find("Process") != std::string::npos && content.find("stopped") != std::string::npos &&
	     content.find("EXC_BAD_ACCESS") != std::string::npos) ||
	    (content.find("frame #") != std::string::npos && content.find("0x") != std::string::npos) ||
	    (content.find("breakpoint") != std::string::npos && content.find("hit count") != std::string::npos) ||
	    (content.find("(lldb)") != std::string::npos) ||
	    (content.find("Reading symbols from") != std::string::npos &&
	     content.find("Starting program:") != std::string::npos)) {
		return TestResultFormat::GDB_LLDB;
	}

	// Check for Mocha/Chai text patterns (should be checked before RSpec since they can share similar symbols)
	if ((content.find("passing") != std::string::npos && content.find("failing") != std::string::npos) ||
	    (content.find("Error:") != std::string::npos && content.find("at Context.<anonymous>") != std::string::npos) ||
	    (content.find("AssertionError:") != std::string::npos &&
	     content.find("at Context.<anonymous>") != std::string::npos) ||
	    (content.find("at Test.Runnable.run") != std::string::npos &&
	     content.find("node_modules/mocha") != std::string::npos) ||
	    (content.find("✓") != std::string::npos && content.find("✗") != std::string::npos &&
	     content.find("(") != std::string::npos && content.find("ms)") != std::string::npos)) {
		return TestResultFormat::MOCHA_CHAI_TEXT;
	}

	// Check for Google Test patterns (should be checked before other C++ test frameworks)
	if ((content.find("[==========]") != std::string::npos && content.find("Running") != std::string::npos &&
	     content.find("tests from") != std::string::npos) ||
	    (content.find("[ RUN      ]") != std::string::npos && content.find("[       OK ]") != std::string::npos) ||
	    (content.find("[  FAILED  ]") != std::string::npos && content.find("ms total") != std::string::npos) ||
	    (content.find("[  PASSED  ]") != std::string::npos && content.find("tests.") != std::string::npos) ||
	    (content.find("[----------]") != std::string::npos &&
	     content.find("Global test environment") != std::string::npos)) {
		return TestResultFormat::GTEST_TEXT;
	}

	// Check for NUnit/xUnit patterns (should be checked before other .NET test frameworks)
	if ((content.find("NUnit") != std::string::npos && content.find("Test Count:") != std::string::npos &&
	     content.find("Passed:") != std::string::npos) ||
	    (content.find("Test Run Summary") != std::string::npos &&
	     content.find("Overall result:") != std::string::npos) ||
	    (content.find("xUnit.net") != std::string::npos && content.find("VSTest Adapter") != std::string::npos) ||
	    (content.find("[PASS]") != std::string::npos && content.find("[FAIL]") != std::string::npos &&
	     content.find(".Tests.") != std::string::npos) ||
	    (content.find("Starting:") != std::string::npos && content.find("Finished:") != std::string::npos &&
	     content.find("==>") != std::string::npos) ||
	    (content.find("Total tests:") != std::string::npos && content.find("Failed:") != std::string::npos &&
	     content.find("Skipped:") != std::string::npos)) {
		return TestResultFormat::NUNIT_XUNIT_TEXT;
	}

	// Check for RSpec text patterns - use strict patterns to avoid false positives with Jest/Mocha
	// Strong RSpec indicators: "Failure/Error:", "rspec ./", "Failed examples:", "(FAILED - N)", "(PENDING:"
	if (content.find("Failure/Error:") != std::string::npos || content.find("rspec ./") != std::string::npos ||
	    content.find("Failed examples:") != std::string::npos || content.find("(FAILED - ") != std::string::npos ||
	    content.find("(PENDING:") != std::string::npos) {
		return TestResultFormat::RSPEC_TEXT;
	}
	// RSpec summary: "N examples, N failures" but NOT Jest/Mocha patterns
	if (content.find(" examples,") != std::string::npos && content.find(" failures") != std::string::npos) {
		// Exclude Jest/Mocha which use "passing"/"Test Suites"
		if (content.find("Test Suites:") == std::string::npos && content.find(" passing") == std::string::npos) {
			return TestResultFormat::RSPEC_TEXT;
		}
	}

	// Check for JUnit text patterns (should be checked before pytest since they can contain similar keywords)
	if ((content.find("T E S T S") != std::string::npos && content.find("Tests run:") != std::string::npos) ||
	    (content.find("JUnit Jupiter") != std::string::npos && content.find("tests found") != std::string::npos) ||
	    (content.find("Running TestSuite") != std::string::npos &&
	     content.find("Total tests run:") != std::string::npos) ||
	    (content.find("Time elapsed:") != std::string::npos && content.find("PASSED!") != std::string::npos) ||
	    (content.find("Time elapsed:") != std::string::npos && content.find("FAILURE!") != std::string::npos) ||
	    (content.find(" > ") != std::string::npos &&
	     (content.find(" PASSED") != std::string::npos || content.find(" FAILED") != std::string::npos))) {
		return TestResultFormat::JUNIT_TEXT;
	}

	// Check for Python linting tools (should be checked before pytest since they can contain similar keywords)

	// Check for Pylint patterns
	if ((content.find("************* Module") != std::string::npos &&
	     content.find("Your code has been rated") != std::string::npos) ||
	    (content.find("C:") != std::string::npos && content.find("W:") != std::string::npos &&
	     content.find("E:") != std::string::npos && content.find("(") != std::string::npos &&
	     content.find(")") != std::string::npos) ||
	    (content.find("missing-module-docstring") != std::string::npos ||
	     content.find("invalid-name") != std::string::npos || content.find("unused-argument") != std::string::npos) ||
	    (content.find("Global evaluation") != std::string::npos && content.find("Raw metrics") != std::string::npos)) {
		return TestResultFormat::PYLINT_TEXT;
	}

	// Check for yapf patterns (must be before autopep8 since both use similar diff formats)
	if ((content.find("--- a/") != std::string::npos && content.find("+++ b/") != std::string::npos &&
	     content.find("(original)") != std::string::npos && content.find("(reformatted)") != std::string::npos) ||
	    (content.find("Reformatted ") != std::string::npos && content.find(".py") != std::string::npos) ||
	    (content.find("yapf --") != std::string::npos) ||
	    (content.find("Style configuration:") != std::string::npos &&
	     content.find("Line length:") != std::string::npos) ||
	    (content.find("Files reformatted:") != std::string::npos &&
	     content.find("Files with no changes:") != std::string::npos) ||
	    (content.find("Processing /") != std::string::npos && content.find("--verbose") != std::string::npos) ||
	    (content.find("ERROR: Files would be reformatted but yapf was run with --check") != std::string::npos)) {
		return TestResultFormat::YAPF_TEXT;
	}

	// Check for autopep8 patterns (must be after yapf since both use diff formats)
	if ((content.find("--- original/") != std::string::npos && content.find("+++ fixed/") != std::string::npos) ||
	    (content.find("fixed ") != std::string::npos && content.find(" files") != std::string::npos) ||
	    (content.find("ERROR:") != std::string::npos && content.find(": E999 SyntaxError:") != std::string::npos) ||
	    (content.find("autopep8 --") != std::string::npos && content.find("--") != std::string::npos) ||
	    (content.find("Applied configuration:") != std::string::npos &&
	     content.find("aggressive:") != std::string::npos) ||
	    (content.find("Files processed:") != std::string::npos &&
	     content.find("Files modified:") != std::string::npos) ||
	    (content.find("Total fixes applied:") != std::string::npos)) {
		return TestResultFormat::AUTOPEP8_TEXT;
	}

	// Check for Flake8 patterns
	if ((content.find("F401") != std::string::npos || content.find("E131") != std::string::npos ||
	     content.find("W503") != std::string::npos) ||
	    (content.find(".py:") != std::string::npos && content.find(":") != std::string::npos &&
	     (content.find(": F") != std::string::npos || content.find(": E") != std::string::npos ||
	      content.find(": W") != std::string::npos || content.find(": C") != std::string::npos)) ||
	    (content.find("imported but unused") != std::string::npos ||
	     content.find("line too long") != std::string::npos ||
	     content.find("continuation line") != std::string::npos)) {
		return TestResultFormat::FLAKE8_TEXT;
	}

	// Check for Black patterns
	if ((content.find("would reformat") != std::string::npos && content.find("Oh no!") != std::string::npos &&
	     content.find("💥") != std::string::npos) ||
	    (content.find("files would be reformatted") != std::string::npos &&
	     content.find("files would be left unchanged") != std::string::npos) ||
	    (content.find("All done!") != std::string::npos && content.find("✨") != std::string::npos &&
	     content.find("🍰") != std::string::npos) ||
	    (content.find("--- ") != std::string::npos && content.find("+++ ") != std::string::npos &&
	     content.find("(original)") != std::string::npos && content.find("(formatted)") != std::string::npos)) {
		return TestResultFormat::BLACK_TEXT;
	}

	// Check for make error patterns (must be before mypy since make errors can contain ": error:" and brackets)
	if (content.find("make: ***") != std::string::npos && content.find("Error") != std::string::npos) {
		return TestResultFormat::MAKE_ERROR;
	}

	// Clang-tidy detection moved to ClangTidyParser::canParse()

	// Check for mypy patterns (more specific to avoid conflicts with clang-tidy)
	// First exclude clang-tidy patterns
	if (content.find("readability-") == std::string::npos && content.find("bugprone-") == std::string::npos &&
	    content.find("cppcoreguidelines-") == std::string::npos && content.find("google-build") == std::string::npos &&
	    content.find("performance-") == std::string::npos && content.find("modernize-") == std::string::npos &&
	    content.find("warnings generated") == std::string::npos &&
	    content.find("errors generated") == std::string::npos) {

		if (((content.find(": error:") != std::string::npos || content.find(": warning:") != std::string::npos) &&
		     content.find("[") != std::string::npos && content.find("]") != std::string::npos) ||
		    (content.find(": note:") != std::string::npos && content.find("Revealed type") != std::string::npos) ||
		    (content.find("Found") != std::string::npos && content.find("error") != std::string::npos &&
		     content.find("files") != std::string::npos && content.find("checked") != std::string::npos) ||
		    (content.find("Success: no issues found") != std::string::npos) ||
		    (content.find("return-value") != std::string::npos || content.find("arg-type") != std::string::npos ||
		     content.find("attr-defined") != std::string::npos)) {
			return TestResultFormat::MYPY_TEXT;
		}
	}

	// Check for Docker build patterns
	if ((content.find("Sending build context to Docker daemon") != std::string::npos &&
	     content.find("Step") != std::string::npos && content.find("FROM") != std::string::npos) ||
	    (content.find("Successfully built") != std::string::npos &&
	     content.find("Successfully tagged") != std::string::npos) ||
	    (content.find("[+] Building") != std::string::npos && content.find("FINISHED") != std::string::npos) ||
	    (content.find("=> [internal] load build definition from Dockerfile") != std::string::npos) ||
	    (content.find("The command") != std::string::npos &&
	     content.find("returned a non-zero code:") != std::string::npos) ||
	    (content.find("SECURITY SCANNING:") != std::string::npos &&
	     content.find("vulnerability found") != std::string::npos) ||
	    (content.find("Step") != std::string::npos && content.find("RUN") != std::string::npos &&
	     content.find("---> Running in") != std::string::npos)) {
		return TestResultFormat::DOCKER_BUILD;
	}

	// Check for Bazel build patterns
	if ((content.find("INFO: Analyzed") != std::string::npos && content.find("targets") != std::string::npos) ||
	    (content.find("INFO: Found") != std::string::npos && content.find("targets") != std::string::npos) ||
	    (content.find("INFO: Build completed") != std::string::npos &&
	     content.find("total actions") != std::string::npos) ||
	    (content.find("PASSED: //") != std::string::npos || content.find("FAILED: //") != std::string::npos) ||
	    (content.find("ERROR: /workspace/") != std::string::npos && content.find("BUILD:") != std::string::npos) ||
	    (content.find("Starting local Bazel server") != std::string::npos &&
	     content.find("connecting to it") != std::string::npos) ||
	    (content.find("Loading:") != std::string::npos && content.find("packages loaded") != std::string::npos) ||
	    (content.find("Analyzing:") != std::string::npos && content.find("targets") != std::string::npos &&
	     content.find("configured") != std::string::npos) ||
	    (content.find("TIMEOUT: //") != std::string::npos || content.find("FLAKY: //") != std::string::npos ||
	     content.find("SKIPPED: //") != std::string::npos)) {
		return TestResultFormat::BAZEL_BUILD;
	}

	// Check for isort patterns
	if ((content.find("Imports are incorrectly sorted") != std::string::npos) ||
	    (content.find("Fixing") != std::string::npos && content.find(".py") != std::string::npos) ||
	    (content.find("files reformatted") != std::string::npos &&
	     content.find("files left unchanged") != std::string::npos) ||
	    (content.find("import-order-style:") != std::string::npos || content.find("profile:") != std::string::npos) ||
	    (content.find("ERROR: isort found an import in the wrong position") != std::string::npos) ||
	    (content.find("Parsing") != std::string::npos && content.find(".py") != std::string::npos &&
	     content.find("Placing imports") != std::string::npos) ||
	    (content.find("would be reformatted") != std::string::npos &&
	     content.find("would be left unchanged") != std::string::npos)) {
		return TestResultFormat::ISORT_TEXT;
	}

	// Check for coverage.py patterns
	if ((content.find("Name") != std::string::npos && content.find("Stmts") != std::string::npos &&
	     content.find("Miss") != std::string::npos && content.find("Cover") != std::string::npos) ||
	    (content.find("coverage run") != std::string::npos && content.find("--source=") != std::string::npos) ||
	    (content.find("Coverage report generated") != std::string::npos) ||
	    (content.find("coverage html") != std::string::npos || content.find("coverage xml") != std::string::npos ||
	     content.find("coverage json") != std::string::npos) ||
	    (content.find("Wrote HTML report to") != std::string::npos ||
	     content.find("Wrote XML report to") != std::string::npos ||
	     content.find("Wrote JSON report to") != std::string::npos) ||
	    (content.find("TOTAL") != std::string::npos && content.find("-------") != std::string::npos &&
	     content.find("%") != std::string::npos) ||
	    (content.find("Coverage failure:") != std::string::npos &&
	     content.find("--fail-under=") != std::string::npos) ||
	    (content.find("Branch") != std::string::npos && content.find("BrPart") != std::string::npos)) {
		return TestResultFormat::COVERAGE_TEXT;
	}

	// Check for bandit text patterns
	if ((content.find("Test results:") != std::string::npos && content.find(">> Issue:") != std::string::npos &&
	     content.find("Severity:") != std::string::npos) ||
	    (content.find("[bandit]") != std::string::npos && content.find("security scan") != std::string::npos) ||
	    (content.find("Total issues (by severity):") != std::string::npos &&
	     content.find("Total issues (by confidence):") != std::string::npos) ||
	    (content.find("Code scanned:") != std::string::npos &&
	     content.find("Total lines of code:") != std::string::npos) ||
	    (content.find("More Info: https://bandit.readthedocs.io") != std::string::npos) ||
	    (content.find("CWE:") != std::string::npos && content.find("https://cwe.mitre.org") != std::string::npos) ||
	    (content.find("running on Python") != std::string::npos && content.find("[main]") != std::string::npos)) {
		return TestResultFormat::BANDIT_TEXT;
	}

	// Check for pytest-cov patterns - requires actual coverage DATA, not just plugin installed
	// Only match when coverage section markers or coverage table are present
	if ((content.find("-- coverage:") != std::string::npos && content.find("python") != std::string::npos) ||
	    (content.find("----------- coverage:") != std::string::npos) ||
	    (content.find("Name") != std::string::npos && content.find("Stmts") != std::string::npos &&
	     content.find("Miss") != std::string::npos && content.find("Cover") != std::string::npos) ||
	    (content.find("Coverage threshold check failed") != std::string::npos &&
	     content.find("Expected:") != std::string::npos) ||
	    (content.find("Required test coverage") != std::string::npos && content.find("not met") != std::string::npos)) {
		return TestResultFormat::PYTEST_COV_TEXT;
	}

	// Check for pytest text patterns (file.py::test_name with PASSED/FAILED/SKIPPED)
	if (content.find("::") != std::string::npos &&
	    (content.find("PASSED") != std::string::npos || content.find("FAILED") != std::string::npos ||
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
	    (content.find("webpack") != std::string::npos &&
	     (content.find("ERROR") != std::string::npos || content.find("WARNING") != std::string::npos)) ||
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
	    (content.find("maven-surefire-plugin") != std::string::npos &&
	     content.find("Tests run:") != std::string::npos) ||
	    (content.find("BUILD FAILURE") != std::string::npos && content.find("Total time:") != std::string::npos)) {
		return TestResultFormat::MAVEN_BUILD;
	}

	// Check for Gradle build patterns
	if ((content.find("> Task :") != std::string::npos) ||
	    (content.find("BUILD SUCCESSFUL") != std::string::npos &&
	     content.find("actionable task") != std::string::npos) ||
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

	// Note: Removed overly broad ": error:" / ": warning:" catch-all that was
	// incorrectly matching syslog and other log formats. Let the modular parser
	// registry handle detection with proper priority ordering. (Issue #20)

	// Fallback: try modular parser registry auto-detection
	auto *parser = TryAutoDetectNewRegistry(content);
	if (parser) {
		// Convert format name back to enum
		return StringToTestResultFormat(parser->getFormatName());
	}

	return TestResultFormat::UNKNOWN;
}

// Static array for enum-to-string conversion (indexed by enum value)
static const char *const FORMAT_NAMES[] = {
    "unknown",             // 0: UNKNOWN
    "auto",                // 1: AUTO
    "pytest_json",         // 2: PYTEST_JSON
    "gotest_json",         // 3: GOTEST_JSON
    "eslint_json",         // 4: ESLINT_JSON
    "pytest_text",         // 5: PYTEST_TEXT
    "make_error",          // 6: MAKE_ERROR
    "generic_lint",        // 7: GENERIC_LINT
    "duckdb_test",         // 8: DUCKDB_TEST
    "rubocop_json",        // 9: RUBOCOP_JSON
    "cargo_test_json",     // 10: CARGO_TEST_JSON
    "swiftlint_json",      // 11: SWIFTLINT_JSON
    "phpstan_json",        // 12: PHPSTAN_JSON
    "shellcheck_json",     // 13: SHELLCHECK_JSON
    "stylelint_json",      // 14: STYLELINT_JSON
    "clippy_json",         // 15: CLIPPY_JSON
    "markdownlint_json",   // 16: MARKDOWNLINT_JSON
    "yamllint_json",       // 17: YAMLLINT_JSON
    "bandit_json",         // 18: BANDIT_JSON
    "spotbugs_json",       // 19: SPOTBUGS_JSON
    "ktlint_json",         // 20: KTLINT_JSON
    "hadolint_json",       // 21: HADOLINT_JSON
    "lintr_json",          // 22: LINTR_JSON
    "sqlfluff_json",       // 23: SQLFLUFF_JSON
    "tflint_json",         // 24: TFLINT_JSON
    "kube_score_json",     // 25: KUBE_SCORE_JSON
    "cmake_build",         // 26: CMAKE_BUILD
    "python_build",        // 27: PYTHON_BUILD
    "node_build",          // 28: NODE_BUILD
    "cargo_build",         // 29: CARGO_BUILD
    "maven_build",         // 30: MAVEN_BUILD
    "gradle_build",        // 31: GRADLE_BUILD
    "msbuild",             // 32: MSBUILD
    "junit_text",          // 33: JUNIT_TEXT
    "valgrind",            // 34: VALGRIND
    "gdb_lldb",            // 35: GDB_LLDB
    "rspec_text",          // 36: RSPEC_TEXT
    "mocha_chai_text",     // 37: MOCHA_CHAI_TEXT
    "gtest_text",          // 38: GTEST_TEXT
    "nunit_xunit_text",    // 39: NUNIT_XUNIT_TEXT
    "pylint_text",         // 40: PYLINT_TEXT
    "flake8_text",         // 41: FLAKE8_TEXT
    "black_text",          // 42: BLACK_TEXT
    "mypy_text",           // 43: MYPY_TEXT
    "docker_build",        // 44: DOCKER_BUILD
    "bazel_build",         // 45: BAZEL_BUILD
    "isort_text",          // 46: ISORT_TEXT
    "bandit_text",         // 47: BANDIT_TEXT
    "autopep8_text",       // 48: AUTOPEP8_TEXT
    "yapf_text",           // 49: YAPF_TEXT
    "coverage_text",       // 50: COVERAGE_TEXT
    "pytest_cov_text",     // 51: PYTEST_COV_TEXT
    "github_actions_text", // 52: GITHUB_ACTIONS_TEXT
    "gitlab_ci_text",      // 53: GITLAB_CI_TEXT
    "jenkins_text",        // 54: JENKINS_TEXT
    "drone_ci_text",       // 55: DRONE_CI_TEXT
    "terraform_text",      // 56: TERRAFORM_TEXT
    "ansible_text",        // 57: ANSIBLE_TEXT
    "github_cli",          // 58: GITHUB_CLI
    "clang_tidy_text",     // 59: CLANG_TIDY_TEXT
    "regexp",              // 60: REGEXP
    "junit_xml",           // 61: JUNIT_XML
    "nunit_xml",           // 62: NUNIT_XML
    "checkstyle_xml",      // 63: CHECKSTYLE_XML
    "jsonl",               // 64: JSONL
    "logfmt",              // 65: LOGFMT
    "syslog",              // 66: SYSLOG
    "apache_access",       // 67: APACHE_ACCESS
    "nginx_access",        // 68: NGINX_ACCESS
    "aws_cloudtrail",      // 69: AWS_CLOUDTRAIL
    "gcp_cloud_logging",   // 70: GCP_CLOUD_LOGGING
    "azure_activity",      // 71: AZURE_ACTIVITY
    "python_logging",      // 72: PYTHON_LOGGING
    "log4j",               // 73: LOG4J
    "logrus",              // 74: LOGRUS
    "iptables",            // 75: IPTABLES
    "pf",                  // 76: PF_FIREWALL
    "cisco_asa",           // 77: CISCO_ASA
    "vpc_flow",            // 78: VPC_FLOW
    "kubernetes",          // 79: KUBERNETES
    "windows_event",       // 80: WINDOWS_EVENT
    "auditd",              // 81: AUDITD
    "s3_access",           // 82: S3_ACCESS
    "winston",             // 83: WINSTON
    "pino",                // 84: PINO
    "bunyan",              // 85: BUNYAN
    "serilog",             // 86: SERILOG
    "nlog",                // 87: NLOG
    "ruby_logger",         // 88: RUBY_LOGGER
    "rails_log",           // 89: RAILS_LOG
    "strace",              // 90: STRACE
};

std::string TestResultFormatToString(TestResultFormat format) {
	auto index = static_cast<uint8_t>(format);
	if (index < sizeof(FORMAT_NAMES) / sizeof(FORMAT_NAMES[0])) {
		return FORMAT_NAMES[index];
	}
	return "unknown";
}

// Static map for string-to-enum conversion (includes aliases)
static const std::unordered_map<std::string, TestResultFormat> &GetFormatMap() {
	static const std::unordered_map<std::string, TestResultFormat> map = {
	    // Primary names (must match FORMAT_NAMES array)
	    {"unknown", TestResultFormat::UNKNOWN},
	    {"auto", TestResultFormat::AUTO},
	    {"pytest_json", TestResultFormat::PYTEST_JSON},
	    {"gotest_json", TestResultFormat::GOTEST_JSON},
	    {"eslint_json", TestResultFormat::ESLINT_JSON},
	    {"pytest_text", TestResultFormat::PYTEST_TEXT},
	    {"make_error", TestResultFormat::MAKE_ERROR},
	    {"generic_lint", TestResultFormat::GENERIC_LINT},
	    {"duckdb_test", TestResultFormat::DUCKDB_TEST},
	    {"rubocop_json", TestResultFormat::RUBOCOP_JSON},
	    {"cargo_test_json", TestResultFormat::CARGO_TEST_JSON},
	    {"swiftlint_json", TestResultFormat::SWIFTLINT_JSON},
	    {"phpstan_json", TestResultFormat::PHPSTAN_JSON},
	    {"shellcheck_json", TestResultFormat::SHELLCHECK_JSON},
	    {"stylelint_json", TestResultFormat::STYLELINT_JSON},
	    {"clippy_json", TestResultFormat::CLIPPY_JSON},
	    {"markdownlint_json", TestResultFormat::MARKDOWNLINT_JSON},
	    {"yamllint_json", TestResultFormat::YAMLLINT_JSON},
	    {"bandit_json", TestResultFormat::BANDIT_JSON},
	    {"spotbugs_json", TestResultFormat::SPOTBUGS_JSON},
	    {"ktlint_json", TestResultFormat::KTLINT_JSON},
	    {"hadolint_json", TestResultFormat::HADOLINT_JSON},
	    {"lintr_json", TestResultFormat::LINTR_JSON},
	    {"sqlfluff_json", TestResultFormat::SQLFLUFF_JSON},
	    {"tflint_json", TestResultFormat::TFLINT_JSON},
	    {"kube_score_json", TestResultFormat::KUBE_SCORE_JSON},
	    {"cmake_build", TestResultFormat::CMAKE_BUILD},
	    {"python_build", TestResultFormat::PYTHON_BUILD},
	    {"node_build", TestResultFormat::NODE_BUILD},
	    {"cargo_build", TestResultFormat::CARGO_BUILD},
	    {"maven_build", TestResultFormat::MAVEN_BUILD},
	    {"gradle_build", TestResultFormat::GRADLE_BUILD},
	    {"msbuild", TestResultFormat::MSBUILD},
	    {"junit_text", TestResultFormat::JUNIT_TEXT},
	    {"valgrind", TestResultFormat::VALGRIND},
	    {"gdb_lldb", TestResultFormat::GDB_LLDB},
	    {"rspec_text", TestResultFormat::RSPEC_TEXT},
	    {"mocha_chai_text", TestResultFormat::MOCHA_CHAI_TEXT},
	    {"gtest_text", TestResultFormat::GTEST_TEXT},
	    {"nunit_xunit_text", TestResultFormat::NUNIT_XUNIT_TEXT},
	    {"pylint_text", TestResultFormat::PYLINT_TEXT},
	    {"flake8_text", TestResultFormat::FLAKE8_TEXT},
	    {"black_text", TestResultFormat::BLACK_TEXT},
	    {"mypy_text", TestResultFormat::MYPY_TEXT},
	    {"docker_build", TestResultFormat::DOCKER_BUILD},
	    {"bazel_build", TestResultFormat::BAZEL_BUILD},
	    {"isort_text", TestResultFormat::ISORT_TEXT},
	    {"bandit_text", TestResultFormat::BANDIT_TEXT},
	    {"autopep8_text", TestResultFormat::AUTOPEP8_TEXT},
	    {"yapf_text", TestResultFormat::YAPF_TEXT},
	    {"coverage_text", TestResultFormat::COVERAGE_TEXT},
	    {"pytest_cov_text", TestResultFormat::PYTEST_COV_TEXT},
	    {"github_actions_text", TestResultFormat::GITHUB_ACTIONS_TEXT},
	    {"gitlab_ci_text", TestResultFormat::GITLAB_CI_TEXT},
	    {"jenkins_text", TestResultFormat::JENKINS_TEXT},
	    {"drone_ci_text", TestResultFormat::DRONE_CI_TEXT},
	    {"terraform_text", TestResultFormat::TERRAFORM_TEXT},
	    {"ansible_text", TestResultFormat::ANSIBLE_TEXT},
	    {"github_cli", TestResultFormat::GITHUB_CLI},
	    {"clang_tidy_text", TestResultFormat::CLANG_TIDY_TEXT},
	    {"regexp", TestResultFormat::REGEXP},
	    {"junit_xml", TestResultFormat::JUNIT_XML},
	    {"nunit_xml", TestResultFormat::NUNIT_XML},
	    {"checkstyle_xml", TestResultFormat::CHECKSTYLE_XML},
	    {"jsonl", TestResultFormat::JSONL},
	    {"logfmt", TestResultFormat::LOGFMT},
	    {"syslog", TestResultFormat::SYSLOG},
	    {"apache_access", TestResultFormat::APACHE_ACCESS},
	    {"nginx_access", TestResultFormat::NGINX_ACCESS},
	    {"aws_cloudtrail", TestResultFormat::AWS_CLOUDTRAIL},
	    {"gcp_cloud_logging", TestResultFormat::GCP_CLOUD_LOGGING},
	    {"azure_activity", TestResultFormat::AZURE_ACTIVITY},
	    {"python_logging", TestResultFormat::PYTHON_LOGGING},
	    {"log4j", TestResultFormat::LOG4J},
	    {"logrus", TestResultFormat::LOGRUS},
	    {"iptables", TestResultFormat::IPTABLES},
	    {"pf", TestResultFormat::PF_FIREWALL},
	    {"cisco_asa", TestResultFormat::CISCO_ASA},
	    {"vpc_flow", TestResultFormat::VPC_FLOW},
	    {"kubernetes", TestResultFormat::KUBERNETES},
	    {"windows_event", TestResultFormat::WINDOWS_EVENT},
	    {"auditd", TestResultFormat::AUDITD},
	    {"s3_access", TestResultFormat::S3_ACCESS},
	    {"winston", TestResultFormat::WINSTON},
	    {"pino", TestResultFormat::PINO},
	    {"bunyan", TestResultFormat::BUNYAN},
	    {"serilog", TestResultFormat::SERILOG},
	    {"nlog", TestResultFormat::NLOG},
	    {"ruby_logger", TestResultFormat::RUBY_LOGGER},
	    {"rails_log", TestResultFormat::RAILS_LOG},
	    {"strace", TestResultFormat::STRACE},
	    // Aliases
	    {"ndjson", TestResultFormat::JSONL},
	    {"cloudtrail", TestResultFormat::AWS_CLOUDTRAIL},
	    {"gcp_logging", TestResultFormat::GCP_CLOUD_LOGGING},
	    {"azure_activity_log", TestResultFormat::AZURE_ACTIVITY},
	    {"python_log", TestResultFormat::PYTHON_LOGGING},
	    {"log4j_text", TestResultFormat::LOG4J},
	    {"logback", TestResultFormat::LOG4J},
	    {"logrus_text", TestResultFormat::LOGRUS},
	    {"winston_json", TestResultFormat::WINSTON},
	    {"pino_json", TestResultFormat::PINO},
	    {"bunyan_json", TestResultFormat::BUNYAN},
	    {"serilog_json", TestResultFormat::SERILOG},
	    {"serilog_text", TestResultFormat::SERILOG},
	    {"nlog_text", TestResultFormat::NLOG},
	    {"ruby_log", TestResultFormat::RUBY_LOGGER},
	    {"rails", TestResultFormat::RAILS_LOG},
	    {"netfilter", TestResultFormat::IPTABLES},
	    {"ufw", TestResultFormat::IPTABLES},
	    {"pf_firewall", TestResultFormat::PF_FIREWALL},
	    {"openbsd_pf", TestResultFormat::PF_FIREWALL},
	    {"asa", TestResultFormat::CISCO_ASA},
	    {"vpc_flow_logs", TestResultFormat::VPC_FLOW},
	    {"aws_vpc_flow", TestResultFormat::VPC_FLOW},
	    {"k8s", TestResultFormat::KUBERNETES},
	    {"kube", TestResultFormat::KUBERNETES},
	    {"windows_event_log", TestResultFormat::WINDOWS_EVENT},
	    {"eventlog", TestResultFormat::WINDOWS_EVENT},
	    {"audit", TestResultFormat::AUDITD},
	    {"ssh_auth", TestResultFormat::AUDITD},
	    {"s3_access_log", TestResultFormat::S3_ACCESS},
	    // Linting tool aliases
	    {"pylint", TestResultFormat::PYLINT_TEXT},
	    {"mypy", TestResultFormat::MYPY_TEXT},
	    {"flake8", TestResultFormat::FLAKE8_TEXT},
	    {"black", TestResultFormat::BLACK_TEXT},
	    {"yapf", TestResultFormat::YAPF_TEXT},
	    {"autopep8", TestResultFormat::AUTOPEP8_TEXT},
	    {"clang_tidy", TestResultFormat::CLANG_TIDY_TEXT},
	    {"clang-tidy", TestResultFormat::CLANG_TIDY_TEXT},
	    // Build system aliases
	    {"make", TestResultFormat::MAKE_ERROR},
	    {"gcc", TestResultFormat::MAKE_ERROR},
	    {"cmake", TestResultFormat::CMAKE_BUILD},
	    {"maven", TestResultFormat::MAVEN_BUILD},
	    {"mvn", TestResultFormat::MAVEN_BUILD},
	    {"gradle", TestResultFormat::GRADLE_BUILD},
	    {"cargo", TestResultFormat::CARGO_BUILD},
	    {"rust", TestResultFormat::CARGO_BUILD},
	    {"node", TestResultFormat::NODE_BUILD},
	    {"npm", TestResultFormat::NODE_BUILD},
	    {"yarn", TestResultFormat::NODE_BUILD},
	    {"pip", TestResultFormat::PYTHON_BUILD},
	    {"setuptools", TestResultFormat::PYTHON_BUILD},
	    {"bazel", TestResultFormat::BAZEL_BUILD},
	    {"visualstudio", TestResultFormat::MSBUILD},
	    {"vs", TestResultFormat::MSBUILD},
	    // Test framework aliases
	    {"gtest", TestResultFormat::GTEST_TEXT},
	    {"googletest", TestResultFormat::GTEST_TEXT},
	    {"rspec", TestResultFormat::RSPEC_TEXT},
	    {"mocha", TestResultFormat::MOCHA_CHAI_TEXT},
	    {"chai", TestResultFormat::MOCHA_CHAI_TEXT},
	    {"nunit", TestResultFormat::NUNIT_XUNIT_TEXT},
	    {"xunit", TestResultFormat::NUNIT_XUNIT_TEXT},
	    {"pytest", TestResultFormat::PYTEST_TEXT},
	    {"pytest_cov", TestResultFormat::PYTEST_COV_TEXT},
	    {"pytest-cov", TestResultFormat::PYTEST_COV_TEXT},
	    // Debugging aliases
	    {"gdb", TestResultFormat::GDB_LLDB},
	    {"lldb", TestResultFormat::GDB_LLDB},
	    {"coverage", TestResultFormat::COVERAGE_TEXT},
	    // JSON format aliases
	    {"go_test_json", TestResultFormat::GOTEST_JSON},
	    {"gotest", TestResultFormat::GOTEST_JSON},
	    {"eslint", TestResultFormat::ESLINT_JSON},
	};
	return map;
}

TestResultFormat StringToTestResultFormat(const std::string &str) {
	// Check for regexp: prefix (dynamic pattern matching)
	if (str.rfind("regexp:", 0) == 0)
		return TestResultFormat::REGEXP;

	auto &map = GetFormatMap();
	auto it = map.find(str);
	return it != map.end() ? it->second : TestResultFormat::UNKNOWN;
}

/**
 * Try to parse content using the new modular parser registry.
 * Returns true if a parser was found and content was parsed.
 * Returns false if no parser found (caller should fall back to legacy switch).
 *
 * This dispatch function allows gradual migration: as parsers are added to
 * ParserRegistry, they will be used automatically without
 * needing to update the legacy switch statement.
 */
/**
 * Try parsing with the new modular parser registry using a format name string.
 * This overload allows direct lookup for registry-only formats.
 * Also supports format groups (e.g., "python", "rust") which try all parsers in the group.
 */
bool TryNewParserRegistryByName(ClientContext &context, const std::string &format_name, const std::string &content,
                                std::vector<ValidationEvent> &events) {
	if (format_name.empty() || format_name == "unknown" || format_name == "auto") {
		return false; // Let legacy code handle these
	}

	// Check new parser registry
	auto &registry = ParserRegistry::getInstance();

	// First check if this is a format group (e.g., "python", "rust", "ci")
	if (registry.isGroup(format_name)) {
		// Get all parsers in this group, sorted by priority
		auto group_parsers = registry.getParsersByGroup(format_name);

		// Try each parser in priority order until one produces events
		for (auto *parser : group_parsers) {
			if (parser->canParse(content)) {
				size_t events_before = events.size();
				if (parser->requiresContext()) {
					auto parsed_events = parser->parseWithContext(context, content);
					events.insert(events.end(), parsed_events.begin(), parsed_events.end());
				} else {
					auto parsed_events = parser->parse(content);
					events.insert(events.end(), parsed_events.begin(), parsed_events.end());
				}
				if (events.size() > events_before) {
					return true; // Found a parser that produced events
				}
			}
		}
		return false; // No parser in group could parse this content
	}

	// Not a group - try direct format lookup
	auto *parser = registry.getParser(format_name);

	if (!parser) {
		return false; // No parser in new registry, use legacy
	}

	// Parser found - use it
	size_t events_before = events.size();
	if (parser->requiresContext()) {
		auto parsed_events = parser->parseWithContext(context, content);
		events.insert(events.end(), parsed_events.begin(), parsed_events.end());
	} else {
		auto parsed_events = parser->parse(content);
		events.insert(events.end(), parsed_events.begin(), parsed_events.end());
	}

	// Only return true if we actually parsed something
	// This allows fallback to old registry if new parser produces no results
	return events.size() > events_before;
}

bool TryNewParserRegistry(ClientContext &context, TestResultFormat format, const std::string &content,
                          std::vector<ValidationEvent> &events) {
	// Get the format name string from enum
	std::string format_name = TestResultFormatToString(format);
	return TryNewParserRegistryByName(context, format_name, content, events);
}

/**
 * Try to auto-detect content format using the new modular parser registry.
 * Returns the parser if found, nullptr otherwise.
 */
IParser *TryAutoDetectNewRegistry(const std::string &content) {
	auto &registry = ParserRegistry::getInstance();
	return registry.findParser(content);
}

std::string ReadContentFromSource(ClientContext &context, const std::string &source) {
	// Use DuckDB's FileSystem to properly handle file paths including UNITTEST_ROOT_DIRECTORY
	auto &fs = FileSystem::GetFileSystem(context);

	// Open the file with automatic compression detection based on file extension
	// This handles .gz, .zst, etc. transparently
	auto flags = FileFlags::FILE_FLAGS_READ | FileCompressionType::AUTO_DETECT;
	auto file_handle = fs.OpenFile(source, flags);

	// Check compression type - for compressed files we can't seek or get size upfront
	auto compression = file_handle->GetFileCompressionType();
	bool can_get_size = (compression == FileCompressionType::UNCOMPRESSED) && file_handle->CanSeek();

	if (can_get_size) {
		// Uncompressed file - read using known size for efficiency
		auto file_size = file_handle->GetFileSize();
		if (file_size > 0) {
			std::string content;
			content.resize(static_cast<size_t>(file_size));
			file_handle->Read((void *)content.data(), file_size);
			return content;
		}
	}

	// Compressed files, pipes, or empty files - read in chunks until EOF
	std::string content;
	constexpr size_t chunk_size = 8192;
	char buffer[chunk_size];

	while (true) {
		auto bytes_read = file_handle->Read(buffer, chunk_size);
		if (bytes_read == 0) {
			break; // EOF
		}
		content.append(buffer, static_cast<size_t>(bytes_read));
	}

	return content;
}

bool IsValidJSON(const std::string &content) {
	// Simple heuristic - starts with { or [
	std::string trimmed = content;
	StringUtil::Trim(trimmed);
	return !trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '[');
}

unique_ptr<FunctionData> ReadDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names) {
	auto bind_data = make_uniq<ReadDuckHuntLogBindData>();

	// Get source parameter (required)
	if (input.inputs.empty()) {
		throw BinderException("read_duck_hunt_log requires at least one parameter (source)");
	}
	bind_data->source = input.inputs[0].ToString();

	// Get format parameter (optional, defaults to auto)
	if (input.inputs.size() > 1) {
		std::string format_str = input.inputs[1].ToString();
		bind_data->format_name = format_str; // Store raw format name for registry lookup
		bind_data->format = StringToTestResultFormat(format_str);

		// Check for unknown format - but allow registry-only formats and format groups
		if (bind_data->format == TestResultFormat::UNKNOWN) {
			// Check if this format exists in the new parser registry or is a format group
			auto &registry = ParserRegistry::getInstance();
			if (!registry.hasFormat(format_str) && !registry.isGroup(format_str)) {
				throw BinderException("Unknown format: '" + format_str +
				                      "'. Use 'auto' for auto-detection or see docs/formats.md for supported formats.");
			}
			// Format exists in registry or is a group, keep UNKNOWN enum but format_name has the string
		}

		// For REGEXP format, extract the pattern after the "regexp:" prefix
		if (bind_data->format == TestResultFormat::REGEXP) {
			if (format_str.length() <= 7) {
				throw BinderException("regexp: format requires a pattern after the prefix, e.g., "
				                      "'regexp:(?P<severity>ERROR|WARN):\\s+(?P<message>.*)'");
			}
			bind_data->regexp_pattern = format_str.substr(7); // Remove "regexp:" prefix
		}
	} else {
		bind_data->format = TestResultFormat::AUTO;
		bind_data->format_name = "auto";
	}

	// Handle severity_threshold named parameter
	auto threshold_param = input.named_parameters.find("severity_threshold");
	if (threshold_param != input.named_parameters.end()) {
		std::string threshold_str = threshold_param->second.ToString();
		bind_data->severity_threshold = StringToSeverityLevel(threshold_str);
	}

	// Handle ignore_errors named parameter
	auto ignore_errors_param = input.named_parameters.find("ignore_errors");
	if (ignore_errors_param != input.named_parameters.end()) {
		bind_data->ignore_errors = ignore_errors_param->second.GetValue<bool>();
	}

	// Handle content named parameter (controls log_content truncation)
	auto content_param = input.named_parameters.find("content");
	if (content_param != input.named_parameters.end()) {
		auto &val = content_param->second;
		if (val.type().id() == LogicalTypeId::INTEGER || val.type().id() == LogicalTypeId::BIGINT) {
			int32_t limit = val.GetValue<int32_t>();
			if (limit <= 0) {
				bind_data->content_mode = ContentMode::NONE;
			} else {
				bind_data->content_mode = ContentMode::LIMIT;
				bind_data->content_limit = limit;
			}
		} else {
			std::string mode_str = StringUtil::Lower(val.ToString());
			if (mode_str == "full") {
				bind_data->content_mode = ContentMode::FULL;
			} else if (mode_str == "none") {
				bind_data->content_mode = ContentMode::NONE;
			} else if (mode_str == "smart") {
				bind_data->content_mode = ContentMode::SMART;
			} else {
				throw BinderException("Invalid content mode: '" + mode_str +
				                      "'. Use integer (char limit), 'full', 'none', or 'smart'.");
			}
		}
	}

	// Define return schema (Schema V2)
	return_types = {
	    // Core identification
	    LogicalType::BIGINT,  // event_id
	    LogicalType::VARCHAR, // tool_name
	    LogicalType::VARCHAR, // event_type
	    // Code location
	    LogicalType::VARCHAR, // file_path
	    LogicalType::INTEGER, // line_number
	    LogicalType::INTEGER, // column_number
	    LogicalType::VARCHAR, // function_name
	    // Classification
	    LogicalType::VARCHAR, // status
	    LogicalType::VARCHAR, // severity
	    LogicalType::VARCHAR, // category
	    LogicalType::VARCHAR, // error_code
	    // Content
	    LogicalType::VARCHAR, // message
	    LogicalType::VARCHAR, // suggestion
	    LogicalType::VARCHAR, // raw_output
	    LogicalType::VARCHAR, // structured_data
	    // Log tracking
	    LogicalType::INTEGER, // log_line_start
	    LogicalType::INTEGER, // log_line_end
	    LogicalType::VARCHAR, // log_file
	    // Test-specific
	    LogicalType::VARCHAR, // test_name
	    LogicalType::DOUBLE,  // execution_time
	    // Identity & Network
	    LogicalType::VARCHAR, // principal
	    LogicalType::VARCHAR, // origin
	    LogicalType::VARCHAR, // target
	    LogicalType::VARCHAR, // actor_type
	    // Temporal
	    LogicalType::VARCHAR, // started_at
	    // Correlation
	    LogicalType::VARCHAR, // external_id
	    // Hierarchical context
	    LogicalType::VARCHAR, // scope
	    LogicalType::VARCHAR, // scope_id
	    LogicalType::VARCHAR, // scope_status
	    LogicalType::VARCHAR, // group
	    LogicalType::VARCHAR, // group_id
	    LogicalType::VARCHAR, // group_status
	    LogicalType::VARCHAR, // unit
	    LogicalType::VARCHAR, // unit_id
	    LogicalType::VARCHAR, // unit_status
	    LogicalType::VARCHAR, // subunit
	    LogicalType::VARCHAR, // subunit_id
	    // Pattern analysis
	    LogicalType::VARCHAR, // fingerprint
	    LogicalType::DOUBLE,  // similarity_score
	    LogicalType::BIGINT   // pattern_id
	};

	names = {// Core identification
	         "event_id", "tool_name", "event_type",
	         // Code location
	         "ref_file", "ref_line", "ref_column", "function_name",
	         // Classification
	         "status", "severity", "category", "error_code",
	         // Content
	         "message", "suggestion", "log_content", "structured_data",
	         // Log tracking
	         "log_line_start", "log_line_end", "log_file",
	         // Test-specific
	         "test_name", "execution_time",
	         // Identity & Network
	         "principal", "origin", "target", "actor_type",
	         // Temporal
	         "started_at",
	         // Correlation
	         "external_id",
	         // Hierarchical context
	         "scope", "scope_id", "scope_status", "group", "group_id", "group_status", "unit", "unit_id", "unit_status",
	         "subunit", "subunit_id",
	         // Pattern analysis
	         "fingerprint", "similarity_score", "pattern_id"};

	return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ReadDuckHuntLogInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto global_state = make_uniq<ReadDuckHuntLogGlobalState>();

	// Phase 3A: Check if source contains glob patterns or multiple files
	std::vector<std::string> files;

	try {
		// Try to expand the source as a glob pattern or file list
		files = GetFilesFromPattern(context, bind_data.source);
	} catch (const IOException &) {
		// If glob expansion fails, treat as single file or direct content
		files.clear();
	}

	if (files.size() > 1) {
		// Multi-file processing path
		ProcessMultipleFiles(context, files, bind_data.format, global_state->events, bind_data.ignore_errors);
	} else {
		// Single file processing path (original behavior)
		// Use the matched file path if available, otherwise use the source directly
		std::string source_path = files.size() == 1 ? files[0] : bind_data.source;
		std::string content;
		try {
			content = ReadContentFromSource(context, source_path);
		} catch (const IOException &) {
			// If file reading fails, treat source as direct content
			content = bind_data.source;
		}

		// Auto-detect format if needed
		TestResultFormat format = bind_data.format;
		std::string format_name = bind_data.format_name;
		if (format == TestResultFormat::AUTO) {
			format = DetectTestResultFormat(content);
			format_name = TestResultFormatToString(format);

			// If legacy detection returned UNKNOWN, try new registry auto-detection
			if (format == TestResultFormat::UNKNOWN) {
				auto *detected_parser = TryAutoDetectNewRegistry(content);
				if (detected_parser) {
					format_name = detected_parser->getFormatName();
				}
			}
		}

		// Try new modular parser registry first
		// For registry-only formats (UNKNOWN enum but valid format_name), use by-name lookup
		bool parsed = false;
		if (format == TestResultFormat::UNKNOWN && !format_name.empty() && format_name != "unknown") {
			// Registry-only format - use format_name directly
			parsed = TryNewParserRegistryByName(context, format_name, content, global_state->events);
		} else {
			// Known enum format - try registry with enum
			parsed = TryNewParserRegistry(context, format, content, global_state->events);
		}

		if (!parsed) {
			// Legacy fallback for special cases not in modular registry
			switch (format) {
			case TestResultFormat::DUCKDB_TEST:
				duckdb::DuckDBTestParser::ParseDuckDBTestOutput(content, global_state->events);
				break;
			case TestResultFormat::GENERIC_LINT:
				duckdb::GenericLintParser::ParseGenericLint(content, global_state->events);
				break;
			case TestResultFormat::REGEXP:
				// Dynamic regexp parser - uses user-provided pattern
				duckdb::RegexpParser::ParseWithRegexp(content, bind_data.regexp_pattern, global_state->events);
				break;
			default:
				// All formats now handled by modular registry above
				break;
			}
		}

		// Set log_file on each event to track source file (single file mode)
		// Only set if source looks like a file path (contains / or \)
		if (bind_data.source.find('/') != std::string::npos || bind_data.source.find('\\') != std::string::npos) {
			for (auto &event : global_state->events) {
				if (event.log_file.empty()) {
					event.log_file = bind_data.source;
				}
			}
		}
	}

	// Phase 3B: Process error patterns for intelligent categorization
	ProcessErrorPatterns(global_state->events);

	// Apply severity threshold filtering
	if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
		// Filter out events below the threshold
		auto new_end = std::remove_if(global_state->events.begin(), global_state->events.end(),
		                              [&bind_data](const ValidationEvent &event) {
			                              return !ShouldEmitEvent(event.severity, bind_data.severity_threshold);
		                              });
		global_state->events.erase(new_end, global_state->events.end());
	}

	return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ReadDuckHuntLogInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                             GlobalTableFunctionState *global_state) {
	return make_uniq<ReadDuckHuntLogLocalState>();
}

void ReadDuckHuntLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto &global_state = data_p.global_state->Cast<ReadDuckHuntLogGlobalState>();
	auto &local_state = data_p.local_state->Cast<ReadDuckHuntLogLocalState>();

	// Populate output chunk with content truncation settings
	PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE,
	                            bind_data.content_mode, bind_data.content_limit);

	// Update offset for next chunk
	local_state.chunk_offset += output.size();
}

std::string TruncateLogContent(const std::string &content, ContentMode mode, int32_t limit, int32_t event_line_start,
                               int32_t event_line_end) {
	switch (mode) {
	case ContentMode::FULL:
		return content;

	case ContentMode::NONE:
		return ""; // Will be converted to NULL in output

	case ContentMode::LIMIT:
		if (content.length() <= static_cast<size_t>(limit)) {
			return content;
		}
		return content.substr(0, limit) + "...";

	case ContentMode::SMART: {
		// Smart truncation: try to preserve context around event lines
		if (content.length() <= static_cast<size_t>(limit)) {
			return content;
		}

		// If we have line info, try to extract around those lines
		if (event_line_start > 0) {
			std::vector<std::string> lines;
			std::istringstream stream(content);
			std::string line;
			while (std::getline(stream, line)) {
				lines.push_back(line);
			}

			// Calculate which lines to include
			int32_t start_line = std::max(0, event_line_start - 2);
			int32_t end_line = std::min(static_cast<int32_t>(lines.size()),
			                            (event_line_end > 0 ? event_line_end : event_line_start) + 2);

			std::string result;
			if (start_line > 0) {
				result = "...\n";
			}
			for (int32_t i = start_line; i < end_line && i < static_cast<int32_t>(lines.size()); i++) {
				result += lines[i] + "\n";
			}
			if (end_line < static_cast<int32_t>(lines.size())) {
				result += "...";
			}

			// If still too long, fall back to simple truncation
			if (result.length() > static_cast<size_t>(limit)) {
				return result.substr(0, limit) + "...";
			}
			return result;
		}

		// No line info, fall back to simple truncation
		return content.substr(0, limit) + "...";
	}

	default:
		return content;
	}
}

void PopulateDataChunkFromEvents(DataChunk &output, const std::vector<ValidationEvent> &events, idx_t start_offset,
                                 idx_t chunk_size, ContentMode content_mode, int32_t content_limit) {
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

		// Core identification
		output.SetValue(col++, i, Value::BIGINT(event.event_id));
		output.SetValue(col++, i, Value(event.tool_name));
		output.SetValue(col++, i, Value(ValidationEventTypeToString(event.event_type)));
		// Code location
		output.SetValue(col++, i, Value(event.ref_file));
		output.SetValue(col++, i, event.ref_line == -1 ? Value() : Value::INTEGER(event.ref_line));
		output.SetValue(col++, i, event.ref_column == -1 ? Value() : Value::INTEGER(event.ref_column));
		output.SetValue(col++, i, Value(event.function_name));
		// Classification
		output.SetValue(col++, i, Value(ValidationEventStatusToString(event.status)));
		output.SetValue(col++, i, Value(event.severity));
		output.SetValue(col++, i, Value(event.category));
		output.SetValue(col++, i, Value(event.error_code));
		// Content
		output.SetValue(col++, i, Value(event.message));
		output.SetValue(col++, i, Value(event.suggestion));

		// Apply content truncation to log_content
		if (content_mode == ContentMode::NONE) {
			output.SetValue(col++, i, Value());
		} else {
			std::string truncated = TruncateLogContent(event.log_content, content_mode, content_limit,
			                                           event.log_line_start, event.log_line_end);
			output.SetValue(col++, i, truncated.empty() ? Value() : Value(truncated));
		}
		output.SetValue(col++, i, Value(event.structured_data));
		// Log tracking
		output.SetValue(col++, i, event.log_line_start == -1 ? Value() : Value::INTEGER(event.log_line_start));
		output.SetValue(col++, i, event.log_line_end == -1 ? Value() : Value::INTEGER(event.log_line_end));
		output.SetValue(col++, i, event.log_file.empty() ? Value() : Value(event.log_file));
		// Test-specific
		output.SetValue(col++, i, Value(event.test_name));
		output.SetValue(col++, i, Value::DOUBLE(event.execution_time));
		// Identity & Network
		output.SetValue(col++, i, event.principal.empty() ? Value() : Value(event.principal));
		output.SetValue(col++, i, event.origin.empty() ? Value() : Value(event.origin));
		output.SetValue(col++, i, event.target.empty() ? Value() : Value(event.target));
		output.SetValue(col++, i, event.actor_type.empty() ? Value() : Value(event.actor_type));
		// Temporal
		output.SetValue(col++, i, event.started_at.empty() ? Value() : Value(event.started_at));
		// Correlation
		output.SetValue(col++, i, event.external_id.empty() ? Value() : Value(event.external_id));
		// Hierarchical context
		output.SetValue(col++, i, event.scope.empty() ? Value() : Value(event.scope));
		output.SetValue(col++, i, event.scope_id.empty() ? Value() : Value(event.scope_id));
		output.SetValue(col++, i, event.scope_status.empty() ? Value() : Value(event.scope_status));
		output.SetValue(col++, i, event.group.empty() ? Value() : Value(event.group));
		output.SetValue(col++, i, event.group_id.empty() ? Value() : Value(event.group_id));
		output.SetValue(col++, i, event.group_status.empty() ? Value() : Value(event.group_status));
		output.SetValue(col++, i, event.unit.empty() ? Value() : Value(event.unit));
		output.SetValue(col++, i, event.unit_id.empty() ? Value() : Value(event.unit_id));
		output.SetValue(col++, i, event.unit_status.empty() ? Value() : Value(event.unit_status));
		output.SetValue(col++, i, event.subunit.empty() ? Value() : Value(event.subunit));
		output.SetValue(col++, i, event.subunit_id.empty() ? Value() : Value(event.subunit_id));
		// Pattern analysis
		output.SetValue(col++, i, event.fingerprint.empty() ? Value() : Value(event.fingerprint));
		output.SetValue(col++, i, event.similarity_score == 0.0 ? Value() : Value::DOUBLE(event.similarity_score));
		output.SetValue(col++, i, event.pattern_id == -1 ? Value() : Value::BIGINT(event.pattern_id));
	}
}

// Parse test results implementation for string input
unique_ptr<FunctionData> ParseDuckHuntLogBind(ClientContext &context, TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types, vector<string> &names) {
	auto bind_data = make_uniq<ReadDuckHuntLogBindData>();

	// Get content parameter (required)
	if (input.inputs.empty()) {
		throw BinderException("parse_duck_hunt_log requires at least one parameter (content)");
	}
	bind_data->source = input.inputs[0].ToString();

	// Get format parameter (optional, defaults to auto)
	if (input.inputs.size() > 1) {
		std::string format_str = input.inputs[1].ToString();
		bind_data->format_name = format_str; // Store raw format name for registry lookup
		bind_data->format = StringToTestResultFormat(format_str);

		// Check for unknown format - but allow registry-only formats and format groups
		if (bind_data->format == TestResultFormat::UNKNOWN) {
			// Check if this format exists in the new parser registry or is a format group
			auto &registry = ParserRegistry::getInstance();
			if (!registry.hasFormat(format_str) && !registry.isGroup(format_str)) {
				throw BinderException("Unknown format: '" + format_str +
				                      "'. Use 'auto' for auto-detection or see docs/formats.md for supported formats.");
			}
			// Format exists in registry or is a group, keep UNKNOWN enum but format_name has the string
		}

		// For REGEXP format, extract the pattern after the "regexp:" prefix
		if (bind_data->format == TestResultFormat::REGEXP) {
			if (format_str.length() <= 7) {
				throw BinderException("regexp: format requires a pattern after the prefix, e.g., "
				                      "'regexp:(?P<severity>ERROR|WARN):\\s+(?P<message>.*)'");
			}
			bind_data->regexp_pattern = format_str.substr(7); // Remove "regexp:" prefix
		}
	} else {
		bind_data->format = TestResultFormat::AUTO;
		bind_data->format_name = "auto";
	}

	// Handle severity_threshold named parameter
	auto threshold_param = input.named_parameters.find("severity_threshold");
	if (threshold_param != input.named_parameters.end()) {
		std::string threshold_str = threshold_param->second.ToString();
		bind_data->severity_threshold = StringToSeverityLevel(threshold_str);
	}

	// Handle ignore_errors named parameter (note: parse_duck_hunt_log doesn't use files, but kept for API consistency)
	auto ignore_errors_param = input.named_parameters.find("ignore_errors");
	if (ignore_errors_param != input.named_parameters.end()) {
		bind_data->ignore_errors = ignore_errors_param->second.GetValue<bool>();
	}

	// Handle content named parameter (controls log_content truncation)
	auto content_param = input.named_parameters.find("content");
	if (content_param != input.named_parameters.end()) {
		auto &val = content_param->second;
		if (val.type().id() == LogicalTypeId::INTEGER || val.type().id() == LogicalTypeId::BIGINT) {
			int32_t limit = val.GetValue<int32_t>();
			if (limit <= 0) {
				bind_data->content_mode = ContentMode::NONE;
			} else {
				bind_data->content_mode = ContentMode::LIMIT;
				bind_data->content_limit = limit;
			}
		} else {
			std::string mode_str = StringUtil::Lower(val.ToString());
			if (mode_str == "full") {
				bind_data->content_mode = ContentMode::FULL;
			} else if (mode_str == "none") {
				bind_data->content_mode = ContentMode::NONE;
			} else if (mode_str == "smart") {
				bind_data->content_mode = ContentMode::SMART;
			} else {
				throw BinderException("Invalid content mode: '" + mode_str +
				                      "'. Use integer (char limit), 'full', 'none', or 'smart'.");
			}
		}
	}

	// Define return schema (Schema V2 - same as read_duck_hunt_log)
	return_types = {
	    // Core identification
	    LogicalType::BIGINT,  // event_id
	    LogicalType::VARCHAR, // tool_name
	    LogicalType::VARCHAR, // event_type
	    // Code location
	    LogicalType::VARCHAR, // file_path
	    LogicalType::INTEGER, // line_number
	    LogicalType::INTEGER, // column_number
	    LogicalType::VARCHAR, // function_name
	    // Classification
	    LogicalType::VARCHAR, // status
	    LogicalType::VARCHAR, // severity
	    LogicalType::VARCHAR, // category
	    LogicalType::VARCHAR, // error_code
	    // Content
	    LogicalType::VARCHAR, // message
	    LogicalType::VARCHAR, // suggestion
	    LogicalType::VARCHAR, // raw_output
	    LogicalType::VARCHAR, // structured_data
	    // Log tracking
	    LogicalType::INTEGER, // log_line_start
	    LogicalType::INTEGER, // log_line_end
	    LogicalType::VARCHAR, // log_file
	    // Test-specific
	    LogicalType::VARCHAR, // test_name
	    LogicalType::DOUBLE,  // execution_time
	    // Identity & Network
	    LogicalType::VARCHAR, // principal
	    LogicalType::VARCHAR, // origin
	    LogicalType::VARCHAR, // target
	    LogicalType::VARCHAR, // actor_type
	    // Temporal
	    LogicalType::VARCHAR, // started_at
	    // Correlation
	    LogicalType::VARCHAR, // external_id
	    // Hierarchical context
	    LogicalType::VARCHAR, // scope
	    LogicalType::VARCHAR, // scope_id
	    LogicalType::VARCHAR, // scope_status
	    LogicalType::VARCHAR, // group
	    LogicalType::VARCHAR, // group_id
	    LogicalType::VARCHAR, // group_status
	    LogicalType::VARCHAR, // unit
	    LogicalType::VARCHAR, // unit_id
	    LogicalType::VARCHAR, // unit_status
	    LogicalType::VARCHAR, // subunit
	    LogicalType::VARCHAR, // subunit_id
	    // Pattern analysis
	    LogicalType::VARCHAR, // fingerprint
	    LogicalType::DOUBLE,  // similarity_score
	    LogicalType::BIGINT   // pattern_id
	};

	names = {// Core identification
	         "event_id", "tool_name", "event_type",
	         // Code location
	         "ref_file", "ref_line", "ref_column", "function_name",
	         // Classification
	         "status", "severity", "category", "error_code",
	         // Content
	         "message", "suggestion", "log_content", "structured_data",
	         // Log tracking
	         "log_line_start", "log_line_end", "log_file",
	         // Test-specific
	         "test_name", "execution_time",
	         // Identity & Network
	         "principal", "origin", "target", "actor_type",
	         // Temporal
	         "started_at",
	         // Correlation
	         "external_id",
	         // Hierarchical context
	         "scope", "scope_id", "scope_status", "group", "group_id", "group_status", "unit", "unit_id", "unit_status",
	         "subunit", "subunit_id",
	         // Pattern analysis
	         "fingerprint", "similarity_score", "pattern_id"};

	return std::move(bind_data);
}

unique_ptr<GlobalTableFunctionState> ParseDuckHuntLogInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto global_state = make_uniq<ReadDuckHuntLogGlobalState>();

	// Use source directly as content (no file reading)
	std::string content = bind_data.source;

	// Auto-detect format if needed
	TestResultFormat format = bind_data.format;
	std::string format_name = bind_data.format_name;
	if (format == TestResultFormat::AUTO) {
		format = DetectTestResultFormat(content);
		format_name = TestResultFormatToString(format);

		// If legacy detection returned UNKNOWN, try new registry auto-detection
		if (format == TestResultFormat::UNKNOWN) {
			auto *detected_parser = TryAutoDetectNewRegistry(content);
			if (detected_parser) {
				format_name = detected_parser->getFormatName();
			}
		}
	}

	// Try new modular parser registry first
	// For registry-only formats (UNKNOWN enum but valid format_name), use by-name lookup
	bool parsed = false;
	if (format == TestResultFormat::UNKNOWN && !format_name.empty() && format_name != "unknown") {
		// Registry-only format - use format_name directly
		parsed = TryNewParserRegistryByName(context, format_name, content, global_state->events);
	} else {
		// Known enum format - try registry with enum
		parsed = TryNewParserRegistry(context, format, content, global_state->events);
	}

	if (!parsed) {
		// Legacy fallback for special cases not in modular registry
		switch (format) {
		case TestResultFormat::DUCKDB_TEST:
			duckdb::DuckDBTestParser::ParseDuckDBTestOutput(content, global_state->events);
			break;
		case TestResultFormat::GENERIC_LINT:
			duckdb::GenericLintParser::ParseGenericLint(content, global_state->events);
			break;
		case TestResultFormat::REGEXP:
			// Dynamic regexp parser - uses user-provided pattern
			duckdb::RegexpParser::ParseWithRegexp(content, bind_data.regexp_pattern, global_state->events);
			break;
		default:
			// All formats now handled by modular registry above
			break;
		}
	}

	// Phase 3B: Process error patterns for intelligent categorization
	ProcessErrorPatterns(global_state->events);

	// Apply severity threshold filtering
	if (bind_data.severity_threshold != SeverityLevel::DEBUG) {
		// Filter out events below the threshold
		auto new_end = std::remove_if(global_state->events.begin(), global_state->events.end(),
		                              [&bind_data](const ValidationEvent &event) {
			                              return !ShouldEmitEvent(event.severity, bind_data.severity_threshold);
		                              });
		global_state->events.erase(new_end, global_state->events.end());
	}

	return std::move(global_state);
}

unique_ptr<LocalTableFunctionState> ParseDuckHuntLogInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                              GlobalTableFunctionState *global_state) {
	return make_uniq<ReadDuckHuntLogLocalState>();
}

void ParseDuckHuntLogFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ReadDuckHuntLogBindData>();
	auto &global_state = data_p.global_state->Cast<ReadDuckHuntLogGlobalState>();
	auto &local_state = data_p.local_state->Cast<ReadDuckHuntLogLocalState>();

	// Populate output chunk with content truncation settings
	PopulateDataChunkFromEvents(output, global_state.events, local_state.chunk_offset, STANDARD_VECTOR_SIZE,
	                            bind_data.content_mode, bind_data.content_limit);

	// Update offset for next chunk
	local_state.chunk_offset += output.size();
}

TableFunctionSet GetReadDuckHuntLogFunction() {
	TableFunctionSet set("read_duck_hunt_log");

	// Single argument version: read_duck_hunt_log(source) - auto-detects format
	TableFunction single_arg("read_duck_hunt_log", {LogicalType::VARCHAR}, ReadDuckHuntLogFunction, ReadDuckHuntLogBind,
	                         ReadDuckHuntLogInitGlobal, ReadDuckHuntLogInitLocal);
	single_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	single_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	single_arg.named_parameters["content"] = LogicalType::ANY;
	set.AddFunction(single_arg);

	// Two argument version: read_duck_hunt_log(source, format)
	TableFunction two_arg("read_duck_hunt_log", {LogicalType::VARCHAR, LogicalType::VARCHAR}, ReadDuckHuntLogFunction,
	                      ReadDuckHuntLogBind, ReadDuckHuntLogInitGlobal, ReadDuckHuntLogInitLocal);
	two_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	two_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	two_arg.named_parameters["content"] = LogicalType::ANY;
	set.AddFunction(two_arg);

	return set;
}

TableFunctionSet GetParseDuckHuntLogFunction() {
	TableFunctionSet set("parse_duck_hunt_log");

	// Single argument version: parse_duck_hunt_log(content) - auto-detects format
	TableFunction single_arg("parse_duck_hunt_log", {LogicalType::VARCHAR}, ParseDuckHuntLogFunction,
	                         ParseDuckHuntLogBind, ParseDuckHuntLogInitGlobal, ParseDuckHuntLogInitLocal);
	single_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	single_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	single_arg.named_parameters["content"] = LogicalType::ANY;
	set.AddFunction(single_arg);

	// Two argument version: parse_duck_hunt_log(content, format)
	TableFunction two_arg("parse_duck_hunt_log", {LogicalType::VARCHAR, LogicalType::VARCHAR}, ParseDuckHuntLogFunction,
	                      ParseDuckHuntLogBind, ParseDuckHuntLogInitGlobal, ParseDuckHuntLogInitLocal);
	two_arg.named_parameters["severity_threshold"] = LogicalType::VARCHAR;
	two_arg.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	two_arg.named_parameters["content"] = LogicalType::ANY;
	set.AddFunction(two_arg);

	return set;
}

// Inline parsers moved to modular files:
// - ParseBazelBuild: src/parsers/build_systems/bazel_parser.cpp
// - ParseAutopep8Text: src/parsers/linting_tools/autopep8_text_parser.cpp
// - ParsePytestCovText: src/parsers/test_frameworks/pytest_cov_text_parser.cpp
// - ParseDuckDBTestOutput: src/parsers/test_frameworks/duckdb_test_parser.cpp
// - ParseGenericLint: src/parsers/tool_outputs/generic_lint_parser.cpp
// - ParseWithRegexp: src/parsers/tool_outputs/regexp_parser.cpp

// Phase 3A: Multi-file processing implementation

std::vector<std::string> GetFilesFromPattern(ClientContext &context, const std::string &pattern) {
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
			std::vector<std::string> patterns = {"*.xml", "*.json", "*.txt", "*.log", "*.out"};
			for (const auto &ext_pattern : patterns) {
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

std::vector<std::string> GetGlobFiles(ClientContext &context, const std::string &pattern) {
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

void ProcessMultipleFiles(ClientContext &context, const std::vector<std::string> &files, TestResultFormat format,
                          std::vector<ValidationEvent> &events, bool ignore_errors) {
	for (size_t file_idx = 0; file_idx < files.size(); file_idx++) {
		const auto &file_path = files[file_idx];

		try {
			// Read file content
			std::string content = ReadContentFromSource(context, file_path);

			// Detect format if AUTO
			TestResultFormat detected_format = format;
			if (format == TestResultFormat::AUTO) {
				detected_format = DetectTestResultFormat(content);
			}

			// Parse content using modular parser registry
			std::vector<ValidationEvent> file_events;

			// Skip REGEXP format in multi-file mode (requires pattern)
			if (detected_format == TestResultFormat::REGEXP) {
				continue;
			}

			// Use modular parser registry for all formats
			if (!TryNewParserRegistry(context, detected_format, content, file_events)) {
				// No parser found for this format, skip file
				continue;
			}

			// Set log_file on each event to track source file
			for (auto &event : file_events) {
				event.log_file = file_path;
			}

			// Add events to main collection
			events.insert(events.end(), file_events.begin(), file_events.end());

		} catch (const IOException &e) {
			// IOException (file not found, can't read, etc.) - always skip and continue
			continue;
		} catch (const std::exception &e) {
			// Parsing errors - skip if ignore_errors, otherwise rethrow
			if (ignore_errors) {
				continue;
			}
			throw;
		}
	}
}

std::string ExtractBuildIdFromPath(const std::string &file_path) {
	// Extract build ID from common patterns like:
	// - /builds/build-123/results.xml -> "build-123"
	// - /ci-logs/pipeline-456/test.log -> "pipeline-456"
	// - /artifacts/20231201-142323/output.txt -> "20231201-142323"

	std::regex build_patterns[] = {
	    std::regex(R"(/(?:build|pipeline|run|job)-([^/\s]+)/)"), // build-123, pipeline-456
	    std::regex(R"(/(\d{8}-\d{6})/)"),                        // 20231201-142323
	    std::regex(R"(/(?:builds?|ci|artifacts)/([^/\s]+)/)"),   // builds/abc123, ci/def456
	    std::regex(R"([_-](\w+\d+)[_-])"),                       // any_build123_ pattern
	};

	for (const auto &pattern : build_patterns) {
		std::smatch match;
		if (std::regex_search(file_path, match, pattern)) {
			return match[1].str();
		}
	}

	return ""; // No build ID found
}

std::string ExtractEnvironmentFromPath(const std::string &file_path) {
	// Extract environment from common patterns like:
	// - /environments/dev/results.xml -> "dev"
	// - /staging/ci-logs/test.log -> "staging"
	// - /prod/artifacts/output.txt -> "prod"

	std::vector<std::string> environments = {"dev",  "development", "staging", "stage",
	                                         "prod", "production",  "test",    "testing"};

	for (const auto &env : environments) {
		if (file_path.find("/" + env + "/") != std::string::npos ||
		    file_path.find("-" + env + "-") != std::string::npos ||
		    file_path.find("_" + env + "_") != std::string::npos) {
			return env;
		}
	}

	return ""; // No environment found
}

} // namespace duckdb
