"""
CAN message and ECU throughput statistics.
"""

import numpy as np
from collections import defaultdict
from typing import Optional


def message_rate_per_ecu(can_log: list[dict]) -> dict:
    """Messages per ECU from a CAN log list."""
    counts: dict[str, int] = defaultdict(int)
    for frame in can_log:
        source = frame.get("source", "UNKNOWN")
        counts[source] += 1
    return dict(counts)


def message_rate_per_message_type(can_log: list[dict]) -> dict:
    """Messages per message name."""
    counts: dict[str, int] = defaultdict(int)
    for frame in can_log:
        name = frame.get("message", "UNKNOWN")
        counts[name] += 1
    return dict(counts)


def compute_throughput(can_log: list[dict]) -> dict:
    """Estimate messages/sec from a list of frames with timestamps."""
    if len(can_log) < 2:
        return {"messages_per_sec": 0.0, "total": len(can_log)}
    timestamps = sorted(f.get("timestamp", 0) for f in can_log)
    duration = timestamps[-1] - timestamps[0]
    if duration < 0.001:
        return {"messages_per_sec": 0.0, "total": len(can_log)}
    rate = (len(can_log) - 1) / duration
    return {
        "messages_per_sec": round(rate, 2),
        "total":            len(can_log),
        "duration_sec":     round(duration, 3),
    }


def can_id_distribution(can_log: list[dict]) -> dict:
    """Frequency of each CAN ID in the log."""
    counts: dict[str, int] = defaultdict(int)
    for frame in can_log:
        can_id = frame.get("can_id", "0x000")
        counts[can_id] += 1
    return dict(sorted(counts.items()))
