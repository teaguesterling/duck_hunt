-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 🔮 PREDICTIVE ANALYTICS: Future Failure Prediction and Risk Assessment
-- ═══════════════════════════════════════════════════════════════════════════════════════
-- Predict when and where errors are likely to occur using historical patterns
-- Load the Duck Hunt extension first: LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 📈 1. ERROR RECURRENCE PREDICTION
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Predict when specific error patterns are likely to recur based on historical intervals

WITH pattern_intervals AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        file_index,
        LAG(file_index) OVER (PARTITION BY pattern_id ORDER BY file_index) as prev_occurrence,
        file_index - LAG(file_index) OVER (PARTITION BY pattern_id ORDER BY file_index) as interval_gap
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE pattern_id IS NOT NULL
),
pattern_statistics AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        COUNT(*) as total_occurrences,
        COUNT(interval_gap) as intervals_count,
        AVG(interval_gap) as avg_interval,
        STDDEV(interval_gap) as interval_stddev,
        MIN(interval_gap) as min_interval,
        MAX(interval_gap) as max_interval,
        MAX(file_index) as last_occurrence,
        MEDIAN(interval_gap) as median_interval
    FROM pattern_intervals
    WHERE interval_gap IS NOT NULL
    GROUP BY pattern_id, error_fingerprint, root_cause_category
    HAVING COUNT(interval_gap) >= 2  -- Need at least 2 intervals for prediction
),
predictions AS (
    SELECT *,
        -- Predict next occurrence based on average interval
        last_occurrence + avg_interval as predicted_next_avg,
        -- Predict based on median (more robust to outliers)
        last_occurrence + median_interval as predicted_next_median,
        -- Calculate prediction confidence
        CASE 
            WHEN interval_stddev / NULLIF(avg_interval, 0) <= 0.2 THEN 'HIGH'
            WHEN interval_stddev / NULLIF(avg_interval, 0) <= 0.5 THEN 'MEDIUM'
            ELSE 'LOW'
        END as prediction_confidence,
        -- Calculate urgency based on predicted timing
        CASE 
            WHEN last_occurrence + avg_interval <= (SELECT MAX(file_index) FROM read_test_results('workspace/**/*.txt', 'auto')) + 2 
            THEN 'IMMINENT'
            WHEN last_occurrence + avg_interval <= (SELECT MAX(file_index) FROM read_test_results('workspace/**/*.txt', 'auto')) + 5 
            THEN 'NEAR_TERM'
            ELSE 'FUTURE'
        END as urgency_level,
        -- Risk score combining frequency and predictability
        ROUND(
            total_occurrences * 
            (1.0 / GREATEST(interval_stddev / NULLIF(avg_interval, 0), 0.1)) *
            CASE 
                WHEN root_cause_category IN ('network', 'permission') THEN 1.5  -- Infrastructure issues are higher priority
                WHEN root_cause_category = 'configuration' THEN 1.3
                ELSE 1.0
            END,
            2
        ) as risk_score
    FROM pattern_statistics
)
SELECT *,
    CASE 
        WHEN risk_score >= 20 AND prediction_confidence = 'HIGH' THEN 'CRITICAL_WATCH'
        WHEN risk_score >= 15 AND prediction_confidence IN ('HIGH', 'MEDIUM') THEN 'HIGH_PRIORITY'
        WHEN risk_score >= 10 THEN 'MONITOR_CLOSELY'
        WHEN risk_score >= 5 THEN 'ROUTINE_MONITORING'
        ELSE 'LOW_PRIORITY'
    END as recommendation,
    -- Days until predicted occurrence (assuming 1 file_index = 0.5 days)
    ROUND((predicted_next_median - (SELECT MAX(file_index) FROM read_test_results('workspace/**/*.txt', 'auto'))) * 0.5, 1) as days_until_predicted
FROM predictions
ORDER BY risk_score DESC, prediction_confidence DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🎯 2. ENVIRONMENT RISK ASSESSMENT
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Assess the risk level of different environments based on error patterns

