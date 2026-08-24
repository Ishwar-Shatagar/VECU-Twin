"""
Pydantic v2 schemas for all API request/response models.
"""

from pydantic import BaseModel, Field
from typing import Optional, List, Dict, Any
import time


# ─── Vehicle State ────────────────────────────────────────────────────────────

class VehicleStateSchema(BaseModel):
    speed_kmh: float = Field(0.0, description="Vehicle speed in km/h")
    rpm: float = Field(800.0, description="Engine RPM")
    engine_temperature_c: float = Field(20.0, description="Engine temperature in °C")
    engine_load_pct: float = Field(0.0, description="Engine load percentage 0–100")
    brake_active: bool = Field(False, description="Brake pedal active")
    brake_pressure_bar: float = Field(0.0, description="Brake hydraulic pressure in bar")
    battery_pct: float = Field(84.0, description="Battery state of charge %")
    battery_voltage_v: float = Field(400.0, description="Battery pack voltage V")
    battery_temperature_c: float = Field(25.0, description="Battery temperature °C")
    battery_charging: bool = Field(False, description="Battery charging active")
    steering_angle_deg: float = Field(0.0, description="Steering angle in degrees")
    mode: str = Field("IDLE", description="Vehicle operating mode")
    timestamp: float = Field(default_factory=time.time, description="Unix timestamp")
    digital_twin_status: str = Field("HEALTHY", description="Digital Twin health status")


# ─── Digital Twin ─────────────────────────────────────────────────────────────

class ECUSyncStatus(BaseModel):
    ecu_name: str
    synced: bool
    last_seen_ago_ms: float = 0.0


class DigitalTwinStatusSchema(BaseModel):
    health: str = Field("HEALTHY", description="HEALTHY | WARNING | FAULT")
    synchronized: bool = True
    update_age_ms: float = 0.0
    frames_processed: int = 0
    validation_errors: int = 0
    ecu_sync: Dict[str, bool] = Field(default_factory=dict)


# ─── ECU ─────────────────────────────────────────────────────────────────────

class ECUStatusSchema(BaseModel):
    name: str
    status: str
    message_count: int = 0
    messages_per_sec: float = 0.0
    fault_count: int = 0
    fault_active: bool = False
    silenced: bool = False


# ─── CAN ─────────────────────────────────────────────────────────────────────

class CANFrameSchema(BaseModel):
    timestamp: float
    can_id: str
    dlc: int
    data: List[int]
    source: str
    message: str


class CANStatisticsSchema(BaseModel):
    total_frames: int = 0
    frames_per_second: float = 0.0
    uptime_seconds: float = 0.0
    active_ecus: int = 0


# ─── Fault Events ─────────────────────────────────────────────────────────────

class FaultEventSchema(BaseModel):
    fault_type: str
    severity: str  # NORMAL | WARNING | CRITICAL
    source_ecu: str
    timestamp: float
    description: str
    current_value: float = 0.0
    expected_min: float = 0.0
    expected_max: float = 0.0
    recommended_action: str = ""


# ─── Simulation Control ───────────────────────────────────────────────────────

class ScenarioRequest(BaseModel):
    scenario: str = Field(
        ...,
        description="Scenario name: NORMAL_DRIVE | ACCELERATION | BRAKING | "
                    "ENGINE_OVERHEAT | BRAKE_FAILURE | BATTERY_FAULT | "
                    "COMMUNICATION_LOSS | SENSOR_STUCK | MIXED_FAULT | NONE"
    )


class FaultRequest(BaseModel):
    fault_id: str = Field(
        ...,
        description="Fault ID: ENGINE_OVERHEAT | ENGINE_RPM_FAULT | BRAKE_FAILURE | "
                    "BATTERY_DEGRADATION | SENSOR_STUCK | COMMUNICATION_LOSS | "
                    "ECU_DELAYED | INCONSISTENT_STATE"
    )


class SimulationStartRequest(BaseModel):
    scenario: Optional[str] = "NORMAL_DRIVE"


# ─── Health ───────────────────────────────────────────────────────────────────

class HealthResponse(BaseModel):
    status: str = "ok"
    version: str = "1.0.0"
    uptime_seconds: float = 0.0
    standalone_mode: bool = True
    simulator_connected: bool = False


# ─── Scenario Status ─────────────────────────────────────────────────────────

class ScenarioStatusSchema(BaseModel):
    active_scenario: str = "NONE"
    active_faults: List[str] = Field(default_factory=list)
