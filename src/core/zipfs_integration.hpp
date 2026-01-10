#pragma once

#include "duckdb.hpp"
#include "duckdb/main/client_context.hpp"
#include <string>

namespace duckdb {

/**
 * Utilities for integrating with the zipfs extension for ZIP archive access.
 * These functions detect if zipfs is loaded and attempt to auto-load it.
 */
class ZipfsIntegration {
public:
	/**
	 * Check if the zipfs extension is loaded by attempting to use the zip:// protocol.
	 */
	static bool IsZipfsAvailable(ClientContext &context);

	/**
	 * Try to auto-load the zipfs extension if not already loaded.
	 * Returns true if zipfs is available after the attempt.
	 */
	static bool TryAutoLoadZipfs(ClientContext &context);

	/**
	 * Ensure zipfs is available, attempting to auto-load if necessary.
	 * Throws an exception with a helpful message if zipfs cannot be loaded.
	 */
	static void EnsureZipfsAvailable(ClientContext &context);

	/**
	 * Get a helpful error message for when zipfs is required but not loaded.
	 */
	static std::string GetZipfsRequiredError();
};

} // namespace duckdb
