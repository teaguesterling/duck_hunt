#include "../include/read_duck_hunt_log_function.hpp"
#include "file_utils.hpp"
#include "parse_content.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/file_glob_options.hpp"
#include "duckdb/common/enums/file_compression_type.hpp"
#include <regex>

// Detect DuckDB v1.5+ (GlobFiles context parameter removed)
#if __has_include("duckdb/common/column_index_map.hpp")
#define DUCKDB_GLOB_V15
#endif

namespace duckdb {

// Maximum file size to read into memory (100MB default)
// Prevents OOM from accidentally processing multi-GB files
constexpr size_t MAX_FILE_SIZE_BYTES = 100 * 1024 * 1024;

/**
 * Validate a file path or glob pattern for basic security issues.
 * Returns true if the path appears safe, false if it should be rejected.
 *
 * Checks for:
 * - Path traversal attempts (..)
 * - Null bytes (can truncate paths in some systems)
 * - Excessively long paths
 */
bool ValidatePath(const std::string &path) {
	// Reject empty paths
	if (path.empty()) {
		return false;
	}

	// Reject excessively long paths (prevent buffer issues)
	constexpr size_t MAX_PATH_LENGTH = 4096;
	if (path.length() > MAX_PATH_LENGTH) {
		return false;
	}

	// Reject null bytes (can truncate paths)
	if (path.find('\0') != std::string::npos) {
		return false;
	}

	// Reject obvious path traversal attempts
	// Note: DuckDB's sandbox provides additional protection
	if (path.find("..") != std::string::npos) {
		// Allow ".." only if it's part of a longer segment like "...log"
		// Check for actual traversal patterns
		if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos || path == ".." ||
		    path.rfind("/..", path.length() - 3) == path.length() - 3 ||
		    path.rfind("\\..", path.length() - 3) == path.length() - 3) {
			return false;
		}
	}

	return true;
}

// ============================================================================
// LineReader Implementation
// ============================================================================

LineReader::LineReader(ClientContext &context, const std::string &source) {
	auto &fs = FileSystem::GetFileSystem(context);
	auto flags = FileFlags::FILE_FLAGS_READ | FileCompressionType::AUTO_DETECT;
	file_handle_ = fs.OpenFile(source, flags);
	buffer_.resize(BUFFER_SIZE);
}

void LineReader::FillBuffer() {
	if (eof_) {
		return;
	}

	// Move any remaining data to the beginning of the buffer
	if (buffer_pos_ < buffer_end_) {
		size_t remaining = buffer_end_ - buffer_pos_;
		std::memmove(&buffer_[0], &buffer_[buffer_pos_], remaining);
		buffer_end_ = remaining;
	} else {
		buffer_end_ = 0;
	}
	buffer_pos_ = 0;

	// Read more data into the buffer
	size_t space_available = buffer_.size() - buffer_end_;
	if (space_available > 0) {
		auto bytes_read = file_handle_->Read(&buffer_[buffer_end_], space_available);
		if (bytes_read == 0) {
			eof_ = true;
		} else {
			buffer_end_ += static_cast<size_t>(bytes_read);
		}
	}
}

bool LineReader::HasNext() {
	// If we have buffered data, check for newline or EOF
	if (buffer_pos_ < buffer_end_) {
		return true;
	}

	// Try to fill the buffer
	if (!eof_) {
		FillBuffer();
	}

	return buffer_pos_ < buffer_end_;
}

std::string LineReader::NextLine() {
	std::string line;

	while (true) {
		// Look for newline in current buffer
		for (size_t i = buffer_pos_; i < buffer_end_; ++i) {
			if (buffer_[i] == '\n') {
				// Found newline - extract line and advance position
				line.append(buffer_.data() + buffer_pos_, i - buffer_pos_);
				buffer_pos_ = i + 1;
				line_number_++;

				// Remove trailing \r if present (Windows line endings)
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				return line;
			}
		}

		// No newline found in buffer - append what we have and try to read more
		line.append(buffer_.data() + buffer_pos_, buffer_end_ - buffer_pos_);
		buffer_pos_ = buffer_end_;

		if (eof_) {
			// At EOF - return whatever we have (last line without newline)
			if (!line.empty()) {
				line_number_++;
				// Remove trailing \r if present
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
			}
			return line;
		}

		// Try to fill the buffer
		FillBuffer();

		if (buffer_pos_ >= buffer_end_ && eof_) {
			// EOF reached after fill - return accumulated line
			if (!line.empty()) {
				line_number_++;
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
			}
			return line;
		}
	}
}

