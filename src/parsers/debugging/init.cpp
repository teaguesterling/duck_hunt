#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "parsers/specialized/strace_parser.hpp"
#include "parsers/specialized/valgrind_parser.hpp"
#include "parsers/specialized/gdb_lldb_parser.hpp"
#include "parsers/specialized/coverage_parser.hpp"

namespace duckdb {

/**
 * Strace parser - wraps existing static parser in new interface.
 */
class StraceParserImpl : public BaseParser {
public:
	StraceParserImpl()
	    : BaseParser("strace", "strace Parser", ParserCategory::DEBUGGING, "strace system call trace output",
	                 ParserPriority::HIGH) {
		addGroup("c_cpp");
		addGroup("shell");
	}

	bool canParse(const std::string &content) const override {
		return duck_hunt::StraceParser().CanParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		std::vector<ValidationEvent> events;
		duck_hunt::StraceParser::ParseStrace(content, events);
		return events;
	}
};

/**
 * Valgrind parser - wraps existing static parser.
 */
class ValgrindParserImpl : public BaseParser {
public:
	ValgrindParserImpl()
	    : BaseParser("valgrind", "Valgrind Parser", ParserCategory::DEBUGGING, "Valgrind memory analysis output",
	                 ParserPriority::HIGH) {
		addGroup("c_cpp");
	}

	bool canParse(const std::string &content) const override {
		// Valgrind output typically contains "==PID==" markers
		return content.find("==") != std::string::npos &&
		       (content.find("Memcheck") != std::string::npos || content.find("HEAP SUMMARY") != std::string::npos ||
		        content.find("LEAK SUMMARY") != std::string::npos ||
		        content.find("Invalid read") != std::string::npos ||
		        content.find("Invalid write") != std::string::npos);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		std::vector<ValidationEvent> events;
		duck_hunt::ValgrindParser::ParseValgrind(content, events);
		return events;
	}
};

/**
 * GDB/LLDB parser - wraps existing static parser.
 */
class GdbLldbParserImpl : public BaseParser {
public:
	GdbLldbParserImpl()
	    : BaseParser("gdb_lldb", "GDB/LLDB Parser", ParserCategory::DEBUGGING, "GDB/LLDB debugger output",
	                 ParserPriority::HIGH) {
		addAlias("gdb");
		addAlias("lldb");
		addGroup("c_cpp");
	}

	bool canParse(const std::string &content) const override {
		// Check for definitive GDB/LLDB markers
		if (content.find("(gdb)") != std::string::npos || content.find("(lldb)") != std::string::npos ||
		    content.find("Program received signal") != std::string::npos) {
			return true;
		}

		// Breakpoint in debugger context (not just any "Breakpoint" word)
		if (content.find("Breakpoint") != std::string::npos &&
		    (content.find("hit") != std::string::npos || content.find("set at") != std::string::npos ||
		     content.find("pending") != std::string::npos)) {
			return true;
		}

		// GDB thread format: "Thread 0x" or "* Thread" (not Java's "Thread[")
		if (content.find("Thread 0x") != std::string::npos || content.find("* Thread") != std::string::npos ||
		    content.find("thread #") != std::string::npos) {
			return true;
		}

		return false;
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		std::vector<ValidationEvent> events;
		duck_hunt::GdbLldbParser::ParseGdbLldb(content, events);
		return events;
	}
};

/**
 * Coverage parser - wraps existing static parser.
 */
class CoverageParserImpl : public BaseParser {
public:
	CoverageParserImpl()
	    : BaseParser("coverage_text", "Coverage Parser", ParserCategory::TEST_FRAMEWORK, "Code coverage report output",
	                 ParserPriority::HIGH) {
		addAlias("coverage");
		addGroup("python");
	}

	bool canParse(const std::string &content) const override {
		return duck_hunt::CoverageParser().CanParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		std::vector<ValidationEvent> events;
		duck_hunt::CoverageParser::ParseCoverageText(content, events);
		return events;
	}
};

/**
 * Register all debugging parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(Debugging);

void RegisterDebuggingParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<StraceParserImpl>());
	registry.registerParser(make_uniq<ValgrindParserImpl>());
	registry.registerParser(make_uniq<GdbLldbParserImpl>());
	registry.registerParser(make_uniq<CoverageParserImpl>());
	// Note: pytest_cov_text is registered in test_frameworks/init.cpp
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(Debugging);

} // namespace duckdb
