# Webbed XML Integration Proposal

## Overview

Enable duck_hunt to parse XML-based validation log formats by dynamically using the `webbed` extension's XML functions when available.

## Motivation

Many validation tools output XML formats:
- **JUnit XML** - Universal test result format (Maven, Gradle, pytest, Jest, Go)
- **Checkstyle XML** - Java/general code style
- **PMD XML** - Java code quality
- **NUnit/xUnit XML** - .NET test results
- **Cobertura XML** - Code coverage

Rather than embedding libxml2 directly, we leverage the existing `webbed` extension which already provides robust XML parsing via libxml2.

## Design

### Runtime Detection

Duck_hunt checks if webbed functions exist in the catalog:

```cpp
bool HasWebbedExtension(ClientContext &context) {
    auto &catalog = Catalog::GetSystemCatalog(context);
    auto entry = catalog.GetEntry(context, CatalogType::SCALAR_FUNCTION_ENTRY,
                                   DEFAULT_SCHEMA, "xml_to_json",
                                   OnEntryNotFound::RETURN_NULL);
    return entry != nullptr;
}
```

### XML Parser Base Class

```cpp
class XmlBasedParser : public BaseParser {
protected:
    // Convert XML to JSON using webbed, then parse
    std::string XmlToJson(ClientContext &context, const std::string &xml_content);

    // Check webbed availability, throw helpful error if missing
    void RequireWebbed(ClientContext &context);
};
```

### Format Detection

XML formats detected by:
1. File extension (`.xml`)
2. Content starts with `<?xml` or `<testsuite`, `<testsuites>`, etc.
3. Specific root elements for each format

### User Experience

```sql
-- With webbed loaded, XML formats work seamlessly
INSTALL webbed; LOAD webbed;
LOAD duck_hunt;

SELECT * FROM read_duck_hunt_log('junit-results.xml', 'auto');
SELECT * FROM read_duck_hunt_log('checkstyle-report.xml', 'checkstyle_xml');

-- Without webbed, helpful error message
-- Error: XML parsing requires the 'webbed' extension.
--        Run: INSTALL webbed; LOAD webbed;
```

## Implementation Plan

### Phase 1: Infrastructure
- [ ] Add webbed detection utility functions
- [ ] Create XmlBasedParser base class
- [ ] Add XML format detection to auto-detection logic
- [ ] Create helpful error messages when webbed not available

### Phase 2: JUnit XML Parser
- [ ] Implement JUnit XML parser (most common format)
- [ ] Handle `<testsuite>`, `<testcase>`, `<failure>`, `<error>`, `<skipped>`
- [ ] Extract timing, class names, error messages
- [ ] Add tests with sample JUnit XML files

### Phase 3: Additional XML Formats
- [ ] Checkstyle XML parser
- [ ] PMD XML parser
- [ ] NUnit XML parser
- [ ] xUnit XML parser
- [ ] Cobertura XML parser (coverage)

## Webbed Functions Used

| Function | Purpose |
|----------|---------|
| `xml_to_json(content)` | Convert XML to JSON for parsing |
| `xml_valid(content)` | Validate XML before parsing |
| `xml_extract_text(xml, xpath)` | Extract specific values |
| `xmltodict(content)` | Alternative XML to dict conversion |

## Testing Strategy

1. Unit tests with webbed mocked/available
2. Integration tests requiring webbed
3. Error handling tests without webbed
4. Sample XML files for each format

## Target Release

v1.1.0
