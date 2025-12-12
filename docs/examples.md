# Examples

Real examples using Duck Hunt with sample files and actual query results.

## pytest (JSON)

**Sample:** [`test/samples/pytest.json`](../test/samples/pytest.json)
```json
{
  "tests": [
    {"nodeid": "tests/test_auth.py::test_login_success", "outcome": "passed", "duration": 0.123},
    {"nodeid": "tests/test_auth.py::test_login_invalid_password", "outcome": "failed", "duration": 0.456},
    {"nodeid": "tests/test_api.py::test_create_user", "outcome": "passed", "duration": 0.089}
  ]
}
```

**Query:**
```sql
SELECT test_name, status, execution_time
FROM read_duck_hunt_log('test/samples/pytest.json', 'pytest_json');
```

**Result:**
|          test_name          | status | execution_time |
|-----------------------------|--------|---------------:|
| test_login_success          | PASS   | 0.0            |
| test_login_invalid_password | FAIL   | 0.0            |
| test_create_user            | PASS   | 0.0            |

---

## ESLint (JSON)

**Sample:** [`test/samples/eslint.json`](../test/samples/eslint.json)
```json
[{
  "filePath": "/src/components/Button.tsx",
  "messages": [
    {"ruleId": "no-unused-vars", "severity": 2, "message": "'useState' is defined but never used.", "line": 1, "column": 10},
    {"ruleId": "prefer-const", "severity": 1, "message": "'count' is never reassigned. Use 'const' instead.", "line": 5, "column": 7}
  ]
}]
```

**Query:**
```sql
SELECT file_path, line_number, error_code, message
FROM read_duck_hunt_log('test/samples/eslint.json', 'eslint_json');
```

**Result:**
|         file_path          | line_number |             error_code             |                      message                      |
|----------------------------|------------:|------------------------------------|---------------------------------------------------|
| /src/components/Button.tsx | 1           | no-unused-vars                     | 'useState' is defined but never used.             |
| /src/components/Button.tsx | 5           | prefer-const                       | 'count' is never reassigned. Use 'const' instead. |
| /src/utils/helpers.ts      | 12          | @typescript-eslint/no-explicit-any | Unexpected any. Specify a different type.         |

---

## GNU Make

**Sample:** [`test/samples/make.out`](../test/samples/make.out)
```
gcc -Wall -Wextra -g -c src/main.c -o build/main.o
src/main.c:15:5: error: 'undefined_var' undeclared (first use in this function)
src/main.c:28:12: warning: unused variable 'temp' [-Wunused-variable]
src/utils.c:8:9: error: assignment to expression with array type
make: *** [Makefile:23: build/main.o] Error 1
```

**Query:**
```sql
SELECT file_path, line_number, severity, message
FROM read_duck_hunt_log('test/samples/make.out', 'make_error');
```

**Result:**
|  file_path  | line_number | severity |                                     message                                      |
|-------------|------------:|----------|----------------------------------------------------------------------------------|
| src/main.c  | 15          | error    | 'undefined_var' undeclared (first use in this function)                          |
| src/main.c  | 15          | info     | each undeclared identifier is reported only once for each function it appears in |
| src/main.c  | 28          | warning  | unused variable 'temp' [-Wunused-variable]                                       |
| src/utils.c | 8           | error    | assignment to expression with array type                                         |
| Makefile    | NULL        | error    | make: *** [Makefile:23: build/main.o] Error 1                                    |

---

## MyPy

**Sample:** [`test/samples/mypy.txt`](../test/samples/mypy.txt)
```
src/api/routes.py:23: error: Argument 1 to "process" has incompatible type "str"; expected "int"  [arg-type]
src/api/routes.py:45: error: "None" has no attribute "items"  [union-attr]
src/models/user.py:12: warning: Unused "type: ignore" comment  [unused-ignore]
src/utils/helpers.py:8: error: Missing return statement  [return]
```

**Query:**
```sql
SELECT file_path, line_number, error_code, message
FROM read_duck_hunt_log('test/samples/mypy.txt', 'mypy_text');
```

**Result:**
|      file_path       | line_number |  error_code   |                               message                               |
|----------------------|------------:|---------------|---------------------------------------------------------------------|
| src/api/routes.py    | 23          | arg-type      | Argument 1 to "process" has incompatible type "str"; expected "int" |
| src/api/routes.py    | 45          | union-attr    | "None" has no attribute "items"                                     |
| src/models/user.py   | 12          | unused-ignore | Unused "type: ignore" comment                                       |
| src/utils/helpers.py | 8           | return        | Missing return statement                                            |

---

## Go Test (JSON)

**Sample:** [`test/samples/gotest.json`](../test/samples/gotest.json)
```json
{"Time":"2024-01-15T10:30:00.123Z","Action":"pass","Package":"github.com/example/app","Test":"TestUserCreate","Elapsed":0.333}
{"Time":"2024-01-15T10:30:01.200Z","Action":"fail","Package":"github.com/example/app","Test":"TestUserDelete","Elapsed":0.7}
{"Time":"2024-01-15T10:30:01.500Z","Action":"skip","Package":"github.com/example/app","Test":"TestUserUpdate","Elapsed":0.2}
```

