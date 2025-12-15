-- lq.sql - Log Query Schema and Macros
-- This file defines the schema, views, and macros for querying lq logs.
--
-- Directory structure (Hive partitioned):
--   .lq/
--   ├── logs/
--   │   └── date=YYYY-MM-DD/
--   │       └── source=run|import|capture/
--   │           └── {run_id}_{name}_{timestamp}.parquet
--   ├── raw/           # Optional: raw log files
--   │   └── {run_id}.log
--   └── lq.sql         # This file (copied on init)
--
-- Usage:
--   .read .lq/lq.sql
--   SELECT * FROM lq_status();

-- ============================================================================
-- CONFIGURATION
-- ============================================================================

-- Base path for logs (can be overridden)
CREATE OR REPLACE MACRO lq_base_path() AS '.lq/logs';

-- Status badge based on error/warning counts
-- Returns: '[FAIL]' if errors, '[WARN]' if warnings, '[ OK ]' otherwise
CREATE OR REPLACE MACRO status_badge(error_count, warning_count) AS
    CASE
        WHEN error_count > 0 THEN '[FAIL]'
        WHEN warning_count > 0 THEN '[WARN]'
        ELSE '[ OK ]'
    END;

-- ============================================================================
-- CORE VIEWS
-- ============================================================================

-- All events from all parquet files (Hive partitioned)
CREATE OR REPLACE VIEW lq_events AS
SELECT
    *,
    -- Extract partition columns if not already present
    regexp_extract(filename, 'date=([^/]+)', 1) AS log_date,
    regexp_extract(filename, 'source=([^/]+)', 1) AS partition_source
FROM read_parquet(
    lq_base_path() || '/**/*.parquet',
    hive_partitioning = true,
    filename = true,
    union_by_name = true
);

-- Distinct runs with aggregated stats
CREATE OR REPLACE VIEW lq_runs AS
SELECT
    run_id,
    source_name,
    source_type,
    command,
    MIN(started_at) AS started_at,
    MAX(completed_at) AS completed_at,
    MAX(exit_code) AS exit_code,
    COUNT(*) AS event_count,
    COUNT(*) FILTER (WHERE severity = 'error') AS error_count,
    COUNT(*) FILTER (WHERE severity = 'warning') AS warning_count,
    MAX(log_date) AS log_date
FROM lq_events
GROUP BY run_id, source_name, source_type, command;

-- Latest run per source with status badge
CREATE OR REPLACE VIEW lq_source_status AS
SELECT
    source_name,
    status_badge(error_count, warning_count) AS badge,
    error_count,
    warning_count,
    event_count,
    started_at,
    completed_at,
    exit_code,
    run_id
FROM lq_runs
QUALIFY row_number() OVER (PARTITION BY source_name ORDER BY started_at DESC) = 1
ORDER BY source_name;

-- ============================================================================
-- STATUS MACROS
-- ============================================================================

-- Quick status overview (for `lq status`)
CREATE OR REPLACE MACRO lq_status() AS TABLE
SELECT
    badge || ' ' || source_name AS status,
    error_count AS errors,
    warning_count AS warnings,
    age(now(), started_at) AS age
FROM lq_source_status
ORDER BY
    CASE WHEN badge = '[FAIL]' THEN 0
         WHEN badge = '[WARN]' THEN 1
         WHEN badge = '[ .. ]' THEN 2
         ELSE 3 END,
    source_name;

-- Verbose status with more details
CREATE OR REPLACE MACRO lq_status_verbose() AS TABLE
SELECT
    badge || ' ' || source_name AS status,
    error_count || ' errors, ' || warning_count || ' warnings' AS summary,
    CASE
        WHEN age(now(), started_at) < INTERVAL '1 minute' THEN 'just now'
        WHEN age(now(), started_at) < INTERVAL '1 hour' THEN
            extract(minute FROM age(now(), started_at))::INT || 'm ago'
        WHEN age(now(), started_at) < INTERVAL '1 day' THEN
            extract(hour FROM age(now(), started_at))::INT || 'h ago'
        ELSE started_at::DATE::VARCHAR
    END AS age,
    exit_code
FROM lq_source_status
ORDER BY started_at DESC;

