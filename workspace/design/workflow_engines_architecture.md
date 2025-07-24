# Workflow Engines vs Test Tools: Architectural Design

## Problem Statement

During Phase 2 modernization, we discovered that workflow engines (GitHub Actions, Docker builds, GitLab CI) have fundamentally different characteristics than direct test tools (pytest, eslint, rubocop). This document defines the architectural approach for handling these differences.

## Core Distinction

### Direct Test Tools
- **Input**: Tool output  
- **Output**: Test/lint results
- **Pattern**: `Tool → Results`
- **Examples**: pytest, eslint, rubocop, terraform, ansible
- **Characteristics**: Single-purpose, direct result mapping

### Workflow Engines  
- **Input**: Orchestration logs
- **Output**: Nested results from multiple tools
- **Pattern**: `Workflow → [Tool1, Tool2, Tool3] → [Results1, Results2, Results3]`
- **Examples**: GitHub Actions, Docker builds, GitLab CI, Jenkins
- **Characteristics**: Multi-stage, contain embedded tool outputs, have orchestration metadata

## Architectural Decision: Workflow-First Approach

### Option 1: Internal C++ Delegation (Rejected)
```sql
SELECT * FROM read_test_results('https://api.github.com/repos/.../logs', 'github-ci');
```
**Issues**: Loses workflow structure, no selective parsing, mixed abstraction levels

### Option 2: Recursive SQL Parsing (Considered)
```sql
SELECT unnest(parse_test_results(r.raw_output, 'auto'), recursive:=true) 
FROM read_test_results('https://api.github.com/repos/.../logs', 'github-ci') AS r
```
**Issues**: Complex SQL, large raw_output fields, API complexity

### Option 3: Separate Workflow Functions (Selected)
```sql
SELECT unnest(parse_test_results(w.stage_log, w.stage_type), recursive:=true) 
FROM read_workflow_logs('https://api.github.com/repos/.../logs', 'github-ci') AS w
```
**Benefits**: Clean separation, clear semantics, flexible, composable

## Implementation Design

### New Function: `read_workflow_logs()`

```sql
CREATE TYPE WorkflowStep AS (
    stage_name VARCHAR,           -- "Install dependencies", "Run tests"
    stage_type VARCHAR,           -- "homebrew", "pytest", "infrastructure" 
    stage_log TEXT,               -- Raw log content for this stage
    status VARCHAR,               -- "success", "failed", "skipped"
    duration INTERVAL,            -- Stage execution time
    started_at TIMESTAMP,         -- Stage start time
    runner_info VARCHAR,          -- GitHub runner, Docker container, etc.
    job_id VARCHAR,               -- Workflow job identifier
    step_index INTEGER            -- Order within workflow
);
```

### Usage Patterns

#### Workflow-level Analysis
```sql
-- Performance and orchestration analysis
SELECT 
    stage_name, 
    stage_type,
    status, 
    duration,
    runner_info
FROM read_workflow_logs('https://api.github.com/repos/.../logs', 'github-ci');
```

#### Tool-level Analysis  
```sql
-- Detailed test results and errors
SELECT 
    w.stage_name,
    tool_results.*
FROM read_workflow_logs('https://api.github.com/repos/.../logs', 'github-ci') AS w
CROSS JOIN LATERAL parse_test_results(w.stage_log, w.stage_type) AS tool_results
WHERE w.stage_type != 'infrastructure';
```

#### Combined Analysis
```sql
-- Workflow context + tool details
WITH failed_stages AS (
    SELECT stage_name, stage_log, stage_type 
    FROM read_workflow_logs('...', 'github-ci') 
    WHERE status = 'failed'
)
SELECT 
    f.stage_name,
    t.tool_name,
    t.message,
    t.file_path,
    t.line_number
FROM failed_stages f
CROSS JOIN LATERAL parse_test_results(f.stage_log, f.stage_type) AS t
WHERE t.status = 'ERROR';
```

## GitHub Actions API Integration

### Direct API Access via DuckDB httpfs
```sql
-- Real-time CI/CD analytics
SELECT * FROM read_workflow_logs(
    'https://api.github.com/repos/teaguesterling/duck_hunt/actions/jobs/46423679607/logs', 
    'github-actions'
);
```

### Authentication Support
```sql
SET httpfs_token = 'ghp_xxxxxxxxxxxx';
SELECT * FROM read_workflow_logs(
    'https://api.github.com/repos/private/repo/actions/jobs/123/logs', 
    'github-actions'
);
```

## Implementation Phases

### Phase 1: GitHub Actions
- Create `read_workflow_logs` function
- Parse GitHub Actions markers (`##[group]`, `[command]`)
- Extract stage boundaries and metadata
- Support API authentication

### Phase 2: Docker & GitLab CI
- Add Docker multi-stage build parsing
- Add GitLab CI job/step parsing
- Standardize workflow metadata across engines

### Phase 3: Advanced Analytics
- Workflow performance analysis
- Cross-repository comparisons
- Build dependency tracking

### Phase 4: Tool Delegation Enhancement
- Smart tool detection within stages
- Automatic delegation to existing parsers
- Recursive analysis capabilities

## Impact on Current Refactoring

### Eliminated from Phase 2
- GitHub Actions parser (becomes workflow engine)
- Docker Build parser (becomes workflow engine)
- GitLab CI parser (remains as workflow engine)

### Remaining Phase 2 Targets
- Ansible Text parser (remains as direct tool)
- Any other direct test/tool parsers

### Benefits
- **Simplified Phase 2**: Focus on direct test tools only
- **Clean Architecture**: Proper separation of concerns
- **Future-Proof**: Extensible to new workflow engines
- **Performance**: Selective parsing and analysis

## Conclusion

This workflow-first architecture provides clean separation between orchestration analysis and test result analysis, while maintaining the power to combine them for comprehensive CI/CD insights. It eliminates the complexity of trying to force workflow engines into the direct test tool paradigm.