"""
SQLite storage layer tests.
"""

import os
import time
import tempfile
import pytest

# Use a temporary DB for tests
os.environ["DATABASE_PATH"] = ":memory:"

from python.storage.database import (
    init_db, get_connection, insert_vehicle_state,
    insert_fault_event, get_vehicle_history, get_fault_history,
    create_session, close_session,
)


@pytest.fixture(autouse=True)
def fresh_db(tmp_path):
    """Each test gets a fresh SQLite database file."""
    db_file = str(tmp_path / f"test_{time.time_ns()}.db")
    os.environ["DATABASE_PATH"] = db_file
    init_db()
    yield



def test_init_db_creates_tables():
    conn = get_connection()
    tables = [r[0] for r in conn.execute(
        "SELECT name FROM sqlite_master WHERE type='table'"
    ).fetchall()]
    assert "vehicle_history" in tables
    assert "fault_events" in tables
    assert "sessions" in tables
    assert "can_messages" in tables


def test_create_session_returns_id():
    session_id = create_session("NORMAL_DRIVE")
    assert isinstance(session_id, int)
    assert session_id > 0


def test_close_session_sets_stopped_at():
    session_id = create_session("NORMAL_DRIVE")
    close_session(session_id)
    conn = get_connection()
    row = conn.execute(
        "SELECT stopped_at FROM sessions WHERE id = ?", (session_id,)
    ).fetchone()
    assert row["stopped_at"] is not None


def test_insert_and_query_vehicle_state():
    session_id = create_session()
    state = {
        "timestamp":             time.time(),
        "speed_kmh":             65.4,
        "rpm":                   2350.0,
        "engine_temperature_c":  72.5,
        "engine_load_pct":       30.0,
        "brake_active":          False,
        "brake_pressure_bar":    0.0,
        "battery_pct":           84.0,
        "battery_voltage_v":     400.0,
        "battery_temperature_c": 25.0,
        "battery_charging":      False,
        "steering_angle_deg":    10.0,
        "mode":                  "DRIVING",
    }
    insert_vehicle_state(session_id, state)
    history = get_vehicle_history(limit=10)
    assert len(history) >= 1
    row = history[0]
    assert abs(row["speed_kmh"] - 65.4) < 0.01
    assert abs(row["rpm"] - 2350.0) < 0.01


def test_insert_multiple_states():
    session_id = create_session()
    for i in range(5):
        insert_vehicle_state(session_id, {
            "timestamp": time.time(),
            "speed_kmh": float(i * 10),
            "rpm": 800.0 + i * 200,
            "engine_temperature_c": 70.0,
            "engine_load_pct": 20.0,
            "brake_active": False,
            "brake_pressure_bar": 0.0,
            "battery_pct": 80.0,
            "battery_voltage_v": 400.0,
            "battery_temperature_c": 25.0,
            "battery_charging": False,
            "steering_angle_deg": 0.0,
            "mode": "DRIVING",
        })
    history = get_vehicle_history(limit=10)
    assert len(history) == 5


def test_insert_and_query_fault_event():
    session_id = create_session()
    event = {
        "timestamp":         time.time(),
        "fault_type":        "ENGINE_OVERHEAT",
        "severity":          "CRITICAL",
        "source_ecu":        "ENGINE_ECU",
        "description":       "Engine temperature critically high",
        "current_value":     135.0,
        "expected_min":      60.0,
        "expected_max":      100.0,
        "recommended_action": "Reduce engine load",
    }
    insert_fault_event(session_id, event)
    faults = get_fault_history(limit=10)
    assert len(faults) >= 1
    f = faults[0]
    assert f["fault_type"] == "ENGINE_OVERHEAT"
    assert f["severity"] == "CRITICAL"


def test_history_limit_respected():
    session_id = create_session()
    for i in range(20):
        insert_vehicle_state(session_id, {
            "timestamp": time.time(),
            "speed_kmh": float(i),
            "rpm": 800.0,
            "engine_temperature_c": 70.0,
            "engine_load_pct": 10.0,
            "brake_active": False,
            "brake_pressure_bar": 0.0,
            "battery_pct": 80.0,
            "battery_voltage_v": 400.0,
            "battery_temperature_c": 25.0,
            "battery_charging": False,
            "steering_angle_deg": 0.0,
            "mode": "IDLE",
        })
    history = get_vehicle_history(limit=5)
    assert len(history) == 5
