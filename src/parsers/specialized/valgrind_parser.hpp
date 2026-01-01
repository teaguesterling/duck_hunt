#pragma once

#include "validation_event_types.hpp"
#include <string>
#include <vector>

namespace duck_hunt {

class ValgrindParser {
public:
	static void ParseValgrind(const std::string &content, std::vector<duckdb::ValidationEvent> &events);

	std::string GetName() const {
		return "valgrind";
	}
	bool CanParse(const std::string &content) const;
	void Parse(const std::string &content, std::vector<duckdb::ValidationEvent> &events) const;
};

} // namespace duck_hunt
