#include "pytest_cov_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

// Pre-compiled regex patterns for pytest-cov text parsing (compiled once, reused)
namespace {
static const std::regex RE_TEST_SESSION_START(R"(={3,} test session starts ={3,})");
static const std::regex RE_PLATFORM_INFO(R"(platform (.+) -- Python (.+), pytest-(.+), pluggy-(.+))");
static const std::regex RE_PYTEST_COV_PLUGIN(R"(plugins: cov-(.+))");
static const std::regex RE_ROOTDIR_INFO(R"(rootdir: (.+))");
static const std::regex RE_COLLECTED_ITEMS(R"(collected (\d+) items?)");
static const std::regex RE_TEST_RESULT(R"((.+\.py)::(.+)\s+(PASSED|FAILED|SKIPPED|ERROR)\s+\[([^\]]+)\])");
static const std::regex RE_TEST_FAILURE_SECTION(R"(={3,} FAILURES ={3,})");
static const std::regex RE_TEST_SHORT_SUMMARY(R"(={3,} short test summary info ={3,})");
static const std::regex RE_TEST_SUMMARY_LINE(R"(={3,} (\d+) failed, (\d+) passed(?:, (\d+) skipped)? in ([\d\.]+)s ={3,})");
static const std::regex RE_COVERAGE_SECTION(R"(----------- coverage: platform (.+), python (.+) -----------)");
static const std::regex RE_COVERAGE_HEADER(R"(Name\s+Stmts\s+Miss\s+Cover(?:\s+Missing)?)");
static const std::regex RE_COVERAGE_BRANCH_HEADER(R"(Name\s+Stmts\s+Miss\s+Branch\s+BrPart\s+Cover(?:\s+Missing)?)");
static const std::regex RE_COVERAGE_ROW(R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_COVERAGE_BRANCH_ROW(
    R"(^([^\s]+(?:\.[^\s]+)*)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_TOTAL_COVERAGE(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_TOTAL_BRANCH_COVERAGE(R"(^TOTAL\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+%|\d+\.\d+%)\s*(.*)?)");
static const std::regex RE_COVERAGE_THRESHOLD_FAIL(R"(Coverage threshold check failed\. Expected: >= (\d+)%, got: ([\d\.]+%))");
static const std::regex RE_REQUIRED_COVERAGE_FAIL(R"(Required test coverage of (\d+)% not met\. Total coverage: ([\d\.]+%))");
static const std::regex RE_COVERAGE_XML_WRITTEN(R"(Coverage XML written to (.+))");
static const std::regex RE_COVERAGE_HTML_WRITTEN(R"(Coverage HTML written to dir (.+))");
static const std::regex RE_COVERAGE_DATA_NOT_FOUND(R"(pytest-cov: Coverage data was not found for source '(.+)')");
static const std::regex RE_MODULE_NEVER_IMPORTED(R"(pytest-cov: Module '(.+)' was never imported\.)");
static const std::regex RE_ASSERTION_ERROR(R"(E\s+(AssertionError: .+))");
} // anonymous namespace

bool PytestCovTextParser::canParse(const std::string &content) const {
	// Only match when actual coverage DATA is present, not just pytest-cov plugin installed
	// Require coverage section markers or coverage table to be present

	// Coverage section header: "----------- coverage: platform"
	if (content.find("coverage:") != std::string::npos && content.find("platform") != std::string::npos &&
	    content.find("-------") != std::string::npos) {
		return true;
	}

	// Coverage table with TOTAL row and column headers
	if (content.find("TOTAL") != std::string::npos && content.find("Stmts") != std::string::npos &&
	    content.find("Miss") != std::string::npos) {
		return true;
	}

	// Coverage threshold failure messages (actual coverage output)
	if (content.find("Coverage threshold check failed") != std::string::npos ||
	    content.find("Required test coverage of") != std::string::npos) {
		return true;
	}

	return false;
}

// Forward declaration for implementation
static void parsePytestCovTextImpl(const std::string &content, std::vector<ValidationEvent> &events);

std::vector<ValidationEvent> PytestCovTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	parsePytestCovTextImpl(content, events);
	return events;
}

