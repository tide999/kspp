#include <string>
#include <chrono>
#include <cmath>
#include <functional>
#include "metric20_key_t.h"
#include <prometheus/registry.h>
#pragma once

#define KSPP_KEY_TYPE_TAG "key_type"
#define KSPP_VALUE_TYPE_TAG "value_type"
#define KSPP_PROCESSOR_TYPE_TAG "processor_type"
#define KSPP_COMPONENT_TYPE_TAG "component"
#define KSPP_DESTINATION_HOST   "destination_host"
//#define KSPP_PROCESSOR_INSTANCE_NAME_TAG "processor_instance_name"
#define KSPP_PARTITION_TAG "partition"
#define KSPP_TOPIC_TAG "topic"

namespace kspp {

  inline metrics20::avro::metrics20_key_tags_t make_metrics_tag(std::string k, std::string v){
    metrics20::avro::metrics20_key_tags_t t;
    t.key=k;
    t.value=v;
    return t;
  }

  struct metric {
    enum mtype { RATE, COUNT, GAUGE, COUNTER, TIMESTAMP, SUMMARY }; // http://metrics20.org/spec/ and some prometheus

    metric(std::string what, mtype mt, std::string unit)
        : _name("kspp_" + what) {
      switch (mt) {
        case RATE: add_tag("mtype", "rate"); break;
        case COUNT: add_tag("mtype", "count"); break;
        case GAUGE: add_tag("mtype", "gauge"); break;
        case COUNTER: add_tag("mtype", "counter"); break;
        case TIMESTAMP: add_tag("mtype", "timestamp"); break;
        case SUMMARY: add_tag("mtype", "summary"); break;
      }
      add_tag("unit", unit);
    }

    virtual double value() const = 0;

    inline std::string name() const {
      return _name;
    }

    virtual void finalize_tags(std::shared_ptr<prometheus::Registry> registry)=0;
    /*{
      _derived_tags.clear();
      for (std::map<std::string, std::string>::const_iterator i =_real_tags.begin(); i!=_real_tags.end(); ++i)
      {
        _derived_tags += i->first + "=" + i->second;
        if (std::next(i) != _real_tags.end())
          _derived_tags += ",";
      }
    }*/


    /*inline std::string tags() const {
      return _derived_tags;
    }
    */

    void add_tag(std::string key, std::string val)
    {
      _real_tags[key]=val;
    }

    /*
     * static bool tag_order_fn (const metrics20::avro::metrics20_key_tags_t& i,const metrics20::avro::metrics20_key_tags_t& j) { return (i.key<j.key); }

    inline void sort_tags() {
      std::sort(_real_tags.begin(), _real_tags.end(), tag_order_fn);
    }
     */

    std::string _name; // what
    //std::string _derived_tags;
    std::map<std::string, std::string> _real_tags;
  };

  struct metric_counter : public metric {
    metric_counter(std::string what, std::string unit)
        : metric(what, COUNTER, unit)
        ,  _counter(nullptr) {
    }

    void finalize_tags(std::shared_ptr<prometheus::Registry> registry) override
    {
      /*_derived_tags.clear();
      for (std::map<std::string, std::string>::const_iterator i =_real_tags.begin(); i!=_real_tags.end(); ++i)
      {
        _derived_tags += i->first + "=" + i->second;
        if (std::next(i) != _real_tags.end())
          _derived_tags += ",";
      }
       */

      auto& counter_family = prometheus::BuildCounter().Name(_name).Register(*registry);
      _counter = &counter_family.Add(_real_tags);
    }

    virtual double value() const {
      return _counter->Value();
    }

    inline metric_counter &operator++() {
      _counter->Increment();
      return *this;
    }

    inline metric_counter &operator+=(double v) {
      _counter->Increment(v);
      return *this;
    }

    prometheus::Counter* _counter;
  };

  struct metric_gauge : public metric {
    metric_gauge(std::string what, std::string unit)
        : metric(what, GAUGE, unit)
        ,  _gauge(nullptr) {
    }

    void finalize_tags(std::shared_ptr<prometheus::Registry> registry) override
    {
      /*
       * _derived_tags.clear();
      for (std::map<std::string, std::string>::const_iterator i =_real_tags.begin(); i!=_real_tags.end(); ++i)
      {
        _derived_tags += i->first + "=" + i->second;
        if (std::next(i) != _real_tags.end())
          _derived_tags += ",";
      }
       */

      auto& family = prometheus::BuildGauge().Name(_name).Register(*registry);
      _gauge = &family.Add(_real_tags);
    }

    void set(double v) {
      _gauge->Set(v);
    }

    virtual double value() const {
     _gauge->Value();
    }

    void clear() {
      _gauge->Set(0);
    }

    prometheus::Gauge* _gauge;
  };

  struct metric_streaming_lag : public metric {
    metric_streaming_lag()
        : metric("streaming_lag", GAUGE, "ms")
        ,  _gauge(nullptr) {

    }

    void finalize_tags(std::shared_ptr<prometheus::Registry> registry) override
    {
      /*_derived_tags.clear();
      for (std::map<std::string, std::string>::const_iterator i =_real_tags.begin(); i!=_real_tags.end(); ++i)
      {
        _derived_tags += i->first + "=" + i->second;
        if (std::next(i) != _real_tags.end())
          _derived_tags += ",";
      }
      */

      auto& family = prometheus::BuildGauge().Name(_name).Register(*registry);
      _gauge = &family.Add(_real_tags);
    }

    inline void add_event_time(int64_t tick, int64_t event_time) {
      if (event_time > 0)
        _gauge->Set(tick - event_time);
      else
        _gauge->Set(-1.0);
    }

    virtual double value() const {
      _gauge->Value();
    }

  private:
    prometheus::Gauge* _gauge;
  };

  struct metric_summary : public metric {
    metric_summary(std::string what, std::string unit, const std::vector<float>& quantiles={0.99})
        : metric(what, SUMMARY, unit)
        ,  _quantiles(quantiles)
        ,  _summary(nullptr) {
      add_tag("window", "60s");
    }

    void finalize_tags(std::shared_ptr<prometheus::Registry> registry) override
    {
     /* _derived_tags.clear();
      for (std::map<std::string, std::string>::const_iterator i =_real_tags.begin(); i!=_real_tags.end(); ++i)
      {
        _derived_tags += i->first + "=" + i->second;
        if (std::next(i) != _real_tags.end())
          _derived_tags += ",";
      }
      */

      std::vector<prometheus::detail::CKMSQuantiles::Quantile> q;
      for (auto i : _quantiles)
        q.emplace_back(i, 0.05);
      auto& family = prometheus::BuildSummary().Name(_name).Register(*registry);
      _summary = &family.Add(_real_tags, q, std::chrono::seconds{60}, 5);
    }

    inline void observe(double v) {
      _summary->Observe(v);
    }

    virtual double value() const {
      return NAN;
    }

  private:
    const std::vector<float> _quantiles;
    prometheus::Summary* _summary;
  };


  /*
   * struct metric_evaluator : public metric {
    using evaluator = std::function<int64_t(void)>;

    metric_evaluator(std::string what,  mtype mt, std::string unit, evaluator f)
        : metric(what, mt, unit), _f(f) {}

    virtual double value() const {
      return _f();
    }

  private:
    evaluator _f;
  };
   */
}
