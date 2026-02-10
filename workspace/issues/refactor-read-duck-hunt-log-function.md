# Refactor read_duck_hunt_log_function.cpp

## Overview

The `src/read_duck_hunt_log_function.cpp` file has grown large (~1600 lines after Phase 3 cleanup) and handles multiple concerns. This issue tracks future refactoring to improve maintainability.

## Current State

After the Phase 3 migration (Feb 2026):
- Removed `DetectTestResultFormat()` function (~551 lines)
- Removed `FORMAT_NAMES` array (~93 lines)
- Removed `TestResultFormatToString()` function
- Simplified to use registry-first format detection
- Added `GetCanonicalFormatName()` for alias resolution

## Remaining Refactoring Opportunities

### 1. Extract Format Conversion Functions
**Priority: Low**

Move to separate file: `src/format_conversion.cpp`
- `GetFormatMap()` - string→enum conversion
- `StringToTestResultFormat()` - wrapper for format lookup
- `GetCanonicalFormatName()` - enum→canonical string

### 2. Simplify TestResultFormat Enum
**Priority: Low**

The enum has 91 values but most are only used for alias resolution. Consider:
- Replacing with UNKNOWN/AUTO/REGEXP only
- Creating direct alias→canonical lookup map
- Benefits: Reduced sync burden with parser registry
- Risk: Breaking changes for external code using enum

### 3. Extract Error Pattern Functions
**Priority: Medium**

Move to separate file: `src/error_patterns.cpp`
- `ProcessErrorPatterns()` (~100 lines)
- `NormalizeErrorMessage()` and related helpers
- Pre-compiled regex patterns

### 4. Extract Content Utilities
**Priority: Low**

Move to separate file: `src/content_utils.cpp`
- `TruncateLogContent()`
- `ExtractContext()`
- `ReadContentFromSource()`
- `IsValidJSON()`

### 5. Split Table Function Implementations
**Priority: Medium**

Consider splitting into:
- `src/read_duck_hunt_log.cpp` - file-based table function
- `src/parse_duck_hunt_log.cpp` - string-based table function
- `src/duck_hunt_common.cpp` - shared utilities

### 6. Extract Multi-file Processing
**Priority: Low**

Move to separate file: `src/multifile_processor.cpp`
- `ProcessMultipleFiles()`
- `GetFilesFromPattern()`
- `GetGlobFiles()`
- `ExtractBuildIdFromPath()`
- `ExtractEnvironmentFromPath()`

## Guidelines

When refactoring:
1. Make one change at a time
2. Ensure all 39+ tests pass after each change
3. Keep function signatures stable to avoid breaking external code
4. Update header files to reflect new source file locations
5. Consider CMakeLists.txt updates for new source files

## Related Files

- `src/include/read_duck_hunt_log_function.hpp` - Header declarations
- `src/core/parser_registry.cpp` - Parser registry (already well-organized)
- `workspace/issues/migrate-to-modular-parser-registry.md` - Migration docs

## Notes

The file is functional and well-tested. This refactoring is for code organization, not functionality. Prioritize based on development needs rather than doing all at once.
