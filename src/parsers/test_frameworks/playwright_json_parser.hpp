#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for Playwright JSON reporter output.
 * Handles structure: {
 *   "config": {...},
 *   "suites": [{
 *     "specs": [{
 *       "tests": [{
 *         "results": [{"status": "passed/failed", "error": {...}}]
 *       }]
 *     }]
 *   }],
 *   "stats": {"expected": N, "unexpected": N, "skipped": N, "flaky": N}
 * }
 */
class PlaywrightJSONParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "playwright_json";
	}
	std::string getName() const override {
		return "playwright_json";
	}
	std::string getDescription() const override {
		return "Playwright JSON reporter output";
	}
	int getPriority() const override {
		return 135;
	} // Higher than pytest_json (130) since Playwright has nested "tests" arrays
	std::string getCategory() const override {
		return "test_framework";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("playwright test%--reporter=json%"),
		    CommandPattern::Like("playwright test%--reporter json%"),
		    CommandPattern::Like("npx playwright test%--reporter=json%"),
		    CommandPattern::Like("npx playwright test%--reporter json%"),
		};
	}

private:
	bool isValidPlaywrightJSON(const std::string &content) const;
	void parseSpec(duckdb_yyjson::yyjson_val *spec, std::vector<ValidationEvent> &events, int64_t &event_id,
	               const std::string &suite_title) const;
	void parseSuite(duckdb_yyjson::yyjson_val *suite, std::vector<ValidationEvent> &events, int64_t &event_id,
	                const std::string &parent_title) const;
};

} // namespace duckdb
