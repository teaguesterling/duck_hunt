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
    
    ValidationEvent() : event_id(0), line_number(-1), column_number(-1), 
                       execution_time(0.0), file_index(-1), similarity_score(0.0), pattern_id(-1) {}
};

// Helper functions for enum conversions
std::string ValidationEventStatusToString(ValidationEventStatus status);
std::string ValidationEventTypeToString(ValidationEventType type);
ValidationEventStatus StringToValidationEventStatus(const std::string& str);
ValidationEventType StringToValidationEventType(const std::string& str);

// Schema is defined directly in the table function

} // namespace duckdb