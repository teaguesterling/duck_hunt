-- Simple test to debug parsing issues
LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- Test ESLint JSON with array format
SELECT 'ESLint Test' as test_name;
SELECT event_id, tool_name, status, error_code
FROM parse_test_results('[{"filePath":"/test.js","messages":[{"ruleId":"no-unused-vars","severity":2,"message":"test error","line":1,"column":8}]}]', 'eslint_json');

-- Test Go test JSON with proper JSONL format
SELECT 'Go Test' as test_name;
SELECT event_id, tool_name, status, test_name
FROM parse_test_results('{\"Action\":\"fail\",\"Package\":\"github.com/test\",\"Test\":\"TestExample\",\"Elapsed\":0.001}', 'gotest_json');

-- Test generic lint
SELECT 'Generic Lint' as test_name;
SELECT event_id, tool_name, status, file_path, line_number
FROM parse_test_results('src/main.c:15:5: error: undeclared variable', 'generic_lint');