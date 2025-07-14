-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 🕒 TEMPORAL ANALYSIS: Time-Series Error Pattern Analytics  
-- ═══════════════════════════════════════════════════════════════════════════════════════
-- Analyze how error patterns evolve over time using DuckDB's powerful window functions
-- Load the Duck Hunt extension first: LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 📈 1. ERROR FREQUENCY TRENDS OVER TIME
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Track how error patterns change frequency over time windows

WITH daily_error_trends AS (
    SELECT 
        -- Extract date from build_id or use file modification patterns
        CASE 
            WHEN build_id ~ '^build-\d+$' THEN 
                DATE '2024-01-01' + INTERVAL (CAST(REGEXP_EXTRACT(build_id, '\d+') AS INTEGER)) DAY
            ELSE DATE '2024-01-01'
        END as error_date,
        root_cause_category,
        pattern_id,
        error_fingerprint,
        COUNT(*) as daily_count,
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failure_count
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE root_cause_category IS NOT NULL
    GROUP BY 1, 2, 3, 4
),
trend_analysis AS (
    SELECT *,
        LAG(daily_count, 1) OVER (PARTITION BY pattern_id ORDER BY error_date) as prev_day_count,
        LAG(daily_count, 7) OVER (PARTITION BY pattern_id ORDER BY error_date) as week_ago_count,
        AVG(daily_count) OVER (PARTITION BY pattern_id ORDER BY error_date ROWS 6 PRECEDING) as week_avg,
        STDDEV(daily_count) OVER (PARTITION BY pattern_id ORDER BY error_date ROWS 6 PRECEDING) as week_stddev
    FROM daily_error_trends
)
SELECT 
    error_date,
    root_cause_category,
    pattern_id,
    error_fingerprint,
    daily_count,
    prev_day_count,
    CASE 
        WHEN prev_day_count IS NULL THEN 'NEW_PATTERN'
        WHEN daily_count > prev_day_count THEN 'INCREASING'
        WHEN daily_count < prev_day_count THEN 'DECREASING' 
        ELSE 'STABLE'
    END as daily_trend,
    CASE
        WHEN week_ago_count IS NULL THEN 'INSUFFICIENT_DATA'
        WHEN daily_count > week_avg + week_stddev THEN 'SPIKE'
        WHEN daily_count < week_avg - week_stddev THEN 'DROP'
        ELSE 'NORMAL'
    END as weekly_trend_status,
    ROUND((daily_count - week_avg) / NULLIF(week_stddev, 0), 2) as z_score
FROM trend_analysis
WHERE error_date >= DATE '2024-01-01'
ORDER BY error_date DESC, daily_count DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🔄 2. CYCLICAL PATTERN DETECTION
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Identify errors that occur in regular cycles (weekly builds, daily deployments)

WITH error_timestamps AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        ROW_NUMBER() OVER (PARTITION BY pattern_id ORDER BY file_index) as occurrence_sequence,
        file_index,
        -- Simulate timestamps based on file processing order
        DATE '2024-01-01' + INTERVAL (file_index * 12) HOUR as estimated_timestamp
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE pattern_id IS NOT NULL
),
interval_analysis AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        occurrence_sequence,
        estimated_timestamp,
        LAG(estimated_timestamp) OVER (PARTITION BY pattern_id ORDER BY occurrence_sequence) as prev_timestamp,
        EXTRACT(EPOCH FROM (estimated_timestamp - LAG(estimated_timestamp) OVER (PARTITION BY pattern_id ORDER BY occurrence_sequence))) / 3600 as hours_since_last
    FROM error_timestamps
),
cyclical_patterns AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        COUNT(*) as total_occurrences,
        AVG(hours_since_last) as avg_interval_hours,
        STDDEV(hours_since_last) as interval_stddev,
        MIN(hours_since_last) as min_interval,
        MAX(hours_since_last) as max_interval,
        CASE 
            WHEN AVG(hours_since_last) BETWEEN 20 AND 28 THEN 'DAILY'
            WHEN AVG(hours_since_last) BETWEEN 160 AND 200 THEN 'WEEKLY'  
            WHEN AVG(hours_since_last) BETWEEN 700 AND 800 THEN 'MONTHLY'
            WHEN STDDEV(hours_since_last) / NULLIF(AVG(hours_since_last), 0) < 0.3 THEN 'REGULAR'
            ELSE 'IRREGULAR'
        END as cycle_type
    FROM interval_analysis
    WHERE hours_since_last IS NOT NULL
    GROUP BY pattern_id, error_fingerprint, root_cause_category
    HAVING COUNT(*) >= 3
)
SELECT *,
    ROUND(interval_stddev / NULLIF(avg_interval_hours, 0), 3) as cycle_consistency_ratio,
    CASE 
        WHEN cycle_type != 'IRREGULAR' AND interval_stddev / NULLIF(avg_interval_hours, 0) < 0.2 THEN 'HIGHLY_PREDICTABLE'
        WHEN cycle_type != 'IRREGULAR' AND interval_stddev / NULLIF(avg_interval_hours, 0) < 0.5 THEN 'MODERATELY_PREDICTABLE'
        ELSE 'UNPREDICTABLE'
    END as predictability
FROM cyclical_patterns
ORDER BY total_occurrences DESC, cycle_consistency_ratio ASC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 📊 3. ERROR PATTERN EVOLUTION ANALYSIS
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Track how specific error patterns change over time and environments

