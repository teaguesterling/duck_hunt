#include "msbuild_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

bool MSBuildParser::canParse(const std::string &content) const {
	// Check for MSBuild patterns
	return (content.find("Microsoft (R) Build Engine") != std::string::npos ||
	        content.find("MSBuild") != std::string::npos) ||
	       (content.find("Build FAILED.") != std::string::npos ||
	        content.find("Build succeeded.") != std::string::npos) ||
	       (content.find("): error CS") != std::string::npos || content.find("): warning CS") != std::string::npos) ||
	       (content.find("[xUnit.net") != std::string::npos && content.find(".csproj") != std::string::npos) ||
	       (content.find("Time Elapsed") != std::string::npos && content.find("Error(s)") != std::string::npos);
}

std::vector<ValidationEvent> MSBuildParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream iss(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// MSBuild patterns
	static const std::regex compile_error_pattern(R"((.+?)\((\d+),(\d+)\): error (CS\d+): (.+?) \[(.+?\.csproj)\])");
	static const std::regex compile_warning_pattern(
	    R"((.+?)\((\d+),(\d+)\): warning (CS\d+|CA\d+): (.+?) \[(.+?\.csproj)\])");
	static const std::regex build_result_pattern(R"(Build (FAILED|succeeded)\.)");
	static const std::regex error_summary_pattern(R"(\s+(\d+) Error\(s\))");
	static const std::regex warning_summary_pattern(R"(\s+(\d+) Warning\(s\))");
	static const std::regex time_elapsed_pattern(R"(Time Elapsed (\d+):(\d+):(\d+)\.(\d+))");
	static const std::regex test_result_pattern(
	    R"((Failed|Passed)!\s+-\s+Failed:\s+(\d+),\s+Passed:\s+(\d+),\s+Skipped:\s+(\d+),\s+Total:\s+(\d+),\s+Duration:\s+(\d+)\s*ms)");
	static const std::regex xunit_test_pattern(R"(\[xUnit\.net\s+[\d:\.]+\]\s+(.+?)\.(.+?)\s+\[(PASS|FAIL|SKIP)\])");
	static const std::regex project_pattern("Project \"(.+?\\.csproj)\" on node (\\d+) \\((.+?) targets\\)\\.");

	std::string current_project = "";

	while (std::getline(iss, line)) {
		current_line_num++;
		std::smatch match;

		// Parse C# compilation errors
		if (std::regex_search(line, match, compile_error_pattern)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "msbuild-csc";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = match[1].str();
			event.ref_line = SafeParsing::SafeStoi(match[2].str());
			event.ref_column = SafeParsing::SafeStoi(match[3].str());
			event.function_name = current_project;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "compilation";
			event.message = match[5].str();
			event.error_code = match[4].str();
			event.log_content = content;
			event.structured_data = "msbuild";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse C# compilation warnings
		else if (std::regex_search(line, match, compile_warning_pattern)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "msbuild-csc";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = match[1].str();
			event.ref_line = SafeParsing::SafeStoi(match[2].str());
			event.ref_column = SafeParsing::SafeStoi(match[3].str());
			event.function_name = current_project;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "compilation";
			event.message = match[5].str();
			event.error_code = match[4].str();
			event.log_content = content;
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
			int failed = SafeParsing::SafeStoi(match[2].str());
			int passed = SafeParsing::SafeStoi(match[3].str());
			int skipped = SafeParsing::SafeStoi(match[4].str());
			int total = SafeParsing::SafeStoi(match[5].str());
			int duration = SafeParsing::SafeStoi(match[6].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "dotnet-test";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.severity = (failed > 0) ? "error" : "info";
			event.category = "test_summary";
			event.message = "Tests: " + std::to_string(total) + " total, " + std::to_string(passed) + " passed, " +
			                std::to_string(failed) + " failed, " + std::to_string(skipped) + " skipped";
			event.execution_time = static_cast<double>(duration) / 1000.0; // Convert ms to seconds
			event.log_content = content;
			event.structured_data = "msbuild";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

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

			event.log_content = content;
			event.structured_data = "msbuild";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

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
			event.log_content = content;
			event.structured_data = "msbuild";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse project context
		else if (std::regex_search(line, match, project_pattern)) {
			current_project = match[1].str();
		}
		// Parse build timing
		else if (std::regex_search(line, match, time_elapsed_pattern)) {
			int hours = SafeParsing::SafeStoi(match[1].str());
			int minutes = SafeParsing::SafeStoi(match[2].str());
			int seconds = SafeParsing::SafeStoi(match[3].str());
			int milliseconds = SafeParsing::SafeStoi(match[4].str());

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
			event.log_content = content;
			event.structured_data = "msbuild";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse error/warning summaries
		else if (std::regex_search(line, match, error_summary_pattern)) {
			int error_count = SafeParsing::SafeStoi(match[1].str());

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
				event.log_content = content;
				event.structured_data = "msbuild";

				events.push_back(event);
			}
		} else if (std::regex_search(line, match, warning_summary_pattern)) {
			int warning_count = SafeParsing::SafeStoi(match[1].str());

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
				event.log_content = content;
				event.structured_data = "msbuild";

				events.push_back(event);
			}
		}
	}

	return events;
}

} // namespace duckdb