-- ============================================================================
-- ERROR/WARNING MACROS
-- ============================================================================

-- Recent errors (for `lq errors`)
CREATE OR REPLACE MACRO lq_errors(n := 10) AS TABLE
SELECT
    source_name,
    file_path,
    line_number,
    column_number,
    LEFT(message, 200) AS message,
    tool_name,
    category
FROM lq_events
WHERE severity = 'error'
ORDER BY started_at DESC, event_id
LIMIT n;

-- Recent errors for a specific source
CREATE OR REPLACE MACRO lq_errors_for(src, n := 10) AS TABLE
SELECT
    file_path,
    line_number,
    column_number,
    LEFT(message, 200) AS message,
    tool_name,
    category
FROM lq_events
WHERE severity = 'error' AND source_name = src
ORDER BY started_at DESC, event_id
LIMIT n;

-- Recent warnings (for `lq warnings`)
CREATE OR REPLACE MACRO lq_warnings(n := 10) AS TABLE
SELECT
    source_name,
    file_path,
    line_number,
    column_number,
    LEFT(message, 200) AS message,
    tool_name,
    category
FROM lq_events
WHERE severity = 'warning'
ORDER BY started_at DESC, event_id
LIMIT n;

-- ============================================================================
-- SUMMARY MACROS
-- ============================================================================

-- Aggregate summary by tool and category
CREATE OR REPLACE MACRO lq_summary() AS TABLE
SELECT
    tool_name,
    category,
    COUNT(*) FILTER (WHERE severity = 'error') AS errors,
    COUNT(*) FILTER (WHERE severity = 'warning') AS warnings,
    COUNT(*) AS total
FROM lq_events
GROUP BY tool_name, category
HAVING errors > 0 OR warnings > 0
ORDER BY errors DESC, warnings DESC;

-- Summary for latest run only
CREATE OR REPLACE MACRO lq_summary_latest() AS TABLE
WITH latest_run AS (
    SELECT run_id FROM lq_runs ORDER BY started_at DESC LIMIT 1
)
SELECT
    tool_name,
    category,
    COUNT(*) FILTER (WHERE severity = 'error') AS errors,
    COUNT(*) FILTER (WHERE severity = 'warning') AS warnings,
    COUNT(*) AS total
FROM lq_events
WHERE run_id = (SELECT run_id FROM latest_run)
GROUP BY tool_name, category
HAVING errors > 0 OR warnings > 0
ORDER BY errors DESC, warnings DESC;

-- ============================================================================
-- DETAIL MACROS
-- ============================================================================

-- Get full event details by ID
CREATE OR REPLACE MACRO lq_event(id) AS TABLE
SELECT * FROM lq_events WHERE event_id = id;

-- Get events for a specific file
CREATE OR REPLACE MACRO lq_file(path) AS TABLE
SELECT
    line_number,
    column_number,
    severity,
    message,
    tool_name
FROM lq_events
WHERE file_path LIKE '%' || path || '%'
ORDER BY line_number, column_number;

-- ============================================================================
-- HISTORY MACROS
-- ============================================================================

-- Run history
CREATE OR REPLACE MACRO lq_history(n := 20) AS TABLE
SELECT
    run_id,
    status_badge(error_count, warning_count) AS badge,
    source_name,
    error_count,
    warning_count,
    started_at,
    age(completed_at, started_at) AS duration
FROM lq_runs
ORDER BY started_at DESC
LIMIT n;

-- Compare two runs
CREATE OR REPLACE MACRO lq_diff(run1, run2) AS TABLE
WITH r1 AS (
    SELECT tool_name, category,
           COUNT(*) FILTER (WHERE severity = 'error') AS errors
    FROM lq_events WHERE run_id = run1
    GROUP BY tool_name, category
),
r2 AS (
    SELECT tool_name, category,
           COUNT(*) FILTER (WHERE severity = 'error') AS errors
    FROM lq_events WHERE run_id = run2
    GROUP BY tool_name, category
)
SELECT
    COALESCE(r1.tool_name, r2.tool_name) AS tool_name,
    COALESCE(r1.category, r2.category) AS category,
    COALESCE(r1.errors, 0) AS run1_errors,
    COALESCE(r2.errors, 0) AS run2_errors,
    COALESCE(r2.errors, 0) - COALESCE(r1.errors, 0) AS delta
