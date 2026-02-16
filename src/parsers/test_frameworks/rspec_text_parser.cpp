#include "rspec_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

// Pre-compiled regex patterns for RSpec parsing (compiled once, reused)
namespace {
static const std::regex RE_TEST_PASSED_MARKER(R"(\s*✓\s*(.+))");
static const std::regex RE_TEST_FAILED_MARKER(R"(\s*✗\s*(.+))");
static const std::regex RE_DOC_TEST_LINE(R"(^(\s{4,})(.+?)\s*(\(FAILED - \d+\)|\(PENDING.*\))?\s*$)");
static const std::regex RE_DOC_CONTEXT_2SPACE(R"(^  (#?\w.+?)\s*$)");
static const std::regex RE_DOC_CONTEXT_TOP(R"(^([A-Z][A-Za-z0-9_:]+)\s*$)");
static const std::regex RE_TEST_PENDING(R"(\s*pending:\s*(.+)\s*\(PENDING:\s*(.+)\))");
static const std::regex RE_FAILURE_ERROR(R"(Failure/Error:\s*(.+))");
static const std::regex RE_EXPECTED_PATTERN(R"(\s*expected\s*(.+))");
static const std::regex RE_GOT_PATTERN(R"(\s*got:\s*(.+))");
static const std::regex RE_FILE_LINE_PATTERN(R"(# (.+):(\d+):in)");
static const std::regex RE_SUMMARY_PATTERN(R"(Finished in (.+) seconds .* (\d+) examples?, (\d+) failures?(, (\d+) pending)?)");
static const std::regex RE_FAILED_EXAMPLE(R"(rspec (.+):(\d+) # (.+))");
static const std::regex RE_SIMPLE_SUMMARY(R"((\d+) examples?, (\d+) failures?)");
} // anonymous namespace

bool RSpecTextParser::canParse(const std::string &content) const {
	// RSpec-specific patterns that distinguish it from other test frameworks
	// Key distinguishing features:
	// 1. "N examples, N failures" summary format (not "tests" or "test suites")
	// 2. "Failure/Error:" pattern unique to RSpec
	// 3. "rspec ./path:line" in failed examples section
	// 4. "Failed examples:" section header

	// Strong RSpec indicators (highly specific)
	bool has_rspec_summary =
	    (content.find(" examples,") != std::string::npos && content.find(" failures") != std::string::npos);
	bool has_failure_error = content.find("Failure/Error:") != std::string::npos;
	bool has_rspec_command = content.find("rspec ./") != std::string::npos;
	bool has_failed_examples = content.find("Failed examples:") != std::string::npos;

	// If we have strong RSpec indicators, it's definitely RSpec
	if (has_failure_error || has_rspec_command || has_failed_examples) {
		return true;
	}

	// RSpec summary format: "N examples, N failures" (NOT "tests" or "Test Suites")
	if (has_rspec_summary) {
		// Make sure it's not Jest/Mocha (they use "passing"/"failing" or "Test Suites")
		bool is_jest_mocha =
		    content.find("Test Suites:") != std::string::npos || content.find(" passing") != std::string::npos;
		if (!is_jest_mocha) {
			return true;
		}
	}

	// RSpec documentation format with pending marker
	// Format: "test name (PENDING: reason)" or "(FAILED - N)"
	bool has_rspec_markers =
	    content.find("(PENDING:") != std::string::npos || content.find("(FAILED - ") != std::string::npos;
	if (has_rspec_markers) {
		return true;
	}

	return false;
}

// Forward declaration for implementation
static void parseRSpecTextImpl(const std::string &content, std::vector<ValidationEvent> &events);

std::vector<ValidationEvent> RSpecTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	parseRSpecTextImpl(content, events);
	return events;
}

