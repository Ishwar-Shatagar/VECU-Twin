# Software CAN Bus & Arbitration Simulation

## 1. CAN Frame Architecture

A standard classical CAN (ISO 11898-1) frame carries an identifier (11-bit standard or 29-bit extended), a Data Length Code (DLC, 0–8 bytes), and payload bytes.

In VECU-Twin, the software abstraction is structured as:

```cpp
struct CANFrame {
    double timestamp;                  // Unix epoch timestamp
    uint32_t can_id;                   // Standard 11-bit identifier (e.g., 0x101)
    uint8_t dlc;                       // 0–8 payload length
    std::array<uint8_t, 8> data{};     // Fixed 8-byte buffer
    std::string source_ecu;            // Originating node string
    std::string message_name;          // Human-readable frame label
};
```

## 2. Priority-Based Bus Arbitration Simulation

### Real CAN Bus Behavior
In physical CAN, dominant bits (logical `0`) overwrite recessive bits (logical `1`) on the differential bus lines. When multiple ECUs transmit simultaneously at start-of-frame (SOF), the transmitter with the lowest numerical CAN ID wins arbitration non-destructively without losing transmission time.

### Software Simulation Approach
VECU-Twin implements this mechanism in `CANBus::processingLoop()`:
1. Producer threads (Virtual ECUs) push frames into a thread-safe MPSC queue.
2. The consumer loop pops the queue in micro-batches (draining all concurrent arrivals).
3. The batch is sorted ascending by `can_id` (`std::sort` with `a.can_id < b.can_id`).
4. Frames are dispatched to registered subscriber callbacks in strict priority order.

```cpp
// Sort batch by CAN ID ascending (lower = higher priority)
std::sort(batch.begin(), batch.end(),
    [](const CANFrame& a, const CANFrame& b) {
        return a.can_id < b.can_id;
    });
```

## 3. Registered CAN Message Catalog

| CAN ID | Name | Source ECU | Period | Description |
|---|---|---|---|---|
| `0x101` | `ENGINE_RPM` | `ENGINE_ECU` | 100 ms | RPM uint16, Load uint8, State uint8 |
| `0x102` | `ENGINE_TEMP` | `ENGINE_ECU` | 200 ms | Temperature (0.1 °C units) int16 |
| `0x103` | `ENGINE_STATUS` | `ENGINE_ECU` | 500 ms | Operating state & fault flags |
| `0x201` | `BRAKE_STATUS` | `BRAKE_ECU` | 50 ms | Active flag, failure flag |
| `0x202` | `BRAKE_PRESSURE`| `BRAKE_ECU` | 50 ms | Hydraulic pressure (0.1 bar) uint16 |
| `0x203` | `VEHICLE_SPEED` | `BRAKE_ECU` | 100 ms | Road speed (0.1 km/h) uint16 |
| `0x301` | `BATTERY_STATUS`| `BATTERY_ECU` | 200 ms | State of charge (%), charging flag |
| `0x302` | `BATTERY_TEMP` | `BATTERY_ECU` | 500 ms | Pack temp (0.1 °C) int16 |
| `0x303` | `BATTERY_VOLTAGE`| `BATTERY_ECU`| 500 ms | Voltage (0.1 V) uint16 |
| `0x401` | `STEERING_ANGLE`| `STEERING_ECU`| 50 ms | Angle (0.1 deg) int16 |
| `0x402` | `STEERING_STATUS`| `STEERING_ECU`| 200 ms | Direction (-1/0/1), Limit flag |
| `0x501` | `GATEWAY_STATUS`| `GATEWAY_ECU` | 1000 ms | Network health heartbeat flags |
| `0x502` | `GATEWAY_STATS` | `GATEWAY_ECU` | 1000 ms | Total delivered frame counter |