static void parsePytestCovTextImpl(const std::string &content, std::vector<ValidationEvent> &events) {
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;

	// Reserve space for events (estimate based on content size)
	events.reserve(content.size() / 100);

	std::smatch match;
	bool in_test_execution = false;
	bool in_RE_COVERAGE_SECTION = false;
	bool in_failure_section = false;
	bool in_coverage_table = false;
	bool in_branch_table = false;
	std::string current_test_file = "";

	while (std::getline(stream, line)) {
		// Handle test session start
		if (std::regex_search(line, match, RE_TEST_SESSION_START)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "test_session";
			event.message = "Test session started";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle platform and pytest info
		if (std::regex_search(line, match, RE_PLATFORM_INFO)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "environment";
			event.message =
			    "Platform: " + match[1].str() + ", Python: " + match[2].str() + ", pytest: " + match[3].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle pytest-cov plugin detection
		if (std::regex_search(line, match, RE_PYTEST_COV_PLUGIN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "plugin";
			event.message = "pytest-cov plugin version: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle collected items
		if (std::regex_search(line, match, RE_COLLECTED_ITEMS)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
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
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;

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
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle test failure section
		if (std::regex_search(line, match, RE_TEST_FAILURE_SECTION)) {
			in_failure_section = true;
			in_test_execution = false;
			continue;
		}

		// Handle test summary section
		if (std::regex_search(line, match, RE_TEST_SHORT_SUMMARY)) {
			in_failure_section = false;
			continue;
		}

		// Handle test execution summary
		if (std::regex_search(line, match, RE_TEST_SUMMARY_LINE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

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
			event.message = "Tests completed: " + failed + " failed, " + passed + " passed, " + skipped +
			                " skipped in " + duration + "s";
			event.execution_time = SafeParsing::SafeStod(duration);
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle coverage section start
		if (std::regex_search(line, match, RE_COVERAGE_SECTION)) {
			in_RE_COVERAGE_SECTION = true;

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "coverage_section";
			event.message = "Coverage analysis started - Platform: " + match[1].str() + ", Python: " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle coverage table headers
		if (in_RE_COVERAGE_SECTION && std::regex_search(line, match, RE_COVERAGE_HEADER)) {
			in_coverage_table = true;
			in_branch_table = false;
			continue;
		}

		if (in_RE_COVERAGE_SECTION && std::regex_search(line, match, RE_COVERAGE_BRANCH_HEADER)) {
			in_coverage_table = true;
			in_branch_table = true;
			continue;
		}

		// Handle coverage rows
		if (in_coverage_table && !in_branch_table && std::regex_search(line, match, RE_COVERAGE_ROW)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::PERFORMANCE_METRIC;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;

			std::string statements = match[2].str();
			std::string missed = match[3].str();
			std::string coverage_pct = match[4].str();

			// Remove % sign and convert to number for severity calculation
			std::string pct_str = coverage_pct;
			pct_str.erase(pct_str.find('%'));
			double coverage_value = SafeParsing::SafeStod(pct_str);

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
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle branch coverage rows
		if (in_coverage_table && in_branch_table && std::regex_search(line, match, RE_COVERAGE_BRANCH_ROW)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::PERFORMANCE_METRIC;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;

			std::string statements = match[2].str();
			std::string missed = match[3].str();
			std::string branches = match[4].str();
			std::string partial = match[5].str();
			std::string coverage_pct = match[6].str();

			// Remove % sign and convert to number for severity calculation
			std::string pct_str = coverage_pct;
			pct_str.erase(pct_str.find('%'));
			double coverage_value = SafeParsing::SafeStod(pct_str);

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
			event.message = "Branch coverage: " + coverage_pct + " (" + statements + " statements, " + missed +
			                " missed, " + branches + " branches, " + partial + " partial)";

			if (match[7].matched && !match[7].str().empty()) {
				event.message += " - Missing: " + match[7].str();
			}

			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle total coverage
		if (in_RE_COVERAGE_SECTION && std::regex_search(line, match, RE_TOTAL_COVERAGE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			std::string statements = match[1].str();
			std::string missed = match[2].str();
			std::string coverage_pct = match[3].str();

			// Remove % sign and convert to number for severity calculation
			std::string pct_str = coverage_pct;
			pct_str.erase(pct_str.find('%'));
			double coverage_value = SafeParsing::SafeStod(pct_str);

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
			event.message =
			    "Total coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed)";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle total branch coverage
		if (in_RE_COVERAGE_SECTION && std::regex_search(line, match, RE_TOTAL_BRANCH_COVERAGE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			std::string statements = match[1].str();
			std::string missed = match[2].str();
			std::string branches = match[3].str();
			std::string partial = match[4].str();
			std::string coverage_pct = match[5].str();

			// Remove % sign and convert to number for severity calculation
			std::string pct_str = coverage_pct;
			pct_str.erase(pct_str.find('%'));
			double coverage_value = SafeParsing::SafeStod(pct_str);

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
			event.message = "Total branch coverage: " + coverage_pct + " (" + statements + " statements, " + missed +
			                " missed, " + branches + " branches, " + partial + " partial)";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle coverage threshold failures
		if (std::regex_search(line, match, RE_COVERAGE_THRESHOLD_FAIL)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "coverage_threshold";
			event.message = "Coverage threshold failed: Expected >= " + match[1].str() + "%, got " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_REQUIRED_COVERAGE_FAIL)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "coverage_threshold";
			event.message = "Required coverage not met: Expected " + match[1].str() + "%, got " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle coverage report generation
		if (std::regex_search(line, match, RE_COVERAGE_XML_WRITTEN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "report_generation";
			event.message = "Coverage XML report written to: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_COVERAGE_HTML_WRITTEN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "report_generation";
			event.message = "Coverage HTML report written to: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle assertion errors in failure section
		if (in_failure_section && std::regex_search(line, match, RE_ASSERTION_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = current_test_file;
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.category = "assertion_error";
			event.message = match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		// Handle configuration warnings/errors
		if (std::regex_search(line, match, RE_COVERAGE_DATA_NOT_FOUND)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "configuration";
			event.message = "Coverage data not found for source: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_MODULE_NEVER_IMPORTED)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "pytest-cov";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "configuration";
			event.message = "Module never imported: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "pytest_cov_text";

			events.push_back(event);
			continue;
		}
	}
}

} // namespace duckdb
