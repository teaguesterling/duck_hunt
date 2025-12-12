#include "src/include/workflow_engine_interface.hpp"
#include "src/parsers/workflow_engines/github_actions_parser.hpp"
#include "src/parsers/workflow_engines/gitlab_ci_parser.hpp"
#include "src/parsers/workflow_engines/jenkins_parser.hpp"
#include "src/parsers/workflow_engines/docker_parser.hpp"
#include <iostream>

int main() {
    auto& registry = duckdb::WorkflowEngineRegistry::getInstance();
    
    std::cout << "Registry has " << registry.getParsers().size() << " parsers registered." << std::endl;
    
    for (const auto& parser : registry.getParsers()) {
        std::cout << "Parser: " << parser->getName() << " (format: " << static_cast<int>(parser->getFormat()) << ")" << std::endl;
    }
    
    // Test with GitHub Actions content
    std::string test_content = "##[group]Test\nHello\n##[endgroup]";
    std::cout << "\nTesting with content: " << test_content << std::endl;
    
    const auto* found_parser = registry.findParser(test_content);
    if (found_parser) {
        std::cout << "Found parser: " << found_parser->getName() << std::endl;
        auto events = found_parser->parseWorkflowLog(test_content);
        std::cout << "Parser returned " << events.size() << " events." << std::endl;
    } else {
        std::cout << "No parser found for this content." << std::endl;
    }
    
    return 0;
}