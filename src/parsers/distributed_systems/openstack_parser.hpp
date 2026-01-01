#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

// Parser for OpenStack logs (Nova, Neutron, etc.)
// Format: [logfile] YYYY-MM-DD HH:MM:SS.mmm PID LEVEL component [req-context] message
// Example: nova-api.log.1.2017-05-16_13:53:08 2017-05-16 00:00:00.008 25746 INFO nova.osapi_compute.wsgi.server
// [req-...] message
class OpenStackParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "openstack";
	}
	std::string getName() const override {
		return "openstack";
	}
	int getPriority() const override {
		return 62;
	}
	std::string getCategory() const override {
		return "distributed_systems";
	}
};

} // namespace duckdb