WITH environment_metrics AS (
    SELECT 
        COALESCE(environment, 'unknown') as environment,
        root_cause_category,
        COUNT(*) as total_events,
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failure_events,
        COUNT(DISTINCT pattern_id) as unique_patterns,
        COUNT(DISTINCT error_fingerprint) as unique_fingerprints,
        AVG(similarity_score) as avg_pattern_similarity,
        -- Calculate error density (errors per unique pattern)
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / 
        NULLIF(COUNT(DISTINCT pattern_id), 0) as error_density
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE pattern_id IS NOT NULL
    GROUP BY environment, root_cause_category
),
environment_risk_scores AS (
    SELECT 
        environment,
        SUM(failure_events) as total_failures,
        SUM(total_events) as total_events,
        COUNT(DISTINCT root_cause_category) as categories_affected,
        SUM(unique_patterns) as total_unique_patterns,
        ROUND(SUM(failure_events)::DECIMAL / NULLIF(SUM(total_events), 0) * 100, 1) as failure_rate_pct,
        ROUND(AVG(error_density), 2) as avg_error_density,
        ROUND(AVG(avg_pattern_similarity), 3) as avg_similarity,
        -- Weight different factors for overall risk score
        ROUND(
            (SUM(failure_events) * 2.0) +                           -- Failure count (heavily weighted)
            (categories_affected * 5.0) +                          -- Diversity of problems
            (AVG(error_density) * 10.0) +                         -- Error concentration
            ((SUM(failure_events)::DECIMAL / NULLIF(SUM(total_events), 0)) * 50.0)  -- Failure rate
        , 1) as risk_score
    FROM environment_metrics
    GROUP BY environment
),
risk_comparison AS (
    SELECT *,
        RANK() OVER (ORDER BY risk_score DESC) as risk_rank,
        CASE 
            WHEN risk_score >= 80 THEN 'CRITICAL'
            WHEN risk_score >= 60 THEN 'HIGH'
            WHEN risk_score >= 40 THEN 'MEDIUM'
            WHEN risk_score >= 20 THEN 'LOW'
            ELSE 'MINIMAL'
        END as risk_level,
        CASE 
            WHEN failure_rate_pct >= 20 THEN 'UNSTABLE'
            WHEN failure_rate_pct >= 10 THEN 'CONCERNING'
            WHEN failure_rate_pct >= 5 THEN 'ACCEPTABLE'
            ELSE 'STABLE'
        END as stability_assessment
    FROM environment_risk_scores
)
SELECT *,
    CASE 
        WHEN risk_level = 'CRITICAL' THEN 'Immediate investigation required - deployment freeze recommended'
        WHEN risk_level = 'HIGH' THEN 'Enhanced monitoring and caution for deployments'
        WHEN risk_level = 'MEDIUM' THEN 'Regular monitoring with proactive issue resolution'
        WHEN risk_level = 'LOW' THEN 'Standard monitoring procedures'
        ELSE 'Minimal oversight required'
    END as recommended_action
FROM risk_comparison
ORDER BY risk_score DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 📊 3. DEPLOYMENT READINESS PREDICTION
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Predict deployment success based on current error patterns and historical data

