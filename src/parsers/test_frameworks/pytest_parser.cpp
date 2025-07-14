#include "pytest_parser.hpp"
#include "../../core/parser_registry.hpp"
#include <sstream>

namespace duckdb {

bool PytestParser::canParse(const std::string& content) const {
    // Check for pytest text patterns (file.py::test_name with PASSED/FAILED/SKIPPED)
    return content.find("::") != std::string::npos && 
           (content.find("PASSED") != std::string::npos || 
            content.find("FAILED") != std::string::npos || 
            content.find("SKIPPED") != std::string::npos);
}

std::vector<ValidationEvent> PytestParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Look for pytest text output patterns: "file.py::test_name STATUS"
        if (line.find("::") != std::string::npos) {
            parseTestLine(line, event_id, events);
        }
    }
    
    return events;
}

void PytestParser::parseTestLine(const std::string& line, int64_t& event_id, 
                                std::vector<ValidationEvent>& events) const {
    // Parse pytest test result line: "test_file.py::test_name STATUS"
    std::regex test_pattern(R"(([^:]+)::([^\s]+)\s+(PASSED|FAILED|SKIPPED|ERROR)(?:\s+\[(.+)\])?)");
    std::smatch match;
    
    if (std::regex_search(line, match, test_pattern)) {
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "pytest";
        event.event_type = ValidationEventType::TEST_RESULT;
        event.file_path = match[1].str();
        event.test_name = match[2].str();
        
        std::string status_str = match[3].str();
        if (status_str == "PASSED") {
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
        } else if (status_str == "FAILED" || status_str == "ERROR") {
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
        } else if (status_str == "SKIPPED") {
            event.status = ValidationEventStatus::SKIP;
            event.severity = "warning";
        }
        
        event.category = "test";
        event.message = line;
        event.line_number = -1;
        event.column_number = -1;
        event.execution_time = 0.0;
        event.raw_output = line;
        event.structured_data = "pytest_text";
        
        // Additional info from match group 4 if present
        if (match[4].matched) {
            event.suggestion = "Additional info: " + match[4].str();
        }
        
        events.push_back(event);
    }
}

// Auto-register this parser
REGISTER_PARSER(PytestParser);

} // namespace duckdb