#include "include/validation_event_types.hpp"
#include "duckdb/function/table_function.hpp"

namespace duckdb {

std::string ValidationEventStatusToString(ValidationEventStatus status) {
    switch (status) {
        case ValidationEventStatus::PASS: return "PASS";
        case ValidationEventStatus::FAIL: return "FAIL";
        case ValidationEventStatus::SKIP: return "SKIP";
        case ValidationEventStatus::ERROR: return "ERROR";
        case ValidationEventStatus::WARNING: return "WARNING";
        case ValidationEventStatus::INFO: return "INFO";
        default: return "UNKNOWN";
    }
}

std::string ValidationEventTypeToString(ValidationEventType type) {
    switch (type) {
        case ValidationEventType::TEST_RESULT: return "test_result";
        case ValidationEventType::LINT_ISSUE: return "lint_issue";
        case ValidationEventType::TYPE_ERROR: return "type_error";
        case ValidationEventType::SECURITY_FINDING: return "security_finding";
        case ValidationEventType::BUILD_ERROR: return "build_error";
        case ValidationEventType::PERFORMANCE_ISSUE: return "performance_issue";
        case ValidationEventType::MEMORY_ERROR: return "memory_error";
        case ValidationEventType::MEMORY_LEAK: return "memory_leak";
        case ValidationEventType::THREAD_ERROR: return "thread_error";
        case ValidationEventType::PERFORMANCE_METRIC: return "performance_metric";
        case ValidationEventType::SUMMARY: return "summary";
        case ValidationEventType::DEBUG_EVENT: return "debug_event";
        case ValidationEventType::CRASH_SIGNAL: return "crash_signal";
        case ValidationEventType::DEBUG_INFO: return "debug_info";
        default: return "unknown";
    }
}

ValidationEventStatus StringToValidationEventStatus(const std::string& str) {
    if (str == "PASS" || str == "passed") return ValidationEventStatus::PASS;
    if (str == "FAIL" || str == "failed") return ValidationEventStatus::FAIL;
    if (str == "SKIP" || str == "skipped") return ValidationEventStatus::SKIP;
    if (str == "ERROR" || str == "error") return ValidationEventStatus::ERROR;
    if (str == "WARNING" || str == "warning") return ValidationEventStatus::WARNING;
    if (str == "INFO" || str == "info") return ValidationEventStatus::INFO;
    return ValidationEventStatus::ERROR;  // Default to error for unknown
}

ValidationEventType StringToValidationEventType(const std::string& str) {
    if (str == "test_result") return ValidationEventType::TEST_RESULT;
    if (str == "lint_issue") return ValidationEventType::LINT_ISSUE;
    if (str == "type_error") return ValidationEventType::TYPE_ERROR;
    if (str == "security_finding") return ValidationEventType::SECURITY_FINDING;
    if (str == "build_error") return ValidationEventType::BUILD_ERROR;
    if (str == "performance_issue") return ValidationEventType::PERFORMANCE_ISSUE;
    if (str == "memory_error") return ValidationEventType::MEMORY_ERROR;
    if (str == "memory_leak") return ValidationEventType::MEMORY_LEAK;
    if (str == "thread_error") return ValidationEventType::THREAD_ERROR;
    if (str == "performance_metric") return ValidationEventType::PERFORMANCE_METRIC;
    if (str == "summary") return ValidationEventType::SUMMARY;
    if (str == "debug_event") return ValidationEventType::DEBUG_EVENT;
    if (str == "crash_signal") return ValidationEventType::CRASH_SIGNAL;
    if (str == "debug_info") return ValidationEventType::DEBUG_INFO;
    return ValidationEventType::TEST_RESULT;  // Default
}

// Severity level helper functions

SeverityLevel StringToSeverityLevel(const std::string& str) {
    // Handle the threshold parameter values
    if (str == "all" || str == "debug") return SeverityLevel::DEBUG;
    if (str == "info") return SeverityLevel::INFO;
    if (str == "warning") return SeverityLevel::WARNING;
    if (str == "error") return SeverityLevel::ERROR;
    if (str == "critical") return SeverityLevel::CRITICAL;
    return SeverityLevel::WARNING;  // Default threshold
}

std::string SeverityLevelToString(SeverityLevel level) {
    switch (level) {
        case SeverityLevel::DEBUG: return "debug";
        case SeverityLevel::INFO: return "info";
        case SeverityLevel::WARNING: return "warning";
        case SeverityLevel::ERROR: return "error";
        case SeverityLevel::CRITICAL: return "critical";
        default: return "warning";
    }
}

int SeverityLevelToInt(SeverityLevel level) {
    return static_cast<int>(level);
}

SeverityLevel SeverityStringToLevel(const std::string& severity_str) {
    // Map event severity strings to SeverityLevel
    // This handles the severity field values in ValidationEvent
    if (severity_str == "debug" || severity_str == "trace") return SeverityLevel::DEBUG;
    if (severity_str == "info") return SeverityLevel::INFO;
    if (severity_str == "warning" || severity_str == "warn") return SeverityLevel::WARNING;
    if (severity_str == "error") return SeverityLevel::ERROR;
    if (severity_str == "critical" || severity_str == "fatal") return SeverityLevel::CRITICAL;
    // Default: treat empty or unknown as warning level
    return SeverityLevel::WARNING;
}

bool ShouldEmitEvent(const std::string& event_severity, SeverityLevel threshold) {
    // Returns true if event should be emitted based on threshold
    // Event is emitted when its severity >= threshold
    SeverityLevel event_level = SeverityStringToLevel(event_severity);
    return static_cast<int>(event_level) >= static_cast<int>(threshold);
}

// Schema definition removed - handled directly in the table function

} // namespace duckdb