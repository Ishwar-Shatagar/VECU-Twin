"""
API integration tests using FastAPI TestClient.
Tests all major endpoints in standalone simulation mode.
"""

import os
os.environ["STANDALONE_MODE"] = "true"
os.environ["DATA_DIR"] = "./data"

import pytest
from fastapi.testclient import TestClient
from python.storage.database import init_db
from python.api.main import app

init_db()
client = TestClient(app)



# ─── Health ───────────────────────────────────────────────────────────────────

def test_health_returns_ok():
    response = client.get("/api/health")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "ok"
    assert "uptime_seconds" in data
    assert data["standalone_mode"] is True


def test_root_returns_info():
    response = client.get("/")
    assert response.status_code == 200
    assert "VECU-Twin" in response.json()["name"]


# ─── Vehicle State ────────────────────────────────────────────────────────────

def test_vehicle_state_returns_required_fields():
    response = client.get("/api/vehicle/state")
    assert response.status_code == 200
    data = response.json()
    assert "speed_kmh" in data
    assert "rpm" in data
    assert "engine_temperature_c" in data
    assert "battery_pct" in data
    assert "brake_active" in data
    assert "steering_angle_deg" in data
    assert "mode" in data
    assert "digital_twin_status" in data


def test_vehicle_state_values_in_range():
    response = client.get("/api/vehicle/state")
    data = response.json()
    assert 0 <= data["speed_kmh"] <= 300
    assert 0 <= data["rpm"] <= 9000
    assert -40 <= data["engine_temperature_c"] <= 200
    assert 0 <= data["battery_pct"] <= 100


def test_vehicle_history_returns_list():
    response = client.get("/api/vehicle/history")
    assert response.status_code == 200
    assert isinstance(response.json(), list)


# ─── ECU Status ───────────────────────────────────────────────────────────────

def test_ecus_returns_list_of_five():
    response = client.get("/api/ecus")
    assert response.status_code == 200
    data = response.json()
    assert isinstance(data, list)
    assert len(data) == 5


def test_ecus_have_required_fields():
    response = client.get("/api/ecus")
    for ecu in response.json():
        assert "name" in ecu
        assert "status" in ecu
        assert "message_count" in ecu
        assert "fault_active" in ecu


def test_get_specific_ecu():
    response = client.get("/api/ecus/ENGINE_ECU")
    assert response.status_code == 200
    assert response.json()["name"] == "ENGINE_ECU"


def test_get_unknown_ecu_returns_404():
    response = client.get("/api/ecus/NONEXISTENT_ECU")
    assert response.status_code == 404


# ─── CAN Messages ────────────────────────────────────────────────────────────

def test_can_messages_returns_list():
    # Wait briefly for simulator to generate messages
    import time; time.sleep(0.5)
    response = client.get("/api/can/messages")
    assert response.status_code == 200
    assert isinstance(response.json(), list)


def test_can_statistics_returns_counts():
    response = client.get("/api/can/statistics")
    assert response.status_code == 200
    data = response.json()
    assert "total_frames" in data
    assert "frames_per_second" in data
    assert "uptime_seconds" in data


# ─── Digital Twin ─────────────────────────────────────────────────────────────

def test_digital_twin_status_has_health():
    response = client.get("/api/digital-twin/status")
    assert response.status_code == 200
    data = response.json()
    assert "health" in data
    assert data["health"] in ["HEALTHY", "WARNING", "FAULT"]


def test_digital_twin_has_ecu_sync():
    response = client.get("/api/digital-twin/status")
    data = response.json()
    assert "ecu_sync" in data
    assert isinstance(data["ecu_sync"], dict)


# ─── Faults ───────────────────────────────────────────────────────────────────

def test_faults_returns_list():
    response = client.get("/api/faults")
    assert response.status_code == 200
    assert isinstance(response.json(), list)


def test_events_endpoint_alias():
    response = client.get("/api/events")
    assert response.status_code == 200


# ─── Scenario Control ─────────────────────────────────────────────────────────

def test_set_valid_scenario():
    response = client.post("/api/simulation/scenario",
                           json={"scenario": "NORMAL_DRIVE"})
    assert response.status_code == 200
    assert response.json()["status"] == "ok"
    assert response.json()["scenario"] == "NORMAL_DRIVE"


def test_set_invalid_scenario_returns_400():
    response = client.post("/api/simulation/scenario",
                           json={"scenario": "INVALID_SCENARIO_XYZ"})
    assert response.status_code == 400


def test_all_scenarios_accepted():
    scenarios = [
        "NORMAL_DRIVE", "ACCELERATION", "BRAKING",
        "ENGINE_OVERHEAT", "BRAKE_FAILURE", "BATTERY_FAULT",
        "COMMUNICATION_LOSS", "SENSOR_STUCK", "MIXED_FAULT", "NONE"
    ]
    for s in scenarios:
        r = client.post("/api/simulation/scenario", json={"scenario": s})
        assert r.status_code == 200, f"Failed for scenario: {s}"


def test_start_simulation():
    response = client.post("/api/simulation/start",
                           json={"scenario": "NORMAL_DRIVE"})
    assert response.status_code == 200


def test_stop_simulation():
    response = client.post("/api/simulation/stop")
    assert response.status_code == 200


def test_start_fault():
    response = client.post("/api/fault/start",
                           json={"fault_id": "ENGINE_OVERHEAT"})
    assert response.status_code == 200
    assert response.json()["status"] == "fault_started"


def test_stop_fault():
    response = client.post("/api/fault/stop",
                           json={"fault_id": "ENGINE_OVERHEAT"})
    assert response.status_code == 200
    assert response.json()["status"] == "fault_stopped"


def test_scenario_status():
    response = client.get("/api/scenario/status")
    assert response.status_code == 200
    data = response.json()
    assert "active_scenario" in data
    assert "active_faults" in data
