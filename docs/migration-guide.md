# Schema V2 Migration Guide

This guide helps you migrate from the previous schema to Schema V2. This is a **breaking change** - queries using removed or renamed fields will need to be updated.

## Quick Reference

### Renamed Fields

| Old Field | New Field | Notes |
|-----------|-----------|-------|
| `error_fingerprint` | `fingerprint` | Same functionality, shorter name |
| `workflow_name` | `scope` | Generic hierarchy level 1 |
| `job_name` | `group` | Generic hierarchy level 2 |
| `step_name` | `unit` | Generic hierarchy level 3 |
| `workflow_run_id` | `scope_id` | ID for hierarchy level 1 |
| `job_id` | `group_id` | ID for hierarchy level 2 |
| `step_id` | `unit_id` | ID for hierarchy level 3 |
| `workflow_status` | `scope_status` | Status for hierarchy level 1 |
| `job_status` | `group_status` | Status for hierarchy level 2 |
| `step_status` | `unit_status` | Status for hierarchy level 3 |

### Removed Fields

| Old Field | Migration Path |
|-----------|----------------|
| `source_file` | Use `file_path` or `structured_data` |
| `build_id` | Use `scope_id` or `external_id` |
| `environment` | Use `scope` or `structured_data` |
| `file_index` | Use `structured_data` if needed |
| `root_cause_category` | Use `fingerprint` + `pattern_id` for clustering |
| `completed_at` | Compute from `started_at` + `execution_time` |
| `duration` | Use `execution_time` |

### New Fields

| Field | Purpose |
|-------|---------|
| `target` | Destination (IP:port, HTTP path, resource ARN) |
| `actor_type` | Type of actor: user, service, system, anonymous |
| `external_id` | External correlation ID (request ID, trace ID) |
| `subunit` | Hierarchy level 4 (container, sub-resource) |
| `subunit_id` | ID for hierarchy level 4 |

---

## Migration Examples

### Test Result Queries

**Before:**
```sql
SELECT test_name, status, error_fingerprint
FROM read_duck_hunt_log('pytest.json', 'pytest_json')
WHERE status = 'FAIL';
```

**After:**
```sql
SELECT test_name, status, fingerprint
FROM read_duck_hunt_log('pytest.json', 'pytest_json')
WHERE status = 'FAIL';
```

### CI/CD Workflow Queries

**Before:**
```sql
SELECT workflow_name, job_name, step_name, step_status
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE step_status = 'failure';
```

**After:**
```sql
SELECT scope as workflow, "group" as job, unit as step, unit_status
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE unit_status = 'failure';
```

> **Note:** `group` is a SQL reserved word in some contexts. Use `"group"` (quoted) or alias it immediately.

### Error Pattern Analysis

**Before:**
```sql
SELECT
    error_fingerprint,
    root_cause_category,
    COUNT(*) as occurrences
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR'
GROUP BY error_fingerprint, root_cause_category;
```

**After:**
```sql
SELECT
    fingerprint,
    pattern_id,
    COUNT(*) as occurrences
FROM read_duck_hunt_log('build.log', 'auto')
WHERE status = 'ERROR'
GROUP BY fingerprint, pattern_id;
```

> **Note:** `root_cause_category` was removed. Use `fingerprint` clustering with `pattern_id` for grouping similar errors.

### Multi-file Processing

**Before:**
```sql
SELECT source_file, build_id, environment, COUNT(*)
FROM read_duck_hunt_log('logs/**/*.log', 'auto')
GROUP BY source_file, build_id, environment;
```

**After:**
```sql
-- source_file, build_id, environment are no longer available
-- Use tool_name or message content for grouping
SELECT tool_name, scope, COUNT(*)
FROM read_duck_hunt_log('logs/**/*.log', 'auto')
GROUP BY tool_name, scope;
```

### Workflow Hierarchy Traversal

**Before:**
```sql
SELECT
    workflow_name,
    job_name,
    step_name,
    workflow_status,
    job_status,
    step_status
FROM read_duck_hunt_workflow_log('ci.log', 'github_actions');
```

**After:**
```sql
SELECT
    scope as workflow_name,
    "group" as job_name,
    unit as step_name,
    scope_status as workflow_status,
    group_status as job_status,
    unit_status as step_status
FROM read_duck_hunt_workflow_log('ci.log', 'github_actions');
```

### Duration Calculations

**Before:**
```sql
SELECT step_name, duration, completed_at
FROM read_duck_hunt_workflow_log('ci.log', 'github_actions');
```

**After:**
```sql
-- duration and completed_at are removed
-- Use execution_time for duration (in milliseconds)
SELECT unit as step_name, execution_time, started_at
FROM read_duck_hunt_workflow_log('ci.log', 'github_actions');
```

---

## Creating Compatibility Views

If you have many queries to migrate, you can create views that provide the old field names:

```sql
-- Compatibility view for read_duck_hunt_log
CREATE VIEW duck_hunt_log_compat AS
SELECT
    *,
    -- Renamed fields
    fingerprint as error_fingerprint,
    scope as workflow_name,
    "group" as job_name,
    unit as step_name,
    scope_id as workflow_run_id,
    group_id as job_id,
    unit_id as step_id,
    scope_status as workflow_status,
    group_status as job_status,
    unit_status as step_status,
    -- Removed fields (NULL placeholders)
    NULL as source_file,
    NULL as build_id,
    NULL as environment,
    NULL as file_index,
    NULL as root_cause_category,
    NULL as completed_at,
    execution_time as duration
FROM read_duck_hunt_log('your_file.log', 'auto');
```

---

## Field Mapping by Domain

The new generic hierarchy maps differently depending on the log type:

| Domain | scope | group | unit | subunit |
|--------|-------|-------|------|---------|
| **CI/CD** | Workflow | Job | Step | - |
| **Kubernetes** | Cluster | Namespace | Pod | Container |
| **Cloud Audit** | Account | Region | Service | - |
| **Tests** | Suite | Class | Method | - |
| **App Logs** | Service | Component | Handler | - |

See [Field Mappings](field_mappings.md) for complete documentation.

---

## Breaking Changes Summary

1. **Column count changed:** 40 → 39 fields
2. **Reserved word:** `group` may need quoting in SQL
3. **No more `root_cause_category`:** Use `fingerprint` + `pattern_id` instead
4. **No more `source_file`:** Multi-file metadata removed
5. **No more `completed_at`/`duration`:** Use `started_at` + `execution_time`

---

## Getting Help

If you encounter issues migrating:

1. Check [Schema Reference](schema.md) for complete field documentation
2. Check [Field Mappings](field_mappings.md) for domain-specific usage
3. Open an issue at https://github.com/teaguesterling/duck_hunt/issues
