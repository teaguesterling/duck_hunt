#include "workflow_engine_interface.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"
#include "validation_event_types.hpp"
#include "duckdb/common/string_util.hpp"
#include <algorithm>
#include <regex>

namespace duckdb {

// WorkflowEngineParser implementation

ValidationEvent WorkflowEngineParser::createBaseEvent(const std::string &raw_line, const std::string &scope_name,
                                                      const std::string &group_name,
                                                      const std::string &unit_name) const {
	ValidationEvent event;

	// Set basic fields
	event.tool_name = getFormatName();
	event.event_type = ValidationEventType::SUMMARY; // Use SUMMARY type for workflow events
	event.log_content = raw_line;
	event.message = raw_line;
	event.status = ValidationEventStatus::INFO;
	event.severity = "info";
	event.category = "workflow";

	// Set hierarchical context (Schema V2)
	event.scope = scope_name;
	event.group = group_name;
	event.unit = unit_name;

	// Extract timestamp if available
	event.started_at = extractTimestamp(raw_line);

	return event;
}

std::string WorkflowEngineParser::extractTimestamp(const std::string &line) const {
	// Common timestamp patterns
	std::vector<std::regex> timestamp_patterns = {
	    std::regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z)"), // GitHub Actions ISO format
	    std::regex(R"(\d{2}:\d{2})"),                               // GitLab CI simple time
	    std::regex(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\])"),   // Jenkins bracketed
	    std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})")        // Generic datetime
	};

	for (const auto &pattern : timestamp_patterns) {
		std::smatch match;
		if (std::regex_search(line, match, pattern)) {
			return match.str();
		}
	}

	return "";
}

std::string WorkflowEngineParser::determineSeverity(const std::string &status, const std::string &message) const {
	std::string lower_status = StringUtil::Lower(status);
	std::string lower_message = StringUtil::Lower(message);

	if (lower_status.find("fail") != std::string::npos || lower_status.find("error") != std::string::npos ||
	    lower_message.find("error") != std::string::npos || lower_message.find("fail") != std::string::npos) {
		return "error";
	}

	if (lower_status.find("warn") != std::string::npos || lower_message.find("warn") != std::string::npos ||
	    lower_message.find("deprecated") != std::string::npos) {
		return "warning";
	}

	if (lower_status.find("success") != std::string::npos || lower_status.find("pass") != std::string::npos ||
	    lower_status.find("complete") != std::string::npos) {
		return "info";
	}

	return "info"; // Default
}

// WorkflowEngineRegistry implementation

WorkflowEngineRegistry &WorkflowEngineRegistry::getInstance() {
	static WorkflowEngineRegistry instance;
	return instance;
}

void WorkflowEngineRegistry::registerParser(std::unique_ptr<WorkflowEngineParser> parser) {
	parsers_.push_back(std::move(parser));
	sorted_ = false; // Need to re-sort
}

const WorkflowEngineParser *WorkflowEngineRegistry::findParser(const std::string &content) const {
	const_cast<WorkflowEngineRegistry *>(this)->ensureSorted();

	for (const auto &parser : parsers_) {
		if (parser->canParse(content)) {
			return parser.get();
		}
	}

	return nullptr;
}

const WorkflowEngineParser *WorkflowEngineRegistry::getParser(const std::string &format_name) const {
	for (const auto &parser : parsers_) {
		if (parser->getFormatName() == format_name) {
			return parser.get();
		}
	}

	return nullptr;
}

const std::vector<std::unique_ptr<WorkflowEngineParser>> &WorkflowEngineRegistry::getParsers() const {
	const_cast<WorkflowEngineRegistry *>(this)->ensureSorted();
	return parsers_;
}

void WorkflowEngineRegistry::ensureSorted() {
	if (!sorted_) {
		std::sort(parsers_.begin(), parsers_.end(),
		          [](const std::unique_ptr<WorkflowEngineParser> &a, const std::unique_ptr<WorkflowEngineParser> &b) {
			          return a->getPriority() > b->getPriority();
		          });
		sorted_ = true;
	}
}

} // namespace duckdb
