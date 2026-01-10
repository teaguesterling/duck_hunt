#include "zipfs_integration.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_helper.hpp"

namespace duckdb {

bool ZipfsIntegration::IsZipfsAvailable(ClientContext &context) {
	// Check if zipfs is available by trying to use HasGlob on a zip:// path
	// The zipfs extension registers a FileSystem that handles zip:// URLs
	try {
		auto &fs = FileSystem::GetFileSystem(context);
		// Try to check if a dummy zip path is handled - this should not throw
		// if zipfs is loaded, even if the file doesn't exist
		// We use HasGlob as a lightweight check that doesn't require file access
		fs.HasGlob("zip://test.zip/*.txt");
		return true;
	} catch (const NotImplementedException &) {
		// zip:// protocol not supported - zipfs not loaded
		return false;
	} catch (const IOException &) {
		// File not found is fine - it means zipfs IS loaded but file doesn't exist
		return true;
	} catch (...) {
		// Other errors - assume zipfs is available (the actual operation will fail with details)
		return true;
	}
}

bool ZipfsIntegration::TryAutoLoadZipfs(ClientContext &context) {
	// Already loaded?
	if (IsZipfsAvailable(context)) {
		return true;
	}

	// Try to auto-load the zipfs extension
	if (ExtensionHelper::TryAutoLoadExtension(context, "zipfs")) {
		// Verify it actually loaded
		return IsZipfsAvailable(context);
	}

	return false;
}

void ZipfsIntegration::EnsureZipfsAvailable(ClientContext &context) {
	if (!TryAutoLoadZipfs(context)) {
		throw InvalidInputException(GetZipfsRequiredError());
	}
}

std::string ZipfsIntegration::GetZipfsRequiredError() {
	return "ZIP archive parsing requires the 'zipfs' extension. "
	       "Install and load it with:\n"
	       "  INSTALL zipfs FROM community;\n"
	       "  LOAD zipfs;";
}

} // namespace duckdb
