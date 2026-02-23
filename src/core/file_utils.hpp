#pragma once

#include <memory>
#include <string>
#include "duckdb/main/client_context.hpp"
#include "duckdb/common/file_system.hpp"

// File utilities for Duck Hunt.
//
// This header is internal - the public API declarations are in
// include/read_duck_hunt_log_function.hpp. This file keeps the
// implementations organized in a separate .cpp file.
//
// Functions implemented in file_utils.cpp:
// - ReadContentFromSource: Read file content with compression support
// - IsValidJSON: Check if content looks like JSON
// - GetFilesFromPattern: Expand glob patterns to file list
// - GetGlobFiles: Get files matching a glob pattern
// - ProcessMultipleFiles: Process multiple log files
// - ExtractBuildIdFromPath: Extract build ID from file path
// - ExtractEnvironmentFromPath: Extract environment from file path
// - LineReader: Line-by-line file reader for streaming support

namespace duckdb {

/**
 * Read file content from a source path.
 * Supports compression detection via file extension (.gz, .zst, etc.)
 * @param context ClientContext for file system access
 * @param source File path to read
 * @return File content as string
 */
std::string ReadContentFromSource(ClientContext &context, const std::string &source);

/**
 * Peek at the first N bytes of a source file without reading the whole thing.
 * @param context ClientContext for file system access
 * @param source File path to peek
 * @param max_bytes Maximum number of bytes to read (default: 8192)
 * @return First max_bytes of file content as string
 */
std::string PeekContentFromSource(ClientContext &context, const std::string &source, size_t max_bytes);

/**
 * LineReader provides buffered, line-by-line reading of files.
 * Used for streaming parsers to enable:
 * - Early termination with LIMIT without reading entire file
 * - Reduced memory footprint for large files
 *
 * Handles compression transparently via DuckDB's FileSystem.
 */
class LineReader {
public:
	/**
	 * Construct a LineReader for the given file.
	 * @param context ClientContext for file system access
	 * @param source File path (supports compression via extension detection)
	 */
	LineReader(ClientContext &context, const std::string &source);

	/**
	 * Check if there are more lines to read.
	 * @return true if more lines available, false at EOF
	 */
	bool HasNext();

	/**
	 * Read and return the next line.
	 * Does not include the newline character.
	 * Advances the line number counter.
	 * @return The next line content
	 */
	std::string NextLine();

	/**
	 * Get the current line number (1-based).
	 * Returns the line number of the most recently read line.
	 * @return Current line number
	 */
	int32_t CurrentLineNumber() const {
		return line_number_;
	}

	/**
	 * Check if the reader has reached end of file.
	 * @return true if at EOF
	 */
	bool IsEOF() const {
		return eof_ && buffer_pos_ >= buffer_end_;
	}

private:
	unique_ptr<FileHandle> file_handle_;
	std::string buffer_;
	size_t buffer_pos_ = 0;
	size_t buffer_end_ = 0;
	int32_t line_number_ = 0;
	bool eof_ = false;

	static constexpr size_t BUFFER_SIZE = 65536; // 64KB buffer

	// Fill the buffer with more data from the file
	void FillBuffer();
};

} // namespace duckdb
