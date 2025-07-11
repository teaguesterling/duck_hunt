-- Test parse_test_results function with different formats
LOAD 'build/release/extension/duck_hunt/duck_hunt.duckdb_extension';

-- Test 1: ESLint JSON format
SELECT 'ESLint JSON Test' as test_name;
SELECT event_id, tool_name, event_type, file_path, line_number, status, severity, message, error_code
FROM parse_test_results('[{"filePath":"/src/Button.js","messages":[{"ruleId":"no-unused-vars","severity":2,"message":"React is defined but never used.","line":1,"column":8}]}]', 'eslint_json');

-- Test 2: Generic lint format
SELECT 'Generic Lint Test' as test_name;
SELECT event_id, tool_name, file_path, line_number, status, message
FROM parse_test_results('src/main.c:15:5: error: undeclared_var undeclared', 'generic_lint');

-- Test 3: Auto-detection with Go test format
SELECT 'Go Test Auto-detection' as test_name;
SELECT event_id, tool_name, status, test_name, execution_time
FROM parse_test_results('{"Action":"fail","Package":"github.com/example/project","Test":"TestCalculator","Elapsed":0.003}', 'auto');

-- Test 4: Make error format
SELECT 'Make Error Test' as test_name;
SELECT event_id, tool_name, file_path, line_number, status, message
FROM parse_test_results('src/main.c:15:5: error: undeclared_var undeclared
make: *** [Makefile:23: build/main] Error 1', 'make_error');