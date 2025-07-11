# Duck Hunt Additional Format Support Plan

## High Priority (Most Common)

### 1. **Java**
- **JUnit XML** - Standard test result format
- **SpotBugs XML** - Bug detection
- **PMD XML** - Code quality
- **Checkstyle XML** - Code style

### 2. **C#/.NET**
- **NUnit XML** - Test results
- **MSTest TRX** - Visual Studio test results
- **xUnit XML** - Test framework
- **StyleCop XML** - Code style

### 3. **Ruby**
- **RSpec JSON** - Test results
- **RuboCop JSON** - Code quality
- **Minitest** - Test framework

### 4. **Rust**
- **cargo test JSON** - Test results
- **clippy JSON** - Linting

### 5. **Swift**
- **XCTest** - Test results
- **SwiftLint JSON** - Code quality

### 6. **PHP**
- **PHPUnit XML** - Test results
- **PHP_CodeSniffer XML** - Code style
- **Psalm JSON** - Static analysis

## Medium Priority

### 7. **Shell/Bash**
- **bats TAP** - Test results
- **ShellCheck JSON** - Shell script analysis

### 8. **CSS/Web**
- **stylelint JSON** - CSS linting
- **HTML validation** - W3C validator

### 9. **Infrastructure**
- **yamllint** - YAML linting
- **markdownlint JSON** - Markdown linting

## Implementation Strategy

### Phase 1: Add 5 Most Common Formats
1. JUnit XML (Java)
2. NUnit XML (C#)
3. RSpec JSON (Ruby)
4. cargo test JSON (Rust)
5. SwiftLint JSON (Swift)

### Phase 2: Add Remaining High Priority
6. SpotBugs XML (Java)
7. RuboCop JSON (Ruby)
8. clippy JSON (Rust)
9. XCTest (Swift)
10. PHPUnit XML (PHP)

### Phase 3: Add Medium Priority
11. bats TAP (Shell)
12. ShellCheck JSON (Shell)
13. stylelint JSON (CSS)
14. yamllint (YAML)
15. markdownlint JSON (Markdown)

## Format Examples Needed

For each format, we need:
- Sample output files
- Parser implementation
- Format detection logic
- Test coverage
- Documentation

## Next Steps

1. Create sample output files for top 5 formats
2. Implement parsers following existing pattern
3. Add format detection logic
4. Add comprehensive tests
5. Update documentation