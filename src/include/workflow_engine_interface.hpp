#pragma once

#include "validation_event_types.hpp"
#include "read_workflow_logs_function.hpp"
#include "duckdb/common/types.hpp"
#include <string>
#include <vector>
#include <memory>

namespace duckdb {

// Forward declaration
struct WorkflowEvent;

// Base interface for all workflow engine parsers
class WorkflowEngineParser {
public:
    virtual ~WorkflowEngineParser() = default;
    
    // Check if this parser can handle the given content
    virtual bool canParse(const std::string& content) const = 0;
    
    // Get the workflow format this parser handles
    virtual WorkflowLogFormat getFormat() const = 0;
    
    // Parse workflow logs into events with hierarchical structure
    virtual std::vector<WorkflowEvent> parseWorkflowLogs(const std::string& content) const = 0;
    
    // Get parser priority (higher = checked first)
    virtual int getPriority() const { return 100; }
    
    // Get parser name for debugging
    virtual std::string getName() const = 0;

protected:
    // Helper to create base ValidationEvent with workflow metadata
    ValidationEvent createBaseEvent(const std::string& raw_line, 
                                   const std::string& workflow_name,
                                   const std::string& job_name = "",
                                   const std::string& step_name = "") const;
    
    // Helper to extract timestamp from common formats
    std::string extractTimestamp(const std::string& line) const;
    
    // Helper to determine event severity from status/message
    std::string determineSeverity(const std::string& status, const std::string& message) const;
};

// Workflow engine registry for managing parsers
class WorkflowEngineRegistry {
public:
    static WorkflowEngineRegistry& getInstance();
    
    // Register a workflow parser
    void registerParser(std::unique_ptr<WorkflowEngineParser> parser);
    
    // Find appropriate parser for content
    const WorkflowEngineParser* findParser(const std::string& content) const;
    
    // Get parser by format
    const WorkflowEngineParser* getParser(WorkflowLogFormat format) const;
    
    // Get all registered parsers (sorted by priority)
    const std::vector<std::unique_ptr<WorkflowEngineParser>>& getParsers() const;
    size_t getParserCount() const { return parsers_.size(); }

private:
    WorkflowEngineRegistry() = default;
    std::vector<std::unique_ptr<WorkflowEngineParser>> parsers_;
    bool sorted_ = false;
    
    void ensureSorted();
};

// Auto-registration helper
template<typename T>
class WorkflowParserRegistrar {
public:
    WorkflowParserRegistrar() {
        WorkflowEngineRegistry::getInstance().registerParser(make_uniq<T>());
    }
};

#define REGISTER_WORKFLOW_PARSER(className) \
    static WorkflowParserRegistrar<className> g_##className##_registrar;

} // namespace duckdb