static void parseRSpecTextImpl(const std::string &content, std::vector<ValidationEvent> &events) {
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_context;
	std::string current_method;
	std::string current_failure_file;
	int current_failure_line = -1;
	std::string current_failure_message;
	bool in_failure_section = false;
	bool in_failed_examples = false;
	std::vector<std::string> context_stack; // Track nested contexts

	while (std::getline(stream, line)) {
		current_line_num++;
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
		if (in_failed_examples && std::regex_search(line, match, RE_FAILED_EXAMPLE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "RSpec";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "test_failure";
			event.ref_file = match[1].str();
			event.ref_line = SafeParsing::SafeStoi(match[2].str());
			event.test_name = match[3].str();
			event.message = "Test failed: " + match[3].str();
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			events.push_back(event);
			continue;
		}

		// Parse top-level context (class/module names) - no indent
		if (std::regex_match(line, match, RE_DOC_CONTEXT_TOP)) {
			current_context = match[1].str();
			context_stack.clear();
			context_stack.push_back(current_context);
			current_method = "";
			continue;
		}

		// Parse 2-space indented context (describe/context blocks)
		if (std::regex_match(line, match, RE_DOC_CONTEXT_2SPACE)) {
			std::string ctx = match[1].str();
			// This is a sub-context
			if (context_stack.size() > 1) {
				context_stack.resize(1); // Keep only top-level
			}
			context_stack.push_back(ctx);
			current_method = ctx;
			continue;
		}

		// Parse documentation format test lines (4+ spaces indent)
		// These are actual test cases (it blocks)
		if (!in_failure_section && !in_failed_examples && std::regex_match(line, match, RE_DOC_TEST_LINE)) {
			std::string indent = match[1].str();
			std::string test_name = match[2].str();
			std::string status_marker = match[3].matched ? match[3].str() : "";

			// Skip if this looks like a context header (starts with #, or is very short capitalized word)
			if (test_name.length() > 0 && test_name[0] == '#') {
				// This is a method context like "#login"
				current_method = test_name.substr(1); // Remove #
				continue;
			}

			// Determine status based on marker
			ValidationEventStatus status = ValidationEventStatus::PASS;
			std::string severity = "info";
			std::string category = "test_success";

			if (status_marker.find("FAILED") != std::string::npos) {
				status = ValidationEventStatus::FAIL;
				severity = "error";
				category = "test_failure";
			} else if (status_marker.find("PENDING") != std::string::npos) {
				status = ValidationEventStatus::SKIP;
				severity = "warning";
				category = "test_pending";
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "RSpec";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = status;
			event.severity = severity;
			event.category = category;

			// Build full context name
			std::string full_context;
			for (const auto &ctx : context_stack) {
				if (!full_context.empty())
					full_context += " ";
				full_context += ctx;
			}
			event.function_name = full_context;
			event.test_name = test_name;
			event.message = (status == ValidationEventStatus::PASS   ? "Test passed: "
			                 : status == ValidationEventStatus::FAIL ? "Test failed: "
			                                                         : "Test pending: ") +
			                test_name;
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			events.push_back(event);
			continue;
		}

		// Parse passed tests with ✓ marker
		if (std::regex_search(line, match, RE_TEST_PASSED_MARKER)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "RSpec";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "test_success";
			std::string full_context;
			for (const auto &ctx : context_stack) {
				if (!full_context.empty())
					full_context += " ";
				full_context += ctx;
			}
			event.function_name = full_context;
			event.test_name = match[1].str();
			event.message = "Test passed: " + match[1].str();
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			events.push_back(event);
		}

		// Parse failed tests with ✗ marker
		else if (std::regex_search(line, match, RE_TEST_FAILED_MARKER)) {
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
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			events.push_back(event);
		}

		// Parse pending tests
		else if (std::regex_search(line, match, RE_TEST_PENDING)) {
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
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			events.push_back(event);
		}

		// Parse failure details
		else if (std::regex_search(line, match, RE_FAILURE_ERROR)) {
			current_failure_message = match[1].str();
		}

		// Parse expected/got patterns for better error details
		else if (std::regex_search(line, match, RE_EXPECTED_PATTERN)) {
			if (!current_failure_message.empty()) {
				current_failure_message += " | Expected: " + match[1].str();
			}
		} else if (std::regex_search(line, match, RE_GOT_PATTERN)) {
			if (!current_failure_message.empty()) {
				current_failure_message += " | Got: " + match[1].str();
			}
		}

		// Parse file and line information
		else if (std::regex_search(line, match, RE_FILE_LINE_PATTERN)) {
			current_failure_file = match[1].str();
			current_failure_line = SafeParsing::SafeStoi(match[2].str());

			// Update the most recent failed test event with file/line info
			for (auto it = events.rbegin(); it != events.rend(); ++it) {
				if (it->tool_name == "RSpec" && it->status == ValidationEventStatus::FAIL && it->ref_file.empty()) {
					it->ref_file = current_failure_file;
					it->ref_line = current_failure_line;
					if (!current_failure_message.empty()) {
						it->message = current_failure_message;
					}
					break;
				}
			}
		}

		// Parse summary
		else if (std::regex_search(line, match, RE_SUMMARY_PATTERN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "RSpec";
			event.event_type = ValidationEventType::SUMMARY;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "test_summary";

			std::string execution_time = match[1].str();
			int total_examples = SafeParsing::SafeStoi(match[2].str());
			int failures = SafeParsing::SafeStoi(match[3].str());
			int pending = 0;
			if (match[5].matched) {
				pending = SafeParsing::SafeStoi(match[5].str());
			}

			event.message = "Test run completed: " + std::to_string(total_examples) + " examples, " +
			                std::to_string(failures) + " failures, " + std::to_string(pending) + " pending";
			event.execution_time = SafeParsing::SafeStod(execution_time);
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			events.push_back(event);
		}
	}
}

} // namespace duckdb
