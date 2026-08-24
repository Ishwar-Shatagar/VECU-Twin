"""
Storage model helpers — convenience wrappers over database.py.
Provides typed dataclass-style helpers for the API layer.
"""

from dataclasses import dataclass, field
from typing import Optional
import time


@dataclass
class VehicleStateRecord:
    """A single vehicle state snapshot as stored in SQLite."""
    id: Optional[int] = None
    session_id: Optional[int] = None
    timestamp: float = field(default_factory=time.time)
    speed_kmh: float = 0.0
    rpm: float = 800.0
    engine_temp_c: float = 20.0
    engine_load_pct: float = 0.0
    brake_active: bool = False
    brake_pressure: float = 0.0
    battery_pct: float = 84.0
    battery_voltage: float = 400.0
    battery_temp: float = 25.0
    steering_angle: float = 0.0
    vehicle_mode: str = "IDLE"


@dataclass
class FaultEventRecord:
    """A fault event as stored in SQLite."""
    id: Optional[int] = None
    session_id: Optional[int] = None
    timestamp: float = field(default_factory=time.time)
    fault_type: str = ""
    severity: str = "NORMAL"
    source_ecu: str = ""
    description: str = ""
    current_value: float = 0.0
    expected_min: float = 0.0
    expected_max: float = 0.0
    recommended_action: str = ""
