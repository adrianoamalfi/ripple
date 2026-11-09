#include <ArduinoJson.h>
#include <string.h>
#include <unity.h>

#include <set>
#include <string>

#include "ripple_ha.h"

using namespace ripple::ha;

namespace {

constexpr size_t kPayloadCap = 800;  // the firmware's static buffer

struct Msg {
  char topic[128];
  char payload[kPayloadCap];
};

bool build(const Device& d, Entity e, Msg& m, const char* prefix = "homeassistant",
           const char* fw = "0.1.0", const char* url = "http://192.168.1.50/") {
  return discovery(d, e, prefix, fw, url, m.topic, sizeof(m.topic), m.payload, sizeof(m.payload));
}

Device device() {
  Device d;
  makeDevice(d, 0x4f7ddc);
  return d;
}

std::string topicOf(const Device& d, Topic t) {
  char buf[96];
  TEST_ASSERT_TRUE(topic(d, t, buf, sizeof(buf)) > 0);
  return buf;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_device_ids_come_from_the_chip_id() {
  Device d = device();
  TEST_ASSERT_EQUAL_STRING("ripple_4f7ddc", d.id);
  TEST_ASSERT_EQUAL_STRING("ripple/ripple_4f7ddc", d.base);
  TEST_ASSERT_EQUAL_STRING("Ripple 4f7ddc", d.name);

  Device small;
  makeDevice(small, 0x12);  // zero padded, so ids keep a fixed width
  TEST_ASSERT_EQUAL_STRING("ripple_000012", small.id);
}

void test_topics() {
  Device d = device();
  TEST_ASSERT_EQUAL_STRING("ripple/ripple_4f7ddc/status", topicOf(d, Topic::Status).c_str());
  TEST_ASSERT_EQUAL_STRING("ripple/ripple_4f7ddc/motion", topicOf(d, Topic::Motion).c_str());
  TEST_ASSERT_EQUAL_STRING("ripple/ripple_4f7ddc/threshold/set",
                           topicOf(d, Topic::ThresholdSet).c_str());
  TEST_ASSERT_EQUAL_STRING("ripple/ripple_4f7ddc/calibrate/set",
                           topicOf(d, Topic::CalibrateSet).c_str());
}

void test_topic_reports_truncation() {
  Device d = device();
  char tiny[8];
  TEST_ASSERT_EQUAL(0, topic(d, Topic::Status, tiny, sizeof(tiny)));
}

void test_every_entity_is_valid_json_with_a_unique_id() {
  Device d = device();
  std::set<std::string> ids;
  for (uint8_t i = 0; i < kEntityCount; i++) {
    Msg m;
    TEST_ASSERT_TRUE(build(d, static_cast<Entity>(i), m));
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(!deserializeJson(doc, m.payload), m.payload);

    const std::string uid = doc["unique_id"] | "";
    TEST_ASSERT_TRUE(uid.rfind("ripple_4f7ddc_", 0) == 0);
    TEST_ASSERT_TRUE(ids.insert(uid).second);  // no duplicates

    TEST_ASSERT_EQUAL_STRING("ripple_4f7ddc", doc["device"]["identifiers"][0] | "");
    TEST_ASSERT_EQUAL_STRING("Ripple 4f7ddc", doc["device"]["name"] | "");
    TEST_ASSERT_EQUAL_STRING("0.1.0", doc["device"]["sw_version"] | "");
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50/", doc["device"]["configuration_url"] | "");
    TEST_ASSERT_TRUE(doc["has_entity_name"] | false);
    TEST_ASSERT_TRUE(strlen(m.payload) < kPayloadCap - 100);  // keeps headroom
  }
  TEST_ASSERT_EQUAL(kEntityCount, ids.size());
}

void test_config_topics_follow_the_ha_scheme() {
  Device d = device();
  Msg m;
  TEST_ASSERT_TRUE(build(d, Entity::Motion, m));
  TEST_ASSERT_EQUAL_STRING("homeassistant/binary_sensor/ripple_4f7ddc/motion/config", m.topic);
  TEST_ASSERT_TRUE(build(d, Entity::Threshold, m));
  TEST_ASSERT_EQUAL_STRING("homeassistant/number/ripple_4f7ddc/threshold/config", m.topic);
  TEST_ASSERT_TRUE(build(d, Entity::Calibrate, m));
  TEST_ASSERT_EQUAL_STRING("homeassistant/button/ripple_4f7ddc/calibrate/config", m.topic);
  TEST_ASSERT_TRUE(build(d, Entity::Rssi, m, "custom/prefix"));
  TEST_ASSERT_EQUAL_STRING("custom/prefix/sensor/ripple_4f7ddc/rssi/config", m.topic);
}

void test_motion_and_presence_need_detection_to_be_available() {
  Device d = device();
  Msg m;
  for (Entity e : {Entity::Motion, Entity::Presence, Entity::Threshold}) {
    TEST_ASSERT_TRUE(build(d, e, m));
    JsonDocument doc;
    deserializeJson(doc, m.payload);
    TEST_ASSERT_EQUAL(2, doc["availability"].size());
    TEST_ASSERT_EQUAL_STRING(topicOf(d, Topic::Status).c_str(), doc["availability"][0]["topic"] | "");
    TEST_ASSERT_EQUAL_STRING(topicOf(d, Topic::Detecting).c_str(), doc["availability"][1]["topic"] | "");
    TEST_ASSERT_EQUAL_STRING("all", doc["availability_mode"] | "");
  }
  for (Entity e : {Entity::Rssi, Entity::Fluctuation, Entity::Calibrate}) {
    TEST_ASSERT_TRUE(build(d, e, m));
    JsonDocument doc;
    deserializeJson(doc, m.payload);
    TEST_ASSERT_EQUAL_STRING(topicOf(d, Topic::Status).c_str(), doc["availability_topic"] | "");
  }
}

void test_entity_types_and_topics() {
  Device d = device();
  Msg m;
  JsonDocument doc;

  TEST_ASSERT_TRUE(build(d, Entity::Motion, m));
  deserializeJson(doc, m.payload);
  TEST_ASSERT_EQUAL_STRING("motion", doc["device_class"] | "");
  TEST_ASSERT_EQUAL_STRING(topicOf(d, Topic::Motion).c_str(), doc["state_topic"] | "");
  TEST_ASSERT_TRUE(doc["command_topic"].isNull());

  TEST_ASSERT_TRUE(build(d, Entity::Presence, m));
  deserializeJson(doc, m.payload);
  TEST_ASSERT_EQUAL_STRING("occupancy", doc["device_class"] | "");

  TEST_ASSERT_TRUE(build(d, Entity::Rssi, m));
  deserializeJson(doc, m.payload);
  TEST_ASSERT_EQUAL_STRING("signal_strength", doc["device_class"] | "");
  TEST_ASSERT_EQUAL_STRING("dBm", doc["unit_of_measurement"] | "");
  TEST_ASSERT_EQUAL_STRING("diagnostic", doc["entity_category"] | "");

  TEST_ASSERT_TRUE(build(d, Entity::Fluctuation, m));
  deserializeJson(doc, m.payload);
  TEST_ASSERT_EQUAL_STRING("dB\xC2\xB2", doc["unit_of_measurement"] | "");  // dB squared, UTF-8

  TEST_ASSERT_TRUE(build(d, Entity::Threshold, m));
  deserializeJson(doc, m.payload);
  TEST_ASSERT_EQUAL_STRING(topicOf(d, Topic::Threshold).c_str(), doc["state_topic"] | "");
  TEST_ASSERT_EQUAL_STRING(topicOf(d, Topic::ThresholdSet).c_str(), doc["command_topic"] | "");
  TEST_ASSERT_EQUAL_STRING("config", doc["entity_category"] | "");

  TEST_ASSERT_TRUE(build(d, Entity::Calibrate, m));
  deserializeJson(doc, m.payload);
  TEST_ASSERT_EQUAL_STRING(topicOf(d, Topic::CalibrateSet).c_str(), doc["command_topic"] | "");
  TEST_ASSERT_EQUAL_STRING("PRESS", doc["payload_press"] | "");
  TEST_ASSERT_TRUE(doc["state_topic"].isNull());  // a button has no state
}

void test_hostile_text_cannot_break_the_json() {
  Device d = device();
  Msg m;
  TEST_ASSERT_TRUE(build(d, Entity::Motion, m, "homeassistant", "1.0\"}],\n evil", "http://x/\"y"));
  JsonDocument doc;
  TEST_ASSERT_TRUE_MESSAGE(!deserializeJson(doc, m.payload), m.payload);
  TEST_ASSERT_TRUE(strchr(m.payload, '\n') == nullptr);
}

void test_small_buffers_fail_cleanly() {
  Device d = device();
  char topicBuf[128];
  char tiny[40];
  TEST_ASSERT_FALSE(discovery(d, Entity::Motion, "homeassistant", "0.1.0", "http://x/",
                              topicBuf, sizeof(topicBuf), tiny, sizeof(tiny)));
  TEST_ASSERT_EQUAL('\0', tiny[0]);  // never leaves a half-written payload
  char payload[kPayloadCap];
  char tinyTopic[10];
  TEST_ASSERT_FALSE(discovery(d, Entity::Motion, "homeassistant", "0.1.0", "http://x/",
                              tinyTopic, sizeof(tinyTopic), payload, sizeof(payload)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_device_ids_come_from_the_chip_id);
  RUN_TEST(test_topics);
  RUN_TEST(test_topic_reports_truncation);
  RUN_TEST(test_every_entity_is_valid_json_with_a_unique_id);
  RUN_TEST(test_config_topics_follow_the_ha_scheme);
  RUN_TEST(test_motion_and_presence_need_detection_to_be_available);
  RUN_TEST(test_entity_types_and_topics);
  RUN_TEST(test_hostile_text_cannot_break_the_json);
  RUN_TEST(test_small_buffers_fail_cleanly);
  return UNITY_END();
}
