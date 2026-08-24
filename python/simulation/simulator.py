"""
Standalone Python vehicle simulator.

When STANDALONE_MODE=true (or the C++ simulator is not running),
this module provides the same simulation in pure Python using
the same config files. All API endpoints work identically.

This is NOT a replacement for the C++ simulator — it's a fallback
that makes the project demonstrable on any machine without a C++ build.
"""

import json
import math
import random
import time
import threading
from pathlib import Path
from typing import Optional
from dataclasses import dataclass, field


def load_config(path: str) -> dict:
    p = Path(path)
    if p.exists():
        return json.loads(p.read_text())
    return {}


# ─── Vehicle State ────────────────────────────────────────────────────────────

@dataclass
class SimState:
    speed_kmh: float = 0.0
    rpm: float = 800.0
    engine_temperature_c: float = 20.0
    engine_load_pct: float = 5.0
    brake_active: bool = False
    brake_pressure_bar: float = 0.0
    battery_pct: float = 84.0
    battery_voltage_v: float = 400.0
    battery_temperature_c: float = 25.0
    battery_charging: bool = False
    steering_angle_deg: float = 0.0
    mode: str = "IDLE"
    timestamp: float = field(default_factory=time.time)


class PythonSimulator:
    """
    Pure-Python vehicle simulator. Runs in a background thread.
    The simulation loop updates state at ~10 Hz.
    """

    VALID_SCENARIOS = [
        "NONE", "NORMAL_DRIVE", "ACCELERATION", "BRAKING",
        "ENGINE_OVERHEAT", "BRAKE_FAILURE", "BATTERY_FAULT",
        "COMMUNICATION_LOSS", "SENSOR_STUCK", "MIXED_FAULT",
    ]

    def __init__(self, config_dir: str = "./config"):
        self._state = SimState()
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None

        # Scenario
        self._scenario = "NORMAL_DRIVE"
        self._active_faults: set[str] = set()

        # CAN log ring buffer
        self._can_log: list[dict] = []
        self._can_lock = threading.Lock()
        self._can_seq = 0

        # Fault events
        self._fault_events: list[dict] = []
        self._fault_lock = threading.Lock()

        # ECU stats
        self._ecu_counts: dict[str, int] = {
            "ENGINE_ECU": 0, "BRAKE_ECU": 0,
            "BATTERY_ECU": 0, "STEERING_ECU": 0, "GATEWAY_ECU": 0,
        }
        self._ecu_last_seen: dict[str, float] = {}
        self._silenced_ecus: set[str] = set()

        # Load config
        self._vehicle_cfg = load_config(f"{config_dir}/vehicle.json")
        self._fault_cfg   = load_config(f"{config_dir}/fault_config.json")

        self._start_time = time.time()
        self._prev_rpm = 800.0

    def start(self):
        if self._running:
            return
        self._running = True
        self._start_time = time.time()
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=2.0)

    def get_state(self) -> dict:
        with self._lock:
            s = self._state
            return {
                "speed_kmh":             round(s.speed_kmh, 2),
                "rpm":                   round(s.rpm, 0),
                "engine_temperature_c":  round(s.engine_temperature_c, 1),
                "engine_load_pct":       round(s.engine_load_pct, 1),
                "brake_active":          s.brake_active,
                "brake_pressure_bar":    round(s.brake_pressure_bar, 1),
                "battery_pct":           round(s.battery_pct, 1),
                "battery_voltage_v":     round(s.battery_voltage_v, 1),
                "battery_temperature_c": round(s.battery_temperature_c, 1),
                "battery_charging":      s.battery_charging,
                "steering_angle_deg":    round(s.steering_angle_deg, 1),
                "mode":                  s.mode,
                "timestamp":             s.timestamp,
            }

    def get_twin_status(self) -> dict:
        now = time.time()
        with self._lock:
            age_ms = (now - self._state.timestamp) * 1000.0
        ecu_sync = {
            ecu: (now - self._ecu_last_seen.get(ecu, 0)) < 3.0
            for ecu in ["ENGINE_ECU", "BRAKE_ECU", "BATTERY_ECU", "STEERING_ECU"]
        }
        any_lost = any(not v for v in ecu_sync.values())
        health = "FAULT" if any_lost else ("WARNING" if age_ms > 1000 else "HEALTHY")
        return {
            "health":            health,
            "synchronized":      age_ms < 1000,
            "update_age_ms":     round(age_ms, 1),
            "frames_processed":  self._can_seq,
            "validation_errors": 0,
            "ecu_sync":          ecu_sync,
        }

    def get_ecu_status(self) -> list[dict]:
        now = time.time()
        result = []
        for ecu_name in ["ENGINE_ECU", "BRAKE_ECU", "BATTERY_ECU", "STEERING_ECU", "GATEWAY_ECU"]:
            last = self._ecu_last_seen.get(ecu_name, 0)
            silenced = ecu_name in self._silenced_ecus
            timed_out = (now - last) > 3.0 if last else True
            count = self._ecu_counts.get(ecu_name, 0)
            elapsed = max(now - self._start_time, 0.001)
            result.append({
                "name":             ecu_name,
                "status":           "FAULT" if "COMMUNICATION_LOSS" in self._active_faults and ecu_name == "ENGINE_ECU"
                                    else ("OFFLINE" if timed_out else "ONLINE"),
                "message_count":    count,
                "messages_per_sec": round(count / elapsed, 1),
                "fault_count":      0,
                "fault_active":     ecu_name in self._silenced_ecus,
                "silenced":         silenced,
            })
        return result

    def get_can_log(self, limit: int = 50) -> list[dict]:
        with self._can_lock:
            return list(reversed(self._can_log[-limit:]))

    def get_fault_events(self, limit: int = 50) -> list[dict]:
        with self._fault_lock:
            return list(reversed(self._fault_events[-limit:]))

    def get_can_stats(self) -> dict:
        elapsed = max(time.time() - self._start_time, 0.001)
        return {
            "total_frames":     self._can_seq,
            "frames_per_second": round(self._can_seq / elapsed, 1),
            "uptime_seconds":   round(elapsed, 1),
            "active_ecus":      5,
        }

    def get_scenario_status(self) -> dict:
        return {
            "active_scenario": self._scenario,
            "active_faults":   list(self._active_faults),
        }

    def set_scenario(self, scenario: str):
        self._scenario = scenario
        self._active_faults.clear()
        self._silenced_ecus.clear()
        # Apply immediate fault configurations
        if scenario == "ENGINE_OVERHEAT":
            self._active_faults.add("ENGINE_OVERHEAT")
        elif scenario == "BRAKE_FAILURE":
            self._active_faults.add("BRAKE_FAILURE")
        elif scenario == "BATTERY_FAULT":
            self._active_faults.add("BATTERY_DEGRADATION")
        elif scenario == "COMMUNICATION_LOSS":
            self._active_faults.add("COMMUNICATION_LOSS")
            self._silenced_ecus.add("ENGINE_ECU")
        elif scenario == "SENSOR_STUCK":
            self._active_faults.add("SENSOR_STUCK")
        elif scenario == "MIXED_FAULT":
            self._active_faults.add("ENGINE_OVERHEAT")
            self._active_faults.add("BATTERY_DEGRADATION")
        elif scenario == "NONE":
            pass

    def start_fault(self, fault_id: str):
        self._active_faults.add(fault_id)
        if fault_id == "COMMUNICATION_LOSS":
            self._silenced_ecus.add("ENGINE_ECU")

    def stop_fault(self, fault_id: str):
        self._active_faults.discard(fault_id)
        if fault_id == "COMMUNICATION_LOSS":
            self._silenced_ecus.discard("ENGINE_ECU")

    # ── Internal simulation loop ───────────────────────────────────────────────

    def _loop(self):
        dt = 0.1  # 10 Hz
        while self._running:
            start = time.time()
            self._tick(dt)
            self._emit_can_frames()
            self._check_faults()
            elapsed = time.time() - start
            time.sleep(max(0, dt - elapsed))

    def _tick(self, dt: float):
        with self._lock:
            s = self._state
            scenario = self._scenario
            faults = self._active_faults.copy()

            # ── Engine ──────────────────────────────────────────────────────────
            if "SENSOR_STUCK" in faults:
                pass  # RPM stays where it was
            elif scenario in ("NORMAL_DRIVE",):
                target_load = 30.0
                target_rpm  = 800 + (target_load / 100) * 7200
                s.rpm = s.rpm + (target_rpm - s.rpm) * 0.05
                s.speed_kmh = 60.0 + math.sin(time.time() * 0.1) * 3.0
                s.engine_load_pct = target_load
            elif scenario == "ACCELERATION":
                s.engine_load_pct = min(s.engine_load_pct + 0.5, 80.0)
                target_rpm = 800 + (s.engine_load_pct / 100) * 7200
                s.rpm = s.rpm + (target_rpm - s.rpm) * 0.08
                s.speed_kmh = min(s.speed_kmh + 2.0 * dt * 10, 150.0)
            elif scenario == "BRAKING":
                s.engine_load_pct = max(s.engine_load_pct - 1.0, 5.0)
                target_rpm = 800 + (s.engine_load_pct / 100) * 7200
                s.rpm = s.rpm + (target_rpm - s.rpm) * 0.1
                s.brake_active = True
                s.brake_pressure_bar = min(s.brake_pressure_bar + 15.0 * dt, 80.0)
                s.speed_kmh = max(s.speed_kmh - 8.0 * dt * 10, 0.0)
            elif scenario in ("ENGINE_OVERHEAT", "MIXED_FAULT"):
                s.engine_load_pct = min(s.engine_load_pct + 0.2, 70.0)
                target_rpm = 800 + (s.engine_load_pct / 100) * 7200
                s.rpm = s.rpm + (target_rpm - s.rpm) * 0.05
            elif scenario == "IDLE":
                s.rpm = 800.0
                s.engine_load_pct = 5.0
                s.speed_kmh = 0.0
            else:
                target_rpm = 800 + (s.engine_load_pct / 100) * 7200
                s.rpm = s.rpm + (target_rpm - s.rpm) * 0.05

            # ── Temperature ──────────────────────────────────────────────────────
            if "ENGINE_OVERHEAT" in faults:
                s.engine_temperature_c = min(s.engine_temperature_c + 0.8 * dt * 10, 150.0)
            elif s.engine_load_pct > 20:
                heat = 0.3 * (s.engine_load_pct / 100) * dt * 10
                cool = 0.15 * dt * 10 if s.engine_temperature_c > 90 else 0
                s.engine_temperature_c = min(s.engine_temperature_c + heat - cool, 200.0)
            else:
                s.engine_temperature_c = max(s.engine_temperature_c - 0.1 * dt * 10, 20.0)

            # ── Battery ──────────────────────────────────────────────────────────
            drain_mult = 20.0 if "BATTERY_DEGRADATION" in faults else 1.0
            drain = 0.003 * (1 + s.engine_load_pct / 100) * drain_mult * dt * 10
            s.battery_pct = max(0.0, s.battery_pct - drain)
            s.battery_voltage_v = 280.0 + (s.battery_pct / 100) * 140.0
            s.battery_temperature_c = min(s.battery_temperature_c + 0.01 * dt * 10, 60.0)

            # ── Steering ─────────────────────────────────────────────────────────
            if scenario == "NORMAL_DRIVE":
                s.steering_angle_deg = 10.0 * math.sin(time.time() * 0.2)
            elif scenario == "ACCELERATION":
                s.steering_angle_deg *= 0.95

            # ── Brake (non-braking scenarios) ────────────────────────────────────
            if scenario != "BRAKING":
                s.brake_active = False
                if "BRAKE_FAILURE" in faults:
                    s.brake_active = True
                    s.brake_pressure_bar = min(s.brake_pressure_bar + 0.5 * dt * 10, 3.0)
                else:
                    s.brake_pressure_bar = max(s.brake_pressure_bar - 5.0 * dt * 10, 0.0)

            # ── Mode ─────────────────────────────────────────────────────────────
            if faults:
                s.mode = "FAULT"
            elif s.brake_active:
                s.mode = "BRAKING"
            elif s.speed_kmh < 1.0:
                s.mode = "IDLE"
            elif s.engine_load_pct > 40:
                s.mode = "ACCELERATING"
            else:
                s.mode = "DRIVING"

            s.timestamp = time.time()

    def _emit_can_frames(self):
        """Generate synthetic CAN frame log entries."""
        now = time.time()
        with self._lock:
            s = self._state

        messages = []
        if "ENGINE_ECU" not in self._silenced_ecus:
            messages += [
                {"can_id": "0x101", "source": "ENGINE_ECU", "message": "ENGINE_RPM",
                 "data": [int(s.rpm) >> 8, int(s.rpm) & 0xFF, int(s.engine_load_pct), 0, 0, 0, 0, 0]},
                {"can_id": "0x102", "source": "ENGINE_ECU", "message": "ENGINE_TEMP",
                 "data": [int(s.engine_temperature_c * 10) >> 8, int(s.engine_temperature_c * 10) & 0xFF, 0, 0, 0, 0, 0, 0]},
            ]
            self._ecu_last_seen["ENGINE_ECU"] = now

        messages += [
            {"can_id": "0x201", "source": "BRAKE_ECU", "message": "BRAKE_STATUS",
             "data": [1 if s.brake_active else 0, 0, 0, 0, 0, 0, 0, 0]},
            {"can_id": "0x301", "source": "BATTERY_ECU", "message": "BATTERY_STATUS",
             "data": [int(s.battery_pct), 1 if s.battery_charging else 0, 0, 0, 0, 0, 0, 0]},
            {"can_id": "0x401", "source": "STEERING_ECU", "message": "STEERING_ANGLE",
             "data": [0, 0, 0, 0, 0, 0, 0, 0]},
        ]

        for ecu in ["BRAKE_ECU", "BATTERY_ECU", "STEERING_ECU", "GATEWAY_ECU"]:
            self._ecu_last_seen[ecu] = now

        with self._can_lock:
            for msg in messages:
                self._can_seq += 1
                self._ecu_counts[msg["source"]] = self._ecu_counts.get(msg["source"], 0) + 1
                frame = {
                    "timestamp": round(now, 3),
                    "can_id":    msg["can_id"],
                    "dlc":       8,
                    "data":      msg["data"],
                    "source":    msg["source"],
                    "message":   msg["message"],
                }
                self._can_log.append(frame)
                if len(self._can_log) > 500:
                    self._can_log.pop(0)

    def _check_faults(self):
        """Evaluate detection rules and emit fault events."""
        with self._lock:
            s = self._state

        events = []
        now = time.time()

        if s.engine_temperature_c >= 130.0:
            events.append({
                "fault_type": "ENGINE_OVERHEAT", "severity": "CRITICAL",
                "source_ecu": "ENGINE_ECU",
                "timestamp": now,
                "description": "Engine temperature critically high",
                "current_value": s.engine_temperature_c,
                "expected_min": 60.0, "expected_max": 100.0,
                "recommended_action": "Reduce engine load immediately",
            })
        elif s.engine_temperature_c >= 110.0:
            events.append({
                "fault_type": "ENGINE_OVERHEAT", "severity": "WARNING",
                "source_ecu": "ENGINE_ECU",
                "timestamp": now,
                "description": "Engine temperature elevated",
                "current_value": s.engine_temperature_c,
                "expected_min": 60.0, "expected_max": 100.0,
                "recommended_action": "Monitor temperature; reduce engine load",
            })

        if s.brake_active and s.brake_pressure_bar < 5.0:
            events.append({
                "fault_type": "BRAKE_FAILURE", "severity": "CRITICAL",
                "source_ecu": "BRAKE_ECU",
                "timestamp": now,
                "description": "Brake active but pressure is too low",
                "current_value": s.brake_pressure_bar,
                "expected_min": 5.0, "expected_max": 150.0,
                "recommended_action": "Inspect brake hydraulic system immediately",
            })

        if s.battery_pct <= 10.0:
            events.append({
                "fault_type": "BATTERY_LOW", "severity": "CRITICAL",
                "source_ecu": "BATTERY_ECU",
                "timestamp": now,
                "description": "Battery critically low",
                "current_value": s.battery_pct,
                "expected_min": 20.0, "expected_max": 100.0,
                "recommended_action": "Charge battery immediately",
            })
        elif s.battery_pct <= 20.0:
            events.append({
                "fault_type": "BATTERY_LOW", "severity": "WARNING",
                "source_ecu": "BATTERY_ECU",
                "timestamp": now,
                "description": "Battery charge low",
                "current_value": s.battery_pct,
                "expected_min": 20.0, "expected_max": 100.0,
                "recommended_action": "Charge battery when possible",
            })

        if events:
            with self._fault_lock:
                # Deduplicate by fault_type (don't flood per-tick)
                existing_types = {e["fault_type"] for e in self._fault_events[-20:]} if self._fault_events else set()
                for ev in events:
                    if ev["fault_type"] not in existing_types:
                        self._fault_events.append(ev)
                        if len(self._fault_events) > 500:
                            self._fault_events.pop(0)


# ─── Global singleton ──────────────────────────────────────────────────────────
_simulator: Optional[PythonSimulator] = None


def get_simulator() -> PythonSimulator:
    global _simulator
    if _simulator is None:
        _simulator = PythonSimulator(config_dir="./config")
        _simulator.start()
    return _simulator
