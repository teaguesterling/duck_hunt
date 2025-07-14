-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 🔗 FAILURE CORRELATION ANALYSIS: Cross-Build Error Relationship Discovery
-- ═══════════════════════════════════════════════════════════════════════════════════════
-- Discover how errors relate to each other across builds, environments, and time
-- Load the Duck Hunt extension first: LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🎯 1. ERROR CO-OCCURRENCE ANALYSIS
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Find error patterns that frequently occur together in the same build/environment

WITH error_cooccurrence AS (
    SELECT 
        a.pattern_id as pattern_a,
        a.error_fingerprint as fingerprint_a,
        a.root_cause_category as category_a,
        b.pattern_id as pattern_b,
        b.error_fingerprint as fingerprint_b,
        b.root_cause_category as category_b,
        COUNT(*) as cooccurrence_count,
        COUNT(DISTINCT COALESCE(a.build_id, a.source_file)) as builds_affected,
        COUNT(DISTINCT COALESCE(a.environment, 'unknown')) as environments_affected
    FROM read_test_results('workspace/**/*.txt', 'auto') a
    JOIN read_test_results('workspace/**/*.txt', 'auto') b 
        ON COALESCE(a.build_id, a.source_file) = COALESCE(b.build_id, b.source_file)
        AND COALESCE(a.environment, 'unknown') = COALESCE(b.environment, 'unknown')
        AND a.pattern_id != b.pattern_id
        AND a.pattern_id < b.pattern_id  -- Avoid duplicate pairs
    WHERE a.pattern_id IS NOT NULL AND b.pattern_id IS NOT NULL
    GROUP BY 1, 2, 3, 4, 5, 6
    HAVING COUNT(*) >= 2  -- Only patterns that co-occur multiple times
),
correlation_metrics AS (
    SELECT *,
        -- Calculate correlation strength
        ROUND(
            cooccurrence_count::DECIMAL / 
            (builds_affected + environments_affected), 
            3
        ) as correlation_strength,
        -- Determine relationship type
        CASE 
            WHEN category_a = category_b THEN 'SAME_CATEGORY'
            WHEN category_a IN ('network', 'permission') AND category_b IN ('network', 'permission') THEN 'INFRASTRUCTURE'
            WHEN category_a IN ('build', 'syntax') AND category_b IN ('build', 'syntax') THEN 'DEVELOPMENT'
            WHEN category_a IN ('configuration', 'resource') AND category_b IN ('configuration', 'resource') THEN 'DEPLOYMENT'
            ELSE 'CROSS_CATEGORY'
        END as relationship_type
    FROM error_cooccurrence
)
SELECT *,
    CASE 
        WHEN correlation_strength >= 0.8 THEN 'VERY_STRONG'
        WHEN correlation_strength >= 0.6 THEN 'STRONG' 
        WHEN correlation_strength >= 0.4 THEN 'MODERATE'
        WHEN correlation_strength >= 0.2 THEN 'WEAK'
        ELSE 'VERY_WEAK'
    END as correlation_category,
    -- Practical interpretation
    CASE 
        WHEN correlation_strength >= 0.7 AND relationship_type = 'SAME_CATEGORY' THEN 'LIKELY_SAME_ROOT_CAUSE'
        WHEN correlation_strength >= 0.6 AND relationship_type != 'SAME_CATEGORY' THEN 'CASCADE_EFFECT'
        WHEN correlation_strength >= 0.4 THEN 'RELATED_ISSUES'
        ELSE 'COINCIDENTAL'
    END as likely_relationship
FROM correlation_metrics
ORDER BY correlation_strength DESC, cooccurrence_count DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- ⚡ 2. FAILURE CASCADE DETECTION
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Identify errors that trigger subsequent failures (temporal causation analysis)

WITH temporal_sequence AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        build_id,
        environment,
        file_index,
        source_file,
        -- Create sequence within each build/environment
        ROW_NUMBER() OVER (
            PARTITION BY COALESCE(build_id, environment, 'global') 
            ORDER BY file_index, event_id
        ) as sequence_order
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE pattern_id IS NOT NULL AND status IN ('ERROR', 'FAIL')
),
cascade_analysis AS (
    SELECT 
        a.pattern_id as trigger_pattern,
        a.error_fingerprint as trigger_fingerprint,
        a.root_cause_category as trigger_category,
        b.pattern_id as subsequent_pattern,
        b.error_fingerprint as subsequent_fingerprint,
        b.root_cause_category as subsequent_category,
        b.sequence_order - a.sequence_order as sequence_gap,
        COUNT(*) as cascade_occurrences,
        COUNT(DISTINCT COALESCE(a.build_id, a.environment)) as builds_with_cascade,
        AVG(b.sequence_order - a.sequence_order) as avg_sequence_gap
    FROM temporal_sequence a
    JOIN temporal_sequence b 
        ON COALESCE(a.build_id, a.environment, 'global') = COALESCE(b.build_id, b.environment, 'global')
        AND b.sequence_order > a.sequence_order
        AND b.sequence_order - a.sequence_order <= 5  -- Look for cascades within 5 events
        AND a.pattern_id != b.pattern_id
    GROUP BY 1, 2, 3, 4, 5, 6, 7
    HAVING COUNT(*) >= 2
)
SELECT *,
    ROUND(cascade_occurrences::DECIMAL / builds_with_cascade, 2) as cascade_frequency,
    CASE 
        WHEN sequence_gap = 1 THEN 'IMMEDIATE_CASCADE'
        WHEN sequence_gap <= 2 THEN 'NEAR_IMMEDIATE'
        WHEN sequence_gap <= 3 THEN 'SHORT_DELAY'
        ELSE 'DELAYED_CASCADE'
    END as cascade_timing,
    CASE 
        WHEN cascade_occurrences >= 5 AND avg_sequence_gap <= 2 THEN 'STRONG_CAUSAL_LINK'
        WHEN cascade_occurrences >= 3 AND avg_sequence_gap <= 3 THEN 'LIKELY_CAUSAL_LINK'
        WHEN cascade_occurrences >= 2 THEN 'POSSIBLE_CAUSAL_LINK'
        ELSE 'WEAK_CORRELATION'
    END as causation_strength
