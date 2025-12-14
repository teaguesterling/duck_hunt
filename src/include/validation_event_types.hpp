#pragma once

#include "duckdb.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb {

// Enum for validation event status
enum class ValidationEventStatus : uint8_t {
    PASS = 0,
    FAIL = 1,
    SKIP = 2,
    ERROR = 3,
    WARNING = 4,
    INFO = 5
};

// Enum for validation event type  
enum class ValidationEventType : uint8_t {
    TEST_RESULT = 0,
    LINT_ISSUE = 1,
    TYPE_ERROR = 2,
    SECURITY_FINDING = 3,
    BUILD_ERROR = 4,
    PERFORMANCE_ISSUE = 5,
    MEMORY_ERROR = 6,
    MEMORY_LEAK = 7,
    THREAD_ERROR = 8,
    PERFORMANCE_METRIC = 9,
    SUMMARY = 10,
    DEBUG_EVENT = 11,
    CRASH_SIGNAL = 12,
    DEBUG_INFO = 13
};

// Main validation event structure
struct ValidationEvent {
    // Core identification
    int64_t event_id;
    std::string tool_name;
    ValidationEventType event_type;

    // Code location (lint, test, stack trace)
    std::string file_path;            // Source code file path
    int32_t line_number;
    int32_t column_number;
    std::string function_name;        // Function/method name in code

    // Classification
    ValidationEventStatus status;
    std::string severity;             // error/warning/info
    std::string category;             // Domain-specific classifier
    std::string error_code;           // Error/status identifier

    // Content
    std::string message;
    std::string suggestion;
    std::string raw_output;
    std::string structured_data;      // JSON for extra fields

    // Log tracking
    int32_t log_line_start;           // 1-indexed line where event starts
    int32_t log_line_end;             // 1-indexed line where event ends

    // Test-specific
    std::string test_name;
    double execution_time;            // Duration in milliseconds

    // Identity & Network
    std::string principal;            // Actor identity (ARN, email, username)
    std::string origin;               // Source (IP address, hostname)
    std::string target;               // Destination (IP:port, HTTP path, resource ARN)
    std::string actor_type;           // user/service/system/anonymous

    // Temporal
    std::string started_at;           // Event timestamp (ISO format)

    // Correlation
    std::string external_id;          // External correlation ID (request ID, trace ID)

    // Hierarchical context (generic names for cross-domain support)
    // Level 1: Broadest context (workflow, cluster, account, test suite)
    std::string scope;
    std::string scope_id;
    std::string scope_status;
    // Level 2: Middle grouping (job, namespace, region, test class)
    std::string group;
    std::string group_id;
    std::string group_status;
    // Level 3: Specific unit (step, pod, service, test method)
    std::string unit;
    std::string unit_id;
    std::string unit_status;
    // Level 4: Sub-unit when needed (container, resource)
    std::string subunit;
    std::string subunit_id;

    // Pattern analysis
    std::string fingerprint;          // Normalized event signature for clustering
    double similarity_score;          // Similarity to cluster centroid (0.0-1.0)
    int64_t pattern_id;               // Pattern cluster ID (-1 if unassigned)

    ValidationEvent() : event_id(0), line_number(-1), column_number(-1),
                       execution_time(0.0), log_line_start(-1), log_line_end(-1),
                       similarity_score(0.0), pattern_id(-1) {}
};

// Helper functions for enum conversions
std::string ValidationEventStatusToString(ValidationEventStatus status);
std::string ValidationEventTypeToString(ValidationEventType type);
ValidationEventStatus StringToValidationEventStatus(const std::string& str);
ValidationEventType StringToValidationEventType(const std::string& str);

// Schema is defined directly in the table function

} // namespace duckdb