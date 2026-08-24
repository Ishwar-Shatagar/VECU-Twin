# Testing Guide & Verification

## 1. Automated Test Suites

VECU-Twin maintains a dual-layer test suite covering low-level C++ concurrency primitives and high-level Python API & analytics pipelines.

### 1.1. C++ GoogleTest Suite
Located under `cpp/tests/`:

- `test_can_frame.cpp`: Validates binary encoding/decoding, big-endian conversions, JSON serialization, and priority comparisons.
- `test_can_bus.cpp`: Validates subscriber dispatch, multi-producer concurrency, arbitration ordering, and statistics counters.
- `test_ecu.cpp`: Validates thread lifecycle (`start`/`stop`), internal physics equations, silence flags, and fault reactions.
- `test_vehicle_model.cpp`: Validates kinematic updates, boundary clamping, and FSM mode transition integrity.
- `test_digital_twin.cpp`: Validates frame decoding, sync status tracking, range validation, and JSON status formatting.
- `test_fault_engine.cpp`: Validates fault injection triggers, scenario management, and edge-triggered detection logic.

```bash
# Build and run C++ tests
cmake -S . -B build
cmake --build build --config Release
cd build && ctest --output-on-failure
```

### 1.2. Python Pytest Suite
Located under `python/tests/`:

- `test_api.py`: Tests all 15+ REST endpoints with FastAPI `TestClient` (happy paths, boundary conditions, HTTP 400/404 errors).
- `test_analysis.py`: Tests NumPy/Pandas rolling filters, trend linear regressions, and statistical aggregators.
- `test_storage.py`: Tests SQLite schema initialization, session lifecycle, and write/read history buffers.

```bash
# Run Python tests
pytest python/tests/ -v
```

## 2. End-to-End System Verification Checklist

| Step | Verification Goal | Expected Output |
|---|---|---|
| **1. Startup** | Backend & C++ Sim launch | `GET /api/health` returns `status: "ok"`, ECUs `ONLINE` |
| **2. CAN Telemetry** | Periodic frame generation | `GET /api/can/messages` returns streaming 8-byte frames |
| **3. Twin Sync** | Ingestion validation | `GET /api/digital-twin/status` reports `HEALTHY`, age < 200 ms |
| **4. Scenario Switch** | Acceleration trigger | Speed ramps 0 $\to$ 120 km/h, RPM scales proportionally |
| **5. Fault Injection** | Engine Overheat injection | Temperature > 130°C $\to$ Fault Event with `CRITICAL` severity |
| **6. Fault Recovery** | Scenario reset | Mode transitions back to `DRIVING`/`IDLE`, fault clears |
| **7. Comm Loss** | Mute Engine ECU | Gateway & Twin report `ECU_COMMUNICATION_LOSS` |
