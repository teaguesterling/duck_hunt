#pragma once

#include "workflow_engine_interface.hpp"
#include <regex>

namespace duckdb {

class SpackParser : public WorkflowEngineParser {
public:
	bool canParse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "spack";
	}
	std::vector<WorkflowEvent> parseWorkflowLog(const std::string &content) const override;
	int getPriority() const override {
		return 140;
	} // High priority for Spack
	std::string getName() const override {
		return "SpackParser";
	}

private:
	struct SpackPhase {
		std::string package_name;
		std::string phase_name;
		std::string phase_id;
		std::string status;
		std::string started_at;
		std::vector<std::string> output_lines;
	};

	struct SpackBuild {
		std::string package_name;
		std::string build_id;
		std::string status;
		std::vector<SpackPhase> phases;
	};

	// Check if line is a Spack marker line
	bool isSpackMarker(const std::string &line) const;
	bool isPhaseMarker(const std::string &line) const;
	bool isTimestampedLine(const std::string &line) const;

	// Extract information from lines
	std::string extractPackageName(const std::string &line) const;
	std::string extractPhaseName(const std::string &line) const;
	std::string extractTimestamp(const std::string &line) const;
	std::string extractCommand(const std::string &line) const;

	// Parse structure
	std::vector<SpackBuild> parseBuilds(const std::string &content) const;
	SpackPhase parsePhase(const std::vector<std::string> &phase_lines, const std::string &package_name,
	                      const std::string &phase_name) const;

	// Convert to events
	std::vector<WorkflowEvent> convertToEvents(const std::vector<SpackBuild> &builds) const;
};

} // namespace duckdb
