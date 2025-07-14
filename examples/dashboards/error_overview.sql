-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 📊 ERROR OVERVIEW DASHBOARD: Executive Summary Queries
-- ═══════════════════════════════════════════════════════════════════════════════════════
-- Ready-to-use queries for building comprehensive error monitoring dashboards
-- Load the Duck Hunt extension first: LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🎯 EXECUTIVE SUMMARY METRICS
-- ───────────────────────────────────────────────────────────────────────────────────────

-- Key Performance Indicators
SELECT 
    'System Health Overview' as dashboard_section,
    COUNT(*) as total_events,
    COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as total_failures,
    ROUND(
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / 
        NULLIF(COUNT(*), 0) * 100, 
        1
    ) as failure_rate_pct,
    COUNT(DISTINCT pattern_id) as unique_error_patterns,
    COUNT(DISTINCT error_fingerprint) as unique_error_types,
    COUNT(DISTINCT root_cause_category) as root_cause_categories,
    COUNT(DISTINCT COALESCE(environment, 'unknown')) as environments_monitored,
    COUNT(DISTINCT COALESCE(build_id, source_file)) as builds_analyzed
FROM read_test_results('workspace/**/*.txt', 'auto');

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🚨 TOP ISSUES REQUIRING ATTENTION  
-- ───────────────────────────────────────────────────────────────────────────────────────

-- Most Frequent Error Patterns
SELECT 
    'Top Error Patterns' as dashboard_section,
    pattern_id,
    error_fingerprint,
    root_cause_category,
    COUNT(*) as occurrences,
    COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failures,
    COUNT(DISTINCT COALESCE(environment, 'unknown')) as environments_affected,
    COUNT(DISTINCT COALESCE(build_id, source_file)) as builds_affected,
    ROUND(AVG(similarity_score), 3) as avg_similarity,
    -- Representative error message (first occurrence)
    (SELECT message 
     FROM read_test_results('workspace/**/*.txt', 'auto') sub 
     WHERE sub.pattern_id = main.pattern_id 
     ORDER BY event_id LIMIT 1) as sample_message
FROM read_test_results('workspace/**/*.txt', 'auto') main
WHERE pattern_id IS NOT NULL
GROUP BY pattern_id, error_fingerprint, root_cause_category
ORDER BY occurrences DESC, failures DESC
LIMIT 10;

-- Root Cause Distribution
SELECT 
    'Root Cause Breakdown' as dashboard_section,
    root_cause_category,
    COUNT(*) as total_events,
    COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failure_events,
    COUNT(DISTINCT pattern_id) as unique_patterns,
    COUNT(DISTINCT COALESCE(environment, 'unknown')) as environments,
    ROUND(
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / 
        NULLIF(COUNT(*), 0) * 100, 
        1
    ) as failure_rate_pct,
    ROUND(
        COUNT(*)::DECIMAL / 
        (SELECT COUNT(*) FROM read_test_results('workspace/**/*.txt', 'auto') WHERE root_cause_category IS NOT NULL) * 100,
        1
    ) as percentage_of_total
FROM read_test_results('workspace/**/*.txt', 'auto')
WHERE root_cause_category IS NOT NULL
GROUP BY root_cause_category
ORDER BY failure_events DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🌍 ENVIRONMENT HEALTH COMPARISON
-- ───────────────────────────────────────────────────────────────────────────────────────

SELECT 
    'Environment Health Comparison' as dashboard_section,
    COALESCE(environment, 'unknown') as environment,
    COUNT(*) as total_events,
    COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failures,
    COUNT(CASE WHEN status = 'PASS' THEN 1 END) as passes,
    COUNT(DISTINCT pattern_id) as unique_error_patterns,
    ROUND(
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / 
        NULLIF(COUNT(*), 0) * 100, 
        1
    ) as failure_rate_pct,
    CASE 
        WHEN COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / NULLIF(COUNT(*), 0) >= 0.2 THEN '🔴 Critical'
        WHEN COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / NULLIF(COUNT(*), 0) >= 0.1 THEN '🟡 Warning'
        WHEN COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / NULLIF(COUNT(*), 0) >= 0.05 THEN '🟠 Caution'
        ELSE '🟢 Healthy'
    END as health_status
