#pragma once

#include "workflow_engine_interface.hpp"
#include <regex>

namespace duckdb {

class GitLabCIParser : public WorkflowEngineParser {
public:
    bool canParse(const std::string& content) const override;
    WorkflowLogFormat getFormat() const override { return WorkflowLogFormat::GITLAB_CI; }
    std::vector<WorkflowEvent> parseWorkflowLog(const std::string& content) const override;
    int getPriority() const override { return 140; } // High priority for GitLab CI
    std::string getName() const override { return "GitLabCIParser"; }

private:
    struct GitLabStage {
        std::string stage_name;
        std::string stage_id;
        std::string status;
        std::string started_at;
        std::string completed_at;
        std::vector<std::string> output_lines;
    };
    
    struct GitLabJob {
        std::string job_name;
        std::string job_id;
        std::string status;
        std::string executor;
        std::vector<GitLabStage> stages;
    };
    
    // Parse GitLab CI specific patterns
    bool isGitLabRunner(const std::string& line) const;
    bool isDockerExecutor(const std::string& line) const;
    bool isJobStart(const std::string& line) const;
    bool isJobEnd(const std::string& line) const;
    bool isStageMarker(const std::string& line) const;
    
    // Extract workflow metadata
    std::string extractWorkflowName(const std::string& content) const;
    std::string extractPipelineId(const std::string& content) const;
    std::string extractJobName(const std::string& line) const;
    std::string extractStageName(const std::string& line) const;
    std::string extractExecutor(const std::string& content) const;
    std::string extractRunnerInfo(const std::string& line) const;
    std::string extractStatus(const std::string& line) const;
    
    // Parse hierarchical structure
    std::vector<GitLabJob> parseJobs(const std::string& content) const;
    GitLabStage parseStage(const std::vector<std::string>& stage_lines, const std::string& stage_name) const;
    
    // Convert to ValidationEvents
    std::vector<WorkflowEvent> convertToEvents(const std::vector<GitLabJob>& jobs,
                                              const std::string& workflow_name,
                                              const std::string& pipeline_id) const;
};

} // namespace duckdb