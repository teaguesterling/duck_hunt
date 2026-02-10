#pragma once

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