WITH recent_patterns AS (
    -- Look at the most recent patterns (last 30% of data)
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        environment,
        COUNT(*) as recent_occurrences,
        AVG(similarity_score) as avg_similarity,
        MAX(file_index) as last_seen
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE file_index >= (SELECT MAX(file_index) * 0.7 FROM read_test_results('workspace/**/*.txt', 'auto'))
      AND pattern_id IS NOT NULL
    GROUP BY pattern_id, error_fingerprint, root_cause_category, environment
),
historical_deployment_impact AS (
    -- Analyze how these patterns affected past deployments
    SELECT 
        rp.pattern_id,
        rp.error_fingerprint,
        rp.root_cause_category,
        rp.environment,
        rp.recent_occurrences,
        -- Count historical failures in similar contexts
        COUNT(CASE WHEN hist.status IN ('ERROR', 'FAIL') THEN 1 END) as historical_failures,
        COUNT(*) as historical_total,
        ROUND(
            COUNT(CASE WHEN hist.status IN ('ERROR', 'FAIL') THEN 1 END)::DECIMAL / 
            NULLIF(COUNT(*), 0) * 100, 
            1
        ) as historical_failure_rate
    FROM recent_patterns rp
    LEFT JOIN read_test_results('workspace/**/*.txt', 'auto') hist
        ON rp.pattern_id = hist.pattern_id
    GROUP BY rp.pattern_id, rp.error_fingerprint, rp.root_cause_category, rp.environment, rp.recent_occurrences
),
deployment_risk_assessment AS (
    SELECT 
        COALESCE(environment, 'all_environments') as target_environment,
        COUNT(*) as active_patterns,
        SUM(recent_occurrences) as total_recent_activity,
        AVG(historical_failure_rate) as avg_historical_failure_rate,
        COUNT(CASE WHEN historical_failure_rate >= 50 THEN 1 END) as high_risk_patterns,
        COUNT(CASE WHEN root_cause_category IN ('network', 'permission', 'configuration') THEN 1 END) as infrastructure_patterns,
        -- Calculate overall deployment risk
        ROUND(
            (AVG(historical_failure_rate) * 0.4) +                 -- Historical performance (40%)
            (COUNT(CASE WHEN historical_failure_rate >= 50 THEN 1 END) * 10.0) +  -- High-risk patterns (weight: 10 each)
            (COUNT(CASE WHEN root_cause_category IN ('network', 'permission', 'configuration') THEN 1 END) * 5.0) +  -- Infrastructure issues (weight: 5 each)
            (LN(SUM(recent_occurrences) + 1) * 2.0)               -- Recent activity level
        , 1) as deployment_risk_score
    FROM historical_deployment_impact
    GROUP BY COALESCE(environment, 'all_environments')
),
readiness_assessment AS (
    SELECT *,
        CASE 
            WHEN deployment_risk_score >= 60 THEN 'NOT_READY'
            WHEN deployment_risk_score >= 40 THEN 'CAUTION_ADVISED'
            WHEN deployment_risk_score >= 20 THEN 'READY_WITH_MONITORING'
            ELSE 'READY'
        END as readiness_status,
        CASE 
            WHEN deployment_risk_score >= 60 THEN 1  -- Delay deployment
            WHEN deployment_risk_score >= 40 THEN 7  -- Wait 1 week
            WHEN deployment_risk_score >= 20 THEN 3  -- Wait 3 days
            ELSE 0  -- Deploy now
        END as recommended_delay_days
    FROM deployment_risk_assessment
)
SELECT *,
    CASE readiness_status
        WHEN 'NOT_READY' THEN 'Resolve critical issues before deployment'
        WHEN 'CAUTION_ADVISED' THEN 'Deploy with enhanced monitoring and rollback plan'
        WHEN 'READY_WITH_MONITORING' THEN 'Deploy with standard monitoring'
        ELSE 'Deploy with confidence'
    END as deployment_recommendation
FROM readiness_assessment
ORDER BY deployment_risk_score DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🚨 4. ANOMALY DETECTION AND EARLY WARNING
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Detect unusual patterns that might indicate emerging issues

