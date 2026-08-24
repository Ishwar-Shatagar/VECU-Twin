# Digital Twin Implementation & Synchronization

## 1. Concept: Digital Twin vs Ground-Truth Model

A crucial engineering principle implemented in VECU-Twin is the distinction between the **Vehicle Model** (the internal physics simulation) and the **Digital Twin** (the telemetry-derived synchronized representation):

```
[ Vehicle Physical Model ] ---> Internal Physics Updates
              |
              v (Generates)
       [ Virtual ECUs ]
              |
              v (Transmits)
       [ Software CAN Bus ]
              |
              v (Decodes independently)
      [ Digital Twin ] ---> Validates, checks sync, tracks latency
```

- **Vehicle Model**: Ground-truth engine calculating equations of motion, thermal exchange, and electrical drain.
- **Digital Twin**: Operates exclusively as a consumer of CAN messages. It parses CAN ID byte buffers, bounds-checks parameters, measures freshness latency, and infers system state.

## 2. Health & Synchronization Model

The Digital Twin evaluates overall telemetry integrity into three distinct tiers:

1. **`HEALTHY`**:
   - Telemetry age < 1000 ms.
   - All monitored ECUs have published within the timeout window (2000 ms).
   - Zero critical validation failures.
2. **`WARNING`**:
   - Telemetry update age between 1000 ms and 2000 ms.
   - Non-critical sensor warnings (e.g., elevated temperatures, minor range bounds clamped).
3. **`FAULT`**:
   - One or more ECUs have timed out (> 2000 ms silence).
   - Critical physical faults detected in received payload (overheating > 130°C, brake hydraulic failure).

## 3. Data Freshness Tracking

```cpp
struct SyncStatus {
    double last_update_s;
    double update_age_ms;
    bool   synchronized;
    std::unordered_map<std::string, double> ecu_last_seen;
    std::unordered_map<std::string, bool>   ecu_synced;
};
```
Every incoming frame updates the timestamp mapping for that ECU's identifier. The status endpoint exposes this map for dashboard visualization.