FROM cascade_analysis
ORDER BY cascade_occurrences DESC, avg_sequence_gap ASC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 🌐 3. CROSS-ENVIRONMENT ERROR PROPAGATION
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Track how errors spread from one environment to another

WITH environment_error_flow AS (
    SELECT 
        pattern_id,
        error_fingerprint,
        root_cause_category,
        environment,
        COUNT(*) as occurrences_in_env,
        MIN(file_index) as first_occurrence_index,
        MAX(file_index) as last_occurrence_index,
        COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failures_in_env
    FROM read_test_results('workspace/**/*.txt', 'auto')
    WHERE pattern_id IS NOT NULL AND environment IS NOT NULL
    GROUP BY pattern_id, error_fingerprint, root_cause_category, environment
),
cross_env_analysis AS (
    SELECT 
        dev.pattern_id,
        dev.error_fingerprint,
        dev.root_cause_category,
        dev.occurrences_in_env as dev_occurrences,
        dev.first_occurrence_index as dev_first_seen,
        staging.occurrences_in_env as staging_occurrences,
        staging.first_occurrence_index as staging_first_seen,
        prod.occurrences_in_env as prod_occurrences,
        prod.first_occurrence_index as prod_first_seen,
        CASE 
            WHEN dev.pattern_id IS NOT NULL AND staging.pattern_id IS NOT NULL AND prod.pattern_id IS NOT NULL 
            THEN 'ALL_ENVIRONMENTS'
            WHEN dev.pattern_id IS NOT NULL AND staging.pattern_id IS NOT NULL 
            THEN 'DEV_TO_STAGING'
            WHEN staging.pattern_id IS NOT NULL AND prod.pattern_id IS NOT NULL 
            THEN 'STAGING_TO_PROD'
            WHEN dev.pattern_id IS NOT NULL AND prod.pattern_id IS NOT NULL 
            THEN 'DEV_TO_PROD_SKIP'
            WHEN dev.pattern_id IS NOT NULL 
            THEN 'DEV_ONLY'
            WHEN staging.pattern_id IS NOT NULL 
            THEN 'STAGING_ONLY'
            WHEN prod.pattern_id IS NOT NULL 
            THEN 'PROD_ONLY'
        END as propagation_pattern
    FROM 
        (SELECT * FROM environment_error_flow WHERE environment = 'dev') dev
    FULL OUTER JOIN 
        (SELECT * FROM environment_error_flow WHERE environment = 'staging') staging
        ON dev.pattern_id = staging.pattern_id
    FULL OUTER JOIN 
        (SELECT * FROM environment_error_flow WHERE environment = 'prod') prod
        ON COALESCE(dev.pattern_id, staging.pattern_id) = prod.pattern_id
)
SELECT 
    COALESCE(pattern_id) as pattern_id,
    COALESCE(error_fingerprint) as error_fingerprint,
    COALESCE(root_cause_category) as root_cause_category,
    propagation_pattern,
    COALESCE(dev_occurrences, 0) as dev_occurrences,
    COALESCE(staging_occurrences, 0) as staging_occurrences,
    COALESCE(prod_occurrences, 0) as prod_occurrences,
    dev_first_seen,
    staging_first_seen,
    prod_first_seen,
    -- Calculate propagation timing
    CASE 
        WHEN dev_first_seen IS NOT NULL AND staging_first_seen IS NOT NULL 
        THEN staging_first_seen - dev_first_seen 
    END as dev_to_staging_delay,
    CASE 
        WHEN staging_first_seen IS NOT NULL AND prod_first_seen IS NOT NULL 
        THEN prod_first_seen - staging_first_seen 
    END as staging_to_prod_delay,
    -- Risk assessment
    CASE 
        WHEN propagation_pattern = 'ALL_ENVIRONMENTS' THEN 'HIGH_RISK'
        WHEN propagation_pattern IN ('STAGING_TO_PROD', 'DEV_TO_PROD_SKIP') THEN 'MEDIUM_RISK'
        WHEN propagation_pattern = 'PROD_ONLY' THEN 'CRITICAL_INVESTIGATION'
        WHEN propagation_pattern IN ('DEV_TO_STAGING', 'STAGING_ONLY') THEN 'CONTAINED'
        ELSE 'LOW_RISK'
    END as risk_level
