"""
SQLite database setup and access layer.

The database stores long-term historical data (sessions, events).
Short-term real-time state is read directly from JSON files written by the C++ simulator.

Tables:
  sessions        — simulation sessions (start/stop times)
  vehicle_history — periodic vehicle state snapshots
  fault_events    — all detected fault events
  can_messages    — rolling window of CAN frames
  ecu_status      — periodic ECU health snapshots
"""

import sqlite3
import json
import os
import time
import threading
from pathlib import Path
from typing import Optional

_memory_conn: Optional[sqlite3.Connection] = None
_conn_lock = threading.Lock()

def get_db_path() -> str:
    return os.getenv("DATABASE_PATH", "./data/vecu_twin.db")


def get_connection() -> sqlite3.Connection:
    """Return a SQLite connection. For :memory:, returns the shared memory connection."""
    db_path = get_db_path()
    if db_path == ":memory:":
        global _memory_conn
        with _conn_lock:
            if _memory_conn is None:
                _memory_conn = sqlite3.connect(":memory:", check_same_thread=False)
                _memory_conn.row_factory = sqlite3.Row
            return _memory_conn

    os.makedirs(os.path.dirname(os.path.abspath(db_path)), exist_ok=True)
    conn = sqlite3.connect(db_path, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    return conn


def init_db() -> None:
    """Create all tables if they do not exist."""
    with get_connection() as conn:
        conn.executescript("""
            CREATE TABLE IF NOT EXISTS sessions (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                started_at  REAL    NOT NULL,
                stopped_at  REAL,
                scenario    TEXT    DEFAULT 'NONE',
                notes       TEXT
            );

            CREATE TABLE IF NOT EXISTS vehicle_history (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id      INTEGER REFERENCES sessions(id),
                timestamp       REAL    NOT NULL,
                speed_kmh       REAL,
                rpm             REAL,
                engine_temp_c   REAL,
                engine_load_pct REAL,
                brake_active    INTEGER,
                brake_pressure  REAL,
                battery_pct     REAL,
                battery_voltage REAL,
                battery_temp    REAL,
                steering_angle  REAL,
                vehicle_mode    TEXT
            );

            CREATE TABLE IF NOT EXISTS fault_events (
                id                INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id        INTEGER REFERENCES sessions(id),
                timestamp         REAL    NOT NULL,
                fault_type        TEXT    NOT NULL,
                severity          TEXT    NOT NULL,
                source_ecu        TEXT,
                description       TEXT,
                current_value     REAL,
                expected_min      REAL,
                expected_max      REAL,
                recommended_action TEXT
            );

            CREATE TABLE IF NOT EXISTS can_messages (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id   INTEGER REFERENCES sessions(id),
                timestamp    REAL    NOT NULL,
                can_id       TEXT,
                source_ecu   TEXT,
                message_name TEXT,
                dlc          INTEGER,
                payload      TEXT
            );

            CREATE TABLE IF NOT EXISTS ecu_status (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id      INTEGER REFERENCES sessions(id),
                timestamp       REAL    NOT NULL,
                ecu_name        TEXT,
                status          TEXT,
                message_count   INTEGER,
                messages_per_sec REAL,
                fault_count     INTEGER,
                fault_active    INTEGER
            );

            CREATE INDEX IF NOT EXISTS idx_vh_timestamp ON vehicle_history(timestamp);
            CREATE INDEX IF NOT EXISTS idx_fe_timestamp ON fault_events(timestamp);
            CREATE INDEX IF NOT EXISTS idx_cm_timestamp ON can_messages(timestamp);
        """)


def insert_vehicle_state(session_id: Optional[int], state: dict) -> None:
    with get_connection() as conn:
        conn.execute("""
            INSERT INTO vehicle_history
                (session_id, timestamp, speed_kmh, rpm, engine_temp_c,
                 engine_load_pct, brake_active, brake_pressure,
                 battery_pct, battery_voltage, battery_temp,
                 steering_angle, vehicle_mode)
            VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)
        """, (
            session_id,
            state.get("timestamp", time.time()),
            state.get("speed_kmh"),
            state.get("rpm"),
            state.get("engine_temperature_c"),
            state.get("engine_load_pct"),
            1 if state.get("brake_active") else 0,
            state.get("brake_pressure_bar"),
            state.get("battery_pct"),
            state.get("battery_voltage_v"),
            state.get("battery_temperature_c"),
            state.get("steering_angle_deg"),
            state.get("mode", "IDLE"),
        ))


def insert_fault_event(session_id: Optional[int], event: dict) -> None:
    with get_connection() as conn:
        conn.execute("""
            INSERT INTO fault_events
                (session_id, timestamp, fault_type, severity, source_ecu,
                 description, current_value, expected_min, expected_max,
                 recommended_action)
            VALUES (?,?,?,?,?,?,?,?,?,?)
        """, (
            session_id,
            event.get("timestamp", time.time()),
            event.get("fault_type"),
            event.get("severity"),
            event.get("source_ecu"),
            event.get("description"),
            event.get("current_value"),
            event.get("expected_min"),
            event.get("expected_max"),
            event.get("recommended_action"),
        ))


def get_vehicle_history(limit: int = 100) -> list[dict]:
    init_db()
    with get_connection() as conn:
        rows = conn.execute(
            "SELECT * FROM vehicle_history ORDER BY timestamp DESC LIMIT ?",
            (limit,)
        ).fetchall()
        return [dict(r) for r in rows]


def get_fault_history(limit: int = 100) -> list[dict]:
    init_db()
    with get_connection() as conn:
        rows = conn.execute(
            "SELECT * FROM fault_events ORDER BY timestamp DESC LIMIT ?",
            (limit,)
        ).fetchall()
        return [dict(r) for r in rows]


def create_session(scenario: str = "NONE") -> int:
    with get_connection() as conn:
        cur = conn.execute(
            "INSERT INTO sessions (started_at, scenario) VALUES (?, ?)",
            (time.time(), scenario)
        )
        return cur.lastrowid


def close_session(session_id: int) -> None:
    with get_connection() as conn:
        conn.execute(
            "UPDATE sessions SET stopped_at = ? WHERE id = ?",
            (time.time(), session_id)
        )
