#include "github_cli_parser.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

bool GitHubCliParser::canParse(const std::string& content) const {
    return isGitHubRunsList(content) || isGitHubRunView(content) || isGitHubWorkflowLog(content);
}

bool GitHubCliParser::isGitHubRunsList(const std::string& content) const {
    // Check for "gh run list" output patterns
    if (content.find("STATUS") != std::string::npos && 
        content.find("CONCLUSION") != std::string::npos && 
        content.find("WORKFLOW") != std::string::npos &&
        content.find("BRANCH") != std::string::npos) {
        return true;
    }
    
    // Also check for run entries with specific patterns
    std::regex run_entry_pattern(R"([✓X]\s+\w+\s+(completed|in_progress|cancelled)\s+.+\s+\w+\s+\d+[mhd])");
    return std::regex_search(content, run_entry_pattern);
}

bool GitHubCliParser::isGitHubRunView(const std::string& content) const {
    // Check for "gh run view" output patterns
    if ((content.find("Run #") != std::string::npos || content.find("Run ID:") != std::string::npos) &&
        (content.find("Status:") != std::string::npos || content.find("Conclusion:") != std::string::npos)) {
        return true;
    }
    
    // Check for job status patterns
    std::regex job_pattern(R"([✓X]\s+\w+\s+(Success|Failure|Cancelled|Skipped))");
    return std::regex_search(content, job_pattern);
}

bool GitHubCliParser::isGitHubWorkflowLog(const std::string& content) const {
    // Check for GitHub Actions workflow log patterns
    if (content.find("##[group]") != std::string::npos ||
        content.find("##[endgroup]") != std::string::npos ||
        content.find("::error::") != std::string::npos ||
        content.find("::warning::") != std::string::npos ||
        content.find("::notice::") != std::string::npos) {
        return true;
    }
    
    // Check for step output patterns
    std::regex step_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z)");
    if (std::regex_search(content, step_pattern)) {
        return content.find("Run ") != std::string::npos || content.find("Setup ") != std::string::npos;
    }
    
    // Check for combined annotation patterns (from main detection logic)
    if ((content.find("::error::") != std::string::npos && content.find("::warning::") != std::string::npos) ||
        (content.find("##[endgroup]") != std::string::npos && content.find("::notice::") != std::string::npos)) {
        return true;
    }
    
    return false;
}

std::vector<ValidationEvent> GitHubCliParser::parse(const std::string& content) const {
    if (isGitHubRunsList(content)) {
        return parseRunsList(content);
    } else if (isGitHubRunView(content)) {
        return parseRunView(content);
    } else if (isGitHubWorkflowLog(content)) {
        return parseWorkflowLog(content);
    }
    
    return {};
}

