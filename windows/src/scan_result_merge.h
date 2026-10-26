#ifndef FLUTTER_PLUGIN_UNIVERSAL_BLE_SCAN_RESULT_MERGE_H_
#define FLUTTER_PLUGIN_UNIVERSAL_BLE_SCAN_RESULT_MERGE_H_

#include <cstdint>
#include <optional>

namespace universal_ble {

// Determines whether the scan result is connectable. On Windows, a scan
// response always reports IsConnectable == false; it inherits connectability
// if the device was already seen advertising connectably.
inline bool DeriveConnectableState(bool is_connectable, bool is_scan_response,
                                   bool address_heard_connectable) {
  return is_connectable || (is_scan_response && address_heard_connectable);
}

// State for one device in one scan session. Result is the Pigeon scan model;
// templating keeps the production merge testable without Flutter or WinRT.
template <typename Result>
class ScanResultMergeState {
public:
  std::optional<Result> latest;
  bool pending_advertisement = false;
  bool heard_connectable = false;

  // Every received packet is an observation, including identical repeats.
  // Duplicate suppression belongs to Dart's ScanDeduplicator.
  Result Observe(Result incoming, bool is_connectable, bool is_scan_response) {
    heard_connectable = heard_connectable || is_connectable;
    incoming.set_connectable(DeriveConnectableState(
        is_connectable, is_scan_response, heard_connectable));

    if (latest) {
      if (latest->name() != nullptr &&
          (incoming.name() == nullptr ||
           latest->name()->size() > incoming.name()->size())) {
        incoming.set_name(*latest->name());
      }
      if (incoming.is_paired() == nullptr) {
        incoming.set_is_paired(latest->is_paired());
      }
      if (!HasItems(incoming.services()) && HasItems(latest->services())) {
        incoming.set_services(*latest->services());
      }
    }

    // Parsing may finish in a different order from reception. Deliver late
    // packets, but do not let them replace or consume newer pending data.
    if (latest && latest->timestamp_microseconds() != nullptr &&
        incoming.timestamp_microseconds() != nullptr &&
        *incoming.timestamp_microseconds() <
            *latest->timestamp_microseconds()) {
      return incoming;
    }

    // Cache packet-local telemetry before merging. A response can consume only
    // the immediately preceding advertisement, never another merged response.
    auto previous = latest;
    latest = incoming;
    const bool merge_payload = is_scan_response && pending_advertisement &&
                               previous && IsRecentPair(*previous, incoming);
    pending_advertisement = !is_scan_response;
    if (merge_payload) {
      if (!HasItems(incoming.manufacturer_data_list()) &&
          HasItems(previous->manufacturer_data_list())) {
        incoming.set_manufacturer_data_list(
            *previous->manufacturer_data_list());
      }
      if (!HasItems(incoming.service_data()) &&
          HasItems(previous->service_data())) {
        incoming.set_service_data(previous->service_data());
      }
    }
    return incoming;
  }

private:
  template <typename Items>
  static bool HasItems(const Items *items) {
    return items != nullptr && !items->empty();
  }

  static bool IsRecentPair(const Result &advertisement,
                           const Result &response) {
    if (advertisement.timestamp_microseconds() == nullptr ||
        response.timestamp_microseconds() == nullptr) {
      return false;
    }
    const auto age = *response.timestamp_microseconds() -
                     *advertisement.timestamp_microseconds();
    // A conservative merge window, not a claim that Windows correlates PDUs.
    // Do not revive telemetry after a gap or an out-of-order event.
    return age >= 0 && age <= 1000000;
  }
};

}  // namespace universal_ble

#endif  // FLUTTER_PLUGIN_UNIVERSAL_BLE_SCAN_RESULT_MERGE_H_
