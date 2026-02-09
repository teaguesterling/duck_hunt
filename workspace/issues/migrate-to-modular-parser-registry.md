# Migrate from Legacy Parser Detection to Modular Registry

## Overview

The codebase currently has two parallel systems for format detection:
1. **Legacy**: `DetectTestResultFormat()` function with 55 hardcoded format detections
2. **Modular**: `ParserRegistry` with 91 registered parser classes using `canParse()` methods

This duplication creates maintenance burden and inconsistency. The goal is to fully migrate to the modular registry and remove the legacy detection code.

## Current State

### Legacy Detection (`DetectTestResultFormat`)
- Location: `src/read_duck_hunt_log_function.cpp` (lines ~280-833)
- 55 unique format detections using string matching
- Returns `TestResultFormat` enum values
- Used when `format="auto"` is specified

### Modular Registry (`ParserRegistry`)
- Location: `src/parsers/*/init.cpp` files
- 91 registered parser classes
- Each parser has `canParse()` for detection and `parse()` for extraction
- Priority-based selection (VERY_HIGH=100, HIGH=80, MEDIUM=50, LOW=30, VERY_LOW=10)

## Migration Tasks

### Phase 1: Audit and Verify Coverage
- [ ] Create mapping of legacy formats to modular parsers
- [ ] Identify any legacy formats without modular equivalents
- [ ] Verify `canParse()` methods are selective enough (avoid false positives)
- [ ] Add test files for each format to ensure detection works

### Phase 2: Unify Detection Path
- [ ] Modify `format="auto"` to use `ParserRegistry::detectParser()` instead of `DetectTestResultFormat()`
- [ ] Keep legacy enum for explicit format specification (e.g., `format="pytest_json"`)
- [ ] Add fallback to `GenericErrorParser` when no parser matches
- [ ] Ensure parser priority ordering matches expected behavior

### Phase 3: Clean Up Legacy Code
- [ ] Remove `DetectTestResultFormat()` function
- [ ] Simplify `TestResultFormat` enum to only include explicitly-specifiable formats
- [ ] Update format name mapping to use registry lookup
- [ ] Remove redundant format detection code paths

### Phase 4: Testing and Validation
- [ ] Run all existing tests to ensure no regressions
- [ ] Test auto-detection with sample logs from each category
- [ ] Verify explicit format specification still works
- [ ] Test edge cases (empty files, mixed formats, ambiguous content)

## Guidance

### Parser Priority Strategy
- **VERY_HIGH (100+)**: Highly specific JSON formats with unique keys (e.g., `playwright_json` at 135)
- **HIGH (80)**: Specific text formats with distinctive patterns
- **MEDIUM (50)**: General formats that could match loosely
- **LOW (30)**: Fallback formats
- **VERY_LOW (10)**: Last-resort parsers like `generic_error`

### Making `canParse()` Selective
When migrating detection logic, ensure parsers don't claim content they shouldn't:

```cpp
// BAD: Too generic, matches too much
bool canParse(const std::string &content) const override {
    return content.find("[FAIL]") != std::string::npos;
}

// GOOD: Requires specific markers
bool canParse(const std::string &content) const override {
    bool has_framework_marker = content.find("NUnit") != std::string::npos;
    bool has_dotnet_pattern = content.find(".Tests") != std::string::npos;
    return has_framework_marker || (content.find("[FAIL]") != std::string::npos && has_dotnet_pattern);
}
```

### Format Name Consistency
- Modular parser `format_name` should match legacy enum name (lowercase with underscores)
- Example: `TestResultFormat::PYTEST_JSON` → parser format_name `"pytest_json"`

### Testing Each Parser
For each parser, create a test file in `tests/<category>/` and verify:
1. Auto-detection selects the correct parser
2. Explicit format specification works
3. Parser extracts expected events

## Files to Modify

### Primary Changes
- `src/read_duck_hunt_log_function.cpp` - Remove `DetectTestResultFormat()`, update auto-detection
- `src/include/test_result_format.hpp` - Simplify enum (if exists)

### Parser Adjustments (as needed)
- `src/parsers/*/init.cpp` - Adjust priorities
- Individual parser `canParse()` methods - Make more selective

## Risk Mitigation

1. **Feature flag**: Consider adding a config option to use legacy detection as fallback during transition
2. **Parallel testing**: Run both detection methods and log discrepancies before removing legacy
3. **Incremental migration**: Migrate one category at a time (test frameworks, then linters, then CI, etc.)

## Related Files
- `src/core/parser_registry.cpp` - Registry implementation
- `src/parsers/base/base_parser.hpp` - Base parser interface
