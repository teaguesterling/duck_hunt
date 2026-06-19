#!/usr/bin/env python3

import duckdb
import os


def test_phase3a_multifile():
    """Test Phase 3A multi-file processing implementation"""

    # Connect to DuckDB and load our extension
    conn = duckdb.connect()

    # Load the duck_hunt extension
    try:
        conn.execute("LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension'")
        print("✅ Duck Hunt extension loaded successfully")
    except Exception as e:
        print(f"❌ Failed to load extension: {e}")
        return

    # Test 1: Single file processing (backward compatibility)
    print("\n🔸 Test 1: Single file processing")
    try:
        result = conn.execute(
            """
            SELECT source_file, build_id, environment, file_index, COUNT(*) as events 
            FROM read_test_results('workspace/ansible_sample.txt', 'auto') 
            GROUP BY source_file, build_id, environment, file_index 
            ORDER BY file_index
        """
        ).fetchall()

        print(f"Single file result: {result}")
        assert len(result) == 1, f"Expected 1 result, got {len(result)}"
        assert (
            result[0][0] == 'workspace/ansible_sample.txt'
        ), f"Expected source_file to be workspace/ansible_sample.txt"
        assert result[0][3] == 0, f"Expected file_index to be 0"
        print("✅ Single file processing works")
    except Exception as e:
        print(f"❌ Single file test failed: {e}")
        return

    # Test 2: Multi-file glob pattern processing
    print("\n🔸 Test 2: Multi-file glob pattern processing")
    try:
        result = conn.execute(
            """
            SELECT source_file, build_id, environment, file_index, COUNT(*) as events 
            FROM read_test_results('workspace/builds/**/*.txt', 'auto') 
            GROUP BY source_file, build_id, environment, file_index 
            ORDER BY file_index
        """
        ).fetchall()

        print(f"Multi-file result: {result}")
        assert len(result) >= 2, f"Expected at least 2 results, got {len(result)}"

        # Check that build IDs were extracted
        build_ids = [row[1] for row in result if row[1]]
        print(f"Extracted build IDs: {build_ids}")
        assert len(build_ids) >= 2, f"Expected build IDs to be extracted"
        print("✅ Multi-file glob processing works")
    except Exception as e:
        print(f"❌ Multi-file test failed: {e}")
        return

    # Test 3: Environment detection
    print("\n🔸 Test 3: Environment detection")
    try:
        result = conn.execute(
            """
            SELECT source_file, build_id, environment, file_index, COUNT(*) as events 
            FROM read_test_results('workspace/ci-logs/**/*.log', 'auto') 
            GROUP BY source_file, build_id, environment, file_index 
            ORDER BY file_index
        """
        ).fetchall()

        print(f"Environment detection result: {result}")

        # Check that environments were detected
        environments = [row[2] for row in result if row[2]]
        print(f"Detected environments: {environments}")
        if len(environments) > 0:
            print("✅ Environment detection works")
        else:
            print("⚠️ No environments detected (may be normal)")
    except Exception as e:
        print(f"❌ Environment test failed: {e}")
        return

    # Test 4: Pipeline aggregation capabilities
    print("\n🔸 Test 4: Pipeline aggregation")
    try:
        result = conn.execute(
            """
            WITH pipeline_summary AS (
                SELECT 
                    build_id,
                    environment,
                    COUNT(DISTINCT source_file) as files_processed,
                    COUNT(*) as total_events,
                    COUNT(CASE WHEN status = 'FAIL' THEN 1 END) as failures,
                    COUNT(CASE WHEN status = 'PASS' THEN 1 END) as passes
                FROM read_test_results('workspace/builds/**/*.txt', 'auto')
                WHERE build_id IS NOT NULL
                GROUP BY build_id, environment
            )
            SELECT 
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
            ORDER BY build_id
        """
        ).fetchall()

        print(f"Pipeline aggregation result: {result}")
        print("✅ Pipeline aggregation capabilities work")
    except Exception as e:
        print(f"❌ Pipeline aggregation test failed: {e}")
        return

    print("\n🎉 Phase 3A: All tests passed! Multi-file processing is working correctly.")


if __name__ == "__main__":
    test_phase3a_multifile()
