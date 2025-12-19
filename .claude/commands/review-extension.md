# Duck Hunt Extension Systematic Review

Do a comprehensive review of this DuckDB extension covering the following areas:

## 1. Architecture Overview
- Main components and how they fit together
- Data flow from input to parsed output
- Registry pattern and parser lifecycle
- File organization and module boundaries

## 2. Parser Coverage
- List all parsers by category (test frameworks, linting, build systems, etc.)
- Identify gaps in coverage (popular tools without parsers)
- Check parser priority ordering for auto-detection conflicts
- Verify canParse() specificity to avoid false positives

## 3. Test Coverage
- Map tests to functionality
- Identify under-tested areas
- Check edge cases and error handling
- Verify sample files exist for each parser

## 4. Code Quality
- Inconsistencies between parsers (naming, patterns, error handling)
- Dead code or unused functions
- Technical debt (TODOs, workarounds, legacy code)
- Check for compiler warnings on strict settings

## 5. Documentation
- README accuracy and completeness
- docs/ folder contents vs actual features
- Code comments quality
- Example usage accuracy

## Output Format

Provide a structured report with:
1. **Strengths** - What's working well
2. **Issues** - Problems ranked by severity (critical/high/medium/low)
3. **Recommendations** - Improvements with rough effort estimates (small/medium/large)
4. **Action Items** - Specific next steps if any are urgent
