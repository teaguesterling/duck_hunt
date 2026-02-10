#pragma once

// Context extraction utilities for Duck Hunt.
//
// This header is internal - the public API declarations are in
// include/read_duck_hunt_log_function.hpp. This file only exists
// to keep the implementations organized in a separate .cpp file.
//
// Functions implemented in context_extraction.cpp:
// - TruncateLogContent: Truncates log content based on mode
// - GetContextColumnType: Returns the LogicalType for context columns
// - ExtractContext: Extracts context lines around an event
// - PopulateDataChunkFromEvents: Populates a DataChunk from events