WITH pattern_evolution AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        environment,
        build_id,
        file_index,
        COUNT(*) as occurrence_count,
        AVG(similarity_score) as avg_similarity,
        MIN(similarity_score) as min_similarity,
        MAX(similarity_score) as max_similarity,
        -- Pattern stability score (higher = more consistent)
        1.0 - (STDDEV(similarity_score) / NULLIF(AVG(similarity_score), 0)) as stability_score
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE pattern_id IS NOT NULL AND similarity_score > 0
    GROUP BY pattern_id, error_fingerprint, root_cause_category, environment, build_id, file_index
),
pattern_maturity AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        COUNT(DISTINCT COALESCE(build_id, 'unknown')) as builds_affected,
        COUNT(DISTINCT COALESCE(environment, 'unknown')) as environments_affected,
        SUM(occurrence_count) as total_occurrences,
        AVG(stability_score) as avg_stability,
        MIN(file_index) as first_seen_index,
        MAX(file_index) as last_seen_index,
        MAX(file_index) - MIN(file_index) as pattern_lifespan
    FROM pattern_evolution
    GROUP BY pattern_id, error_fingerprint, root_cause_category
)
SELECT *,
    CASE 
        WHEN pattern_lifespan = 0 THEN 'ISOLATED'
        WHEN pattern_lifespan <= 2 THEN 'SHORT_LIVED'
        WHEN pattern_lifespan <= 5 THEN 'MEDIUM_TERM'
        ELSE 'PERSISTENT'
    END as pattern_duration_category,
    CASE
        WHEN environments_affected = 1 AND builds_affected = 1 THEN 'ISOLATED_INCIDENT'
        WHEN environments_affected = 1 AND builds_affected > 1 THEN 'BUILD_SPECIFIC'
        WHEN environments_affected > 1 AND builds_affected = 1 THEN 'ENVIRONMENT_CROSSING'
        WHEN environments_affected > 1 AND builds_affected > 1 THEN 'WIDESPREAD'
        ELSE 'UNKNOWN'
    END as impact_scope,
    ROUND(total_occurrences * avg_stability * LOG(1 + pattern_lifespan), 2) as pattern_severity_score
FROM pattern_maturity
ORDER BY pattern_severity_score DESC, total_occurrences DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🎯 4. TIME-BASED ROOT CAUSE TRENDING  
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Analyze how different types of errors trend over time

WITH root_cause_timeline AS (
    SELECT 
        CASE 
            WHEN build_id ~ '^build-\d+$' THEN 
                DATE '2024-01-01' + INTERVAL (CAST(REGEXP_EXTRACT(build_id, '\d+') AS INTEGER)) DAY
            ELSE DATE '2024-01-01' + INTERVAL (file_index) DAY
        END as time_period,
        root_cause_category,
        COUNT(*) as error_count,
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failure_count,
        COUNT(DISTINCT pattern_id) as unique_patterns,
        COUNT(DISTINCT error_fingerprint) as unique_fingerprints
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE root_cause_category IS NOT NULL
    GROUP BY time_period, root_cause_category
),
trending_analysis AS (
    SELECT *,
        LAG(error_count) OVER (PARTITION BY root_cause_category ORDER BY time_period) as prev_error_count,
        AVG(error_count) OVER (PARTITION BY root_cause_category ORDER BY time_period ROWS 2 PRECEDING) as rolling_avg,
        ROW_NUMBER() OVER (PARTITION BY root_cause_category ORDER BY time_period) as period_sequence
    FROM root_cause_timeline
)
SELECT 
    time_period,
    root_cause_category,
    error_count,
    failure_count,
    unique_patterns,
    unique_fingerprints,
    prev_error_count,
    rolling_avg,
    CASE 
        WHEN prev_error_count IS NULL THEN 'FIRST_OCCURRENCE'
        WHEN error_count > prev_error_count * 1.5 THEN 'SIGNIFICANT_INCREASE'
        WHEN error_count > prev_error_count THEN 'INCREASING'
        WHEN error_count < prev_error_count * 0.5 THEN 'SIGNIFICANT_DECREASE'
        WHEN error_count < prev_error_count THEN 'DECREASING'
        ELSE 'STABLE'
    END as trend_direction,
    ROUND((error_count - rolling_avg) / NULLIF(rolling_avg, 0) * 100, 1) as deviation_from_average_pct,
    ROUND(failure_count::DECIMAL / NULLIF(error_count, 0) * 100, 1) as failure_rate_pct
FROM trending_analysis
WHERE period_sequence >= 1
ORDER BY time_period DESC, error_count DESC;

-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 💡 USAGE EXAMPLES:
-- ═══════════════════════════════════════════════════════════════════════════════════════
/*

1. Find patterns with concerning upward trends:
   SELECT * FROM (above query 1) WHERE daily_trend = 'INCREASING' AND weekly_trend_status = 'SPIKE';

2. Identify highly predictable cyclical errors for scheduling maintenance:
   SELECT * FROM (above query 2) WHERE predictability = 'HIGHLY_PREDICTABLE';

3. Track persistent widespread issues needing urgent attention:
   SELECT * FROM (above query 3) WHERE impact_scope = 'WIDESPREAD' AND pattern_duration_category = 'PERSISTENT';

4. Monitor root cause category trends for process improvements:
   SELECT * FROM (above query 4) WHERE trend_direction IN ('INCREASING', 'SIGNIFICANT_INCREASE');

*/