std::string PeekContentFromSource(ClientContext &context, const std::string &source, size_t max_bytes) {
	auto &fs = FileSystem::GetFileSystem(context);
	auto flags = FileFlags::FILE_FLAGS_READ | FileCompressionType::AUTO_DETECT;
	auto file_handle = fs.OpenFile(source, flags);

	// Read up to max_bytes
	std::string content;
	content.resize(max_bytes);
	auto bytes_read = file_handle->Read((void *)content.data(), max_bytes);
	content.resize(static_cast<size_t>(bytes_read));

	return content;
}

std::string ReadContentFromSource(ClientContext &context, const std::string &source) {
	// Validate path before attempting to read
	if (!ValidatePath(source)) {
		throw InvalidInputException("Invalid file path: '%s'", source);
	}

	// Use DuckDB's FileSystem to properly handle file paths including UNITTEST_ROOT_DIRECTORY
	auto &fs = FileSystem::GetFileSystem(context);

	// Open the file with automatic compression detection based on file extension
	// This handles .gz, .zst, etc. transparently
	auto flags = FileFlags::FILE_FLAGS_READ | FileCompressionType::AUTO_DETECT;
	auto file_handle = fs.OpenFile(source, flags);

	// Check compression type - for compressed files we can't seek or get size upfront
	auto compression = file_handle->GetFileCompressionType();
	bool can_get_size = (compression == FileCompressionType::UNCOMPRESSED) && file_handle->CanSeek();

	if (can_get_size) {
		// Uncompressed file - read using known size for efficiency
		auto file_size = file_handle->GetFileSize();
		if (file_size > 0) {
			// Check file size limit to prevent OOM
			if (static_cast<size_t>(file_size) > MAX_FILE_SIZE_BYTES) {
				throw InvalidInputException("File '%s' exceeds maximum size limit of %zu MB (actual: %zu MB)", source,
				                            MAX_FILE_SIZE_BYTES / (1024 * 1024),
				                            static_cast<size_t>(file_size) / (1024 * 1024));
			}
			std::string content;
			content.resize(static_cast<size_t>(file_size));
			file_handle->Read((void *)content.data(), file_size);
			return content;
		}
	}

	// Compressed files, pipes, or empty files - read in chunks until EOF
	std::string content;
	constexpr size_t chunk_size = 65536; // 64KB chunks for better performance
	char buffer[chunk_size];

	while (true) {
		auto bytes_read = file_handle->Read(buffer, chunk_size);
		if (bytes_read == 0) {
			break; // EOF
		}
		content.append(buffer, static_cast<size_t>(bytes_read));

		// Check size limit during streaming read to prevent OOM
		if (content.size() > MAX_FILE_SIZE_BYTES) {
			throw InvalidInputException("File '%s' exceeds maximum size limit of %zu MB (read so far: %zu MB)", source,
			                            MAX_FILE_SIZE_BYTES / (1024 * 1024), content.size() / (1024 * 1024));
		}
	}

	return content;
}

bool IsValidJSON(const std::string &content) {
	// Simple heuristic - starts with { or [
	std::string trimmed = content;
	StringUtil::Trim(trimmed);
	return !trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '[');
}

std::vector<std::string> GetGlobFiles(ClientContext &context, const std::string &pattern) {
	// Validate pattern before attempting glob
	if (!ValidatePath(pattern)) {
		throw InvalidInputException("Invalid glob pattern: '%s'", pattern);
	}

	auto &fs = FileSystem::GetFileSystem(context);
	std::vector<std::string> result;

	// Don't bother if we can't identify a glob pattern
	try {
		bool has_glob = fs.HasGlob(pattern);
		if (!has_glob) {
			// For remote URLs, still try GlobFiles as it has better support
			if (pattern.find("://") == std::string::npos || pattern.find("file://") == 0) {
				return result;
			}
		}
	} catch (const NotImplementedException &) {
		// If HasGlob is not implemented, still try GlobFiles for remote URLs
		if (pattern.find("://") == std::string::npos || pattern.find("file://") == 0) {
			return result;
		}
	}

	// Use GlobFiles which handles extension auto-loading and directory filtering
	try {
#ifdef DUCKDB_GLOB_V15
		auto glob_files = fs.GlobFiles(pattern, FileGlobOptions::ALLOW_EMPTY);
#else
		auto glob_files = fs.GlobFiles(pattern, context, FileGlobOptions::ALLOW_EMPTY);
#endif
		for (auto &file : glob_files) {
			result.push_back(file.path);
		}
	} catch (const NotImplementedException &) {
		// No glob support available
		return result;
	} catch (const IOException &) {
		// Glob failed, return empty result
		return result;
	}

	return result;
}

