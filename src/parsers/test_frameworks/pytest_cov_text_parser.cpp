#include "pytest_cov_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool PytestCovTextParser::CanParse(const std::string& content) const {
    // Check for pytest-cov specific patterns
    return (content.find("coverage:") != std::string::npos &&
            content.find("platform") != std::string::npos) ||
           (content.find("test session starts") != std::string::npos &&
            content.find("cov") != std::string::npos) ||
           content.find("pytest-cov") != std::string::npos ||
           content.find("TOTAL") != std::string::npos && content.find("Stmts") != std::string::npos;
}

void PytestCovTextParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParsePytestCovText(content, events);
}

void PytestCovTextParser::ParsePytestCovText(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
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
    std::regex coverage_xml_written(R"(Coverage XML written to (.+))");
    std::regex coverage_html_written(R"(Coverage HTML written to dir (.+))");
    std::regex coverage_data_not_found(R"(pytest-cov: Coverage data was not found for source '(.+)')");
    std::regex module_never_imported(R"(pytest-cov: Module '(.+)' was never imported\.)");
    std::regex assertion_error(R"(E\s+(AssertionError: .+))");

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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.file_path = match[1].str();
            event.line_number = -1;
            event.column_number = -1;

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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;

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

            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::PERFORMANCE_METRIC;
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
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";

            events.push_back(event);
            continue;
        }

        // Handle branch coverage rows
        if (in_coverage_table && in_branch_table && std::regex_search(line, match, coverage_branch_row)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::PERFORMANCE_METRIC;
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
                event.status = duckdb::ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (coverage_value >= 75.0) {
                event.status = duckdb::ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = duckdb::ValidationEventStatus::FAIL;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
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
            event.message = "Total coverage: " + coverage_pct + " (" + statements + " statements, " + missed + " missed)";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "pytest_cov_text";

            events.push_back(event);
            continue;
        }

        // Handle total branch coverage
        if (in_coverage_section && std::regex_search(line, match, total_branch_coverage)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
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
                event.status = duckdb::ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (coverage_value >= 75.0) {
                event.status = duckdb::ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = duckdb::ValidationEventStatus::FAIL;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::FAIL;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::FAIL;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::INFO;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.file_path = current_test_file;
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::FAIL;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::WARNING;
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest-cov";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = duckdb::ValidationEventStatus::WARNING;
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

} // namespace duck_hunt
