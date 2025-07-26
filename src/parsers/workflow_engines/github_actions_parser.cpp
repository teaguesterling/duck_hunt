#include "github_actions_parser.hpp"
#include "read_workflow_logs_function.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

bool GitHubActionsParser::canParse(const std::string& content) const {
    // GitHub Actions specific patterns
    return content.find("##[group]") != std::string::npos ||
           content.find("##[endgroup]") != std::string::npos ||
           content.find("actions/checkout@") != std::string::npos ||
           content.find("actions/setup-") != std::string::npos ||
           isGitHubActionsTimestamp(content);
}

std::vector<WorkflowEvent> GitHubActionsParser::parseWorkflowLogs(const std::string& content) const {
    std::vector<WorkflowEvent> events;
    
    // Extract workflow metadata
    std::string workflow_name = extractWorkflowName(content);
    std::string run_id = extractRunId(content);
    
    // Parse jobs and steps
    std::vector<GitHubJob> jobs = parseJobs(content);
    
    // Convert to events
    return convertToEvents(jobs, workflow_name, run_id);
}

bool GitHubActionsParser::isGitHubActionsTimestamp(const std::string& line) const {
    // GitHub Actions timestamp format: 2023-10-15T14:30:15.1234567Z
    std::regex timestamp_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z)");
    return std::regex_search(line, timestamp_pattern);
}

bool GitHubActionsParser::isGroupStart(const std::string& line) const {
    return line.find("##[group]") != std::string::npos;
}

bool GitHubActionsParser::isGroupEnd(const std::string& line) const {
    return line.find("##[endgroup]") != std::string::npos;
}

bool GitHubActionsParser::isActionStep(const std::string& line) const {
    return line.find("actions/") != std::string::npos ||
           line.find("Run ") != std::string::npos ||
           line.find("shell: ") != std::string::npos;
}

std::string GitHubActionsParser::extractWorkflowName(const std::string& content) const {
    // Try to extract from job/step context
    std::istringstream iss(content);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.find("##[group]Run ") != std::string::npos) {
            size_t start = line.find("Run ") + 4;
            size_t end = line.find_first_of("\r\n", start);
            if (end == std::string::npos) end = line.length();
            return line.substr(start, end - start);
        }
    }
    
    // Default workflow name
    return "GitHub Actions Workflow";
}

std::string GitHubActionsParser::extractRunId(const std::string& content) const {
    // GitHub Actions doesn't usually include run ID in logs
    // Generate a simple hash-based ID
    std::hash<std::string> hasher;
    return std::to_string(hasher(content.substr(0, 100)));
}

std::string GitHubActionsParser::extractJobName(const std::string& line) const {
    // Extract job name from context - for now use a default
    return "build"; // Most common GitHub Actions job name
}

std::string GitHubActionsParser::extractStepName(const std::string& line) const {
    if (isGroupStart(line)) {
        size_t start = line.find("##[group]") + 9;
        size_t end = line.find_first_of("\r\n", start);
        if (end == std::string::npos) end = line.length();
        return line.substr(start, end - start);
    }
    
    if (line.find("actions/") != std::string::npos) {
        size_t start = line.find("actions/");
        size_t end = line.find("@", start);
        if (end != std::string::npos) {
            return line.substr(start, end - start);
        }
    }
    
    return "";
}

std::string GitHubActionsParser::extractTimestamp(const std::string& line) const {
    std::regex timestamp_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z)");
    std::smatch match;
    if (std::regex_search(line, match, timestamp_pattern)) {
        return match.str();
    }
    return "";
}

std::string GitHubActionsParser::extractStatus(const std::string& line) const {
    std::string lower_line = StringUtil::Lower(line);
    
    if (lower_line.find("error") != std::string::npos ||
        lower_line.find("failed") != std::string::npos ||
        lower_line.find("fail") != std::string::npos) {
        return "failure";
    }
    
    if (lower_line.find("warning") != std::string::npos ||
        lower_line.find("warn") != std::string::npos) {
        return "warning";
    }
    
    if (lower_line.find("success") != std::string::npos ||
        lower_line.find("completed") != std::string::npos) {
        return "success";
    }
    
    return "running";
}

