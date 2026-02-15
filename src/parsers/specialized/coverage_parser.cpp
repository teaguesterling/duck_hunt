#include "coverage_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

// Pre-compiled regex patterns for coverage.py parsing (compiled once, reused)
namespace {
// Coverage.py patterns
static const std::regex RE_COVERAGE_HEADER(R"(Name\s+Stmts\s+Miss\s+Cover(?:\s+Missing)?)");
static const std::regex RE_COVERAGE_BRANCH_HEADER(R"(Name\s+Stmts\s+Miss\s+Branch\s+BrPart\s+Cover(?:\s+Missing)?)");
static const std::regex RE_SEPARATOR_LINE(R"(^-+$)");
static const std::regex RE_COVERAGE_ROW(R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_COVERAGE_BRANCH_ROW(
    R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_TOTAL_ROW(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_TOTAL_BRANCH_ROW(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_COVERAGE_RUN(R"(coverage run (.+))");
static const std::regex RE_COVERAGE_COMMAND(R"(coverage (html|xml|json|report|erase|combine|debug))");
static const std::regex RE_REPORT_GENERATED(R"(Coverage report generated in ([\d\.]+) seconds)");
static const std::regex RE_WROTE_REPORT(R"(Wrote (HTML|XML|JSON) report to (.+))");
static const std::regex RE_COVERAGE_FAILURE(R"(Coverage failure: total of (\d+%|\d+\.\d+%) is below --fail-under=(\d+%))");
static const std::regex RE_NO_DATA(R"(coverage: No data to report\.)");
static const std::regex RE_NO_DATA_COLLECTED(R"(coverage: CoverageWarning: No data was collected\. \(no-data-collected\))");
static const std::regex RE_CONTEXT_RECORDED(R"(Context '(.+)' recorded)");
static const std::regex RE_COMBINED_DATA(R"(Combined data file (.+))");
static const std::regex RE_WROTE_COMBINED(R"(Wrote combined data to (.+))");
static const std::regex RE_ERASED_COVERAGE(R"(Erased (.coverage))");
static const std::regex RE_DELTA_SUMMARY(R"(Total coverage: ([\d\.]+%))");
static const std::regex RE_FILES_CHANGED(R"(Files changed: (\d+))");
static const std::regex RE_LINES_ADDED(R"(Lines added: (\d+))");
static const std::regex RE_LINES_COVERED(R"(Lines covered: (\d+))");
static const std::regex RE_PERCENTAGE_COVERED(R"(Percentage covered: ([\d\.]+%))");

// Pytest-cov patterns
static const std::regex RE_TEST_SESSION_START(R"(={3,} test session starts ={3,})");
static const std::regex RE_PLATFORM_INFO(R"(platform (.+) -- Python (.+), pytest-(.+), pluggy-(.+))");
static const std::regex RE_PYTEST_COV_PLUGIN(R"(plugins: cov-(.+))");
static const std::regex RE_COLLECTED_ITEMS(R"(collected (\d+) items?)");
static const std::regex RE_TEST_RESULT(R"((.+\.py)::(.+)\s+(PASSED|FAILED|SKIPPED|ERROR)\s+\[([^\]]+)\])");
static const std::regex RE_TEST_SUMMARY_LINE(R"(={3,} (\d+) failed, (\d+) passed(?:, (\d+) skipped)? in ([\d\.]+)s ={3,})");
static const std::regex RE_COVERAGE_SECTION(R"(----------- coverage: platform (.+), python (.+) -----------)");
static const std::regex RE_TOTAL_COVERAGE(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_TOTAL_BRANCH_COVERAGE(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_COVERAGE_THRESHOLD_FAIL(R"(Coverage threshold check failed\. Expected: >= (\d+)%, got: ([\d\.]+%))");
static const std::regex RE_REQUIRED_COVERAGE_FAIL(R"(Required test coverage of (\d+)% not met\. Total coverage: ([\d\.]+%))");
static const std::regex RE_COVERAGE_XML_WRITTEN(R"(Coverage XML written to (.+))");
static const std::regex RE_COVERAGE_HTML_WRITTEN(R"(Coverage HTML written to dir (.+))");
static const std::regex RE_COVERAGE_DATA_NOT_FOUND(R"(pytest-cov: Coverage data was not found for source '(.+)')");
static const std::regex RE_MODULE_NEVER_IMPORTED(R"(pytest-cov: Module '(.+)' was never imported\.)");
} // anonymous namespace

bool CoverageParser::CanParse(const std::string &content) const {
	// Check for coverage.py patterns
	return ((content.find("Name") != std::string::npos && content.find("Stmts") != std::string::npos &&
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
	        (content.find("Branch") != std::string::npos && content.find("BrPart") != std::string::npos)) ||
	       // Check for pytest-cov patterns
	       ((content.find("-- coverage:") != std::string::npos && content.find("python") != std::string::npos) ||
	        (content.find("collected") != std::string::npos && content.find("items") != std::string::npos &&
	         content.find("----------- coverage:") != std::string::npos) ||
	        (content.find("PASSED") != std::string::npos && content.find("::") != std::string::npos &&
	         content.find("Name") != std::string::npos && content.find("Stmts") != std::string::npos &&
	         content.find("Miss") != std::string::npos && content.find("Cover") != std::string::npos) ||
	        (content.find("platform") != std::string::npos && content.find("plugins: cov-") != std::string::npos) ||
	        (content.find("Coverage threshold check failed") != std::string::npos &&
	         content.find("Expected:") != std::string::npos) ||
	        (content.find("Required test coverage") != std::string::npos &&
	         content.find("not met") != std::string::npos));
}

void CoverageParser::Parse(const std::string &content, std::vector<duckdb::ValidationEvent> &events) const {
	// Check which type of coverage output this is
	if (content.find("pytest") != std::string::npos && content.find("test session starts") != std::string::npos) {
		ParsePytestCovText(content, events);
	} else {
		ParseCoverageText(content, events);
	}
}

void CoverageParser::ParseCoverageText(const std::string &content, std::vector<duckdb::ValidationEvent> &events) {
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::smatch match;
	bool in_coverage_table = false;
	bool in_branch_table = false;

	while (std::getline(stream, line)) {
		current_line_num++;
		// Handle coverage table headers
		if (std::regex_search(line, match, RE_COVERAGE_HEADER)) {
			in_coverage_table = true;
			in_branch_table = false;

			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "report_header";
			event.message = "Coverage report table started";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle branch coverage table headers
		if (std::regex_search(line, match, RE_COVERAGE_BRANCH_HEADER)) {
			in_coverage_table = true;
			in_branch_table = true;

			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "report_header";
			event.message = "Branch coverage report table started";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle separator lines
		if (std::regex_search(line, match, RE_SEPARATOR_LINE)) {
			continue; // Skip separator lines
		}

		// Handle branch coverage rows
		if (in_branch_table && std::regex_search(line, match, RE_COVERAGE_BRANCH_ROW)) {
			std::string file_path = match[1].str();
			std::string stmts = match[2].str();
			std::string miss = match[3].str();
			std::string branch = match[4].str();
			std::string br_part = match[5].str();
			std::string cover = match[6].str();
			std::string missing = match[7].str();

			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = file_path;
			event.ref_line = -1;
			event.ref_column = -1;

			// Set status based on coverage percentage
			double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
			if (coverage_pct >= 90.0) {
				event.status = duckdb::ValidationEventStatus::INFO;
				event.severity = "info";
			} else if (coverage_pct >= 70.0) {
				event.status = duckdb::ValidationEventStatus::WARNING;
				event.severity = "warning";
			} else {
				event.status = duckdb::ValidationEventStatus::ERROR;
				event.severity = "error";
			}

			event.category = "branch_coverage";
			event.message = "Stmts: " + stmts + ", Miss: " + miss + ", Branch: " + branch + ", BrPart: " + br_part +
			                ", Cover: " + cover;
			if (!missing.empty()) {
				event.suggestion = "Missing lines: " + missing;
			}
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle regular coverage rows
		if (in_coverage_table && !in_branch_table && std::regex_search(line, match, RE_COVERAGE_ROW)) {
			std::string file_path = match[1].str();
			std::string stmts = match[2].str();
			std::string miss = match[3].str();
			std::string cover = match[4].str();
			std::string missing = match[5].str();

			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = file_path;
			event.ref_line = -1;
			event.ref_column = -1;

			// Set status based on coverage percentage
			double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
			if (coverage_pct >= 90.0) {
				event.status = duckdb::ValidationEventStatus::INFO;
				event.severity = "info";
			} else if (coverage_pct >= 70.0) {
				event.status = duckdb::ValidationEventStatus::WARNING;
				event.severity = "warning";
			} else {
				event.status = duckdb::ValidationEventStatus::ERROR;
				event.severity = "error";
			}

			event.category = "line_coverage";
			event.message = "Stmts: " + stmts + ", Miss: " + miss + ", Cover: " + cover;
			if (!missing.empty()) {
				event.suggestion = "Missing lines: " + missing;
			}
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle TOTAL rows for branch coverage
		if (in_branch_table && std::regex_search(line, match, RE_TOTAL_BRANCH_ROW)) {
			in_coverage_table = false;
			in_branch_table = false;

			std::string stmts = match[1].str();
			std::string miss = match[2].str();
			std::string branch = match[3].str();
			std::string br_part = match[4].str();
			std::string cover = match[5].str();
			std::string missing = match[6].str();

			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			// Set status based on coverage percentage
			double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
			if (coverage_pct >= 90.0) {
				event.status = duckdb::ValidationEventStatus::INFO;
				event.severity = "info";
			} else if (coverage_pct >= 70.0) {
				event.status = duckdb::ValidationEventStatus::WARNING;
				event.severity = "warning";
			} else {
				event.status = duckdb::ValidationEventStatus::ERROR;
				event.severity = "error";
			}

			event.category = "total_branch_coverage";
			event.message = "Total branch coverage: " + cover + " (Stmts: " + stmts + ", Miss: " + miss +
			                ", Branch: " + branch + ", BrPart: " + br_part + ")";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle TOTAL rows for regular coverage
		if (in_coverage_table && !in_branch_table && std::regex_search(line, match, RE_TOTAL_ROW)) {
			in_coverage_table = false;

			std::string stmts = match[1].str();
			std::string miss = match[2].str();
			std::string cover = match[3].str();
			std::string missing = match[4].str();

			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			// Set status based on coverage percentage
			double coverage_pct = std::stod(cover.substr(0, cover.length() - 1));
			if (coverage_pct >= 90.0) {
				event.status = duckdb::ValidationEventStatus::INFO;
				event.severity = "info";
			} else if (coverage_pct >= 70.0) {
				event.status = duckdb::ValidationEventStatus::WARNING;
				event.severity = "warning";
			} else {
				event.status = duckdb::ValidationEventStatus::ERROR;
				event.severity = "error";
			}

			event.category = "total_coverage";
			event.message = "Total coverage: " + cover + " (Stmts: " + stmts + ", Miss: " + miss + ")";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle coverage run commands
		if (std::regex_search(line, match, RE_COVERAGE_RUN)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "command";
			event.message = "Coverage run: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle coverage commands
		if (std::regex_search(line, match, RE_COVERAGE_COMMAND)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "command";
			event.message = "Coverage command: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle report generation
		if (std::regex_search(line, match, RE_REPORT_GENERATED)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "performance";
			event.message = "Coverage report generated";
			event.execution_time = std::stod(match[1].str());
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle report writing
		if (std::regex_search(line, match, RE_WROTE_REPORT)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = match[2].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "output";
			event.message = "Wrote " + match[1].str() + " report to " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle coverage failure
		if (std::regex_search(line, match, RE_COVERAGE_FAILURE)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "threshold";
			event.message = "Coverage failure: total of " + match[1].str() + " is below --fail-under=" + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle no data cases
		if (std::regex_search(line, match, RE_NO_DATA)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "no_data";
			event.message = "No data to report";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_NO_DATA_COLLECTED)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "coverage";
			event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "no_data";
			event.message = "No data was collected";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "coverage_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}
	}
}

void CoverageParser::ParsePytestCovText(const std::string &content, std::vector<duckdb::ValidationEvent> &events) {
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::smatch match;
	bool in_test_execution = false;
	bool in_coverage_section = false;
	bool in_coverage_table = false;
	bool in_branch_table = false;

	while (std::getline(stream, line)) {
		current_line_num++;
		// Handle test session start
		if (std::regex_search(line, match, RE_TEST_SESSION_START)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "test_session";
			event.message = "Test session started";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle platform and pytest info
		if (std::regex_search(line, match, RE_PLATFORM_INFO)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "environment";
			event.message =
			    "Platform: " + match[1].str() + ", Python: " + match[2].str() + ", pytest: " + match[3].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle pytest-cov plugin detection
		if (std::regex_search(line, match, RE_PYTEST_COV_PLUGIN)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "plugin";
			event.message = "pytest-cov plugin version: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle collected items
		if (std::regex_search(line, match, RE_COLLECTED_ITEMS)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "test_collection";
			event.message = "Collected " + match[1].str() + " test items";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			in_test_execution = true;
			continue;
		}

		// Handle individual test results
		if (in_test_execution && std::regex_search(line, match, RE_TEST_RESULT)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::TEST_RESULT;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;

			std::string status = match[3].str();
			if (status == "PASSED") {
				event.status = duckdb::ValidationEventStatus::PASS;
				event.severity = "info";
			} else if (status == "FAILED") {
				event.status = duckdb::ValidationEventStatus::FAIL;
				event.severity = "error";
			} else if (status == "SKIPPED") {
				event.status = duckdb::ValidationEventStatus::SKIP;
				event.severity = "warning";
			} else if (status == "ERROR") {
				event.status = duckdb::ValidationEventStatus::ERROR;
				event.severity = "error";
			}

			event.category = "test_execution";
			event.message = "Test " + match[2].str() + " " + status;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle test execution summary
		if (std::regex_search(line, match, RE_TEST_SUMMARY_LINE)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			std::string failed = match[1].str();
			std::string passed = match[2].str();
			std::string skipped = match[3].matched ? match[3].str() : "0";
			std::string duration = match[4].str();

			if (failed != "0") {
				event.status = duckdb::ValidationEventStatus::FAIL;
				event.severity = "error";
			} else {
				event.status = duckdb::ValidationEventStatus::PASS;
				event.severity = "info";
			}

			event.category = "test_summary";
			event.message = "Tests completed: " + failed + " failed, " + passed + " passed, " + skipped +
			                " skipped in " + duration + "s";
			event.execution_time = std::stod(duration);
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle coverage section start
		if (std::regex_search(line, match, RE_COVERAGE_SECTION)) {
			in_coverage_section = true;

			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "coverage_section";
			event.message = "Coverage analysis started - Platform: " + match[1].str() + ", Python: " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle coverage table headers
		if (in_coverage_section && std::regex_search(line, match, RE_COVERAGE_HEADER)) {
			in_coverage_table = true;
			in_branch_table = false;
			continue;
		}

		if (in_coverage_section && std::regex_search(line, match, RE_COVERAGE_BRANCH_HEADER)) {
			in_coverage_table = true;
			in_branch_table = true;
			continue;
		}

		// Handle coverage rows
		if (in_coverage_table && !in_branch_table && std::regex_search(line, match, RE_COVERAGE_ROW)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::PERFORMANCE_METRIC;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;

			std::string statements = match[2].str();
			std::string missed = match[3].str();
			std::string coverage_pct = match[4].str();

			// Remove % sign and convert to number for severity calculation
			std::string pct_str = coverage_pct;
			pct_str.erase(pct_str.find('%'));
			double coverage_value = std::stod(pct_str);

			if (coverage_value >= 90.0) {
				event.status = duckdb::ValidationEventStatus::PASS;
				event.severity = "info";
			} else if (coverage_value >= 75.0) {
				event.status = duckdb::ValidationEventStatus::WARNING;
				event.severity = "warning";
			} else {
				event.status = duckdb::ValidationEventStatus::FAIL;
				event.severity = "error";
			}

			event.category = "file_coverage";
			event.message = "Coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed)";

			if (match[5].matched && !match[5].str().empty()) {
				event.message += " - Missing lines: " + match[5].str();
			}

			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle total coverage
		if (in_coverage_section && std::regex_search(line, match, RE_TOTAL_COVERAGE)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			std::string statements = match[1].str();
			std::string missed = match[2].str();
			std::string coverage_pct = match[3].str();

			// Remove % sign and convert to number for severity calculation
			std::string pct_str = coverage_pct;
			pct_str.erase(pct_str.find('%'));
			double coverage_value = std::stod(pct_str);

			if (coverage_value >= 90.0) {
				event.status = duckdb::ValidationEventStatus::PASS;
				event.severity = "info";
			} else if (coverage_value >= 75.0) {
				event.status = duckdb::ValidationEventStatus::WARNING;
				event.severity = "warning";
			} else {
				event.status = duckdb::ValidationEventStatus::FAIL;
				event.severity = "error";
			}

			event.category = "total_coverage";
			event.message =
			    "Total coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed)";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle coverage threshold failures
		if (std::regex_search(line, match, RE_COVERAGE_THRESHOLD_FAIL)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "coverage_threshold";
			event.message = "Coverage threshold failed: Expected >= " + match[1].str() + "%, got " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_REQUIRED_COVERAGE_FAIL)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "coverage_threshold";
			event.message = "Required coverage not met: Expected " + match[1].str() + "%, got " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle coverage report generation
		if (std::regex_search(line, match, RE_COVERAGE_XML_WRITTEN)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "report_generation";
			event.message = "Coverage XML report written to: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_COVERAGE_HTML_WRITTEN)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "report_generation";
			event.message = "Coverage HTML report written to: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle configuration warnings/errors
		if (std::regex_search(line, match, RE_COVERAGE_DATA_NOT_FOUND)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "configuration";
			event.message = "Coverage data not found for source: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_MODULE_NEVER_IMPORTED)) {
			duckdb::ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = duckdb::ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "configuration";
			event.message = "Module never imported: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}
	}
}

} // namespace duck_hunt
