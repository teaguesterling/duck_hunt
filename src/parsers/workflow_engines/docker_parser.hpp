#pragma once

#include "workflow_engine_interface.hpp"
#include <regex>

namespace duckdb {

class DockerParser : public WorkflowEngineParser {
public:
    bool canParse(const std::string& content) const override;
    std::string getFormatName() const override { return "docker_build"; }
    std::vector<WorkflowEvent> parseWorkflowLog(const std::string& content) const override;
    int getPriority() const override { return 120; } // High priority for Docker builds
    std::string getName() const override { return "DockerParser"; }

private:
    struct DockerLayer {
        std::string layer_id;
        std::string command;
        std::string status;
        std::string started_at;
        std::string completed_at;
        std::vector<std::string> output_lines;
    };
    
    struct DockerStage {
        std::string stage_name;
        std::string stage_id;
        std::string base_image;
        std::string status;
        std::vector<DockerLayer> layers;
    };
    
    struct DockerBuild {
        std::string build_name;
        std::string build_id;
        std::string dockerfile_path;
        std::string context_path;
        std::vector<DockerStage> stages;
    };
    
    // Parse Docker specific patterns
    bool isDockerCommand(const std::string& line) const;
    bool isDockerStep(const std::string& line) const;
    bool isMultiStageFrom(const std::string& line) const;
    bool isLayerCache(const std::string& line) const;
    bool isBuildComplete(const std::string& line) const;
    
    // Extract workflow metadata
    std::string extractBuildName(const std::string& content) const;
    std::string extractBuildId(const std::string& content) const;
    std::string extractStageName(const std::string& line) const;
    std::string extractBaseImage(const std::string& line) const;
    std::string extractCommand(const std::string& line) const;
    std::string extractLayerId(const std::string& line) const;
    std::string extractStatus(const std::string& line) const;
    
    // Parse hierarchical structure
    std::vector<DockerBuild> parseBuilds(const std::string& content) const;
    DockerStage parseStage(const std::vector<std::string>& stage_lines, const std::string& stage_name) const;
    DockerLayer parseLayer(const std::vector<std::string>& layer_lines, const std::string& command) const;
    
    // Convert to ValidationEvents
    std::vector<WorkflowEvent> convertToEvents(const std::vector<DockerBuild>& builds) const;
};

} // namespace duckdb