#pragma once

#include "duckdb.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/scalar_function_catalog_entry.hpp"
#include <string>

namespace duckdb {

/**
 * Utilities for integrating with the webbed extension for XML parsing.
 * These functions detect if webbed is loaded and invoke its functions.
 */
class WebbedIntegration {
public:
	/**
	 * Check if the webbed extension is loaded by looking for its functions.
	 */
	static bool IsWebbedAvailable(ClientContext &context);

	/**
	 * Try to auto-load the webbed extension if not already loaded.
	 * Returns true if webbed is available after the attempt.
	 */
	static bool TryAutoLoadWebbed(ClientContext &context);

	/**
	 * Convert XML content to JSON using webbed's xml_to_json function.
	 * Throws if webbed is not available.
	 */
	static std::string XmlToJson(ClientContext &context, const std::string &xml_content);

	/**
	 * Check if XML content is valid using webbed's xml_valid function.
	 * Returns false if webbed is not available (fails gracefully).
	 */
	static bool IsValidXml(ClientContext &context, const std::string &xml_content);

	/**
	 * Read XML file using webbed's read_xml table function.
	 * Returns a materialized result with all rows.
	 * @param file_path Path to the XML file
	 * @param record_element The XML element to treat as records (e.g., "test-case")
	 */
	static unique_ptr<MaterializedQueryResult> ReadXml(ClientContext &context, const std::string &file_path,
	                                                   const std::string &record_element);

	/**
	 * Get a helpful error message for when webbed is required but not loaded.
	 */
	static std::string GetWebbedRequiredError();

private:
	/**
	 * Look up a scalar function in the catalog.
	 */
	static optional_ptr<CatalogEntry> LookupFunction(ClientContext &context, const std::string &name);

	/**
	 * Invoke a scalar function with a single string argument.
	 */
	static Value InvokeScalarFunction(ClientContext &context, const std::string &func_name, const std::string &arg);
};

} // namespace duckdb
