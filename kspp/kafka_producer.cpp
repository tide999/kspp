#include "kafka_producer.h"

#include <iostream>

namespace csi {
kafka_producer::kafka_producer(std::string brokers, std::string topic) :
  _topic(topic),
  _producer(NULL),
  _rd_topic(NULL),
  _msg_cnt(0),
  _msg_bytes(0) {
  /*
  * Create configuration objects
  */
  RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

  /*
  * Set configuration properties
  */
  std::string errstr;
  conf->set("metadata.broker.list", brokers, errstr);

  //ExampleEventCb ex_event_cb;
  //conf->set("event_cb", &ex_event_cb, errstr);

  conf->set("default_topic_conf", tconf, errstr);
  delete tconf;

  /*
  * Create consumer using accumulated global configuration.
  */
  _producer = RdKafka::Producer::create(conf, errstr);
  if (!_producer) {
    std::cerr << "Failed to create producer: " << errstr << std::endl;
    exit(1);
  }

  delete conf;
  std::cout << "% Created producer " << _producer->name() << std::endl;

  RdKafka::Conf *tconf2 = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
  _rd_topic = RdKafka::Topic::create(_producer, _topic, tconf2, errstr);
  delete tconf2;

  if (!_rd_topic) {
    std::cerr << "Failed to create topic: " << errstr << std::endl;
    exit(1);
  }
}

kafka_producer::~kafka_producer() {
  /*
  * Stop producer??
  */
  while (_producer->outq_len() > 0) {
    std::cerr << "Waiting for " << _producer->outq_len() << std::endl;
    _producer->poll(1000);
  }
  delete _producer;
  delete _rd_topic;
  std::cerr << "% produced " << _msg_cnt << " messages (" << _msg_bytes << " bytes)" << std::endl;
}

int kafka_producer::produce(int32_t partition, rdkafka_memory_management_mode mode, void* key, size_t keysz, void* value, size_t valuesz) {
  RdKafka::ErrorCode resp = _producer->produce(_rd_topic, partition, (int) mode, value, valuesz, key, keysz, NULL);
  if (resp != RdKafka::ERR_NO_ERROR) {
    std::cerr << "% Produce failed: " << RdKafka::err2str(resp) << std::endl;
    return (int) resp;
  }

  _msg_cnt++;
  _msg_bytes += valuesz;
  return 0;
}
}; // namespace