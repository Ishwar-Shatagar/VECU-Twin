"""
FastAPI route handlers for all VECU-Twin API endpoints.

Data source strategy:
  - STANDALONE_MODE=true:  reads from PythonSimulator (no C++ binary needed)
  - STANDALONE_MODE=false: reads from JSON files written by vecu_sim binary

All endpoints return consistent schemas regardless of data source.
"""

import json
import os
import time
from pathlib import Path
from typing import Optional

from fastapi import APIRouter, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from python.api.schemas import (
    VehicleStateSchema, DigitalTwinStatusSchema, ECUStatusSchema,
    CANFrameSchema, CANStatisticsSchema, FaultEventSchema,
    ScenarioRequest, FaultRequest, SimulationStartRequest,
    HealthResponse, ScenarioStatusSchema,
)

router = APIRouter()

DATA_DIR        = os.getenv("DATA_DIR", "./data")
STANDALONE_MODE = os.getenv("STANDALONE_MODE", "true").lower() == "true"

_start_time = time.time()


# ─── Data source helpers ──────────────────────────────────────────────────────

def _read_json_file(filename: str, default=None):
    """Read a JSON file written by the C++ simulator."""
    path = Path(DATA_DIR) / filename
    try:
        return json.loads(path.read_text())
    except Exception:
        return default if default is not None else {}


def _write_command(cmd: dict):
    """Append a command to the commands.json file (polled by C++ sim)."""
    path = Path(DATA_DIR) / "commands.json"
    try:
        existing = json.loads(path.read_text()) if path.exists() else []
        existing.append(cmd)
        path.write_text(json.dumps(existing))
    except Exception:
        path.write_text(json.dumps([cmd]))


def _get_sim():
    """Get the Python simulator (standalone mode only)."""
    from python.simulation.simulator import get_simulator
    return get_simulator()


# ─── Health ───────────────────────────────────────────────────────────────────

@router.get("/health", response_model=HealthResponse, tags=["System"])
def get_health():
    """API health check. Returns server status and connection info."""
    sim_connected = False
    if not STANDALONE_MODE:
        state_file = Path(DATA_DIR) / "vehicle_state.json"
        sim_connected = state_file.exists() and (
            time.time() - state_file.stat().st_mtime < 5.0
        )
    return HealthResponse(
        status="ok",
        uptime_seconds=round(time.time() - _start_time, 1),
        standalone_mode=STANDALONE_MODE,
        simulator_connected=sim_connected,
    )


# ─── Vehicle State ────────────────────────────────────────────────────────────

@router.get("/vehicle/state", response_model=VehicleStateSchema, tags=["Vehicle"])
def get_vehicle_state():
    """Current aggregated vehicle state from all ECUs."""
    if STANDALONE_MODE:
        state = _get_sim().get_state()
        twin = _get_sim().get_twin_status()
        state["digital_twin_status"] = twin.get("health", "HEALTHY")
        return VehicleStateSchema(**state)

    state = _read_json_file("vehicle_state.json")
    twin  = _read_json_file("digital_twin_status.json", {"health": "HEALTHY"})
    state["digital_twin_status"] = twin.get("health", "HEALTHY")
    return VehicleStateSchema(**state)


@router.get("/vehicle/history", tags=["Vehicle"])
def get_vehicle_history(limit: int = 100):
    """Vehicle state history from SQLite (persisted snapshots)."""
    from python.storage.database import get_vehicle_history
    return get_vehicle_history(limit=limit)


# ─── ECU Status ───────────────────────────────────────────────────────────────

@router.get("/ecus", tags=["ECU"])
def get_all_ecus():
    """Status of all 5 Virtual ECUs."""
    if STANDALONE_MODE:
        return _get_sim().get_ecu_status()
    raw = _read_json_file("ecu_status.json", [])
    return raw


@router.get("/ecus/{ecu_id}", tags=["ECU"])
def get_ecu(ecu_id: str):
    """Status of a specific ECU by name (e.g., ENGINE_ECU)."""
    ecus = get_all_ecus()
    for ecu in ecus:
        if ecu.get("name", "").upper() == ecu_id.upper():
            return ecu
    raise HTTPException(status_code=404, detail=f"ECU '{ecu_id}' not found")


# ─── CAN Bus ──────────────────────────────────────────────────────────────────

@router.get("/can/messages", tags=["CAN"])
def get_can_messages(limit: int = 50):
    """Most recent CAN frames (ring buffer, most recent first)."""
    if STANDALONE_MODE:
        return _get_sim().get_can_log(limit=limit)
    raw = _read_json_file("can_log.json", [])
    return raw[:limit]


