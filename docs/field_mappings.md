# Field Mappings by Domain

Duck Hunt uses a generic schema that maps to many different log domains. This document explains how schema fields are used for each category of parsers.

## Hierarchy Overview

The schema uses a 4-level hierarchy that adapts to different contexts:

```
scope → group → unit → subunit
```

| Level | CI/CD | Kubernetes | Cloud Audit | Tests | App Logs |
|-------|-------|------------|-------------|-------|----------|
| scope | Workflow | Cluster | Account | Suite | Service |
| group | Job | Namespace | Region | Class | Component |
| unit | Step | Pod | Service | Method | Handler |
| subunit | - | Container | - | - | - |

---

## CI/CD Workflows

**Parsers:** `github_actions`, `gitlab_ci`, `jenkins`, `docker_build`

### Field Mappings

| Field | CI/CD Meaning | Example |
|-------|---------------|---------|
| `scope` | Workflow/pipeline name | "build", "deploy", "CI" |
| `scope_id` | Workflow run ID | "run-12345", "pipeline-789" |
| `scope_status` | Workflow status | "running", "success", "failure" |
| `group` | Job name | "test", "lint", "build-linux" |
| `group_id` | Job ID | "job-456" |
| `group_status` | Job status | "running", "success", "failure" |
| `unit` | Step name | "npm install", "Run tests", "Upload artifacts" |
| `unit_id` | Step ID | "step-789" |
| `unit_status` | Step status | "success", "failure", "skipped" |
| `origin` | Runner/agent | "ubuntu-latest", "self-hosted", workspace path |
| `external_id` | Commit SHA | "abc123def456" |
| `started_at` | Step start time | "2024-01-15T10:30:00Z" |

### Example Query

```sql
-- Find failed steps in GitHub Actions
SELECT
    scope as workflow,
    "group" as job,
    unit as step,
    unit_status,
    message
FROM read_duck_hunt_workflow_log('actions.log', 'github_actions')
WHERE unit_status = 'failure';
```

---

## Test Frameworks

**Parsers:** `pytest_json`, `pytest_text`, `gotest_json`, `junit_xml`, `cargo_test_json`, `rspec_text`, `gtest_text`, `mocha_chai_text`

### Field Mappings

| Field | Test Meaning | Example |
|-------|--------------|---------|
| `scope` | Test suite/directory | "tests/unit", "integration" |
| `group` | Test class/module | "TestUserService", "test_auth" |
| `unit` | Test method/function | "test_create_user", "test_login_success" |
| `test_name` | Full test identifier | "tests/test_user.py::TestUserService::test_create" |
| `file_path` | Test file path | "tests/test_user.py" |
| `line_number` | Failure line | 42 |
| `status` | Test result | "PASS", "FAIL", "SKIP", "ERROR" |
| `execution_time` | Test duration (ms) | 1234.5 |
| `message` | Failure message | "AssertionError: expected 1, got 2" |
| `fingerprint` | Failure signature | "AssertionError:test_create:expected" |

### Example Query

```sql
-- Find slowest failing tests
SELECT
    test_name,
    execution_time,
    message
FROM read_duck_hunt_log('pytest.json', 'pytest_json')
WHERE status = 'FAIL'
ORDER BY execution_time DESC
LIMIT 10;
```

---

## Linting & Static Analysis

**Parsers:** `eslint_json`, `pylint_text`, `mypy_text`, `rubocop_json`, `clippy_json`, `shellcheck_json`, `hadolint_json`, `generic_lint`

### Field Mappings

| Field | Lint Meaning | Example |
|-------|--------------|---------|
| `file_path` | Source file | "src/utils.js", "lib/api.py" |
| `line_number` | Line number | 42 |
| `column_number` | Column number | 15 |
| `error_code` | Rule ID | "no-unused-vars", "E501", "W0611" |
| `function_name` | Rule ID (alternate) | "no-unused-vars" |
| `category` | Rule category | "Best Practices", "Style", "Error" |
| `severity` | Issue severity | "error", "warning", "info" |
| `message` | Lint message | "'foo' is defined but never used" |
| `suggestion` | Fix suggestion | "Remove unused variable" |

### Example Query

```sql
-- Count issues by rule
SELECT
    error_code as rule,
    severity,
    COUNT(*) as count
FROM read_duck_hunt_log('eslint.json', 'eslint_json')
GROUP BY error_code, severity
ORDER BY count DESC;
```

---

## Build Systems

**Parsers:** `make_error`, `cmake_build`, `cargo_build`, `maven_build`, `gradle_build`, `msbuild`, `node_build`

### Field Mappings

| Field | Build Meaning | Example |
|-------|---------------|---------|
| `file_path` | Source file with error | "src/main.c" |
| `line_number` | Error line | 15 |
| `column_number` | Error column | 5 |
| `error_code` | Compiler error code | "C2065", "E0001" |
| `category` | Error category | "compilation", "linking", "syntax" |
| `severity` | Error severity | "error", "warning", "note" |
| `message` | Error message | "'undefined_var' undeclared" |
| `function_name` | Function context | "main", "process_data" |
| `fingerprint` | Error pattern | "undeclared identifier" |

### Example Query

```sql
-- Find all compilation errors
SELECT
    file_path,
    line_number,
    message
FROM read_duck_hunt_log('build.log', 'make_error')
WHERE category = 'compilation' AND severity = 'error';
```

---

## Web Access Logs

**Parsers:** `apache_combined`, `apache_common`, `nginx_combined`, `nginx_json`

### Field Mappings

