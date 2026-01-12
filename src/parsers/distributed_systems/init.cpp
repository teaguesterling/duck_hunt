#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "hdfs_parser.hpp"
#include "spark_parser.hpp"
#include "android_parser.hpp"
#include "zookeeper_parser.hpp"
#include "openstack_parser.hpp"
#include "bgl_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all distributed systems parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(DistributedSystems);

void RegisterDistributedSystemsParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<HdfsParser>>(
	    "hdfs", "HDFS Parser", ParserCategory::DISTRIBUTED_SYSTEMS, "Hadoop HDFS log output", ParserPriority::HIGH,
	    std::vector<std::string> {"hadoop_hdfs"}, std::vector<std::string> {"distributed", "java"}));

	registry.registerParser(make_uniq<P<SparkParser>>(
	    "spark", "Spark Parser", ParserCategory::DISTRIBUTED_SYSTEMS, "Apache Spark log output", ParserPriority::HIGH,
	    std::vector<std::string> {"apache_spark"}, std::vector<std::string> {"distributed", "java"}));

	registry.registerParser(make_uniq<P<AndroidParser>>(
	    "android", "Android Parser", ParserCategory::DISTRIBUTED_SYSTEMS, "Android logcat output", ParserPriority::HIGH,
	    std::vector<std::string> {"logcat", "android_logcat"}, std::vector<std::string> {"distributed", "mobile"}));

	registry.registerParser(make_uniq<P<ZookeeperParser>>(
	    "zookeeper", "Zookeeper Parser", ParserCategory::DISTRIBUTED_SYSTEMS, "Apache Zookeeper log output",
	    ParserPriority::HIGH, std::vector<std::string> {"zk", "apache_zookeeper"},
	    std::vector<std::string> {"distributed", "java"}));

	registry.registerParser(make_uniq<P<OpenStackParser>>(
	    "openstack", "OpenStack Parser", ParserCategory::DISTRIBUTED_SYSTEMS, "OpenStack service log output",
	    ParserPriority::HIGH, std::vector<std::string> {"nova", "neutron", "cinder"},
	    std::vector<std::string> {"distributed", "cloud", "python"}));

	registry.registerParser(make_uniq<P<BglParser>>(
	    "bgl", "BGL Parser", ParserCategory::DISTRIBUTED_SYSTEMS, "Blue Gene/L supercomputer log output",
	    ParserPriority::HIGH, std::vector<std::string> {"bluegene", "blue_gene_l"},
	    std::vector<std::string> {"distributed"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(DistributedSystems);

} // namespace duckdb
