// Central data polling hook — fetches all vehicle data at configurable interval

import { useState, useEffect, useCallback, useRef } from 'react';
import type {
  VehicleState, DigitalTwinStatus, ECUStatus,
  CANFrame, CANStatistics, FaultEvent, ScenarioStatus, HistoryPoint,
} from '../types';
import {
  getVehicleState, getDigitalTwinStatus, getAllECUs,
  getCANMessages, getCANStatistics, getFaults, getScenarioStatus,
} from '../services/api';

const POLL_INTERVAL_MS = 500;
const HISTORY_MAX = 60; // 30 seconds at 500ms

export interface VehicleData {
  state:         VehicleState | null;
  twinStatus:    DigitalTwinStatus | null;
  ecus:          ECUStatus[];
  canFrames:     CANFrame[];
  canStats:      CANStatistics | null;
  faults:        FaultEvent[];
  scenarioStatus: ScenarioStatus | null;
  history:       HistoryPoint[];
  connected:     boolean;
  lastUpdate:    Date | null;
}

export function useVehicleData(): VehicleData {
  const [state, setState]          = useState<VehicleState | null>(null);
  const [twin, setTwin]            = useState<DigitalTwinStatus | null>(null);
  const [ecus, setECUs]            = useState<ECUStatus[]>([]);
  const [canFrames, setCANFrames]  = useState<CANFrame[]>([]);
  const [canStats, setCANStats]    = useState<CANStatistics | null>(null);
  const [faults, setFaults]        = useState<FaultEvent[]>([]);
  const [scenario, setScenario]    = useState<ScenarioStatus | null>(null);
  const [history, setHistory]      = useState<HistoryPoint[]>([]);
  const [connected, setConnected]  = useState(false);
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);

  const historyRef = useRef<HistoryPoint[]>([]);

  const fetchAll = useCallback(async () => {
    try {
      const [s, t, e, cf, cs, f, sc] = await Promise.allSettled([
        getVehicleState(),
        getDigitalTwinStatus(),
        getAllECUs(),
        getCANMessages(30),
        getCANStatistics(),
        getFaults(30),
        getScenarioStatus(),
      ]);

      if (s.status === 'fulfilled') {
        setState(s.value);

        // Append to history
        const pt: HistoryPoint = {
          timestamp:            s.value.timestamp * 1000,
          speed_kmh:            s.value.speed_kmh,
          rpm:                  s.value.rpm,
          engine_temperature_c: s.value.engine_temperature_c,
          battery_pct:          s.value.battery_pct,
        };
        historyRef.current = [...historyRef.current.slice(-HISTORY_MAX + 1), pt];
        setHistory([...historyRef.current]);
        setConnected(true);
        setLastUpdate(new Date());
      }

      if (t.status === 'fulfilled') setTwin(t.value);
      if (e.status === 'fulfilled') setECUs(e.value);
      if (cf.status === 'fulfilled') setCANFrames(cf.value);
      if (cs.status === 'fulfilled') setCANStats(cs.value);
      if (f.status === 'fulfilled') setFaults(f.value);
      if (sc.status === 'fulfilled') setScenario(sc.value);

    } catch {
      setConnected(false);
    }
  }, []);

  useEffect(() => {
    fetchAll();
    const timer = setInterval(fetchAll, POLL_INTERVAL_MS);
    return () => clearInterval(timer);
  }, [fetchAll]);

  return {
    state,
    twinStatus:     twin,
    ecus,
    canFrames,
    canStats,
    faults,
    scenarioStatus: scenario,
    history,
    connected,
    lastUpdate,
  };
}
