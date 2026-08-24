# Fault Injection & Detection Engine

## 1. Fault Injection Architecture (`FaultEngine`)

The `FaultEngine` coordinates deterministic fault injection profiles across virtual nodes without disrupting background thread concurrency:

| Fault ID | Target Subsystem | Mechanism |
|---|---|---|
| `ENGINE_OVERHEAT` | `ENGINE_ECU` | Multiplies thermal generation coefficient ($5\times$). |
| `ENGINE_RPM_FAULT` | `ENGINE_ECU` | Forces target RPM runaway ($7500\text{ RPM}$). |
| `BRAKE_FAILURE` | `BRAKE_ECU` | Clamps hydraulic pressure response to $5\%$ of nominal. |
| `BATTERY_DEGRADATION`| `BATTERY_ECU` | Accelerates SOC drain rate ($20\times$). |
| `COMMUNICATION_LOSS` | `ENGINE_ECU` | Mutes CAN frame publishing (`silenced = true`). |
| `SENSOR_STUCK` | `ENGINE_ECU` | Freezes output RPM sensor payload to previous value. |
| `ECU_DELAYED` | `BRAKE_ECU` | Injects an artificial transmission delay ($1500\text{ ms}$). |
| `INCONSISTENT_STATE` | Cross-Subsystem | Injects high RPM ($6000\text{ RPM}$) with low vehicle speed ($5\text{ km/h}$). |

## 2. Transparent Rule-Based Detection (`FaultDetector`)

Automotive safety compliance requires auditable, deterministic diagnostic logic:

```cpp
// Rule: Brake Failure Detection
if (state.brake_active && state.brake_pressure_bar < brake_pressure_min_) {
    emitEvent({
        .fault_type = "BRAKE_FAILURE",
        .severity = FaultSeverity::CRITICAL,
        .source_ecu = "BRAKE_ECU",
        .description = "Brake commanded but pressure is critically low",
        .current_value = state.brake_pressure_bar,
        .expected_min = brake_pressure_min_,
        .expected_max = 150.0,
        .recommended_action = "Emergency: inspect brake hydraulic system"
    });
}
```

### Detection Hierarchy & Severity
- **`NORMAL`**: All signals within nominal envelope.
- **`WARNING`**: Signal exceeds operational warning threshold (e.g., $110^\circ\text{C} \le T < 130^\circ\text{C}$).
- **`CRITICAL`**: Immediate hazard to vehicle integrity (e.g., $T \ge 130^\circ\text{C}$, brake hydraulic pressure failure, ECU bus timeout).
