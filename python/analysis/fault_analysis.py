"""
Fault analysis utilities — frequency, duration, ECU fault rates.
"""

import numpy as np
import pandas as pd
from collections import Counter
from typing import Optional


def fault_frequency(events: list[dict]) -> dict:
    """Count occurrences of each fault type."""
    types = [e.get("fault_type", "UNKNOWN") for e in events]
    counts = Counter(types)
    return dict(counts.most_common())


def fault_severity_distribution(events: list[dict]) -> dict:
    """Return count per severity level."""
    severities = [e.get("severity", "NORMAL") for e in events]
    return {
        "NORMAL":   severities.count("NORMAL"),
        "WARNING":  severities.count("WARNING"),
        "CRITICAL": severities.count("CRITICAL"),
    }


def ecu_fault_rate(events: list[dict]) -> dict:
    """Return fault count per ECU."""
    ecus = [e.get("source_ecu", "UNKNOWN") for e in events]
    return dict(Counter(ecus).most_common())


def most_recent_fault(events: list[dict]) -> Optional[dict]:
    """Return the most recent fault event."""
    if not events:
        return None
    return max(events, key=lambda e: e.get("timestamp", 0))


def fault_summary(events: list[dict]) -> dict:
    """Comprehensive fault summary for the API."""
    return {
        "total_faults":        len(events),
        "frequency":           fault_frequency(events),
        "severity_distribution": fault_severity_distribution(events),
        "ecu_fault_rates":     ecu_fault_rate(events),
        "most_recent":         most_recent_fault(events),
        "has_active_critical": any(
            e.get("severity") == "CRITICAL" for e in events[-10:]
        ),
    }
