#include <boost/filesystem.hpp>
#include "join.h"
#include "kstream.h"
#include "ktable.h"
#include "kafka_sink.h"
#include "kafka_source.h"
#pragma once

namespace csi {
template<class codec>
class topology_builder
{
  public:
  topology_builder(std::string brokers, std::string storage_path, std::shared_ptr<codec> default_codec) :
    _brokers(brokers),
    _default_codec(default_codec),
    _storage_path(storage_path) {
    boost::filesystem::create_directories(boost::filesystem::path(storage_path));
  }

  template<class K, class streamV, class tableV, class R>
  std::shared_ptr<left_join<K, streamV, tableV, R>> create_left_join(std::string tag, std::string stream_topic, std::string table_topic, int32_t partition, typename csi::left_join<K, streamV, tableV, R>::value_joiner value_joiner) {
    auto stream = std::make_shared<csi::kstream<K, streamV, codec>>(tag, _brokers, stream_topic, partition, _storage_path, _default_codec);
    auto table = std::make_shared<csi::ktable<K, tableV, codec>>(tag, _brokers, table_topic, partition, _storage_path, _default_codec);
    return std::make_shared<csi::left_join<K, streamV, tableV, R>>(stream, table, value_joiner);
  }

  template<class K, class V>
  std::shared_ptr<csi::sink<K, V>> create_kafka_sink(std::string topic, int32_t partition) {
    return std::make_shared<csi::kafka_sink<K, V, codec>>(_brokers, topic, _default_codec, [partition](const K&) { return partition; });
  }

  template<class K, class V>
  std::shared_ptr<csi::sink<K, V>> create_kafka_sink(std::string topic, std::function<uint32_t(const K& key)> partitioner) {
    return std::make_shared<csi::kafka_sink<K, V, codec>>(_brokers, topic, _default_codec, partitioner);
  }

  template<class K, class V>
  std::shared_ptr<csi::ksource<K, V>> create_kafka_source(std::string topic, int32_t partition) {
    return std::make_shared<csi::kafka_source<K, V, codec>>(_brokers, topic, partition, _default_codec);
  }

  template<class K, class V>
  std::shared_ptr<csi::ksource<K, V>> create_kstream(std::string tag, std::string topic, int32_t partition) {
    return std::make_shared<csi::kstream<K, V, codec>>(tag, _brokers, topic, partition, _storage_path, _default_codec);
  }

  template<class K, class V>
  std::shared_ptr<csi::ksource<K, V>> create_ktable(std::string tag, std::string topic, int32_t partition) {
    return std::make_shared<csi::ktable<K, V, codec>>(tag, _brokers, topic, partition, _storage_path, _default_codec);
  }

  //csi::kstream<int64_t, page_view_data, csi::binary_codec>  pageviews("example3-pageviews_tmp", "localhost", "kspp_PageViews", PARTITION, "C:\\tmp", codec);


  //csi::kafka_source<int64_t, user_profile_data, csi::binary_codec> pageviews_source("example3-pageviews_source", "localhost", "kspp_PageViews", PARTITION, "C:\\tmp", codec);

  private:
  std::string             _brokers;
  std::shared_ptr<codec>  _default_codec;
  std::string             _storage_path;
};




};