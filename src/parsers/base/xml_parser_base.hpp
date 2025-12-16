#pragma once

#include "parser_interface.hpp"
#include "core/webbed_integration.hpp"
#include "duckdb/main/client_context.hpp"
#include <string>

namespace duckdb {

/**
 * Base class for XML-based parsers.
 * Uses the webbed extension to convert XML to JSON, then parses the JSON.
 *
 * Subclasses should implement:
 * - canParse(): Check if content is the expected XML format
 * - parseJsonContent(): Parse the JSON representation of the XML
 * - getFormat(), getName(), getPriority(), getCategory()
 */
class XmlParserBase : public IParser {
public:
    virtual ~XmlParserBase() = default;

    /**
     * XML parsers require context to call webbed functions.
     */
    bool requiresContext() const override {
        return true;
    }

    /**
     * Parse without context - throws an error since we need webbed.
     */
    std::vector<ValidationEvent> parse(const std::string& content) const override {
        throw InvalidInputException(
            "XML parser requires ClientContext. Use parseWithContext() instead.\n%s",
            WebbedIntegration::GetWebbedRequiredError());
    }

    /**
     * Parse with context - converts XML to JSON using webbed, then parses.
     */
    std::vector<ValidationEvent> parseWithContext(ClientContext &context,
                                                   const std::string& content) const override {
        // Try to auto-load webbed if not already available
        if (!WebbedIntegration::TryAutoLoadWebbed(context)) {
            throw InvalidInputException(WebbedIntegration::GetWebbedRequiredError());
        }

        // Convert XML to JSON using webbed
        std::string json_content = WebbedIntegration::XmlToJson(context, content);

        // Parse the JSON content (implemented by subclass)
        return parseJsonContent(json_content);
    }

protected:
    /**
     * Parse the JSON representation of the XML content.
     * Implemented by subclasses for each specific XML format.
     */
    virtual std::vector<ValidationEvent> parseJsonContent(const std::string& json_content) const = 0;

    /**
     * Helper to check if content looks like XML.
     */
    static bool LooksLikeXml(const std::string& content) {
        // Skip whitespace
        size_t pos = content.find_first_not_of(" \t\n\r");
        if (pos == std::string::npos) {
            return false;
        }

        // Check for XML declaration or opening tag
        return content[pos] == '<';
    }

    /**
     * Helper to check for specific XML root element.
     */
    static bool HasRootElement(const std::string& content, const std::string& element_name) {
        // Look for <element_name or <element_name>
        std::string pattern1 = "<" + element_name + ">";
        std::string pattern2 = "<" + element_name + " ";

        return content.find(pattern1) != std::string::npos ||
               content.find(pattern2) != std::string::npos;
    }

    /**
     * Helper to check for XML declaration.
     */
    static bool HasXmlDeclaration(const std::string& content) {
        return content.find("<?xml") != std::string::npos;
    }
};

} // namespace duckdb
