#include "webbed_integration.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/scalar_function_catalog_entry.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/query_result.hpp"

namespace duckdb {

bool WebbedIntegration::IsWebbedAvailable(ClientContext &context) {
    auto entry = LookupFunction(context, "xml_to_json");
    return entry != nullptr;
}

std::string WebbedIntegration::XmlToJson(ClientContext &context, const std::string &xml_content) {
    if (!IsWebbedAvailable(context)) {
        throw InvalidInputException(GetWebbedRequiredError());
    }

    // Use a query to invoke the function - this is the most reliable approach
    // We need to escape the XML content for use in a query
    auto escaped = StringUtil::Replace(xml_content, "'", "''");

    // Build and execute query
    auto query = "SELECT xml_to_json('" + escaped + "')";

    // Execute using the context's connection
    auto &db = DatabaseInstance::GetDatabase(context);
    Connection con(db);
    auto result = con.Query(query);

    if (result->HasError()) {
        throw InvalidInputException("xml_to_json failed: %s", result->GetError());
    }

    auto chunk = result->Fetch();
    if (!chunk || chunk->size() == 0) {
        throw InvalidInputException("xml_to_json returned no results");
    }

    auto val = chunk->data[0].GetValue(0);
    if (val.IsNull()) {
        throw InvalidInputException("xml_to_json returned NULL - XML content may be invalid");
    }

    return val.ToString();
}

bool WebbedIntegration::IsValidXml(ClientContext &context, const std::string &xml_content) {
    // If webbed is not available, we can't validate - return true and let parsing fail later
    if (!IsWebbedAvailable(context)) {
        return true;
    }

    try {
        auto escaped = StringUtil::Replace(xml_content, "'", "''");
        auto query = "SELECT xml_valid('" + escaped + "')";

        auto &db = DatabaseInstance::GetDatabase(context);
        Connection con(db);
        auto result = con.Query(query);

        if (result->HasError()) {
            return false;
        }

        auto chunk = result->Fetch();
        if (!chunk || chunk->size() == 0) {
            return false;
        }

        auto val = chunk->data[0].GetValue(0);
        if (val.IsNull()) {
            return false;
        }

        return val.GetValue<bool>();
    } catch (...) {
        return false;
    }
}

std::string WebbedIntegration::GetWebbedRequiredError() {
    return "XML parsing requires the 'webbed' extension. "
           "Install and load it with:\n"
           "  INSTALL webbed FROM community;\n"
           "  LOAD webbed;";
}

optional_ptr<CatalogEntry> WebbedIntegration::LookupFunction(ClientContext &context, const std::string &name) {
    try {
        auto &catalog = Catalog::GetSystemCatalog(context);
        auto entry = catalog.GetEntry(context, CatalogType::SCALAR_FUNCTION_ENTRY,
                                       DEFAULT_SCHEMA, name, OnEntryNotFound::RETURN_NULL);
        return entry;
    } catch (...) {
        return nullptr;
    }
}

Value WebbedIntegration::InvokeScalarFunction(ClientContext &context, const std::string &func_name,
                                               const std::string &arg) {
    // Use query-based invocation for reliability
    auto escaped = StringUtil::Replace(arg, "'", "''");
    auto query = "SELECT " + func_name + "('" + escaped + "')";

    auto &db = DatabaseInstance::GetDatabase(context);
    Connection con(db);
    auto result = con.Query(query);

    if (result->HasError()) {
        throw InvalidInputException("Function %s failed: %s", func_name, result->GetError());
    }

    auto chunk = result->Fetch();
    if (!chunk || chunk->size() == 0) {
        return Value();
    }

    return chunk->data[0].GetValue(0);
}

} // namespace duckdb
