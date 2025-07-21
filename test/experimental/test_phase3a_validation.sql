-- Phase 3A Comprehensive Test Suite
LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- Test 1: Single file processing with metadata (backward compatibility)
.print "Test 1: Single file processing with metadata"
SELECT 'Test 1.1: Basic metadata' as test_name,
       source_file, build_id, environment, file_index, COUNT(*) as events
FROM read_test_results('workspace/ansible_sample.txt', 'auto') 
GROUP BY source_file, build_id, environment, file_index;

-- Test 2: Multi-file glob pattern processing
.print "Test 2: Multi-file glob pattern processing"
SELECT 'Test 2.1: Build ID extraction' as test_name,
       build_id, file_index, COUNT(*) as events
FROM read_test_results('workspace/builds/**/*.txt', 'auto') 
GROUP BY build_id, file_index
ORDER BY file_index;

-- Test 3: Environment detection from file paths
.print "Test 3: Environment detection"
SELECT 'Test 3.1: Environment analysis' as test_name,
       environment, 
       COUNT(DISTINCT source_file) as files_processed,
       COUNT(*) as total_events,
       COUNT(CASE WHEN status = 'FAIL' THEN 1 END) as failures,
       COUNT(CASE WHEN status = 'PASS' THEN 1 END) as passes
FROM read_test_results('workspace/ci-logs/**/*.txt', 'pytest_text')
WHERE environment IS NOT NULL
GROUP BY environment
ORDER BY environment;

-- Test 4: Pipeline aggregation capabilities
.print "Test 4: Pipeline aggregation by build ID"
WITH pipeline_summary AS (
    SELECT 
        build_id,
        COUNT(DISTINCT source_file) as files_processed,
        COUNT(*) as total_events,
        COUNT(CASE WHEN status = 'FAIL' THEN 1 END) as failures,
        COUNT(CASE WHEN status = 'PASS' THEN 1 END) as passes
    FROM read_test_results('workspace/builds/**/*.txt', 'auto')
    WHERE build_id IS NOT NULL
    GROUP BY build_id
)
SELECT 'Test 4.1: Pipeline health' as test_name,
    build_id,
    files_processed,
    total_events,
    failures,
    passes,
    CASE 
        WHEN failures = 0 THEN 'SUCCESS'
        WHEN failures > 0 AND passes > failures THEN 'MOSTLY_PASS'
        ELSE 'FAILURE'
    END as pipeline_status
FROM pipeline_summary
ORDER BY build_id;

-- Test 5: File processing sequence validation
.print "Test 5: File index ordering validation"
SELECT 'Test 5.1: Processing order' as test_name,
       file_index, 
       CASE WHEN source_file LIKE '%terraform%' THEN 'terraform' 
            WHEN source_file LIKE '%ansible%' THEN 'ansible' 
            ELSE 'other' END as file_type,
       COUNT(*) as events
FROM read_test_results('workspace/builds/**/*.txt', 'auto')
GROUP BY file_index, source_file
ORDER BY file_index;

-- Test 6: Schema validation
.print "Test 6: New schema columns validation"
SELECT 'Test 6.1: All columns present' as test_name,
       COUNT(DISTINCT source_file) > 0 as has_source_file,
       COUNT(DISTINCT COALESCE(build_id, 'null')) >= 1 as has_build_id,
       COUNT(DISTINCT COALESCE(environment, 'null')) >= 1 as has_environment,
       COUNT(DISTINCT file_index) >= 1 as has_file_index
FROM read_test_results('workspace/ci-logs/staging/pytest-results.txt', 'pytest_text');

.print "✅ Phase 3A test suite completed!"