@router.get("/can/statistics", response_model=CANStatisticsSchema, tags=["CAN"])
def get_can_statistics():
    """CAN bus throughput statistics."""
    if STANDALONE_MODE:
        return CANStatisticsSchema(**_get_sim().get_can_stats())
    # Derive from can_log length
    raw = _read_json_file("can_log.json", [])
    uptime = time.time() - _start_time
    return CANStatisticsSchema(
        total_frames=len(raw),
        frames_per_second=round(len(raw) / max(uptime, 1), 1),
        uptime_seconds=round(uptime, 1),
        active_ecus=5,
    )


# ─── Digital Twin ─────────────────────────────────────────────────────────────

@router.get("/digital-twin/status", response_model=DigitalTwinStatusSchema, tags=["Digital Twin"])
def get_digital_twin_status():
    """Digital Twin synchronization and health status."""
    if STANDALONE_MODE:
        return DigitalTwinStatusSchema(**_get_sim().get_twin_status())
    raw = _read_json_file("digital_twin_status.json", {})
    return DigitalTwinStatusSchema(**raw)


# ─── Faults ───────────────────────────────────────────────────────────────────

@router.get("/faults", tags=["Faults"])
def get_faults(limit: int = 50):
    """All detected fault events (most recent first)."""
    if STANDALONE_MODE:
        return _get_sim().get_fault_events(limit=limit)
    raw = _read_json_file("fault_events.json", [])
    return raw[:limit]


@router.get("/events", tags=["Faults"])
def get_events(limit: int = 50):
    """Alias for /faults — returns all recent events."""
    return get_faults(limit=limit)


# ─── Simulation Control ───────────────────────────────────────────────────────

@router.post("/simulation/start", tags=["Control"])
def start_simulation(req: Optional[SimulationStartRequest] = None):
    """Start the simulation (optionally with a scenario)."""
    scenario = req.scenario if req else "NORMAL_DRIVE"
    if STANDALONE_MODE:
        _get_sim().set_scenario(scenario)
    else:
        _write_command({"type": "scenario", "value": scenario})
    return {"status": "started", "scenario": scenario}


@router.post("/simulation/stop", tags=["Control"])
def stop_simulation():
    """Stop the active scenario and return to NONE."""
    if STANDALONE_MODE:
        _get_sim().set_scenario("NONE")
    else:
        _write_command({"type": "stop", "value": "NONE"})
    return {"status": "stopped"}


@router.post("/simulation/scenario", tags=["Control"])
def set_scenario(req: ScenarioRequest):
    """Switch to a named scenario."""
    valid = [
        "NONE", "NORMAL_DRIVE", "ACCELERATION", "BRAKING",
        "ENGINE_OVERHEAT", "BRAKE_FAILURE", "BATTERY_FAULT",
        "COMMUNICATION_LOSS", "SENSOR_STUCK", "MIXED_FAULT",
    ]
    if req.scenario not in valid:
        raise HTTPException(status_code=400,
                            detail=f"Unknown scenario '{req.scenario}'. Valid: {valid}")
    if STANDALONE_MODE:
        _get_sim().set_scenario(req.scenario)
    else:
        _write_command({"type": "scenario", "value": req.scenario})
    return {"status": "ok", "scenario": req.scenario}


@router.post("/fault/start", tags=["Control"])
def start_fault(req: FaultRequest):
    """Inject a named fault."""
    if STANDALONE_MODE:
        _get_sim().start_fault(req.fault_id)
    else:
        _write_command({"type": "fault_start", "value": req.fault_id})
    return {"status": "fault_started", "fault_id": req.fault_id}


@router.post("/fault/stop", tags=["Control"])
def stop_fault(req: FaultRequest):
    """Stop a named fault."""
    if STANDALONE_MODE:
        _get_sim().stop_fault(req.fault_id)
    else:
        _write_command({"type": "fault_stop", "value": req.fault_id})
    return {"status": "fault_stopped", "fault_id": req.fault_id}


# ─── Scenario Status ──────────────────────────────────────────────────────────

@router.get("/scenario/status", response_model=ScenarioStatusSchema, tags=["Control"])
def get_scenario_status():
    """Current active scenario and fault list."""
    if STANDALONE_MODE:
        return ScenarioStatusSchema(**_get_sim().get_scenario_status())
    raw = _read_json_file("scenario.json", {"active_scenario": "NONE", "active_faults": []})
    return ScenarioStatusSchema(**raw)


# ─── WebSocket: live CAN stream ───────────────────────────────────────────────

@router.websocket("/ws/can-stream")
async def can_stream(websocket: WebSocket):
    """Live WebSocket feed of CAN frames at ~5 Hz."""
    await websocket.accept()
    import asyncio
    try:
        while True:
            if STANDALONE_MODE:
                frames = _get_sim().get_can_log(limit=10)
            else:
                frames = _read_json_file("can_log.json", [])[:10]
            await websocket.send_json({"frames": frames})
            await asyncio.sleep(0.2)
    except WebSocketDisconnect:
        pass