| Field | HTTP Meaning | Example |
|-------|--------------|---------|
| `origin` | Client IP address | "203.0.113.50" |
| `target` | Request path | "/api/users/123", "/static/app.js" |
| `principal` | Authenticated user | "admin", "-" (anonymous) |
| `category` | HTTP method | "GET", "POST", "PUT", "DELETE" |
| `error_code` | HTTP status code | "200", "404", "500" |
| `status` | Request status | "PASS" (2xx), "FAIL" (4xx/5xx) |
| `execution_time` | Request duration (ms) | 45.2 |
| `started_at` | Request timestamp | "2024-01-15T10:30:00Z" |
| `external_id` | Request ID (if present) | "req-abc-123" |
| `message` | Request summary | "GET /api/users 200" |

### Example Query

```sql
-- Find slow API requests
SELECT
    target as path,
    category as method,
    error_code as status,
    execution_time
FROM read_duck_hunt_log('access.log', 'nginx_combined')
WHERE execution_time > 1000
ORDER BY execution_time DESC;
```

---

## Cloud Audit Logs

**Parsers:** `cloudtrail_json`, `gcp_audit_json`, `azure_activity_json`

### Field Mappings

| Field | Cloud Meaning | Example |
|-------|---------------|---------|
| `scope` | Account/Project ID | "123456789012", "my-project" |
| `group` | Region | "us-east-1", "europe-west1" |
| `unit` | Service | "ec2", "s3", "iam", "compute" |
| `principal` | User/Role ARN | "arn:aws:iam::123:user/admin" |
| `actor_type` | Actor type | "user", "service", "system" |
| `origin` | Source IP | "203.0.113.50" |
| `target` | Resource ARN/path | "arn:aws:s3:::my-bucket/key" |
| `function_name` | API operation | "CreateBucket", "PutObject" |
| `external_id` | Request ID | "abc-123-def-456" |
| `started_at` | Event timestamp | "2024-01-15T10:30:00Z" |
| `status` | API result | "PASS" (success), "FAIL" (error) |
| `error_code` | Error code | "AccessDenied", "NoSuchBucket" |

### Example Query

```sql
-- Find failed API calls by user
SELECT
    principal,
    function_name as operation,
    target as resource,
    error_code
FROM read_duck_hunt_log('cloudtrail.json', 'cloudtrail_json')
WHERE status = 'FAIL'
ORDER BY started_at DESC;
```

---

## Application Logs

**Parsers:** `winston_json`, `pino_json`, `bunyan_json`, `serilog_json`, `nlog_json`, `log4j_json`, `ruby_logger`, `rails_log`

### Field Mappings

| Field | App Log Meaning | Example |
|-------|-----------------|---------|
| `scope` | Service/app name | "api-server", "worker" |
| `group` | Component/module | "database", "auth", "cache" |
| `unit` | Function/handler | "getUserById", "processOrder" |
| `severity` | Log level | "error", "warn", "info", "debug" |
| `category` | Logger name | "com.example.service", "app.db" |
| `origin` | Hostname | "server-1.example.com" |
| `external_id` | Request/trace ID | "trace-abc-123", "req-456" |
| `principal` | User context | "user@example.com" |
| `message` | Log message | "Database connection failed" |
| `started_at` | Log timestamp | "2024-01-15T10:30:00Z" |
| `fingerprint` | Error signature | "ConnectionError:getUserById" |
| `structured_data` | Extra fields (JSON) | `{"duration": 123, "query": "..."}` |

### Example Query

```sql
-- Find errors by component
SELECT
    "group" as component,
    COUNT(*) as error_count,
    ANY_VALUE(message) as example
FROM read_duck_hunt_log('app.log', 'winston_json')
WHERE severity = 'error'
GROUP BY "group"
ORDER BY error_count DESC;
```

---

## Infrastructure Logs

**Parsers:** `terraform_text`, `ansible_text`, `kubernetes_events`, `docker_events`

### Field Mappings

| Field | Infra Meaning | Example |
|-------|---------------|---------|
| `scope` | Environment/cluster | "production", "staging" |
| `group` | Resource type/namespace | "aws_instance", "default" |
| `unit` | Resource name | "web-server", "nginx-pod" |
| `subunit` | Sub-resource | "container-1" |
| `category` | Operation type | "create", "update", "delete" |
| `status` | Operation result | "PASS", "FAIL", "WARNING" |
| `message` | Operation message | "Resource created successfully" |
| `file_path` | Config file | "main.tf", "playbook.yml" |
| `origin` | Host/node | "worker-1", "localhost" |

### Example Query

```sql
-- Find failed Terraform operations
SELECT
    unit as resource,
    category as operation,
    message
FROM read_duck_hunt_log('terraform.log', 'terraform_text')
WHERE status = 'FAIL';
```

---

## Network Logs

**Parsers:** `iptables_log`, `vpc_flow_json`

### Field Mappings

| Field | Network Meaning | Example |
|-------|-----------------|---------|
| `origin` | Source IP:port | "192.168.1.100:54321" |
| `target` | Destination IP:port | "10.0.0.1:443" |
| `category` | Protocol | "TCP", "UDP", "ICMP" |
| `scope` | Interface/ENI | "eth0", "eni-abc123" |
| `status` | Action | "PASS" (ACCEPT), "FAIL" (DROP) |
| `message` | Action summary | "ACCEPT: 192.168.1.100 -> 10.0.0.1:443" |
| `started_at` | Timestamp | "2024-01-15T10:30:00Z" |

### Example Query

```sql
-- Find blocked connections
SELECT
    origin as source,
    target as destination,
    category as protocol,
    started_at
FROM read_duck_hunt_log('firewall.log', 'iptables_log')
WHERE status = 'FAIL'
ORDER BY started_at DESC;
```
