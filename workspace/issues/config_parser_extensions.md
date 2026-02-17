# Future Direction: Extend Config-Based Parsers for XML and JSON Formats

## Summary

Expand the config-based parser system (introduced in v1.8.0) to support XML and JSON data formats in addition to the current regex-based text parsing.

## Motivation

The current config-based parser system (`duck_hunt_load_parser_config()`) uses regex patterns to parse text-based log formats. Many tools output structured data in XML or JSON formats that could benefit from a similar declarative configuration approach.

This would allow users to define custom parsers for XML/JSON formats without writing C++ code.

## Proposed Variants

### 1. JSON Config Parser

Define parsers for JSON log formats using JSONPath or simple field mappings:

```yaml
parser:
  name: my_json_parser
  format: json
  type: json_path

  mappings:
    message: "$.error.message"
    severity: "$.level"
    ref_file: "$.file"
    ref_line: "$.line"

  # Conditional mappings
  status:
    path: "$.status"
    map:
      "success": "PASS"
      "failure": "FAIL"
      "error": "ERROR"
```

### 2. XML Config Parser (XPath-based)

Define parsers for XML formats using XPath expressions:

```yaml
parser:
  name: my_xml_parser
  format: xml
  type: xpath

  # Define what elements to iterate over
  iterate: "//test-case"

  # XPath mappings relative to each iterated element
  mappings:
    test_name: "@name"
    status: "@result"
    message: "failure/message/text()"
    ref_file: "@classname"
    execution_time: "@duration"

  # Status mapping
  status_map:
    "Passed": "PASS"
    "Failed": "FAIL"
    "Skipped": "SKIP"
```

### 3. XML-to-JSON Config Parser

Leverage webbed's `xml_to_json()` function, then apply JSON mappings:

```yaml
parser:
  name: my_xmltojson_parser
  format: xml
  type: xml_to_json

  # After XML-to-JSON conversion, use JSON path mappings
  iterate: "$.testsuites.testsuite[*].testcase[*]"

  mappings:
    test_name: "$['@name']"
    ref_file: "$['@classname']"
    # Handle webbed's attribute naming (@attr -> @attr in JSON)
```

## Implementation Considerations

1. **XPath Support**: Would need to add an XPath library dependency or implement basic XPath subset
2. **JSONPath Support**: Could use DuckDB's built-in JSON functions or a JSONPath library
3. **XML-to-JSON Hybrid**: Simpler to implement since webbed already does the heavy lifting

## Parsers That Could Be Simplified

If implemented, several existing parsers could potentially be expressed as config files:

### JSON-based parsers:
- `pytest_json` - pytest JSON report
- `ruff_json` - Ruff linter JSON output
- `eslint_json` - ESLint JSON output
- `playwright_json` - Playwright JSON reporter
- Most `*_json_parser.cpp` files

### XML-based parsers:
- `junit_xml` - JUnit XML test results
- `unity_test_xml` - Unity Test Runner XML (NUnit 3 format)
- Future: Coverage XML formats (Cobertura, JaCoCo)

## Priority

Medium - this is a nice-to-have that would reduce maintenance burden and make the extension more user-extensible.

## Related

- Config-based parser system: `src/parsers/config_based/`
- Webbed XML-to-JSON: `src/core/webbed_integration.hpp`
- Current XML base class: `src/parsers/base/xml_parser_base.hpp`
