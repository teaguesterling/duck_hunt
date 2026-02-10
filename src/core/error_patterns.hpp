#pragma once

#include "../include/validation_event_types.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Error pattern analysis functions for Duck Hunt.
 *
 * These functions process validation events to:
 * - Generate fingerprints for error deduplication
 * - Detect root cause categories
 * - Calculate similarity scores between errors
 * - Cluster similar errors by pattern
 */

/**
 * Normalize error message for fingerprinting by removing variable content.
 * Strips file paths, timestamps, line numbers, memory addresses, etc.
 *
 * @param message The raw error message
 * @return Normalized message suitable for fingerprinting
 */
std::string NormalizeErrorMessage(const std::string &message);

/**
 * Generate a fingerprint for an error based on normalized message and context.
 * The fingerprint is a hash of the tool name, category, and normalized message.
 *
 * @param event The validation event to fingerprint
 * @return A unique fingerprint string (e.g., "pytest_test_abc123def")
 */
std::string GenerateErrorFingerprint(const ValidationEvent &event);

/**
 * Calculate similarity between two error messages.
 * Uses normalized messages and common keyword detection.
 *
 * @param msg1 First error message
 * @param msg2 Second error message
 * @return Similarity score between 0.0 and 1.0
 */
double CalculateMessageSimilarity(const std::string &msg1, const std::string &msg2);

/**
 * Detect root cause category based on error content and context.
 * Categories: network, permission, configuration, resource, syntax, build, test_logic, unknown
 *
 * @param event The validation event to categorize
 * @return The detected category name
 */
std::string DetectRootCauseCategory(const ValidationEvent &event);

/**
 * Process events to generate error pattern metadata.
 * Adds fingerprints, pattern IDs, and similarity scores to each event.
 *
 * @param events Vector of events to process (modified in place)
 */
void ProcessErrorPatterns(std::vector<ValidationEvent> &events);

} // namespace duckdb