std::vector<ValidationEvent> GitHubCliParser::parseRunsList(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Skip header line
    if (std::getline(stream, line) && line.find("STATUS") != std::string::npos) {
        current_line_num++;
        // This is the header, continue to data lines
    }

    // Parse run entries: [✓X] status conclusion workflow branch time
    std::regex run_pattern(R"(([✓X])\s+(\w+)\s+(\w+)\s+(.+?)\s+(\w+)\s+(\d+[mhd]|[a-zA-Z]+\s+\d+))");

    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;
        if (std::regex_search(line, match, run_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-cli";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.category = "ci_run";
            
            std::string icon = match[1].str();
            std::string status = match[2].str();
            std::string conclusion = match[3].str();
            std::string workflow = match[4].str();
            std::string branch = match[5].str();
            std::string time = match[6].str();
            
            // Set status based on conclusion and icon
            if (icon == "✓" && conclusion == "success") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (icon == "X" && conclusion == "failure") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else if (conclusion == "cancelled") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            event.message = "Workflow '" + workflow + "' " + conclusion + " on branch '" + branch + "'";
            event.function_name = workflow;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"status\": \"" + status + "\", \"conclusion\": \"" + conclusion +
                                   "\", \"branch\": \"" + branch + "\", \"time\": \"" + time + "\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }
    
    return events;
}

std::vector<ValidationEvent> GitHubCliParser::parseRunView(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;
    std::string run_id, run_status, run_conclusion;

    // Parse run metadata first
    while (std::getline(stream, line)) {
        current_line_num++;
        if (line.find("Run #") != std::string::npos || line.find("Run ID:") != std::string::npos) {
            std::regex run_id_pattern(R"(Run #?(\d+)|Run ID:\s*(\d+))");
            std::smatch match;
            if (std::regex_search(line, match, run_id_pattern)) {
                run_id = match[1].str().empty() ? match[2].str() : match[1].str();
            }
        } else if (line.find("Status:") != std::string::npos) {
            std::regex status_pattern(R"(Status:\s*(\w+))");
            std::smatch match;
            if (std::regex_search(line, match, status_pattern)) {
                run_status = match[1].str();
            }
        } else if (line.find("Conclusion:") != std::string::npos) {
            std::regex conclusion_pattern(R"(Conclusion:\s*(\w+))");
            std::smatch match;
            if (std::regex_search(line, match, conclusion_pattern)) {
                run_conclusion = match[1].str();
            }
        }
        
        // Parse job status lines: [✓X] job_name Success/Failure
        std::regex job_pattern(R"(([✓X])\s+(.+?)\s+(Success|Failure|Cancelled|Skipped))");
        std::smatch job_match;
        if (std::regex_search(line, job_match, job_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-cli";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.category = "ci_job";
            
            std::string icon = job_match[1].str();
            std::string job_name = job_match[2].str();
            std::string job_status = job_match[3].str();
            
            // Set status based on job result
            if (job_status == "Success") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
            } else if (job_status == "Failure") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else if (job_status == "Cancelled") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "warning";
            } else if (job_status == "Skipped") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            event.message = "Job '" + job_name + "' " + job_status;
            event.function_name = job_name;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"run_id\": \"" + run_id + "\", \"job_status\": \"" + job_status + "\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }

    return events;
}

std::vector<ValidationEvent> GitHubCliParser::parseWorkflowLog(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;
    std::string current_step;

    // Parse GitHub Actions workflow log format
    std::regex timestamp_pattern(R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z)\s*(.+))");
    std::regex error_pattern(R"(::error::(.+))");
    std::regex warning_pattern(R"(::warning::(.+))");
    std::regex notice_pattern(R"(::notice::(.+))");
    std::regex group_pattern(R"(##\[group\](.+))");
    std::regex endgroup_pattern(R"(##\[endgroup\])");
    std::regex step_pattern(R"(Run (.+)|Setup (.+))");

    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;
        
        // Parse group markers (step names)
        if (std::regex_search(line, match, group_pattern)) {
            current_step = match[1].str();
            continue;
        }
        
        if (std::regex_search(line, match, endgroup_pattern)) {
            current_step.clear();
            continue;
        }
        
        // Parse step start markers
        if (std::regex_search(line, match, step_pattern)) {
            std::string step_name = match[1].str().empty() ? match[2].str() : match[1].str();
            current_step = step_name;
            continue;
        }
        
        // Parse error messages
        if (std::regex_search(line, match, error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.category = "workflow_error";
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.message = match[1].str();
            event.function_name = current_step;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"step\": \"" + current_step + "\", \"type\": \"error\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }

        // Parse warning messages  
        else if (std::regex_search(line, match, warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "workflow_warning";
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.message = match[1].str();
            event.function_name = current_step;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"step\": \"" + current_step + "\", \"type\": \"warning\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }

        // Parse notice messages
        else if (std::regex_search(line, match, notice_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "github-actions";
            event.event_type = ValidationEventType::SUMMARY;
            event.category = "workflow_notice";
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.message = match[1].str();
            event.function_name = current_step;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"step\": \"" + current_step + "\", \"type\": \"notice\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }

    return events;
}


} // namespace duckdb