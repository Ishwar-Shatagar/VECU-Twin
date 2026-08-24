"""
Tests for vehicle and fault analysis functions.
"""

import pytest
from python.analysis.vehicle_analysis import (
    compute_rolling_average, detect_trend, compute_statistics,
    analyze_vehicle_history, compute_speed_profile,
)
from python.analysis.fault_analysis import (
    fault_frequency, fault_severity_distribution, ecu_fault_rate,
    most_recent_fault, fault_summary,
)
from python.analysis.statistics import (
    message_rate_per_ecu, compute_throughput, can_id_distribution,
)
import time


# ─── Vehicle Analysis ─────────────────────────────────────────────────────────

def test_rolling_average_basic():
    result = compute_rolling_average([1.0, 2.0, 3.0, 4.0, 5.0], window=3)
    assert len(result) == 5
    assert result[-1] == pytest.approx(4.0, abs=0.1)


def test_rolling_average_empty():
    result = compute_rolling_average([], window=5)
    assert result == []


def test_detect_trend_rising():
    values = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0]
    assert detect_trend(values) == "RISING"


def test_detect_trend_falling():
    values = [7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0]
    assert detect_trend(values) == "FALLING"


def test_detect_trend_stable():
    values = [5.0, 5.1, 4.9, 5.0, 5.05, 4.95, 5.0]
    assert detect_trend(values) == "STABLE"


def test_detect_trend_short_list():
    assert detect_trend([1.0]) == "STABLE"
    assert detect_trend([]) == "STABLE"


def test_compute_statistics_correctness():
    values = [2.0, 4.0, 6.0, 8.0, 10.0]
    stats = compute_statistics(values)
    assert stats["min"] == pytest.approx(2.0)
    assert stats["max"] == pytest.approx(10.0)
    assert stats["mean"] == pytest.approx(6.0)
    assert stats["count"] == 5


def test_compute_statistics_single_value():
    stats = compute_statistics([42.0])
    assert stats["mean"] == pytest.approx(42.0)
    assert stats["std"] == pytest.approx(0.0)


def test_compute_statistics_empty():
    stats = compute_statistics([])
    assert stats["count"] == 0


def test_analyze_vehicle_history_returns_dict():
    history = [
        {"speed_kmh": 50 + i, "rpm": 2000 + i * 50,
         "engine_temp_c": 70 + i * 0.5, "battery_pct": 80 - i * 0.1}
        for i in range(20)
    ]
    result = analyze_vehicle_history(history)
    assert "speed_kmh" in result
    assert "rpm" in result
    assert "trend" in result["speed_kmh"]
    assert "stats" in result["speed_kmh"]


def test_analyze_vehicle_history_empty():
    result = analyze_vehicle_history([])
    assert result == {}


def test_compute_speed_profile():
    history = (
        [{"speed_kmh": 0}] * 5 +   # idle
        [{"speed_kmh": 30}] * 5 +  # city
        [{"speed_kmh": 80}] * 5 +  # highway
        [{"speed_kmh": 130}] * 5   # fast
    )
    profile = compute_speed_profile(history)
    assert profile["idle_pct"] == pytest.approx(25.0, abs=1.0)
    assert profile["city_pct"] == pytest.approx(25.0, abs=1.0)
    assert profile["highway_pct"] == pytest.approx(25.0, abs=1.0)
    assert profile["fast_pct"] == pytest.approx(25.0, abs=1.0)


# ─── Fault Analysis ───────────────────────────────────────────────────────────

sample_events = [
    {"fault_type": "ENGINE_OVERHEAT", "severity": "CRITICAL",
     "source_ecu": "ENGINE_ECU", "timestamp": time.time() - 10},
    {"fault_type": "ENGINE_OVERHEAT", "severity": "WARNING",
     "source_ecu": "ENGINE_ECU", "timestamp": time.time() - 5},
    {"fault_type": "BRAKE_FAILURE", "severity": "CRITICAL",
     "source_ecu": "BRAKE_ECU", "timestamp": time.time() - 2},
    {"fault_type": "BATTERY_LOW", "severity": "WARNING",
     "source_ecu": "BATTERY_ECU", "timestamp": time.time() - 1},
]


def test_fault_frequency():
    freq = fault_frequency(sample_events)
    assert freq["ENGINE_OVERHEAT"] == 2
    assert freq["BRAKE_FAILURE"] == 1


def test_fault_severity_distribution():
    dist = fault_severity_distribution(sample_events)
    assert dist["CRITICAL"] == 2
    assert dist["WARNING"] == 2
    assert dist["NORMAL"] == 0


def test_ecu_fault_rate():
    rates = ecu_fault_rate(sample_events)
    assert rates["ENGINE_ECU"] == 2
    assert rates["BRAKE_ECU"] == 1


def test_most_recent_fault_returns_latest():
    recent = most_recent_fault(sample_events)
    assert recent is not None
    assert recent["fault_type"] == "BATTERY_LOW"


def test_most_recent_fault_empty():
    assert most_recent_fault([]) is None


def test_fault_summary_structure():
    summary = fault_summary(sample_events)
    assert "total_faults" in summary
    assert "frequency" in summary
    assert "severity_distribution" in summary
    assert "ecu_fault_rates" in summary
    assert "most_recent" in summary
    assert "has_active_critical" in summary
    assert summary["total_faults"] == 4


# ─── Statistics ───────────────────────────────────────────────────────────────

sample_can_log = [
    {"can_id": "0x101", "source": "ENGINE_ECU", "message": "ENGINE_RPM",
     "timestamp": 1000.0},
    {"can_id": "0x101", "source": "ENGINE_ECU", "message": "ENGINE_RPM",
     "timestamp": 1001.0},
    {"can_id": "0x201", "source": "BRAKE_ECU", "message": "BRAKE_STATUS",
     "timestamp": 1000.5},
]


def test_message_rate_per_ecu():
    rates = message_rate_per_ecu(sample_can_log)
    assert rates["ENGINE_ECU"] == 2
    assert rates["BRAKE_ECU"] == 1


def test_compute_throughput():
    result = compute_throughput(sample_can_log)
    assert "messages_per_sec" in result
    assert result["total"] == 3
    assert result["messages_per_sec"] > 0


def test_compute_throughput_empty():
    result = compute_throughput([])
    assert result["messages_per_sec"] == 0.0


def test_can_id_distribution():
    dist = can_id_distribution(sample_can_log)
    assert dist["0x101"] == 2
    assert dist["0x201"] == 1
