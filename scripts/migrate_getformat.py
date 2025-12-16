#!/usr/bin/env python3
"""
Migrate parser files from getFormat() returning TestResultFormat enum
to getFormatName() returning string.

Transforms:
    TestResultFormat getFormat() const override { return TestResultFormat::FLAKE8_TEXT; }
To:
    std::string getFormatName() const override { return "flake8_text"; }
"""

import re
import os
from pathlib import Path

def transform_file(filepath: Path) -> bool:
    """Transform a single file. Returns True if modified."""
    content = filepath.read_text()

    # Pattern to match: TestResultFormat getFormat() const override { return TestResultFormat::XXX; }
    pattern = r'TestResultFormat getFormat\(\) const override \{ return TestResultFormat::([A-Z_0-9]+); \}'

    def replacer(match):
        enum_value = match.group(1)
        string_value = enum_value.lower()
        return f'std::string getFormatName() const override {{ return "{string_value}"; }}'

    new_content, count = re.subn(pattern, replacer, content)

    if count > 0:
        filepath.write_text(new_content)
        return True
    return False

def main():
    src_dir = Path(__file__).parent.parent / "src" / "parsers"

    modified = []
    skipped = []

    for hpp_file in src_dir.rglob("*.hpp"):
        content = hpp_file.read_text()
        if "TestResultFormat getFormat" in content:
            if transform_file(hpp_file):
                modified.append(hpp_file)
                print(f"✓ Modified: {hpp_file.relative_to(src_dir.parent.parent)}")
            else:
                skipped.append(hpp_file)
                print(f"✗ Skipped (no match): {hpp_file.relative_to(src_dir.parent.parent)}")

    print(f"\nSummary: {len(modified)} files modified, {len(skipped)} skipped")

    # Verify transformations
    print("\nVerifying transformations:")
    for hpp_file in modified[:5]:  # Show first 5
        content = hpp_file.read_text()
        match = re.search(r'getFormatName\(\) const override \{ return "([^"]+)"; \}', content)
        if match:
            print(f"  {hpp_file.name}: {match.group(1)}")

if __name__ == "__main__":
    main()