WITH pattern_baselines AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        -- Calculate baseline metrics from historical data (first 70% of timeline)
        COUNT(*) as baseline_occurrences,
        AVG(similarity_score) as baseline_similarity,
        COUNT(DISTINCT COALESCE(environment, 'unknown')) as baseline_env_spread,
        STDDEV(similarity_score) as baseline_similarity_stddev
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE file_index <= (SELECT MAX(file_index) * 0.7 FROM read_test_results('workspace/**/*.txt', 'auto'))
      AND pattern_id IS NOT NULL
    GROUP BY pattern_id, error_fingerprint, root_cause_category
    HAVING COUNT(*) >= 3  -- Need sufficient baseline data
),
recent_behavior AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        -- Calculate recent metrics (last 30% of timeline)
        COUNT(*) as recent_occurrences,
        AVG(similarity_score) as recent_similarity,
        COUNT(DISTINCT COALESCE(environment, 'unknown')) as recent_env_spread,
        STDDEV(similarity_score) as recent_similarity_stddev
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE file_index > (SELECT MAX(file_index) * 0.7 FROM read_test_results('workspace/**/*.txt', 'auto'))
      AND pattern_id IS NOT NULL
    GROUP BY pattern_id, error_fingerprint, root_cause_category
),
anomaly_detection AS (
    SELECT 
        b.pattern_id,
        b.error_fingerprint,
        b.root_cause_category,
        b.baseline_occurrences,
        COALESCE(r.recent_occurrences, 0) as recent_occurrences,
        b.baseline_similarity,
        COALESCE(r.recent_similarity, 0) as recent_similarity,
        b.baseline_env_spread,
        COALESCE(r.recent_env_spread, 0) as recent_env_spread,
        -- Calculate anomaly scores
        CASE 
            WHEN r.recent_occurrences IS NULL THEN 0  -- Pattern disappeared
            ELSE ABS(r.recent_occurrences - b.baseline_occurrences)::DECIMAL / 
                 GREATEST(b.baseline_occurrences, 1)
        END as frequency_anomaly_score,
        CASE 
            WHEN r.recent_similarity IS NULL THEN 0
            ELSE ABS(r.recent_similarity - b.baseline_similarity) / 
                 GREATEST(b.baseline_similarity, 0.1)
        END as similarity_anomaly_score,
        CASE 
            WHEN r.recent_env_spread IS NULL THEN 0
            ELSE ABS(r.recent_env_spread - b.baseline_env_spread)::DECIMAL / 
                 GREATEST(b.baseline_env_spread, 1)
        END as spread_anomaly_score
    FROM pattern_baselines b
    LEFT JOIN recent_behavior r ON b.pattern_id = r.pattern_id
),
anomaly_classification AS (
    SELECT *,
        frequency_anomaly_score + similarity_anomaly_score + spread_anomaly_score as total_anomaly_score,
        CASE 
            WHEN recent_occurrences = 0 THEN 'PATTERN_DISAPPEARED'
            WHEN frequency_anomaly_score >= 2.0 THEN 'FREQUENCY_SPIKE'
            WHEN similarity_anomaly_score >= 1.0 THEN 'PATTERN_DRIFT'
            WHEN spread_anomaly_score >= 1.0 THEN 'ENVIRONMENT_SPREAD'
            WHEN frequency_anomaly_score + similarity_anomaly_score + spread_anomaly_score >= 1.5 THEN 'COMBINED_ANOMALY'
            ELSE 'NORMAL'
        END as anomaly_type
    FROM anomaly_detection
)
SELECT *,
    CASE anomaly_type
        WHEN 'PATTERN_DISAPPEARED' THEN 'Good - Issue may be resolved, but monitor for return'
        WHEN 'FREQUENCY_SPIKE' THEN 'Critical - Investigate cause of increased error frequency'
        WHEN 'PATTERN_DRIFT' THEN 'Warning - Error characteristics changing, potential new issues'
        WHEN 'ENVIRONMENT_SPREAD' THEN 'Alert - Issue spreading across environments'
        WHEN 'COMBINED_ANOMALY' THEN 'Investigation needed - Multiple behavioral changes detected'
        ELSE 'No action required - Pattern behaving normally'
    END as recommended_response
FROM anomaly_classification
WHERE anomaly_type != 'NORMAL'
ORDER BY total_anomaly_score DESC;

-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 💡 USAGE EXAMPLES:
-- ═══════════════════════════════════════════════════════════════════════════════════════
/*

1. Critical patterns requiring immediate attention:
   SELECT * FROM (query 1) WHERE recommendation = 'CRITICAL_WATCH' AND urgency_level = 'IMMINENT';

2. Environments not ready for deployment:
   SELECT * FROM (query 3) WHERE readiness_status = 'NOT_READY';

3. Emerging issues requiring investigation:
   SELECT * FROM (query 4) WHERE anomaly_type IN ('FREQUENCY_SPIKE', 'COMBINED_ANOMALY');

4. High-risk environments needing intervention:
   SELECT * FROM (query 2) WHERE risk_level IN ('CRITICAL', 'HIGH');

5. Predict maintenance windows based on error cycles:
   SELECT pattern_id, predicted_next_median, prediction_confidence 
   FROM (query 1) WHERE prediction_confidence = 'HIGH' ORDER BY predicted_next_median;

*/