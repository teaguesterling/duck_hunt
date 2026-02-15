#include "drone_ci_text_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Pre-compiled regex patterns for DroneCI text parsing (compiled once, reused)
namespace {
static const std::regex RE_DRONE_STEP_START(R"(\[drone:exec\] .* starting build step: (.+))");
static const std::regex RE_DRONE_STEP_COMPLETE(R"(\[drone:exec\] .* completed build step: (.+) \(exit code (\d+)\))");
static const std::regex RE_DRONE_PIPELINE_COMPLETE(R"(\[drone:exec\] .* pipeline execution complete)");
static const std::regex RE_DRONE_PIPELINE_FAILED(R"(\[drone:exec\] .* pipeline failed with exit code (\d+))");
static const std::regex RE_GIT_CLONE(R"(\+ git clone (.+) \.)");
static const std::regex RE_GIT_CHECKOUT(R"(\+ git checkout ([a-f0-9]+))");
static const std::regex RE_NPM_INSTALL(R"(added (\d+) packages .* in ([\d.]+)s)");
static const std::regex RE_NPM_VULNERABILITIES(R"(found (\d+) vulnerabilit)");
static const std::regex RE_JEST_TEST_PASS(R"(PASS (.+) \(([\d.]+) s\))");
static const std::regex RE_JEST_TEST_FAIL(R"(FAIL (.+) \(([\d.]+) s\))");
static const std::regex RE_JEST_TEST_ITEM(R"(✓ (.+) \((\d+) ms\))");
static const std::regex RE_JEST_TEST_FAIL_ITEM(R"(✗ (.+) \(([\d.]+) s\))");
static const std::regex RE_JEST_SUMMARY(R"(Test Suites: (\d+) failed, (\d+) passed, (\d+) total)");
static const std::regex RE_JEST_TEST_SUMMARY(R"(Tests: (\d+) failed, (\d+) passed, (\d+) total)");
static const std::regex RE_JEST_TIMING(R"(Time: ([\d.]+) s)");
static const std::regex RE_WEBPACK_BUILD(R"(Hash: ([a-f0-9]+))");
static const std::regex RE_WEBPACK_WARNING(R"(Module Warning \(from ([^)]+)\):)");
static const std::regex RE_ESLINT_WARNING(R"((\d+):(\d+)\s+(warning|error)\s+(.+)\s+([a-z-]+))");
static const std::regex RE_DOCKER_BUILD_START(R"(Sending build context to Docker daemon\s+([\d.]+[KMG]?B))");
static const std::regex RE_DOCKER_STEP(R"(Step (\d+)/(\d+) : (.+))");
static const std::regex RE_DOCKER_SUCCESS(R"(Successfully built ([a-f0-9]+))");
static const std::regex RE_DOCKER_TAGGED(R"(Successfully tagged (.+))");
static const std::regex RE_CURL_NOTIFICATION(R"(\+ curl -X POST .* --data '(.+)' )");
} // anonymous namespace

bool DroneCITextParser::canParse(const std::string &content) const {
	return isValidDroneCIText(content);
}

bool DroneCITextParser::isValidDroneCIText(const std::string &content) const {
	// Look for DroneCI specific markers
	if (content.find("[drone:exec]") != std::string::npos ||
	    content.find("starting build step:") != std::string::npos ||
	    content.find("pipeline execution") != std::string::npos) {
		return true;
	}
	return false;
}

