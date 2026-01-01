#include "node_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

bool NodeParser::canParse(const std::string &content) const {
	// Check for Node.js/npm/yarn patterns
	return (content.find("npm ERR!") != std::string::npos || content.find("yarn install") != std::string::npos) ||
	       (content.find("FAIL ") != std::string::npos && content.find(".test.js") != std::string::npos) ||
	       (content.find("ERROR in") != std::string::npos && content.find("webpack") != std::string::npos) ||
	       (content.find("● Test suite failed to run") != std::string::npos) ||
	       (content.find("Could not resolve dependency:") != std::string::npos) ||
	       (content.find("Module not found:") != std::string::npos);
}

std::vector<ValidationEvent> NodeParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;
	std::string current_test_file;

	// Pre-compiled regex patterns
	static const std::regex code_pattern(R"(npm ERR! code ([A-Z_]+))");
	static const std::regex test_file_pattern(R"(FAIL\s+([^\s]+\.test\.js))");
	static const std::regex test_name_pattern(R"(●\s+([^›]+)\s+›\s+(.+))");
	static const std::regex eslint_pattern(R"(\s*(\d+):(\d+)\s+(error|warning)\s+(.+?)\s+([^\s]+)$)");
	static const std::regex webpack_error_pattern(R"(ERROR in (.+?)(?:\s+(\d+):(\d+))?$)");
	static const std::regex webpack_warn_pattern(R"(WARNING in (.+))");
	static const std::regex runtime_pattern(R"(at Object\.<anonymous> \(([^:]+):(\d+):(\d+)\))");
	static const std::regex eslint_line_pattern(R"(\s*\d+:\d+\s+(error|warning)\s+.+)");

	while (std::getline(stream, line)) {
		current_line_num++;

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
			event.ref_line = -1;
			event.ref_column = -1;
			event.log_content = content;
			event.structured_data = "node_build";

			// Extract error codes
			std::smatch code_match;
			if (line.find("npm ERR! code") != std::string::npos) {
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
			event.ref_line = -1;
			event.ref_column = -1;
			event.log_content = content;
			event.structured_data = "node_build";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

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
			event.log_content = content;
			event.structured_data = "node_build";

			// Extract test file
			std::smatch test_match;
			if (std::regex_search(line, test_match, test_file_pattern)) {
				event.ref_file = test_match[1].str();
				current_test_file = event.ref_file;
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
			event.ref_file = current_test_file;
			event.log_content = content;
			event.structured_data = "node_build";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

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
			event.ref_file = current_test_file;
			event.log_content = content;
			event.structured_data = "node_build";

			// Extract test name
			std::smatch name_match;
			if (std::regex_search(line, name_match, test_name_pattern)) {
				event.test_name = name_match[1].str() + " › " + name_match[2].str();
			}

			events.push_back(event);
		}
		// Parse ESLint errors and warnings
		else if (std::regex_match(line, eslint_line_pattern)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "eslint";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.log_content = content;
			event.structured_data = "node_build";

			// Parse ESLint format: "  15:5   error    'console' is not defined    no-undef"
			std::smatch eslint_match;
			if (std::regex_search(line, eslint_match, eslint_pattern)) {
				event.ref_line = std::stoi(eslint_match[1].str());
				event.ref_column = std::stoi(eslint_match[2].str());
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
			event.log_content = content;
			event.structured_data = "node_build";

			// Extract file and line info: "ERROR in ./src/components/Header.js"
			std::smatch webpack_match;
			if (std::regex_search(line, webpack_match, webpack_error_pattern)) {
				event.ref_file = webpack_match[1].str();
				if (webpack_match[2].matched) {
					event.ref_line = std::stoi(webpack_match[2].str());
					event.ref_column = std::stoi(webpack_match[3].str());
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
			event.log_content = content;
			event.structured_data = "node_build";

			// Extract file info
			std::smatch webpack_match;
			if (std::regex_search(line, webpack_match, webpack_warn_pattern)) {
				event.ref_file = webpack_match[1].str();
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
			event.log_content = content;
			event.structured_data = "node_build";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

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
			event.log_content = content;
			event.structured_data = "node_build";

			// Extract file and line info: "at Object.<anonymous> (src/index.test.js:5:23)"
			std::smatch runtime_match;
			if (std::regex_search(line, runtime_match, runtime_pattern)) {
				event.ref_file = runtime_match[1].str();
				event.ref_line = std::stoi(runtime_match[2].str());
				event.ref_column = std::stoi(runtime_match[3].str());
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
			event.ref_line = -1;
			event.ref_column = -1;
			event.log_content = content;
			event.structured_data = "node_build";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
