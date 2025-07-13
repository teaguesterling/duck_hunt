# Black and mypy Output Formats Research

This document provides comprehensive examples of Black (Python code formatter) and mypy (Python static type checker) output formats for implementing parsers in Duck Hunt.

## Black Output Formats

### 1. Black `--check` Mode

Black's `--check` mode verifies if files need reformatting without modifying them.

#### Exit Codes:
- **0**: All files are properly formatted
- **1**: One or more files would be reformatted  
- **123**: Internal error occurred during formatting

#### Success Output (Exit Code 0):
```
All done! ✨ 🍰 ✨
1 file would be left unchanged.
```

#### Files Need Reformatting (Exit Code 1):
```
would reformat /path/to/file.py

Oh no! 💥 💔 💥
1 file would be reformatted.
```

#### Multiple Files:
```
would reformat /path/to/file1.py
would reformat /path/to/file2.py

Oh no! 💥 💔 💥
2 files would be reformatted, 3 files would be left unchanged.
```

### 2. Black `--diff` Mode

When using `--check --diff`, Black shows unified diff format output:

```
would reformat /mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/example.py

Oh no! 💥 💔 💥
1 file would be reformatted.

--- /mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/example.py	2025-07-13 03:51:58.805554+00:00
+++ /mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/example.py	2025-07-13 03:54:42.250560+00:00
@@ -1,19 +1,24 @@
 # Example Python file with formatting and type issues
 import sys
-def bad_function(    x,y   ):
-    z=x+y
+
+
+def bad_function(x, y):
+    z = x + y
     return z
+
 
 def type_error_function(name: str) -> int:
     return "not an int"  # Type error
 
+
 def another_function():
     result = bad_function(1, "two")  # Type error: str + int
-    print(   result   )
+    print(result)
 
-class   BadClass:
-    def __init__(self,value):
-        self.value=value
-    
-    def method(self)->str:
-        return self.value
\ No newline at end of file
+
+class BadClass:
+    def __init__(self, value):
+        self.value = value
+
+    def method(self) -> str:
+        return self.value
```

### 3. Black Error Output (Exit Code 123):
```
error: cannot format test.py: INTERNAL ERROR: Black produced code that is not equivalent to the source.
Please report a bug on https://github.com/psf/black/issues.
This diff might be helpful: /tmp/blk_kjdr1oog.log
Oh no! 💥 💔 💥 1 file would fail to reformat.
```

## mypy Output Formats

### 1. Standard Text Output

Default mypy error format: `filename:line: error: message [error-code]`

```
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:30: error: Incompatible return value type (got "int", expected "str")  [return-value]
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:33: error: "MyClass" has no attribute "nonexistent_attr"  [attr-defined]
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:36: error: Argument 1 to "badly_formatted" has incompatible type "str"; expected "int"  [arg-type]
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:36: error: Argument 2 to "badly_formatted" has incompatible type "int"; expected "str"  [arg-type]
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:44: error: Incompatible return value type (got "list[str]", expected "list[int]")  [return-value]
Found 5 errors in 1 file (checked 1 source file)
```

### 2. Enhanced Text Output with Column Numbers

Using `--show-column-numbers`:

```
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:30:18: error: Incompatible return value type (got "int", expected "str")  [return-value]
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:33:35: error: "MyClass" has no attribute "nonexistent_attr"  [attr-defined]
/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py:36:26: error: Argument 1 to "badly_formatted" has incompatible type "str"; expected "int"  [arg-type]
```

### 3. JSON Output Format

Using `--output=json`:

```json
{"file": "/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py", "line": 30, "column": 17, "message": "Incompatible return value type (got \"int\", expected \"str\")", "hint": null, "code": "return-value", "severity": "error"}
{"file": "/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py", "line": 33, "column": 34, "message": "\"MyClass\" has no attribute \"nonexistent_attr\"", "hint": null, "code": "attr-defined", "severity": "error"}
{"file": "/mnt/aux-data/teague/Projects/duck_hunt/workspace/test_files/complex_example.py", "line": 36, "column": 25, "message": "Argument 1 to \"badly_formatted\" has incompatible type \"str\"; expected \"int\"", "hint": null, "code": "arg-type", "severity": "error"}
```

#### JSON Schema Fields:
- `file`: Absolute file path
- `line`: Line number (1-indexed)
- `column`: Column number (1-indexed) 
- `message`: Human-readable error message
- `hint`: Additional hint information (nullable)
- `code`: mypy error code (e.g., "return-value", "attr-defined", "arg-type")
- `severity`: Always "error" in current mypy versions

### 4. Success Output

When no issues are found:
```
Success: no issues found in 1 source file
```

### 5. Common Error Types and Codes

| Error Code | Description | Example Message |
|------------|-------------|-----------------|
| `return-value` | Incompatible return types | `Incompatible return value type (got "int", expected "str")` |
| `attr-defined` | Attribute doesn't exist | `"MyClass" has no attribute "nonexistent_attr"` |
| `arg-type` | Argument type mismatch | `Argument 1 to "function" has incompatible type "str"; expected "int"` |
| `call-arg` | Wrong number of arguments | `Too many arguments for "greet"` |
| `name-defined` | Undefined variable | `Name "variable" is not defined` |
| `assignment` | Assignment type mismatch | `Incompatible types in assignment (expression has type "str", variable has type "int")` |

### 6. Note and Warning Examples

```
mypy_test.py:8: note: Left operand is of type "Optional[datetime]"
mypy_test.py: note: In function "some_function":
error: Library stubs not installed for "dependency" (or incompatible with Python 3.9) [import]
note: Hint: "python3 -m pip install types-dependency"
note: (or run "mypy --install-types" to install all missing stub packages)
```

## Parser Implementation Recommendations

### For Black Parser:
1. **Parse exit codes** to determine if reformatting is needed
2. **Extract file paths** from "would reformat" lines using regex: `would reformat (.+)`
3. **Parse unified diff** to extract specific changes when using `--diff`
4. **Handle emojis** in output messages (✨ 🍰 ✨, 💥 💔 💥)
5. **Extract file counts** from summary messages like "X files would be reformatted, Y files would be left unchanged"

### For mypy Parser:
1. **Prefer JSON output** (`--output=json`) for structured parsing
2. **Parse standard text format** as fallback with regex: `(.+):(\d+)(?::(\d+))?: (error|warning|note): (.+?)(?:\s+\[(.+?)\])?$`
3. **Extract location information**: file path, line number, optional column number
4. **Parse error codes** from brackets `[error-code]`
5. **Handle success messages** and multi-file summaries
6. **Group related errors** by file for better reporting

### Key Parsing Patterns:

#### Black:
```regex
# File needs reformatting
would reformat (.+)

# Summary line  
(\d+) files? would be reformatted(?:, (\d+) files? would be left unchanged)?

# Success message
All done! .+ (\d+) files? would be left unchanged
```

#### mypy (Text Format):
```regex
# Error line with optional column and error code
^(.+?):(\d+)(?::(\d+))?: (error|warning|note): (.+?)(?:\s+\[(.+?)\])?$

# Success message
^Success: no issues found in (\d+) source files?$

# Summary line
^Found (\d+) errors? in (\d+) files? \(checked (\d+) source files?\)$
```

This comprehensive format documentation enables effective parser implementation for both Black and mypy output in Duck Hunt.