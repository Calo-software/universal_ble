import 'package:flutter_test/flutter_test.dart';
import 'package:universal_ble/src/universal_ble.g.dart';
import 'package:universal_ble/src/universal_ble_pigeon/universal_ble_pigeon_channel.dart';

void main() {
  test('Android clocks survive the native codec, public conversion and copy', () {
    final native = UniversalBleScanResult(
      name: 'clock fixture',
      deviceId: 'AA:BB:CC:DD:EE:FF',
      isPaired: false,
      rssi: -62,
      manufacturerDataList: [],
      serviceData: {},
      services: [],
      timestamp: 1792600000000,
      timestampMicroseconds: 1792600000000123,
      connectable: true,
      nativeTimestampNanos: 1234567890123,
      nativeCallbackElapsedRealtimeNanos: 1234567990123,
      nativeCallbackTimeMillis: 1792600000000,
    );
    final decoded = UniversalBleScanResult.decode(native.encode());
    final result = decoded.toBleDevice().copyWith(deviceId: 'aa:bb:cc:dd:ee:ff');
    expect(result.nativeTimestampNanos, 1234567890123);
    expect(result.nativeCallbackElapsedRealtimeNanos, 1234567990123);
    expect(result.nativeCallbackTimeMillis, 1792600000000);
    expect(result.timestampMicroseconds, 1792600000000123);
    expect(result.deviceId, 'aa:bb:cc:dd:ee:ff');
  });

  test('other platforms do not invent Android clocks', () {
    final result = UniversalBleScanResult(
      name: null,
      deviceId: 'AA:BB:CC:DD:EE:FF',
      isPaired: null,
      rssi: -62,
      manufacturerDataList: [],
      serviceData: {},
      services: [],
      timestamp: null,
      timestampMicroseconds: null,
      connectable: null,
    ).toBleDevice();
    expect(result.nativeTimestampNanos, isNull);
    expect(result.nativeCallbackElapsedRealtimeNanos, isNull);
    expect(result.nativeCallbackTimeMillis, isNull);
  });
}
