#include "parsers/workflow_engines/github_actions_zip_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include "parsers/workflow_engines/github_actions_parser.hpp"
#include "read_duck_hunt_log_function.hpp"
#include "core/zipfs_integration.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include <algorithm>
#include <regex>

namespace duckdb {

// Pre-compiled regex patterns for GitHub Actions ZIP parsing (compiled once, reused)
namespace {
static const std::regex RE_JOB_FILE_PATTERN(R"(^(\d+)_(.+)\.txt$)");
} // anonymous namespace

bool GitHubActionsZipParser::isZipPath(const std::string &path) {
	std::string lower_path = StringUtil::Lower(path);
	return StringUtil::EndsWith(lower_path, ".zip");
}

bool GitHubActionsZipParser::canParse(const std::string &content) const {
	// This parser is only used for ZIP files, not content detection
	// ZIP files are binary, so we can't detect them from content
	// The format must be explicitly specified as 'github_actions_zip'
	return false;
}

bool GitHubActionsZipParser::isJobLogFile(const std::string &filename) const {
	// Job log files are at the root level and end with .txt
	// They typically start with a number prefix: {N}_{job_name}.txt
	// Exclude system.txt files which are in subdirectories

	// Check if file is in a subdirectory (contains /)
	if (filename.find('/') != std::string::npos) {
		// If it's in a subdirectory, it's metadata (like system.txt), not a job log
		return false;
	}

	// Must end with .txt
	if (!StringUtil::EndsWith(StringUtil::Lower(filename), ".txt")) {
		return false;
	}

	// Should start with a digit (job order prefix)
	if (filename.empty() || !std::isdigit(filename[0])) {
		return false;
	}

	return true;
}

GitHubActionsZipParser::JobMetadata GitHubActionsZipParser::extractJobMetadata(const std::string &filename) const {
	JobMetadata meta;
	meta.job_order = -1;
	meta.file_path = filename;

	// Pattern: {N}_{job_name}.txt
	// Example: "0_Build extension binaries _ DuckDB-Wasm (linux_amd64).txt"
	std::smatch match;

	if (std::regex_match(filename, match, RE_JOB_FILE_PATTERN)) {
		meta.job_order = SafeParsing::SafeStoi(match[1].str());
		meta.job_name = match[2].str();
	} else {
		// Fallback: just use the filename without extension as job name
		if (StringUtil::EndsWith(filename, ".txt")) {
			meta.job_name = filename.substr(0, filename.length() - 4);
		} else {
			meta.job_name = filename;
		}
	}

	return meta;
}

std::vector<std::string> GitHubActionsZipParser::listJobFiles(ClientContext &context,
                                                              const std::string &zip_path) const {
	// Ensure zipfs extension is available (auto-load if possible)
	ZipfsIntegration::EnsureZipfsAvailable(context);

	std::vector<std::string> job_files;
	auto &fs = FileSystem::GetFileSystem(context);

	// Construct glob pattern for files in ZIP
	// Format: zip://path/to/archive.zip/*.txt
	std::string glob_pattern = "zip://" + zip_path + "/*.txt";

	try {
		auto files = fs.GlobFiles(glob_pattern, context, FileGlobOptions::ALLOW_EMPTY);

		for (const auto &file : files) {
			// Extract just the filename from the full path
			// The path will be like: zip://archive.zip/0_job.txt
			std::string file_path = file.path;

			// Find the last / to get the filename
			size_t last_slash = file_path.rfind('/');
			std::string filename;
			if (last_slash != std::string::npos) {
				filename = file_path.substr(last_slash + 1);
			} else {
				filename = file_path;
			}

			// Check if this is a job log file
			if (isJobLogFile(filename)) {
				job_files.push_back(file_path);
			}
		}
	} catch (const IOException &e) {
		// Check if it's a "no files found" type error
		std::string msg = e.what();
		if (msg.find("No files found") != std::string::npos || msg.find("does not exist") != std::string::npos) {
			throw IOException("No job log files found in ZIP archive '" + zip_path + "'.");
		}
		throw;
	}

	// Sort by filename to ensure consistent ordering
	std::sort(job_files.begin(), job_files.end());

	return job_files;
}

std::vector<WorkflowEvent> GitHubActionsZipParser::parseWorkflowLog(const std::string &content) const {
	// This method is called when content is passed directly
	// For ZIP files, we need the context-aware parseZipArchive method
	// Since we can't detect ZIP content, this just returns empty
	return std::vector<WorkflowEvent>();
}

std::vector<WorkflowEvent> GitHubActionsZipParser::parseZipArchive(ClientContext &context,
                                                                   const std::string &zip_path) const {
	std::vector<WorkflowEvent> all_events;

	// Get list of job log files in ZIP
	auto job_files = listJobFiles(context, zip_path);

	if (job_files.empty()) {
		throw IOException("No GitHub Actions job log files found in ZIP archive '" + zip_path +
		                  "'. Expected files matching pattern: {N}_{job_name}.txt");
	}

	// Create GitHub Actions parser for delegating parsing
	GitHubActionsParser ga_parser;

	for (const auto &file_path : job_files) {
		// Extract filename for metadata
		size_t last_slash = file_path.rfind('/');
		std::string filename = (last_slash != std::string::npos) ? file_path.substr(last_slash + 1) : file_path;

		JobMetadata meta = extractJobMetadata(filename);

		// Read file content from ZIP
		std::string content;
		try {
			content = ReadContentFromSource(context, file_path);
		} catch (const IOException &e) {
			// Skip files we can't read, but log/continue
			continue;
		}

		// Parse with GitHub Actions parser
		std::vector<WorkflowEvent> job_events = ga_parser.parseWorkflowLog(content);

		// Enrich events with ZIP metadata
		for (auto &event : job_events) {
			event.job_order = meta.job_order;
			event.job_name = meta.job_name;

			// Set the log_file to the ZIP path for tracking
			event.base_event.log_file = zip_path + ":" + filename;
		}

		// Append to all events
		all_events.insert(all_events.end(), job_events.begin(), job_events.end());
	}

	return all_events;
}

} // namespace duckdb