std::vector<ValidationEvent> DroneCITextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// Reserve space for events (estimate based on content size)
	events.reserve(content.size() / 100);

	std::smatch match;
	std::string current_step = "";

	while (std::getline(stream, line)) {
		current_line_num++;
		// Parse DroneCI step start
		if (std::regex_search(line, match, RE_DRONE_STEP_START)) {
			current_step = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "step_start";
			event.message = "Starting build step: " + current_step;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse DroneCI step completion
		if (std::regex_search(line, match, RE_DRONE_STEP_COMPLETE)) {
			std::string step_name = match[1].str();
			int exit_code = SafeParsing::SafeStoi(match[2].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = exit_code == 0 ? ValidationEventStatus::PASS : ValidationEventStatus::FAIL;
			event.severity = exit_code == 0 ? "info" : "error";
			event.category = "step_complete";
			event.message = "Completed build step: " + step_name + " (exit code " + std::to_string(exit_code) + ")";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse pipeline completion
		if (std::regex_search(line, match, RE_DRONE_PIPELINE_COMPLETE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "pipeline_complete";
			event.message = "Pipeline execution complete";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse pipeline failure
		if (std::regex_search(line, match, RE_DRONE_PIPELINE_FAILED)) {
			int exit_code = SafeParsing::SafeStoi(match[1].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "pipeline_failed";
			event.message = "Pipeline failed with exit code " + std::to_string(exit_code);
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Git operations
		if (std::regex_search(line, match, RE_GIT_CLONE)) {
			std::string repo_url = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "git_clone";
			event.message = "Cloning repository: " + repo_url;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_GIT_CHECKOUT)) {
			std::string commit_hash = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "git_checkout";
			event.message = "Checkout commit: " + commit_hash;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse NPM operations
		if (std::regex_search(line, match, RE_NPM_INSTALL)) {
			int package_count = SafeParsing::SafeStoi(match[1].str());
			double install_time = SafeParsing::SafeStod(match[2].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "npm_install";
			event.message =
			    "NPM install: " + std::to_string(package_count) + " packages in " + std::to_string(install_time) + "s";
			event.execution_time = install_time;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_NPM_VULNERABILITIES)) {
			int vuln_count = SafeParsing::SafeStoi(match[1].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SECURITY_FINDING;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "npm_vulnerabilities";
			event.message = "Found " + std::to_string(vuln_count) + " npm vulnerabilities";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Jest test results
		if (std::regex_search(line, match, RE_JEST_TEST_PASS)) {
			std::string test_file = match[1].str();
			double test_time = SafeParsing::SafeStod(match[2].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = test_file;
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "jest_test";
			event.message = "Test passed: " + test_file;
			event.execution_time = test_time;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_JEST_TEST_FAIL)) {
			std::string test_file = match[1].str();
			double test_time = SafeParsing::SafeStod(match[2].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = test_file;
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "jest_test";
			event.message = "Test failed: " + test_file;
			event.execution_time = test_time;
			event.log_content = content;
			event.structured_data = "drone_ci_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_JEST_TEST_ITEM)) {
			std::string test_name = match[1].str();
			int test_time_ms = SafeParsing::SafeStoi(match[2].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "jest_test_item";
			event.message = "Test passed: " + test_name;
			event.test_name = test_name;
			event.execution_time = test_time_ms / 1000.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_JEST_TEST_FAIL_ITEM)) {
			std::string test_name = match[1].str();
			double test_time = SafeParsing::SafeStod(match[2].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "jest_test_item";
			event.message = "Test failed: " + test_name;
			event.test_name = test_name;
			event.execution_time = test_time;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Jest summaries
		if (std::regex_search(line, match, RE_JEST_SUMMARY)) {
			int failed = SafeParsing::SafeStoi(match[1].str());
			int passed = SafeParsing::SafeStoi(match[2].str());
			int total = SafeParsing::SafeStoi(match[3].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = failed > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.severity = failed > 0 ? "error" : "info";
			event.category = "jest_suite_summary";
			event.message = "Test Suites: " + std::to_string(failed) + " failed, " + std::to_string(passed) +
			                " passed, " + std::to_string(total) + " total";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_JEST_TEST_SUMMARY)) {
			int failed = SafeParsing::SafeStoi(match[1].str());
			int passed = SafeParsing::SafeStoi(match[2].str());
			int total = SafeParsing::SafeStoi(match[3].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = failed > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.severity = failed > 0 ? "error" : "info";
			event.category = "jest_test_summary";
			event.message = "Tests: " + std::to_string(failed) + " failed, " + std::to_string(passed) + " passed, " +
			                std::to_string(total) + " total";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_JEST_TIMING)) {
			double total_time = SafeParsing::SafeStod(match[1].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "jest_timing";
			event.message = "Test execution time: " + std::to_string(total_time) + "s";
			event.execution_time = total_time;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Webpack build
		if (std::regex_search(line, match, RE_WEBPACK_BUILD)) {
			std::string build_hash = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "webpack_build";
			event.message = "Webpack build hash: " + build_hash;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_WEBPACK_WARNING)) {
			std::string warning_source = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = warning_source;
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "webpack_warning";
			event.message = "Webpack module warning from " + warning_source;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse ESLint warnings/errors
		if (std::regex_search(line, match, RE_ESLINT_WARNING)) {
			int line_num = SafeParsing::SafeStoi(match[1].str());
			int col_num = SafeParsing::SafeStoi(match[2].str());
			std::string level = match[3].str();
			std::string message = match[4].str();
			std::string rule = match[5].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = line_num;
			event.ref_column = col_num;
			event.status = level == "error" ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
			event.severity = level;
			event.category = "eslint";
			event.message = message;
			event.error_code = rule;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Docker operations
		if (std::regex_search(line, match, RE_DOCKER_BUILD_START)) {
			std::string context_size = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "docker_build";
			event.message = "Docker build context: " + context_size;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_DOCKER_STEP)) {
			int step_num = SafeParsing::SafeStoi(match[1].str());
			int total_steps = SafeParsing::SafeStoi(match[2].str());
			std::string step_command = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "docker_step";
			event.message =
			    "Docker step " + std::to_string(step_num) + "/" + std::to_string(total_steps) + ": " + step_command;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_DOCKER_SUCCESS)) {
			std::string image_id = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "docker_success";
			event.message = "Docker build successful: " + image_id;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_DOCKER_TAGGED)) {
			std::string tag = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "docker_tagged";
			event.message = "Docker image tagged: " + tag;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse notification curl
		if (std::regex_search(line, match, RE_CURL_NOTIFICATION)) {
			std::string notification_data = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "drone-ci";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "notification";
			event.message = "Sending notification: " + notification_data;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "drone_ci_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}
	}

	return events;
}

} // namespace duckdb
