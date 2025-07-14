# 🦆 Duck Hunt Analytics Examples

This directory contains sophisticated SQL analytics examples that demonstrate the full analytical power of Duck Hunt's error pattern analysis capabilities. These examples leverage DuckDB's advanced analytics functions to provide insights that would traditionally require complex C++ implementations.

## 📁 Directory Structure

```
examples/
├── analytics/              # Advanced analytical queries
│   ├── temporal_analysis.sql       # Time-series error trends and cyclical patterns
│   ├── failure_correlation.sql     # Cross-build error relationships
│   └── predictive_analytics.sql    # Future failure prediction
├── dashboards/             # Dashboard-ready queries  
│   └── error_overview.sql          # Executive summary metrics
├── datasets/               # Sample datasets for testing
└── README.md              # This file
```

## 🚀 Quick Start

1. **Load Duck Hunt Extension**:
   ```sql
   LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';
   ```

2. **Run Your First Analysis**:
   ```sql
   -- Get basic error pattern overview
   SELECT 
       root_cause_category,
       COUNT(*) as occurrences,
       COUNT(DISTINCT pattern_id) as unique_patterns
   FROM read_test_results('workspace/**/*.txt', 'auto')
   WHERE root_cause_category IS NOT NULL
   GROUP BY root_cause_category
   ORDER BY occurrences DESC;
   ```

3. **Explore Advanced Analytics**:
   - Copy queries from the `analytics/` directory
   - Modify file patterns to match your data structure
   - Combine queries for comprehensive analysis

## 📊 Available Analytics

### 🕒 Temporal Analysis (`analytics/temporal_analysis.sql`)

**What it does**: Analyzes how error patterns evolve over time
- **Error Frequency Trends**: Track increases/decreases in specific errors
- **Cyclical Pattern Detection**: Find errors that occur on regular schedules
- **Pattern Evolution**: How error characteristics change over time
- **Root Cause Trending**: Which types of errors are becoming more/less common

**Use Cases**:
- Predict when maintenance windows should be scheduled
- Identify recurring issues that need permanent fixes
- Track the effectiveness of fixes over time
- Plan capacity based on error cycles

**Example Output**:
```
error_date    | pattern_id | daily_trend | weekly_trend_status | z_score
2024-01-15    | 42         | INCREASING  | SPIKE              | 2.1
2024-01-14    | 23         | STABLE      | NORMAL             | 0.3
```

### 🔗 Failure Correlation (`analytics/failure_correlation.sql`)

**What it does**: Discovers relationships between different error types
- **Co-occurrence Analysis**: Errors that happen together
- **Failure Cascades**: Errors that trigger other errors
- **Cross-Environment Propagation**: How errors spread between dev/staging/prod
- **Root Cause Correlation Matrix**: Which error types are related

**Use Cases**:
- Identify root causes that affect multiple systems
- Prevent cascade failures by fixing trigger issues
- Understand environment-specific vs. systemic problems
- Plan fixes based on error relationships

**Example Output**:
```
pattern_a | pattern_b | correlation_strength | likely_relationship
42        | 87        | 0.85                | LIKELY_SAME_ROOT_CAUSE
23        | 91        | 0.73                | CASCADE_EFFECT
```

### 🔮 Predictive Analytics (`analytics/predictive_analytics.sql`)

**What it does**: Predicts future failures and assesses risks
- **Error Recurrence Prediction**: When specific errors will likely reoccur
- **Environment Risk Assessment**: Which environments are most risky
- **Deployment Readiness**: Whether it's safe to deploy
- **Anomaly Detection**: Unusual patterns that need investigation

**Use Cases**:
- Schedule preventive maintenance before errors occur
- Make deployment go/no-go decisions
- Allocate monitoring resources to high-risk areas
- Get early warning of emerging issues

**Example Output**:
```
pattern_id | predicted_next_median | prediction_confidence | urgency_level | recommendation
42         | 147                  | HIGH                  | IMMINENT      | CRITICAL_WATCH
87         | 152                  | MEDIUM                | NEAR_TERM     | MONITOR_CLOSELY
```

### 📊 Dashboard Queries (`dashboards/error_overview.sql`)

**What it does**: Provides executive-level summary metrics
- **KPI Overview**: Failure rates, unique patterns, system health
- **Top Issues**: Most frequent errors requiring attention
- **Environment Health**: Comparative analysis across environments
- **Trend Analysis**: Recent vs. historical performance
- **Tool Performance**: Which tools are working well vs. poorly

**Use Cases**:
- Executive dashboards and reports
- Daily operational health checks
- SLA monitoring and reporting
- Team performance metrics

**Example Output**:
```
dashboard_section        | total_events | failure_rate_pct | health_status
System Health Overview   | 1,247        | 8.3             | 🟡 Warning
Environment Comparison   | -            | -               | -
```

## 🎯 Advanced Use Cases

### 1. **Predictive Maintenance Scheduling**
Combine temporal analysis with predictive analytics:
```sql
-- Find patterns that occur cyclically and predict next occurrence
WITH cyclical_errors AS (
    SELECT * FROM (temporal_analysis_query) WHERE cycle_type = 'WEEKLY'
),
predictions AS (
    SELECT * FROM (predictive_analytics_query) WHERE prediction_confidence = 'HIGH'
)
SELECT c.pattern_id, c.avg_interval_hours, p.predicted_next_median
FROM cyclical_errors c
JOIN predictions p ON c.pattern_id = p.pattern_id
ORDER BY p.predicted_next_median;
```

