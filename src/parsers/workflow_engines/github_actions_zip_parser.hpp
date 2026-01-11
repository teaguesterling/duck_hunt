#pragma once

#include "workflow_engine_interface.hpp"
#include "duckdb/main/client_context.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for GitHub Actions workflow logs from ZIP archives.
 *
 * GitHub Actions logs are downloaded as ZIP files with structure:
 * - {N}_{job_name}.txt - Main job logs (numbered by execution order)
 * - {job_name}/system.txt - Runner metadata (optional, not parsed in v1)
 *
 * This parser:
 * 1. Lists all job log files in the ZIP
 * 2. Extracts job_order and job_name from filenames
 * 3. Delegates parsing to GitHubActionsParser for each job
 * 4. Enriches events with job metadata
 */
class GitHubActionsZipParser : public WorkflowEngineParser {
public:
	bool canParse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "github_actions_zip";
	}
	std::vector<WorkflowEvent> parseWorkflowLog(const std::string &content) const override;
	int getPriority() const override {
		return 160;
	} // Higher than regular GitHub Actions parser
	std::string getName() const override {
		return "GitHubActionsZipParser";
	}

	// ZIP-aware parsing (requires ClientContext for FileSystem access)
	std::vector<WorkflowEvent> parseZipArchive(ClientContext &context, const std::string &zip_path) const;

	// Static method to check if path looks like a ZIP file
	static bool isZipPath(const std::string &path);

private:
	// Job metadata extracted from ZIP filename
	struct JobMetadata {
		int32_t job_order;     // From {N}_ prefix
		std::string job_name;  // From filename (without prefix and extension)
		std::string file_path; // Full path within ZIP
	};

	// Extract job metadata from filename like "0_Build extension binaries.txt"
	JobMetadata extractJobMetadata(const std::string &filename) const;

	// List job log files in ZIP archive
	std::vector<std::string> listJobFiles(ClientContext &context, const std::string &zip_path) const;

	// Check if a filename is a job log (vs system.txt or other metadata)
	bool isJobLogFile(const std::string &filename) const;
};

} // namespace duckdb
