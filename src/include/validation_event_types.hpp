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
    int64_t event_id;
    std::string tool_name;
    ValidationEventType event_type;
    std::string file_path;
    int32_t line_number;
    int32_t column_number;
    std::string function_name;
    ValidationEventStatus status;
    std::string severity;
    std::string category;
    std::string message;
    std::string suggestion;
    std::string error_code;
    std::string test_name;
    double execution_time;
    std::string raw_output;
    std::string structured_data;  // JSON string

    // Log line tracking - position within source log file
    int32_t log_line_start;       // 1-indexed line where event starts in log file
    int32_t log_line_end;         // 1-indexed line where event ends in log file

    // Phase 3A: Multi-file processing metadata
    std::string source_file;      // Which log/result file this event came from
    std::string build_id;         // Extract from file path pattern (e.g., build-123)
    std::string environment;      // dev/staging/prod extracted from path
    int64_t file_index;          // Order of processing (0-based)
    
    // Phase 3B: Error pattern analysis
    std::string error_fingerprint;     // Normalized error signature for pattern detection
    double similarity_score;           // Similarity to pattern cluster centroid (0.0-1.0)
    int64_t pattern_id;                // Assigned error pattern group ID (-1 if unassigned)
    std::string root_cause_category;   // Detected root cause type (network, permission, config, etc.)
    
    // Phase 3C: Workflow hierarchy metadata for CI/CD log parsing
    std::string workflow_name;          // Name of the workflow/pipeline (e.g., "build", "deploy")
    std::string job_name;               // Name of the current job/stage (e.g., "test", "lint", "build")
    std::string step_name;              // Name of the current step (e.g., "run tests", "setup node")
    std::string workflow_run_id;        // Unique identifier for the workflow run
    std::string job_id;                 // Unique identifier for the job
    std::string step_id;                // Unique identifier for the step  
    std::string workflow_status;        // Overall workflow status (running, success, failure, cancelled)
    std::string job_status;             // Job status (pending, running, success, failure, skipped)
    std::string step_status;            // Step status (pending, running, success, failure, skipped)
    std::string started_at;             // When the workflow/job/step started (ISO timestamp)
    std::string completed_at;           // When it completed (ISO timestamp)
    double duration;                    // Duration in seconds
    
    ValidationEvent() : event_id(0), line_number(-1), column_number(-1),
                       execution_time(0.0), log_line_start(-1), log_line_end(-1),
                       file_index(-1), similarity_score(0.0), pattern_id(-1), duration(0.0) {}
};

// Helper functions for enum conversions
std::string ValidationEventStatusToString(ValidationEventStatus status);
std::string ValidationEventTypeToString(ValidationEventType type);
ValidationEventStatus StringToValidationEventStatus(const std::string& str);
ValidationEventType StringToValidationEventType(const std::string& str);

// Schema is defined directly in the table function

} // namespace duckdb