### 2. **Release Risk Assessment**
Combine correlation analysis with environment health:
```sql
-- Assess deployment risk based on recent error patterns and correlations
WITH high_risk_patterns AS (
    SELECT * FROM (correlation_analysis_query) WHERE correlation_strength >= 0.7
),
environment_health AS (
    SELECT * FROM (dashboard_query) WHERE health_status LIKE '%Critical%'
)
-- Join and analyze for deployment decision
```

### 3. **Root Cause Investigation**
Chain multiple analyses for comprehensive investigation:
```sql
-- Start with frequent patterns, find correlations, predict impact
WITH frequent_errors AS (
    SELECT pattern_id FROM (dashboard_query) ORDER BY occurrences DESC LIMIT 10
),
correlations AS (
    SELECT * FROM (correlation_query) WHERE pattern_a IN (SELECT pattern_id FROM frequent_errors)
),
predictions AS (
    SELECT * FROM (prediction_query) WHERE pattern_id IN (SELECT pattern_a FROM correlations)
)
-- Comprehensive root cause analysis
```

## 🛠 Customization Guide

### Adapting File Patterns
Replace `'workspace/**/*.txt'` with your actual file patterns:
```sql
-- Examples for different directory structures
read_test_results('logs/ci-cd/**/*.json', 'auto')           -- CI/CD logs
read_test_results('results/*/test-output.xml', 'auto')     -- XML test results  
read_test_results('builds/{2024,2023}/**/*.log', 'auto')   -- Yearly organization
```

### Time Window Adjustments
Modify time calculations for your timeline:
```sql
-- Adjust the 70/30 split for recent vs. historical
WHERE file_index >= (SELECT MAX(file_index) * 0.8 FROM ...)  -- More recent data
WHERE file_index <= (SELECT MAX(file_index) * 0.5 FROM ...)  -- More historical data
```

### Threshold Tuning
Customize alert thresholds for your environment:
```sql
-- Adjust risk thresholds
WHEN failure_rate_pct >= 15 THEN 'CRITICAL'    -- Lower threshold for sensitive systems
WHEN correlation_strength >= 0.6 THEN 'STRONG' -- Different correlation sensitivity
```

## 📈 Performance Optimization

### For Large Datasets
1. **Use LIMIT clauses** in exploratory queries
2. **Create materialized views** for frequently-used aggregations
3. **Add WHERE clauses** to focus on specific time periods or patterns
4. **Use SAMPLE** for development with large datasets

### Query Optimization Tips
```sql
-- Good: Filter early and use indexes
SELECT * FROM read_test_results('data/**/*.txt', 'auto')
WHERE pattern_id IS NOT NULL AND status IN ('ERROR', 'FAIL')
LIMIT 1000;

-- Better: Focus on specific patterns or time periods
SELECT * FROM read_test_results('data/**/*.txt', 'auto')
WHERE file_index >= 100 AND root_cause_category = 'network';
```

## 🔧 Integration Examples

### With BI Tools (Tableau, PowerBI, Grafana)
```sql
-- Create views for BI tool consumption
CREATE VIEW error_kpis AS
SELECT * FROM (dashboard_overview_query);

CREATE VIEW pattern_trends AS  
SELECT * FROM (temporal_analysis_query);
```

### With Alerting Systems
```sql
-- Find critical issues for alerting
SELECT pattern_id, recommendation, urgency_level
FROM (predictive_analytics_query)
WHERE recommendation = 'CRITICAL_WATCH'
  AND urgency_level = 'IMMINENT';
```

### With CI/CD Pipelines
```sql
-- Deployment readiness check
SELECT readiness_status, deployment_recommendation
FROM (deployment_readiness_query)
WHERE target_environment = 'prod';
```

## 🤝 Contributing

To add new analytics examples:

1. **Follow naming conventions**: `verb_noun.sql` (e.g., `analyze_performance.sql`)
2. **Include documentation**: Add header comments explaining purpose and use cases
3. **Provide examples**: Show sample input/output in comments
4. **Test thoroughly**: Verify queries work with sample data
5. **Update this README**: Add your new analytics to the appropriate section

## 📚 Additional Resources

- **Duck Hunt Core Documentation**: See main README for basic usage
- **DuckDB Analytics Functions**: [DuckDB Window Functions](https://duckdb.org/docs/sql/window_functions)
- **SQL Optimization**: [DuckDB Performance Guide](https://duckdb.org/docs/guides/performance/how_to_tune_workloads)

## 💡 Pro Tips

1. **Start Simple**: Begin with dashboard queries, then move to advanced analytics
2. **Iterate**: Build complex analyses by combining simpler queries
3. **Materialize**: Cache expensive computations as views or tables
4. **Monitor Performance**: Use `EXPLAIN` to understand query execution
5. **Validate Results**: Cross-check analytical outputs with known issues

---

**Happy Analyzing! 🦆📊**

These examples demonstrate that Duck Hunt's rich error pattern data structure enables sophisticated analytics without requiring complex C++ implementations. The combination of error fingerprinting, pattern clustering, and temporal metadata provides a foundation for advanced error intelligence that scales with your needs.