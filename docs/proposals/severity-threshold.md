# Proposal: Severity Threshold Parameter

## Overview

Add a `severity_threshold` parameter to control which events are emitted based on their severity level. This enables:
- Filtering out low-priority events for focused analysis
- Including summary/info events when needed for reporting
- Consistent behavior across all parsers

## API

```sql
-- Default behavior (threshold = 'warning')
SELECT * FROM read_duck_hunt_log('build.log', 'make_error');

-- Include info-level events (summaries, passing tests)
SELECT * FROM read_duck_hunt_log('pytest.json', 'pytest_json', severity_threshold := 'info');

-- Only errors and critical
SELECT * FROM read_duck_hunt_log('build.log', 'make_error', severity_threshold := 'error');

-- Everything including debug
SELECT * FROM read_duck_hunt_log('app.log', 'logrus', severity_threshold := 'debug');
```

## Severity Levels

Ordered from lowest to highest:

| Level | Numeric | Description |
|-------|---------|-------------|
| `debug` | 0 | Trace/debug information, internal details |
| `info` | 1 | Informational: summaries, passing tests, successful operations |
| `warning` | 2 | Warnings: skipped tests, deprecations, non-fatal issues |
| `error` | 3 | Errors: failures, build errors, test failures |
| `critical` | 4 | Critical: fatal errors, crashes, security vulnerabilities |

Special value:
| Level | Description |
|-------|-------------|
| `all` | Alias for `debug` - emit everything |

## Default Threshold

**Default: `'warning'`**

This maintains backwards compatibility - current parsers emit WARNING and ERROR events, and this behavior continues unchanged.

## Severity Mappings by Event Type

### Test Frameworks

| Status | Severity | Rationale |
|--------|----------|-----------|
| PASS | `info` | Successful test - informational |
| SKIP | `warning` | Skipped test - may need attention |
| FAIL | `error` | Failed assertion |
| ERROR | `error` | Test error (exception, setup failure) |
| XFAIL | `info` | Expected failure - informational |
| XPASS | `warning` | Unexpected pass - needs attention |

**Summary events** (e.g., "317 passed in 43.79s"):
- Severity: `info`
- Status: `INFO`

### Linting & Static Analysis

| Event Type | Severity | Rationale |
|------------|----------|-----------|
| Style issue | `info` | Cosmetic, auto-fixable |
| Warning | `warning` | Potential problem |
| Error | `error` | Definite problem |
| Fatal | `critical` | Cannot continue |

**Summary events** (e.g., "No problems found"):
- Severity: `info`
- Status: `INFO`

### Build Systems

| Event Type | Severity | Example |
|------------|----------|---------|
| Command output/trace | `debug` | Compiler verbose output, linker details |
| Directory change | `debug` | `make: Entering directory '/path'` |
| Command executed | `info` | `gcc -c foo.c -o foo.o` |
| Target complete | `info` | `Nothing to be done for 'all'` |
| Build summary | `info` | `Build succeeded`, `2 warnings generated` |
| Deprecation warning | `warning` | CMake policy warnings |
| Compiler warning | `warning` | `-Wunused-variable`, `-Wdeprecated` |
| Compiler error | `error` | Syntax error, type mismatch |
| Linker error | `error` | Undefined reference, missing library |
| Fatal/internal error | `critical` | ICE, out of memory |

**Distinction:**
- **Command run** (`info`): The actual command being executed - useful for understanding what the build is doing
- **Command output** (`debug`): Verbose/trace output from the command - only needed for deep debugging

### Application Logs

| Log Level | Severity | Rationale |
|-----------|----------|-----------|
| TRACE | `debug` | Detailed tracing |
| DEBUG | `debug` | Debug information |
| INFO | `info` | Normal operation |
| WARN/WARNING | `warning` | Potential issue |
| ERROR | `error` | Error condition |
| FATAL/CRITICAL | `critical` | Application failure |

### Security Scanners

| Finding Severity | Duck Hunt Severity |
|------------------|-------------------|
| LOW | `info` |
| MEDIUM | `warning` |
| HIGH | `error` |
| CRITICAL | `critical` |

### CI/CD Workflows

| Event Type | Severity | Rationale |
|------------|----------|-----------|
| Step started | `debug` | Workflow trace |
| Step succeeded | `info` | Informational |
| Step warning | `warning` | Non-fatal issue |
| Step failed | `error` | Workflow failure |
| Workflow cancelled | `warning` | Interrupted |

## Implementation Notes

### Parser Interface

```cpp
struct ParserConfig {
    SeverityLevel threshold = SeverityLevel::WARNING;
};

enum class SeverityLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

// In parser:
if (event.severity >= config.threshold) {
    emit(event);
}
```

### Backwards Compatibility

- Default threshold is `'warning'`
- Existing parsers continue to emit the same events
- New INFO-level events are only emitted when threshold is lowered

### Performance

- Parsers can short-circuit parsing of low-severity patterns when threshold is high
- Minimal overhead for threshold check

## Migration Path

1. **Phase 1**: Add `severity_threshold` parameter, default to `'warning'`
2. **Phase 2**: Update parsers to detect INFO-level events (summaries, passing tests)
3. **Phase 3**: Add DEBUG-level events for verbose tracing

## Examples

### Get test summary with pass counts

```sql
SELECT status, COUNT(*) as count
FROM read_duck_hunt_log('pytest.json', 'pytest_json', severity_threshold := 'info')
GROUP BY status;
-- Returns: PASS: 317, FAIL: 2, SKIP: 5
```

### Build log with only errors

```sql
SELECT ref_file, ref_line, message
FROM read_duck_hunt_log('build.log', 'make_error', severity_threshold := 'error')
WHERE status = 'ERROR';
```

### Full debug trace

```sql
SELECT *
FROM read_duck_hunt_log('ci.log', 'github_actions', severity_threshold := 'all');
```

## Open Questions

1. Should `severity_threshold` also apply to `read_duck_hunt_workflow_log()`?
2. Should we add a `--severity-threshold` flag to any CLI tools?
3. Should the default be `'warning'` or `'error'`?

## Related Issues

- Issue #26: Add INFO-level events for tool summaries
