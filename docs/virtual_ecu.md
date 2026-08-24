# Virtual ECU Design Specification

## 1. Abstract Base Class (`VirtualECU`)

All simulated nodes inherit from `vecu::VirtualECU`. It enforces RAII, lifecycle semantics, and thread safety.

```cpp
class VirtualECU {
public:
    virtual void start();
    virtual void stop();
    virtual void receiveMessage(const CANFrame& frame);
    // Accessors for metrics & status...
protected:
    virtual void update() = 0;
    virtual void generateAndPublish() = 0;
};
```

## 2. Implemented ECU Nodes

### 2.1. Engine ECU (`ENGINE_ECU`)
- **CAN Base ID**: `0x100`
- **Update Frequency**: 10 Hz
- **Simulated Variables**:
  - `RPM`: 0 – 8000 RPM (calculated from load and throttle target)
  - `Engine Temperature`: Ambient (20°C) to 150°C
  - `Engine Load`: 0.0 – 100.0%
  - `Engine State`: `IDLE`, `RUNNING`, `HIGH_LOAD`, `FAULT`
- **Published Frames**:
  - `0x101 ENGINE_RPM`: Bytes [0-1] = RPM (uint16), Byte 2 = Load (uint8), Byte 3 = State
  - `0x102 ENGINE_TEMP`: Bytes [0-1] = Temp × 10 (int16)
  - `0x103 ENGINE_STATUS`: Byte 0 = State, Byte 1 = Fault flags

### 2.2. Brake ECU (`BRAKE_ECU`)
- **CAN Base ID**: `0x200`
- **Update Frequency**: 20 Hz
- **Simulated Variables**:
  - `Brake State`: 0 = Inactive, 1 = Active
  - `Brake Hydraulic Pressure`: 0.0 – 150.0 bar
  - `Vehicle Speed`: 0.0 – 250.0 km/h
- **Published Frames**:
  - `0x201 BRAKE_STATUS`: Byte 0 = Active, Byte 1 = Fault
  - `0x202 BRAKE_PRESSURE`: Bytes [0-1] = Pressure × 10 (uint16)
  - `0x203 VEHICLE_SPEED`: Bytes [0-1] = Speed × 10 (uint16)

### 2.3. Battery ECU (`BATTERY_ECU`)
- **CAN Base ID**: `0x300`
- **Update Frequency**: 5 Hz
- **Simulated Variables**:
  - `State of Charge (SOC)`: 0.0 – 100.0%
  - `Pack Voltage`: 280.0 – 420.0 V (linear-mapped to SOC)
  - `Pack Temperature`: 15.0 – 80.0 °C
  - `Charging State`: Active / Inactive
- **Published Frames**:
  - `0x301 BATTERY_STATUS`: Byte 0 = SOC %, Byte 1 = Charging flag
  - `0x302 BATTERY_TEMP`: Bytes [0-1] = Temp × 10 (int16)
  - `0x303 BATTERY_VOLTAGE`: Bytes [0-1] = Voltage × 10 (uint16)

### 2.4. Steering ECU (`STEERING_ECU`)
- **CAN Base ID**: `0x400`
- **Update Frequency**: 20 Hz
- **Simulated Variables**:
  - `Steering Angle`: −540.0° to +540.0° (signed int16 scaled by 10)
  - `Steering Direction`: Left (-1), Center (0), Right (+1)
- **Published Frames**:
  - `0x401 STEERING_ANGLE`: Bytes [0-1] = Angle × 10 (int16)
  - `0x402 STEERING_STATUS`: Byte 0 = Direction, Byte 1 = Safe Limit Breach

### 2.5. Central Gateway ECU (`GATEWAY_ECU`)
- **CAN Base ID**: `0x500`
- **Update Frequency**: 10 Hz
- **Responsibilities**:
  - Listens to all broadcast CAN frames.
  - Maintains a heartbeat registry (`last_seen_timestamp`) per ECU.
  - Flags timeouts if an ECU fails to publish for > 2000 ms.
- **Published Frames**:
  - `0x501 GATEWAY_STATUS`: Bytes [0-3] = Health flags for Engine, Brake, Battery, Steering
  - `0x502 GATEWAY_STATS`: Bytes [0-3] = Cumulative frame count (uint32)
