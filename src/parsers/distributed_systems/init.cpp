#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "hdfs_parser.hpp"
#include "spark_parser.hpp"
#include "android_parser.hpp"
#include "zookeeper_parser.hpp"
#include "openstack_parser.hpp"
#include "bgl_parser.hpp"

namespace duckdb {

/**
 * HDFS parser wrapper.
 */
class HdfsParserImpl : public BaseParser {
public:
    HdfsParserImpl()
        : BaseParser("hdfs",
                     "HDFS Parser",
                     ParserCategory::DISTRIBUTED_SYSTEMS,
                     "Hadoop HDFS log output",
                     ParserPriority::HIGH) {
        addAlias("hadoop_hdfs");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    HdfsParser parser_;
};

/**
 * Spark parser wrapper.
 */
class SparkParserImpl : public BaseParser {
public:
    SparkParserImpl()
        : BaseParser("spark",
                     "Spark Parser",
                     ParserCategory::DISTRIBUTED_SYSTEMS,
                     "Apache Spark log output",
                     ParserPriority::HIGH) {
        addAlias("apache_spark");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    SparkParser parser_;
};

/**
 * Android logcat parser wrapper.
 */
class AndroidParserImpl : public BaseParser {
public:
    AndroidParserImpl()
        : BaseParser("android",
                     "Android Parser",
                     ParserCategory::DISTRIBUTED_SYSTEMS,
                     "Android logcat output",
                     ParserPriority::HIGH) {
        addAlias("logcat");
        addAlias("android_logcat");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    AndroidParser parser_;
};

/**
 * Zookeeper parser wrapper.
 */
class ZookeeperParserImpl : public BaseParser {
public:
    ZookeeperParserImpl()
        : BaseParser("zookeeper",
                     "Zookeeper Parser",
                     ParserCategory::DISTRIBUTED_SYSTEMS,
                     "Apache Zookeeper log output",
                     ParserPriority::HIGH) {
        addAlias("zk");
        addAlias("apache_zookeeper");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    ZookeeperParser parser_;
};

/**
 * OpenStack parser wrapper.
 */
class OpenStackParserImpl : public BaseParser {
public:
    OpenStackParserImpl()
        : BaseParser("openstack",
                     "OpenStack Parser",
                     ParserCategory::DISTRIBUTED_SYSTEMS,
                     "OpenStack service log output",
                     ParserPriority::HIGH) {
        addAlias("nova");
        addAlias("neutron");
        addAlias("cinder");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    OpenStackParser parser_;
};

/**
 * BGL (Blue Gene/L) parser wrapper.
 */
class BglParserImpl : public BaseParser {
public:
    BglParserImpl()
        : BaseParser("bgl",
                     "BGL Parser",
                     ParserCategory::DISTRIBUTED_SYSTEMS,
                     "Blue Gene/L supercomputer log output",
                     ParserPriority::HIGH) {
        addAlias("bluegene");
        addAlias("blue_gene_l");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    BglParser parser_;
};

/**
 * Register all distributed systems parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(DistributedSystems);

void RegisterDistributedSystemsParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<HdfsParserImpl>());
    registry.registerParser(make_uniq<SparkParserImpl>());
    registry.registerParser(make_uniq<AndroidParserImpl>());
    registry.registerParser(make_uniq<ZookeeperParserImpl>());
    registry.registerParser(make_uniq<OpenStackParserImpl>());
    registry.registerParser(make_uniq<BglParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(DistributedSystems);

} // namespace duckdb
