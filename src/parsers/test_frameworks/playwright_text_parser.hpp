#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Parser for Playwright test runner text output (list/line reporter).
 * Handles test results with ✓/✘ markers, error blocks, and summary.
 *
 * Example output:
 *   ✓  1 [chromium] › tests/file.spec.js:228:3 › Suite › test name (929ms)
 *   ✘  1 [chromium] › tests/file.spec.js:1:50 › test name (5ms)
 *
 *   1) [chromium] › tests/fail.spec.js:1:50 › test name ──────
 *     Error: expect(received).toBe(expected)
 *     Expected: 2
 *     Received: 1
 *     at /path/to/file.spec.js:1:102
 */
class PlaywrightTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "playwright_text";
	}
	std::string getName() const override {
		return "playwright";
	}
	std::string getDescription() const override {
		return "Playwright test runner text output (list/line reporter)";
	}
	int getPriority() const override {
		return 80;
	} // HIGH
	std::string getCategory() const override {
		return "test_framework";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("playwright test%"),
		    CommandPattern::Like("npx playwright test%"),
		    CommandPattern::Like("yarn playwright test%"),
		    CommandPattern::Like("pnpm playwright test%"),
		    CommandPattern::Regexp(R"(playwright\s+test.*)"),
		};
	}
};

} // namespace duckdb