FROM r1 FULL OUTER JOIN r2
  ON r1.tool_name = r2.tool_name AND r1.category = r2.category
WHERE COALESCE(r1.errors, 0) != COALESCE(r2.errors, 0)
ORDER BY ABS(delta) DESC;

-- ============================================================================
-- REFERENCE MACROS
-- ============================================================================

-- Create event reference string: "5:3" for run 5, event 3
CREATE OR REPLACE MACRO lq_ref(run_id, event_id) AS
    run_id::VARCHAR || ':' || event_id::VARCHAR;

-- Parse event reference back to struct
CREATE OR REPLACE MACRO lq_parse_ref(ref) AS {
    run_id: CAST(split_part(ref, ':', 1) AS INTEGER),
    event_id: CAST(split_part(ref, ':', 2) AS INTEGER)
};

-- Format location string: "src/main.c:15:5"
CREATE OR REPLACE MACRO lq_location(file_path, line_number, column_number) AS
    COALESCE(file_path, '?') ||
    CASE WHEN line_number IS NOT NULL THEN ':' || line_number::VARCHAR ELSE '' END ||
    CASE WHEN column_number IS NOT NULL AND column_number > 0 THEN ':' || column_number::VARCHAR ELSE '' END;

-- Short fingerprint for display: "make_98586554"
CREATE OR REPLACE MACRO lq_short_fp(fp) AS
    CASE WHEN fp IS NULL THEN NULL
         ELSE split_part(fp, '_', 1) || '_' || LEFT(split_part(fp, '_', 3), 8)
    END;

-- Get event by reference string
CREATE OR REPLACE MACRO lq_get_event(ref) AS TABLE
SELECT
    lq_ref(run_id, event_id) AS ref,
    source_name,
    severity,
    lq_location(file_path, line_number, column_number) AS location,
    message,
    error_fingerprint,
    log_line_start,
    log_line_end
FROM lq_events
WHERE run_id = (lq_parse_ref(ref)).run_id
  AND event_id = (lq_parse_ref(ref)).event_id;

-- Find events with same fingerprint (same error across runs)
CREATE OR REPLACE MACRO lq_similar_events(fp, n := 10) AS TABLE
SELECT
    lq_ref(run_id, event_id) AS ref,
    source_name,
    started_at,
    lq_location(file_path, line_number, column_number) AS location,
    LEFT(message, 80) AS message
FROM lq_events
WHERE error_fingerprint = fp
ORDER BY started_at DESC
LIMIT n;

-- ============================================================================
-- UTILITY MACROS
-- ============================================================================

-- Compact error format for agents (minimal tokens)
CREATE OR REPLACE MACRO lq_errors_compact(n := 10) AS TABLE
SELECT
    lq_ref(run_id, event_id) AS ref,
    lq_location(file_path, line_number, column_number) || ': ' || LEFT(message, 100) AS error
FROM lq_events
WHERE severity = 'error'
ORDER BY started_at DESC, event_id
LIMIT n;

-- JSON output for MCP/agents
CREATE OR REPLACE MACRO lq_errors_json(n := 10) AS
SELECT to_json(list(err)) FROM (
    SELECT {
        ref: lq_ref(run_id, event_id),
        file_path: file_path,
        line: line_number,
        column: column_number,
        message: message,
        tool: tool_name,
        category: category,
        fingerprint: lq_short_fp(error_fingerprint),
        log_lines: CASE WHEN log_line_start IS NOT NULL
                        THEN [log_line_start, log_line_end]
                        ELSE NULL END
    } AS err
    FROM lq_events
    WHERE severity = 'error'
    ORDER BY started_at DESC, event_id
    LIMIT n
);

-- ============================================================================
-- MAINTENANCE
-- ============================================================================

-- Show log file sizes
CREATE OR REPLACE MACRO lq_files() AS TABLE
SELECT
    filename,
    log_date,
    source_type,
    COUNT(*) AS events
FROM lq_events
GROUP BY filename, log_date, source_type
ORDER BY log_date DESC, source_type;
