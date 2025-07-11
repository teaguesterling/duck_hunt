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
    PERFORMANCE_ISSUE = 5
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

    ValidationEvent() : event_id(0), line_number(-1), column_number(-1), 
                       execution_time(0.0) {}
};

// Helper functions for enum conversions
std::string ValidationEventStatusToString(ValidationEventStatus status);
std::string ValidationEventTypeToString(ValidationEventType type);
ValidationEventStatus StringToValidationEventStatus(const std::string& str);
ValidationEventType StringToValidationEventType(const std::string& str);

// Schema is defined directly in the table function

} // namespace duckdb