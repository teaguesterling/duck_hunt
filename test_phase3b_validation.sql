-- Phase 3B: Error Pattern Analysis Validation Test Suite
LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- Test 1: Error Fingerprinting and Pattern Detection
.print "Test 1: Error Fingerprinting and Pattern Detection"
SELECT 'Test 1.1: Fingerprinting works' as test_name,
       COUNT(DISTINCT error_fingerprint) as unique_fingerprints,
       COUNT(*) as total_events,
       ROUND(100.0 * COUNT(DISTINCT error_fingerprint) / COUNT(*), 1) as fingerprint_diversity_pct
FROM read_test_results('workspace/builds/**/*.txt', 'auto')
WHERE error_fingerprint IS NOT NULL;

-- Test 2: Pattern Grouping Quality
.print "Test 2: Pattern Grouping Quality" 
WITH pattern_stats AS (
    SELECT pattern_id,
           COUNT(*) as events_in_pattern,
           COUNT(DISTINCT error_fingerprint) as fingerprints_in_pattern,
           AVG(similarity_score) as avg_similarity
    FROM read_test_results('workspace/builds/**/*.txt', 'auto')
    WHERE pattern_id IS NOT NULL
    GROUP BY pattern_id
)
SELECT 'Test 2.1: Pattern coherence' as test_name,
       COUNT(*) as total_patterns,
       COUNT(CASE WHEN fingerprints_in_pattern = 1 THEN 1 END) as coherent_patterns,
       ROUND(AVG(avg_similarity), 3) as overall_avg_similarity,
       MAX(events_in_pattern) as largest_pattern_size
FROM pattern_stats;

-- Test 3: Root Cause Detection Accuracy
.print "Test 3: Root Cause Detection Accuracy"
SELECT 'Test 3.1: Root cause distribution' as test_name,
       root_cause_category,
       COUNT(*) as events,
       COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) as failures,
       ROUND(100.0 * COUNT(CASE WHEN status IN ('ERROR', 'FAIL') THEN 1 END) / COUNT(*), 1) as failure_rate_pct
FROM read_test_results('workspace/builds/**/*.txt', 'auto')
WHERE root_cause_category IS NOT NULL
GROUP BY root_cause_category
ORDER BY failures DESC;

-- Test 4: Specific Pattern Examples
.print "Test 4: Specific Pattern Examples"
SELECT 'Test 4.1: Configuration errors' as test_name,
       pattern_id,
       error_fingerprint,
       similarity_score,
       message
FROM read_test_results('workspace/builds/**/*.txt', 'auto')
WHERE root_cause_category = 'configuration' AND status IN ('ERROR', 'FAIL')
ORDER BY pattern_id
LIMIT 3;

-- Test 5: Error Fingerprint Consistency
.print "Test 5: Error Fingerprint Consistency"
WITH fingerprint_consistency AS (
    SELECT error_fingerprint,
           COUNT(*) as occurrences,
           COUNT(DISTINCT tool_name) as tools,
           COUNT(DISTINCT category) as categories,
           COUNT(DISTINCT root_cause_category) as root_causes
    FROM read_test_results('workspace/builds/**/*.txt', 'auto')
    WHERE error_fingerprint IS NOT NULL
    GROUP BY error_fingerprint
    HAVING COUNT(*) > 1
)
SELECT 'Test 5.1: Fingerprint consistency' as test_name,
       COUNT(*) as multi_occurrence_fingerprints,
       COUNT(CASE WHEN root_causes = 1 THEN 1 END) as consistent_root_cause,
       ROUND(100.0 * COUNT(CASE WHEN root_causes = 1 THEN 1 END) / COUNT(*), 1) as consistency_pct
FROM fingerprint_consistency;

-- Test 6: Cross-Environment Pattern Analysis
.print "Test 6: Cross-Environment Pattern Analysis"
SELECT 'Test 6.1: Environment error distribution' as test_name,
       environment,
       root_cause_category,
       COUNT(*) as events,
       COUNT(DISTINCT pattern_id) as unique_patterns
FROM read_test_results('workspace/ci-logs/**/*.txt', 'pytest_text')
WHERE environment IS NOT NULL AND root_cause_category IS NOT NULL
GROUP BY environment, root_cause_category
ORDER BY environment, events DESC;

-- Test 7: Schema Validation
.print "Test 7: Schema Validation"
SELECT 'Test 7.1: All new columns present' as test_name,
       COUNT(DISTINCT error_fingerprint) > 0 as has_fingerprints,
       COUNT(DISTINCT COALESCE(pattern_id, -999)) > 1 as has_pattern_ids,
       COUNT(DISTINCT COALESCE(similarity_score, -1)) > 1 as has_similarity_scores,
       COUNT(DISTINCT root_cause_category) > 1 as has_root_causes
FROM read_test_results('workspace/builds/**/*.txt', 'auto');

.print "✅ Phase 3B.1 error pattern analysis test suite completed!"