# Duck Hunt Pipeline Patterns

Duck Hunt enables powerful real-time data processing patterns by combining standard Unix pipes with DuckDB's streaming capabilities and our universal ValidationEvent schema.

## Core Pipeline Pattern

```bash
# Basic pattern: Tool output → Duck Hunt → Database
<tool_command> | duckdb database.db -s "INSERT INTO test_results FROM read_duck_hunt_log('/dev/stdin');"
```

## Real-World Examples

### 1. Direct Database Ingestion
```bash
# Store pytest results directly in database
pytest --json tests/ | duckdb test_repository.db -s "INSERT INTO test_results FROM read_duck_hunt_log('/dev/stdin');"

# Store eslint results with metadata
eslint --format json src/ | duckdb quality.db -s "INSERT INTO lint_results SELECT NOW() as timestamp, * FROM read_duck_hunt_log('/dev/stdin');"
```

### 2. Timestamped Archival with Transformation
```bash
# Archive test results with timestamps for trend analysis
pytest --json tests/ | duckdb -s "COPY (SELECT NOW() AS timestamp, * FROM read_duck_hunt_log('/dev/stdin')) TO '/dev/stdout' (FORMAT json);" > test_results/run_$(date +%Y%m%d%H%M%S).json

# Add custom metadata during ingestion
cargo test --message-format json | duckdb ci.db -s "INSERT INTO build_results SELECT '${BUILD_ID}' as build_id, '${BRANCH}' as branch, NOW() as timestamp, * FROM read_duck_hunt_log('/dev/stdin');"
```

### 3. Multi-Tool Unified Processing
```bash
# Combine multiple tools into unified schema
{
  pytest --json tests/ | duckdb -s "SELECT 'pytest' as source, * FROM read_duck_hunt_log('/dev/stdin');"
  eslint --format json src/ | duckdb -s "SELECT 'eslint' as source, * FROM read_duck_hunt_log('/dev/stdin');"
  cargo test --message-format json | duckdb -s "SELECT 'cargo' as source, * FROM read_duck_hunt_log('/dev/stdin');"
} | duckdb analytics.db -s "COPY (SELECT * FROM read_csv('/dev/stdin', header=true)) TO analytics_results;"
```

### 4. Real-Time Analytics and Alerting
```bash
# Check for failures and alert if needed
pytest --json tests/ | duckdb -s "
  WITH results AS (SELECT * FROM read_duck_hunt_log('/dev/stdin'))
  SELECT 
    COUNT(*) as total_tests,
    COUNT(CASE WHEN status = 'FAIL' THEN 1 END) as failures,
    CASE WHEN COUNT(CASE WHEN status = 'FAIL' THEN 1 END) > 0 THEN 'ALERT' ELSE 'OK' END as status
  FROM results
" | tee /dev/stderr | grep -q "ALERT" && notify-send "Tests failed!"
```

### 5. Continuous Quality Monitoring
```bash
# Monitor code quality trends over time
eslint --format json src/ | duckdb quality_trends.db -s "
  INSERT INTO daily_quality 
  SELECT 
    DATE_TRUNC('day', NOW()) as date,
    tool_name,
    category,
    COUNT(*) as issue_count,
    COUNT(CASE WHEN status = 'ERROR' THEN 1 END) as error_count
  FROM read_duck_hunt_log('/dev/stdin')
  GROUP BY 1, 2, 3
  ON CONFLICT (date, tool_name, category) 
  DO UPDATE SET issue_count = excluded.issue_count, error_count = excluded.error_count
"
```

## Advanced Analytics Examples

### Cross-Tool Correlation Analysis
```sql
-- Find files with both test failures and lint issues
SELECT 
  file_path,
  COUNT(CASE WHEN tool_name = 'pytest' AND status = 'FAIL' THEN 1 END) as test_failures,
  COUNT(CASE WHEN tool_name = 'eslint' AND status = 'ERROR' THEN 1 END) as lint_errors
FROM unified_results 
WHERE timestamp >= NOW() - INTERVAL '1 day'
GROUP BY file_path
HAVING test_failures > 0 AND lint_errors > 0
ORDER BY test_failures + lint_errors DESC;
```

### Quality Trend Analysis
```sql
-- Track improvement/degradation over time
SELECT 
  DATE_TRUNC('week', timestamp) as week,
  tool_name,
  COUNT(*) as total_issues,
  LAG(COUNT(*)) OVER (PARTITION BY tool_name ORDER BY DATE_TRUNC('week', timestamp)) as prev_week_issues,
  COUNT(*) - LAG(COUNT(*)) OVER (PARTITION BY tool_name ORDER BY DATE_TRUNC('week', timestamp)) as change
FROM test_results
WHERE timestamp >= NOW() - INTERVAL '12 weeks'
GROUP BY 1, 2
ORDER BY 1 DESC, 2;
```

### Developer Performance Insights
```sql
-- Analyze commit impact on test quality (requires git metadata)
SELECT 
  author,
  DATE_TRUNC('month', commit_date) as month,
  COUNT(CASE WHEN status IN ('FAIL', 'ERROR') THEN 1 END) as issues_introduced,
  COUNT(*) as total_changes
FROM test_results r
JOIN git_commits g ON r.file_path LIKE '%' || g.file_path || '%'
WHERE r.timestamp >= g.commit_date
  AND r.timestamp <= g.commit_date + INTERVAL '1 day'
GROUP BY 1, 2
ORDER BY 2 DESC, 3 DESC;
```

## CI/CD Integration Patterns

### GitHub Actions
```yaml
- name: Run tests and store results
  run: |
    pytest --json tests/ | duckdb ci_results.db -s "
      INSERT INTO test_runs 
      SELECT 
        '${{ github.sha }}' as commit_sha,
        '${{ github.ref }}' as branch,
        '${{ github.actor }}' as author,
        NOW() as timestamp,
        * 
      FROM read_duck_hunt_log('/dev/stdin')
    "
```

### Jenkins Pipeline
```groovy
stage('Quality Analysis') {
    steps {
        sh '''
            # Combine multiple quality tools
            {
                eslint --format json src/ | duckdb -s "SELECT 'eslint' as tool, * FROM read_duck_hunt_log('/dev/stdin');"
                pytest --json tests/ | duckdb -s "SELECT 'pytest' as tool, * FROM read_duck_hunt_log('/dev/stdin');"
            } | duckdb quality.db -s "
                INSERT INTO build_quality 
                SELECT '${BUILD_NUMBER}' as build_id, * FROM read_csv('/dev/stdin', header=true)
            "
        '''
    }
}
```

## Performance Considerations

- **Streaming**: Duck Hunt processes stdin in real-time without buffering entire output
- **Memory Efficient**: ValidationEvent schema is optimized for memory usage
- **Concurrent Safe**: Multiple tools can write to same database safely
- **Indexing**: Add indexes on commonly queried fields:
  ```sql
  CREATE INDEX idx_timestamp ON test_results(timestamp);
  CREATE INDEX idx_file_status ON test_results(file_path, status);
  CREATE INDEX idx_tool_category ON test_results(tool_name, category);
  ```

## Benefits

1. **Universal Format**: All tools output unified ValidationEvent schema
2. **Real-time Processing**: Stream processing with immediate insights
3. **Historical Analysis**: Time-series data for trend analysis
4. **Multi-tool Correlation**: Cross-tool analysis in single queries
5. **CI/CD Ready**: Easy integration with existing pipelines
6. **SQL Analytics**: Full power of SQL for analysis and reporting

This pattern transforms Duck Hunt from a simple parser into a universal test intelligence platform.