FROM read_test_results('workspace/**/*.txt', 'auto')
GROUP BY environment
ORDER BY failure_rate_pct DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 📈 TREND INDICATORS
-- ───────────────────────────────────────────────────────────────────────────────────────

-- Recent vs Historical Comparison
WITH recent_period AS (
    SELECT 
        COUNT(*) as recent_total,
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as recent_failures,
        COUNT(DISTINCT pattern_id) as recent_patterns
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE file_index >= (SELECT MAX(file_index) * 0.7 FROM read_test_results('workspace/**/*.txt', 'auto'))
),
historical_period AS (
    SELECT 
        COUNT(*) as historical_total,
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as historical_failures,
        COUNT(DISTINCT pattern_id) as historical_patterns
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE file_index < (SELECT MAX(file_index) * 0.7 FROM read_test_results('workspace/**/*.txt', 'auto'))
)
SELECT 
    'Trend Analysis' as dashboard_section,
    r.recent_total,
    r.recent_failures,
    ROUND(r.recent_failures::DECIMAL / NULLIF(r.recent_total, 0) * 100, 1) as recent_failure_rate,
    h.historical_total,
    h.historical_failures,
    ROUND(h.historical_failures::DECIMAL / NULLIF(h.historical_total, 0) * 100, 1) as historical_failure_rate,
    ROUND(
        (r.recent_failures::DECIMAL / NULLIF(r.recent_total, 0)) - 
        (h.historical_failures::DECIMAL / NULLIF(h.historical_total, 0)),
        3
    ) as failure_rate_change,
    r.recent_patterns - h.historical_patterns as new_error_patterns,
    CASE 
        WHEN (r.recent_failures::DECIMAL / NULLIF(r.recent_total, 0)) > 
             (h.historical_failures::DECIMAL / NULLIF(h.historical_total, 0)) * 1.2 THEN '📈 Deteriorating'
        WHEN (r.recent_failures::DECIMAL / NULLIF(r.recent_total, 0)) < 
             (h.historical_failures::DECIMAL / NULLIF(h.historical_total, 0)) * 0.8 THEN '📉 Improving'
        ELSE '➡️ Stable'
    END as trend_direction
FROM recent_period r, historical_period h;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- ⚡ ACTIONABLE INSIGHTS
-- ───────────────────────────────────────────────────────────────────────────────────────

-- Critical Issues Needing Immediate Attention
SELECT 
    'Critical Action Items' as dashboard_section,
    pattern_id,
    root_cause_category,
    COUNT(*) as occurrences,
    COUNT(DISTINCT COALESCE(environment, 'unknown')) as environments_affected,
    ROUND(AVG(similarity_score), 3) as pattern_consistency,
    (SELECT message 
     FROM read_test_results('workspace/**/*.txt', 'auto') sub 
     WHERE sub.pattern_id = main.pattern_id 
     ORDER BY event_id LIMIT 1) as representative_error,
    CASE 
        WHEN COUNT(*) >= 5 AND COUNT(DISTINCT COALESCE(environment, 'unknown')) > 1 THEN 'Widespread recurring issue'
        WHEN COUNT(*) >= 3 AND root_cause_category IN ('network', 'permission') THEN 'Infrastructure problem'
        WHEN COUNT(*) >= 3 AND root_cause_category = 'configuration' THEN 'Configuration issue'
        WHEN COUNT(DISTINCT COALESCE(environment, 'unknown')) > 1 THEN 'Cross-environment issue'
        ELSE 'Monitor for recurrence'
    END as recommended_action