std::vector<std::string> GetFilesFromPattern(ClientContext &context, const std::string &pattern) {
	auto &fs = FileSystem::GetFileSystem(context);
	std::vector<std::string> result;

	// Helper lambda to handle individual file paths (adapted from duckdb_yaml)
	auto processPath = [&](const std::string &file_path) {
		// First: check if we're dealing with just a single file that exists
		if (fs.FileExists(file_path)) {
			result.push_back(file_path);
			return;
		}

		// Second: attempt to use the path as a glob
		auto glob_files = GetGlobFiles(context, file_path);
		if (glob_files.size() > 0) {
			result.insert(result.end(), glob_files.begin(), glob_files.end());
			return;
		}

		// Third: if it looks like a directory, try to glob common test result files
		if (StringUtil::EndsWith(file_path, "/")) {
			// Common test result file patterns
			std::vector<std::string> patterns = {"*.xml", "*.json", "*.txt", "*.log", "*.out"};
			for (const auto &ext_pattern : patterns) {
				auto files = GetGlobFiles(context, fs.JoinPath(file_path, ext_pattern));
				result.insert(result.end(), files.begin(), files.end());
			}
			return;
		}

		// If file doesn't exist and isn't a valid glob, throw error
		throw IOException("File or directory does not exist: " + file_path);
	};

	processPath(pattern);
	return result;
}

void ProcessMultipleFiles(ClientContext &context, const std::vector<std::string> &files, TestResultFormat format,
                          const std::string &format_name, std::vector<ValidationEvent> &events, bool ignore_errors) {
	for (size_t file_idx = 0; file_idx < files.size(); file_idx++) {
		const auto &file_path = files[file_idx];

		try {
			// Determine format using sniff approach (peek first, full read only if needed)
			std::string effective_format_name = format_name;
			if (format == TestResultFormat::AUTO) {
				// Peek first SNIFF_BUFFER_SIZE bytes for format detection
				// This avoids loading entire file into memory for unrecognized formats
				std::string peek_content = PeekContentFromSource(context, file_path, SNIFF_BUFFER_SIZE);
				effective_format_name = DetectFormat(peek_content);
				if (effective_format_name.empty()) {
					continue; // No parser found, skip file without reading full content
				}
			}

			// Format detected (or explicitly specified) - now read full content
			std::string content = ReadContentFromSource(context, file_path);

			// Skip REGEXP format in multi-file mode (requires pattern)
			if (format == TestResultFormat::REGEXP) {
				continue;
			}

			// Parse content using core API
			auto file_events = ParseContent(context, content, effective_format_name);
			if (file_events.empty()) {
				continue; // No events parsed, skip file
			}

			// Set log_file on each event to track source file
			for (auto &event : file_events) {
				event.log_file = file_path;
			}

			// Add events to main collection
			events.insert(events.end(), file_events.begin(), file_events.end());

		} catch (const IOException &e) {
			// IOException (file not found, can't read, etc.) - always skip and continue
			continue;
		} catch (const std::exception &e) {
			// Parsing errors - skip if ignore_errors, otherwise rethrow
			if (ignore_errors) {
				continue;
			}
			throw;
		}
	}
}

std::string ExtractBuildIdFromPath(const std::string &file_path) {
	// Extract build ID from common patterns like:
	// - /builds/build-123/results.xml -> "build-123"
	// - /ci-logs/pipeline-456/test.log -> "pipeline-456"
	// - /artifacts/20231201-142323/output.txt -> "20231201-142323"

	std::regex build_patterns[] = {
	    std::regex(R"(/(?:build|pipeline|run|job)-([^/\s]+)/)"), // build-123, pipeline-456
	    std::regex(R"(/(\d{8}-\d{6})/)"),                        // 20231201-142323
	    std::regex(R"(/(?:builds?|ci|artifacts)/([^/\s]+)/)"),   // builds/abc123, ci/def456
	    std::regex(R"([_-](\w+\d+)[_-])"),                       // any_build123_ pattern
	};

	for (const auto &pattern : build_patterns) {
		std::smatch match;
		if (std::regex_search(file_path, match, pattern)) {
			return match[1].str();
		}
	}

	return ""; // No build ID found
}

std::string ExtractEnvironmentFromPath(const std::string &file_path) {
	// Extract environment from common patterns like:
	// - /environments/dev/results.xml -> "dev"
	// - /staging/ci-logs/test.log -> "staging"
	// - /prod/artifacts/output.txt -> "prod"

	std::vector<std::string> environments = {"dev",  "development", "staging", "stage",
	                                         "prod", "production",  "test",    "testing"};

	for (const auto &env : environments) {
		if (file_path.find("/" + env + "/") != std::string::npos ||
		    file_path.find("-" + env + "-") != std::string::npos ||
		    file_path.find("_" + env + "_") != std::string::npos) {
			return env;
		}
	}

	return ""; // No environment found
}

} // namespace duckdb
