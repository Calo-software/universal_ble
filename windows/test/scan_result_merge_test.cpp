// Exercises the production merge with a small value model, without WinRT or
// Flutter. Reception time, RSSI and telemetry must survive repeated packets.
#include "scan_result_merge.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

// The same value/getter/setter contract as UniversalBleScanResult.
struct Packet {
  std::string id = "01:23:45:67:89:ab";
  int rssi = -60;
  std::optional<std::string> packet_name;
  std::optional<bool> paired;
  std::optional<bool> can_connect;
  std::optional<int64_t> time = 1000000;
  std::vector<int> manufacturer;
  std::vector<std::string> service_uuids;
  std::map<std::string, std::vector<int>> payload;

  const std::string *name() const {
    return packet_name ? &*packet_name : nullptr;
  }
  void set_name(const std::string &value) { packet_name = value; }
  const bool *is_paired() const { return paired ? &*paired : nullptr; }
  void set_is_paired(const bool *value) {
    paired = value ? std::optional<bool>(*value) : std::nullopt;
  }
  const bool *connectable() const {
    return can_connect ? &*can_connect : nullptr;
  }
  void set_connectable(bool value) { can_connect = value; }
  const int64_t *timestamp_microseconds() const {
    return time ? &*time : nullptr;
  }
  const std::vector<int> *manufacturer_data_list() const {
    return &manufacturer;
  }
  void set_manufacturer_data_list(const std::vector<int> &value) {
    manufacturer = value;
  }
  const std::vector<std::string> *services() const { return &service_uuids; }
  void set_services(const std::vector<std::string> &value) {
    service_uuids = value;
  }
  const std::map<std::string, std::vector<int>> *service_data() const {
    return &payload;
  }
  void set_service_data(const std::map<std::string, std::vector<int>> *value) {
    payload = *value;
  }
};

bool Check(bool condition, const char *expression, int line) {
  if (condition)
    return true;
  std::cerr << "CHECK failed at line " << line << ": " << expression
            << std::endl;
  return false;
}

}  // namespace

#define CHECK(expression)                            \
  do {                                               \
    if (!Check((expression), #expression, __LINE__)) \
      return 1;                                      \
  } while (false)

int main() {
  using universal_ble::ScanResultMergeState;
  Packet advertisement;
  advertisement.packet_name = "Sensor";
  advertisement.paired = false;
  advertisement.manufacturer = {0x39, 0x06, 0x10};
  advertisement.service_uuids = {"0000181a-0000-1000-8000-00805f9b34fb"};
  advertisement.payload = {{advertisement.service_uuids.front(), {1, 2}}};
  ScanResultMergeState<Packet> state;

  // Identical readings remain observations, with each packet's reception time.
  for (int i = 0; i < 100; ++i) {
    advertisement.time = 1000000 + i * 10000;
    const auto result = state.Observe(advertisement, false, false);
    CHECK(result.id == advertisement.id);
    CHECK(result.time == advertisement.time);
    CHECK(result.rssi == -60);
    CHECK(result.manufacturer == advertisement.manufacturer);
    CHECK(result.payload == advertisement.payload);
    CHECK(!*result.connectable());
  }

  // Same field presence, changed bytes and RSSI: no cache-dedup early return.
  advertisement.time = 3000000;
  advertisement.rssi = -75;
  advertisement.manufacturer.back() = 0x11;
  advertisement.payload.begin()->second = {3, 4};
  auto result = state.Observe(advertisement, true, false);
  CHECK(result.rssi == -75);
  CHECK(result.manufacturer.back() == 0x11);
  CHECK(result.payload.begin()->second == std::vector<int>({3, 4}));
  CHECK(*result.connectable());

  // A recent sparse scan response merges complementary fields once.
  Packet response;
  response.time = 3100000;
  result = state.Observe(response, false, true);
  CHECK(result.packet_name == advertisement.packet_name);
  CHECK(result.paired == advertisement.paired);
  CHECK(result.service_uuids == advertisement.service_uuids);
  CHECK(result.manufacturer == advertisement.manufacturer);
  CHECK(result.payload == advertisement.payload);
  CHECK(result.time == response.time);
  CHECK(*result.connectable());
  response.time = 3200000;
  result = state.Observe(response, false, true);
  CHECK(result.manufacturer.empty());
  CHECK(result.payload.empty());

  // Fresh fields on a response win; neither payload flows into the next ad.
  state = {};
  state.Observe(advertisement, true, false);
  response.manufacturer = {9};
  response.payload = {{"response-service", {8}}};
  result = state.Observe(response, false, true);
  CHECK(result.manufacturer == response.manufacturer);
  CHECK(result.payload == response.payload);
  Packet sparse;
  sparse.time = 3300000;
  result = state.Observe(sparse, false, false);
  CHECK(result.manufacturer.empty());
  CHECK(result.payload.empty());
  CHECK(!*result.connectable());
  result = state.Observe(response, false, true);
  CHECK(*result.connectable());

  // A sparse ordinary advertisement invalidates earlier telemetry too.
  state = {};
  state.Observe(advertisement, true, false);
  sparse.time = 3050000;
  state.Observe(sparse, false, false);
  response.manufacturer.clear();
  response.payload.clear();
  response.time = 3100000;
  result = state.Observe(response, false, true);
  CHECK(result.manufacturer.empty());
  CHECK(result.payload.empty());

  // A late response cannot consume a newer pending advertisement; a late ad
  // cannot replace it. Both still reach the caller with their own time/bytes.
  state = {};
  advertisement.time = 3000000;
  state.Observe(advertisement, true, false);
  Packet late = advertisement;
  late.time = 2000000;
  late.manufacturer = {7};
  result = state.Observe(late, false, false);
  CHECK(result.manufacturer == late.manufacturer);
  CHECK(result.time == late.time);
  response.time = 2500000;
  result = state.Observe(response, false, true);
  CHECK(result.manufacturer.empty());
  response.time = 4000000;  // Inclusive one-second boundary.
  result = state.Observe(response, false, true);
  CHECK(result.manufacturer == advertisement.manufacturer);
  CHECK(result.payload == advertisement.payload);

  // Missing advertisement time also disables complementary payload merging.
  state = {};
  advertisement.time.reset();
  state.Observe(advertisement, true, false);
  response.time = 5000000;
  result = state.Observe(response, false, true);
  CHECK(result.manufacturer.empty());
  CHECK(result.payload.empty());

  // Expired, out-of-order and unknown timestamps cannot revive payloads.
  advertisement.time = 3000000;
  for (const auto time : {4000001LL, 2999999LL}) {
    state = {};
    state.Observe(advertisement, true, false);
    response.time = time;
    result = state.Observe(response, false, true);
    CHECK(result.manufacturer.empty());
    CHECK(result.payload.empty());
  }
  state = {};
  state.Observe(advertisement, true, false);
  response.time.reset();
  result = state.Observe(response, false, true);
  CHECK(result.manufacturer.empty());
  CHECK(result.payload.empty());

  // Clearing per-session state leaves no name, services, payload or connection
  // hint to leak into a new scan. Independent identities have independent
  // state.
  state = {};
  result = state.Observe(sparse, false, true);
  CHECK(!result.packet_name);
  CHECK(!result.paired);
  CHECK(result.service_uuids.empty());
  CHECK(result.manufacturer.empty());
  CHECK(result.payload.empty());
  CHECK(!*result.connectable());
  ScanResultMergeState<Packet> other;
  sparse.id = "01:23:45:67:89:ac";
  result = other.Observe(sparse, false, true);
  CHECK(result.id == sparse.id);
  CHECK(result.manufacturer.empty());

  return 0;
}