std::vector<GitHubActionsParser::GitHubJob> GitHubActionsParser::parseJobs(const std::string& content) const {
    std::vector<GitHubJob> jobs;
    GitHubJob current_job;
    current_job.job_name = "build";
    current_job.job_id = "job_1";
    current_job.status = "running";
    
    std::istringstream iss(content);
    std::string line;
    std::vector<std::string> current_step_lines;
    bool in_step = false;
    
    while (std::getline(iss, line)) {
        if (isGroupStart(line)) {
            // Starting a new step
            if (in_step && !current_step_lines.empty()) {
                // Finish previous step
                current_job.steps.push_back(parseStep(current_step_lines));
                current_step_lines.clear();
            }
            in_step = true;
            current_step_lines.push_back(line);
        } else if (isGroupEnd(line)) {
            // Ending current step
            if (in_step) {
                current_step_lines.push_back(line);
                current_job.steps.push_back(parseStep(current_step_lines));
                current_step_lines.clear();
                in_step = false;
            }
        } else if (in_step) {
            current_step_lines.push_back(line);
        } else {
            // Lines outside of steps - could be job-level info
            current_step_lines.push_back(line);
        }
    }
    
    // Handle remaining step if any
    if (in_step && !current_step_lines.empty()) {
        current_job.steps.push_back(parseStep(current_step_lines));
    }
    
    // If no steps were found, create a default step
    if (current_job.steps.empty()) {
        GitHubStep default_step;
        default_step.step_name = "Workflow Execution";
        default_step.step_id = "step_1";
        default_step.status = "success";
        default_step.output_lines = {content.substr(0, std::min(size_t(500), content.length()))};
        current_job.steps.push_back(default_step);
    }
    
    jobs.push_back(current_job);
    return jobs;
}

GitHubActionsParser::GitHubStep GitHubActionsParser::parseStep(const std::vector<std::string>& step_lines) const {
    GitHubStep step;
    
    if (step_lines.empty()) {
        step.step_name = "Unknown Step";
        step.step_id = "unknown";
        step.status = "unknown";
        return step;
    }
    
    // Extract step name from first line (group start)
    step.step_name = extractStepName(step_lines[0]);
    if (step.step_name.empty()) {
        step.step_name = "Unnamed Step";
    }
    
    // Generate step ID
    std::hash<std::string> hasher;
    step.step_id = "step_" + std::to_string(hasher(step.step_name) % 10000);
    
    // Extract timestamps and status
    for (const auto& line : step_lines) {
        std::string timestamp = extractTimestamp(line);
        if (!timestamp.empty()) {
            if (step.started_at.empty()) {
                step.started_at = timestamp;
            }
            step.completed_at = timestamp; // Last timestamp is completion
        }
    }
    
    // Determine overall step status
    step.status = "success"; // Default
    for (const auto& line : step_lines) {
        std::string line_status = extractStatus(line);
        if (line_status == "failure") {
            step.status = "failure";
            break;
        } else if (line_status == "warning" && step.status != "failure") {
            step.status = "warning";
        }
    }
    
    step.output_lines = step_lines;
    return step;
}

std::vector<WorkflowEvent> GitHubActionsParser::convertToEvents(const std::vector<GitHubJob>& jobs,
                                                               const std::string& workflow_name,
                                                               const std::string& run_id) const {
    std::vector<WorkflowEvent> events;
    
    for (const auto& job : jobs) {
        for (const auto& step : job.steps) {
            for (const auto& output_line : step.output_lines) {
                WorkflowEvent event;
                
                // Create base validation event
                ValidationEvent base_event = createBaseEvent(output_line, workflow_name, job.job_name, step.step_name);
                
                // Set the base_event
                event.base_event = base_event;
                
                // Override specific fields in base_event
                event.base_event.status = ValidationEventStatus::INFO;
                event.base_event.severity = determineSeverity(step.status, output_line);
                event.base_event.workflow_name = workflow_name;
                event.base_event.job_name = job.job_name;
                event.base_event.step_name = step.step_name;
                event.base_event.workflow_run_id = run_id;
                event.base_event.job_id = job.job_id;
                event.base_event.step_id = step.step_id;
                event.base_event.workflow_status = "running";
                event.base_event.job_status = job.status;
                event.base_event.step_status = step.status;
                event.base_event.started_at = step.started_at;
                event.base_event.completed_at = step.completed_at;
                event.base_event.duration = 0.0; // Calculate if timestamps available
                
                // Set workflow-specific fields
                event.workflow_type = "github_actions";
                event.hierarchy_level = 3; // Step level
                event.parent_id = job.job_id;
                
                events.push_back(event);
            }
        }
    }
    
    return events;
}

// Register the parser
REGISTER_WORKFLOW_PARSER(GitHubActionsParser);

} // namespace duckdb