FROM read_test_results('workspace/**/*.txt', 'auto') main
WHERE pattern_id IS NOT NULL 
  AND status IN ('ERROR', 'FAIL')
GROUP BY pattern_id, root_cause_category
HAVING COUNT(*) >= 2  -- Only show patterns that occurred multiple times
ORDER BY occurrences DESC, environments_affected DESC
LIMIT 15;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 📊 QUALITY METRICS BY TOOL
-- ───────────────────────────────────────────────────────────────────────────────────────

SELECT 
    'Tool Performance Analysis' as dashboard_section,
    tool_name,
    COUNT(*) as total_events,
    COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failures,
    COUNT(CASE WHEN status = 'PASS' THEN 1 END) as passes,
    COUNT(CASE WHEN status = 'WARNING' THEN 1 END) as warnings,
    COUNT(DISTINCT pattern_id) as unique_error_patterns,
    ROUND(
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / 
        NULLIF(COUNT(*), 0) * 100, 
        1
    ) as failure_rate_pct,
    CASE 
        WHEN COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / NULLIF(COUNT(*), 0) >= 0.15 THEN '🔴 Problematic'
        WHEN COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / NULLIF(COUNT(*), 0) >= 0.05 THEN '🟡 Needs attention'
        ELSE '🟢 Performing well'
    END as tool_health
FROM read_test_results('workspace/**/*.txt', 'auto')
WHERE tool_name IS NOT NULL AND tool_name != ''
GROUP BY tool_name
HAVING COUNT(*) >= 3  -- Only show tools with sufficient data
ORDER BY failure_rate_pct DESC, total_events DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🎯 SUCCESS METRICS
-- ───────────────────────────────────────────────────────────────────────────────────────

-- Positive Indicators
SELECT 
    'Success Metrics' as dashboard_section,
    COUNT(CASE WHEN status = 'PASS' THEN 1 END) as total_passes,
    COUNT(CASE WHEN status = 'SKIP' THEN 1 END) as total_skips,
    COUNT(DISTINCT COALESCE(build_id, source_file)) as successful_builds,
    ROUND(AVG(execution_time), 3) as avg_execution_time,
    COUNT(CASE WHEN status = 'PASS' AND execution_time > 0 THEN 1 END) as timed_successes,
    ROUND(
        COUNT(CASE WHEN status = 'PASS' THEN 1 END)::DECIMAL / 
        NULLIF(COUNT(*), 0) * 100, 
        1
    ) as overall_success_rate,
    -- Build quality score
    ROUND(
        (COUNT(CASE WHEN status = 'PASS' THEN 1 END)::DECIMAL / NULLIF(COUNT(*), 0)) * 100 - 
        (COUNT(DISTINCT pattern_id) * 2) + 
        CASE WHEN AVG(execution_time) < 10 THEN 5 ELSE 0 END,
        1
    ) as quality_score
FROM read_test_results('workspace/**/*.txt', 'auto');

-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 💡 DASHBOARD USAGE NOTES:
-- ═══════════════════════════════════════════════════════════════════════════════════════
/*

This file provides executive-level dashboard queries for:

1. **KPI Overview**: Total events, failure rates, unique patterns
2. **Top Issues**: Most frequent error patterns requiring attention  
3. **Environment Health**: Comparative analysis across dev/staging/prod
4. **Trend Analysis**: Recent vs historical performance comparison
5. **Action Items**: Critical issues needing immediate intervention
6. **Tool Performance**: Which tools are performing well vs. poorly
7. **Success Metrics**: Positive indicators and quality scores

To use in BI tools:
- Copy each query section into your dashboard tool
- Adjust file patterns in read_test_results() calls for your data
- Set up automated refresh schedules
- Create alerts based on failure_rate_pct thresholds
- Use the health_status and trend_direction for visual indicators

Query Performance Tips:
- These queries are optimized for dashboard use
- Consider materializing results for large datasets
- Use LIMIT clauses to control data volume
- Cache results for frequently accessed metrics

*/