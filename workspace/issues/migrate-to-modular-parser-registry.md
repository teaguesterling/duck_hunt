# Migrate from Legacy Parser Detection to Modular Registry

## Overview

The codebase currently has two parallel systems for format detection:
1. **Legacy**: `DetectTestResultFormat()` function with 55 hardcoded format detections
2. **Modular**: `ParserRegistry` with 100 registered parser classes using `canParse()` methods

This duplication creates maintenance burden and inconsistency. The goal is to fully migrate to the modular registry and remove the legacy detection code.

## Current State (Updated Feb 2026)

### Legacy Detection (`DetectTestResultFormat`)
- Location: `src/read_duck_hunt_log_function.cpp` (lines ~280-833)
- 55 unique format detections using string matching
- Returns `TestResultFormat` enum values
- Used when `format="auto"` is specified
- **Status**: Still in use, checked before modular registry

### Modular Registry (`ParserRegistry`)
- Location: `src/parsers/*/init.cpp` files
- **100 registered parser classes** (was 91, now includes all legacy formats)
- Each parser has `canParse()` for detection and `parse()` for extraction
- Priority-based selection (VERY_HIGH=100, HIGH=80, MEDIUM=50, LOW=30, VERY_LOW=10)

## Migration Tasks

### Phase 1: Audit and Verify Coverage ✅ COMPLETE
- [x] Create mapping of legacy formats to modular parsers
- [x] Identify any legacy formats without modular equivalents
- [x] Verify `canParse()` methods are selective enough (avoid false positives)
- [x] Add test files for each format to ensure detection works

**Completed work:**
- Added 5 missing parsers to achieve parity with legacy 55 formats
- Added parsers: `github_actions_text`, `gitlab_ci_text`, `jenkins_text`, `docker_build`, `bandit_text`
- Added `ruff_text` parser (new Python linter format)
- Improved `pytest_text` parser to extract ref_line from FAILURES section
- Total: 100 modular parsers (95 log + 5 workflow)

### Phase 2: Unify Detection Path 🔄 IN PROGRESS
- [ ] Modify `format="auto"` to use `ParserRegistry::detectParser()` **before** `DetectTestResultFormat()`
- [x] Keep legacy enum for explicit format specification (e.g., `format="pytest_json"`)
- [x] Add fallback to `GenericErrorParser` when no parser matches
- [ ] Ensure parser priority ordering matches expected behavior
- [ ] Address conflict where flake8 matches before ruff (need registry-first detection)

**Current issue:** `duck_hunt_detect_format()` tries legacy detection FIRST, then falls back to modular registry. This causes newer modular parsers (like ruff) to be masked by legacy parsers (flake8).

### Phase 3: Clean Up Legacy Code
- [ ] Remove `DetectTestResultFormat()` function
- [ ] Simplify `TestResultFormat` enum to only include explicitly-specifiable formats
- [ ] Update format name mapping to use registry lookup
- [ ] Remove redundant format detection code paths

### Phase 4: Testing and Validation
- [x] Run all existing tests to ensure no regressions (39 tests pass)
- [ ] Test auto-detection with sample logs from each category
- [x] Verify explicit format specification still works
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