FROM cross_env_analysis
ORDER BY 
    CASE propagation_pattern 
        WHEN 'PROD_ONLY' THEN 1
        WHEN 'ALL_ENVIRONMENTS' THEN 2
        WHEN 'STAGING_TO_PROD' THEN 3
        ELSE 4 
    END,
    COALESCE(prod_occurrences, staging_occurrences, dev_occurrences, 0) DESC;

-- ───────────────────────────────────────────────────────────────────────────────────────
-- 📊 4. ROOT CAUSE CORRELATION MATRIX
-- ───────────────────────────────────────────────────────────────────────────────────────
-- Create a correlation matrix showing how different root cause categories relate

WITH root_cause_pairs AS (
    SELECT 
        a.root_cause_category as cause_a,
        b.root_cause_category as cause_b,
        COUNT(*) as cooccurrence_count,
        COUNT(DISTINCT COALESCE(a.build_id, a.source_file)) as shared_contexts
    FROM read_test_results('workspace/**/*.txt', 'auto') a
    JOIN read_test_results('workspace/**/*.txt', 'auto') b 
        ON COALESCE(a.build_id, a.source_file) = COALESCE(b.build_id, b.source_file)
        AND a.root_cause_category != b.root_cause_category
    WHERE a.root_cause_category IS NOT NULL AND b.root_cause_category IS NOT NULL
    GROUP BY a.root_cause_category, b.root_cause_category
),
normalized_correlations AS (
    SELECT 
        cause_a,
        cause_b,
        cooccurrence_count,
        shared_contexts,
        ROUND(
            cooccurrence_count::DECIMAL / shared_contexts, 
            3
        ) as correlation_coefficient,
        -- Get total occurrences for each cause for normalization
        (SELECT COUNT(*) FROM read_test_results('workspace/**/*.txt', 'auto') 
         WHERE root_cause_category = cause_a) as total_a,
        (SELECT COUNT(*) FROM read_test_results('workspace/**/*.txt', 'auto') 
         WHERE root_cause_category = cause_b) as total_b
    FROM root_cause_pairs
)
SELECT 
    cause_a,
    cause_b,
    cooccurrence_count,
    shared_contexts,
    correlation_coefficient,
    ROUND(
        (cooccurrence_count::DECIMAL * 2) / (total_a + total_b),
        3
    ) as jaccard_similarity,
    CASE 
        WHEN correlation_coefficient >= 0.5 THEN 'STRONGLY_CORRELATED'
        WHEN correlation_coefficient >= 0.3 THEN 'MODERATELY_CORRELATED'
        WHEN correlation_coefficient >= 0.1 THEN 'WEAKLY_CORRELATED'
        ELSE 'UNCORRELATED'
    END as correlation_strength,
    -- Practical insights
    CASE 
        WHEN cause_a = 'configuration' AND cause_b = 'network' THEN 'Config issues may cause network problems'
        WHEN cause_a = 'permission' AND cause_b = 'configuration' THEN 'Access control and config often related'
        WHEN cause_a = 'build' AND cause_b = 'syntax' THEN 'Build failures often due to syntax issues'
        WHEN cause_a = 'network' AND cause_b = 'resource' THEN 'Network issues may indicate resource constraints'
        ELSE 'Review for operational patterns'
    END as interpretation
FROM normalized_correlations
WHERE correlation_coefficient >= 0.1  -- Only show meaningful correlations
ORDER BY correlation_coefficient DESC;

-- ═══════════════════════════════════════════════════════════════════════════════════════
-- 💡 USAGE EXAMPLES:
-- ═══════════════════════════════════════════════════════════════════════════════════════
/*

1. Find error pairs that always occur together (potential same root cause):
   SELECT * FROM (query 1) WHERE likely_relationship = 'LIKELY_SAME_ROOT_CAUSE';

2. Identify strong failure cascades for prevention:
   SELECT * FROM (query 2) WHERE causation_strength = 'STRONG_CAUSAL_LINK';

3. Track high-risk environment propagation patterns:
   SELECT * FROM (query 3) WHERE risk_level IN ('HIGH_RISK', 'CRITICAL_INVESTIGATION');

4. Understand systemic issues across error categories:
   SELECT * FROM (query 4) WHERE correlation_strength = 'STRONGLY_CORRELATED';

5. Plan prevention strategies based on cascade patterns:
   SELECT trigger_pattern, trigger_category, COUNT(DISTINCT subsequent_pattern) as cascade_breadth
   FROM (query 2) GROUP BY 1, 2 ORDER BY cascade_breadth DESC;

*/