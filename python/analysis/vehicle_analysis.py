"""
Vehicle analysis utilities using NumPy and Pandas.
"""

import numpy as np
import pandas as pd
from typing import Optional


def compute_rolling_average(values: list[float], window: int = 10) -> list[float]:
    """Compute rolling average over a list of float values."""
    if not values:
        return []
    arr = np.array(values, dtype=float)
    result = pd.Series(arr).rolling(window=min(window, len(arr)), min_periods=1).mean()
    return result.tolist()


def detect_trend(values: list[float]) -> str:
    """Return 'RISING', 'FALLING', or 'STABLE' based on linear regression slope."""
    if len(values) < 3:
        return "STABLE"
    arr = np.array(values, dtype=float)
    x = np.arange(len(arr))
    slope = np.polyfit(x, arr, 1)[0]
    if slope > 0.5:
        return "RISING"
    elif slope < -0.5:
        return "FALLING"
    return "STABLE"


def compute_statistics(values: list[float]) -> dict:
    """Compute descriptive statistics for a list of values."""
    if not values:
        return {"min": 0, "max": 0, "mean": 0, "std": 0, "count": 0}
    arr = np.array(values, dtype=float)
    return {
        "min":   float(np.min(arr)),
        "max":   float(np.max(arr)),
        "mean":  float(np.mean(arr)),
        "std":   float(np.std(arr)),
        "count": int(len(arr)),
    }


def analyze_vehicle_history(history: list[dict]) -> dict:
    """
    Perform trend analysis on a list of vehicle state records.
    Returns summary statistics and trends for key signals.
    """
    if not history:
        return {}

    df = pd.DataFrame(history)

    result = {}

    for col in ["speed_kmh", "rpm", "engine_temp_c", "battery_pct"]:
        if col in df.columns:
            values = df[col].dropna().tolist()
            result[col] = {
                "stats":  compute_statistics(values),
                "trend":  detect_trend(values),
                "recent": values[-10:] if len(values) >= 10 else values,
            }

    return result


def compute_speed_profile(history: list[dict]) -> dict:
    """Compute speed profile metrics: time at speed bands."""
    if not history:
        return {}

    speeds = [r.get("speed_kmh", 0) for r in history]
    total = len(speeds)
    if total == 0:
        return {}

    return {
        "idle_pct":       round(sum(1 for s in speeds if s < 5) / total * 100, 1),
        "city_pct":       round(sum(1 for s in speeds if 5 <= s < 50) / total * 100, 1),
        "highway_pct":    round(sum(1 for s in speeds if 50 <= s < 120) / total * 100, 1),
        "fast_pct":       round(sum(1 for s in speeds if s >= 120) / total * 100, 1),
        "max_speed_kmh":  round(max(speeds), 1),
        "avg_speed_kmh":  round(float(np.mean(speeds)), 1),
    }