**Query:**
```sql
SELECT test_name, status, execution_time
FROM read_duck_hunt_log('test/samples/gotest.json', 'gotest_json');
```

**Result:**
|   test_name    | status | execution_time |
|----------------|--------|---------------:|
| TestUserCreate | PASS   | 0.333          |
| TestUserDelete | FAIL   | 0.7            |
| TestUserUpdate | SKIP   | 0.2            |

---

## GitHub Actions

**Sample:** [`test/samples/github_actions.log`](../test/samples/github_actions.log)
```
2024-01-15T10:00:00.000Z ##[group]Run actions/checkout@v4
2024-01-15T10:00:01.000Z Syncing repository: owner/repo
2024-01-15T10:00:05.000Z ##[endgroup]
2024-01-15T10:00:06.000Z ##[group]Run npm install
2024-01-15T10:00:07.000Z npm WARN deprecated package@1.0.0: This package is deprecated
2024-01-15T10:00:21.000Z ##[error]Process completed with exit code 1.
```

**Query:**
```sql
SELECT step_name, message, severity
FROM read_duck_hunt_workflow_log('test/samples/github_actions.log', 'github_actions')
WHERE length(message) > 0
LIMIT 5;
```

**Result:**
|        step_name        |                                        message                                         | severity |
|-------------------------|----------------------------------------------------------------------------------------|----------|
| Run actions/checkout@v4 | 2024-01-15T10:00:00.000Z ##[group]Run actions/checkout@v4                              | info     |
| Run actions/checkout@v4 | 2024-01-15T10:00:01.000Z Syncing repository: owner/repo                                | info     |
| Run actions/checkout@v4 | 2024-01-15T10:00:05.000Z ##[endgroup]                                                  | info     |
| Run npm install         | 2024-01-15T10:00:06.000Z ##[group]Run npm install                                      | info     |
| Run npm install         | 2024-01-15T10:00:07.000Z npm WARN deprecated package@1.0.0: This package is deprecated | warning  |

---

## Status Badges

**Query:**
```sql
SELECT status_badge(status) as badge, tool_name, file_path, message
FROM read_duck_hunt_log('test/samples/make.out', 'make_error')
WHERE file_path NOT LIKE '%Makefile%';
```

**Result:**
| badge  | tool_name |  file_path  |                                     message                                      |
|--------|-----------|-------------|----------------------------------------------------------------------------------|
| [FAIL] | make      | src/main.c  | 'undefined_var' undeclared (first use in this function)                          |
| [ ?? ] | make      | src/main.c  | each undeclared identifier is reported only once for each function it appears in |
| [WARN] | make      | src/main.c  | unused variable 'temp' [-Wunused-variable]                                       |
| [FAIL] | make      | src/utils.c | assignment to expression with array type                                         |

---

## Aggregation

**Query:**
```sql
SELECT tool_name,
       COUNT(*) as total,
       COUNT(*) FILTER (WHERE status = 'ERROR') as errors,
       COUNT(*) FILTER (WHERE status = 'WARNING') as warnings
FROM read_duck_hunt_log('test/samples/make.out', 'make_error')
GROUP BY tool_name;
```

**Result:**
| tool_name | total | errors | warnings |
|-----------|------:|-------:|---------:|
| make      | 5     | 3      | 1        |

---

## Dynamic Regexp Parser

**Input (inline):**
```
ERROR: Connection failed
WARNING: Retrying in 5s
ERROR: Max retries exceeded
INFO: Shutting down
```

**Query:**
```sql
SELECT severity, message
FROM parse_duck_hunt_log(
  'ERROR: Connection failed
   WARNING: Retrying in 5s
   ERROR: Max retries exceeded
   INFO: Shutting down',
  'regexp:(?P<severity>ERROR|WARNING|INFO):\s+(?P<message>.+)'
);
```

**Result:**
| severity |       message        |
|----------|----------------------|
| error    | Connection failed    |
| warning  | Retrying in 5s       |
| error    | Max retries exceeded |
| info     | Shutting down        |

---

## Pipeline Integration

### Bash: Real-time Analysis

```bash
# Parse build output and show errors
make 2>&1 | duckdb -markdown -s "
  SELECT status_badge(status) as badge, file_path, line_number, message
  FROM read_duck_hunt_log('/dev/stdin', 'auto')
  WHERE status = 'ERROR'
"

# JSON output for CI
./build.sh 2>&1 | duckdb -json -s "
  SELECT tool_name, COUNT(*) as errors
  FROM read_duck_hunt_log('/dev/stdin', 'auto')
  WHERE status = 'ERROR'
  GROUP BY tool_name
"
```

### Quality Gate

```sql
-- Fail if error count exceeds threshold
SELECT CASE
  WHEN COUNT(*) FILTER (WHERE status = 'ERROR') > 5 THEN 'FAIL'
  WHEN COUNT(*) FILTER (WHERE status = 'WARNING') > 20 THEN 'WARN'
  ELSE 'PASS'
END as gate_status
FROM read_duck_hunt_log('build.log', 'auto');
```
