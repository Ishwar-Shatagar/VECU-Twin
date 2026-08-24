// API service — all HTTP calls to the FastAPI backend

import type {
  VehicleState, DigitalTwinStatus, ECUStatus,
  CANFrame, CANStatistics, FaultEvent, ScenarioStatus, ScenarioId,
} from '../types';

const BASE_URL = '/api';

async function get<T>(path: string): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`);
  if (!res.ok) throw new Error(`GET ${path} → ${res.status}`);
  return res.json() as Promise<T>;
}

async function post<T>(path: string, body?: unknown): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined,
  });
  if (!res.ok) throw new Error(`POST ${path} → ${res.status}`);
  return res.json() as Promise<T>;
}

// ─── Vehicle ──────────────────────────────────────────────────────────────────
export const getVehicleState    = () => get<VehicleState>('/vehicle/state');
export const getVehicleHistory  = (limit = 100) =>
  get<VehicleState[]>(`/vehicle/history?limit=${limit}`);

// ─── ECUs ─────────────────────────────────────────────────────────────────────
export const getAllECUs          = () => get<ECUStatus[]>('/ecus');

// ─── CAN ─────────────────────────────────────────────────────────────────────
export const getCANMessages     = (limit = 50) =>
  get<CANFrame[]>(`/can/messages?limit=${limit}`);
export const getCANStatistics   = () => get<CANStatistics>('/can/statistics');

// ─── Digital Twin ─────────────────────────────────────────────────────────────
export const getDigitalTwinStatus = () => get<DigitalTwinStatus>('/digital-twin/status');

// ─── Faults ───────────────────────────────────────────────────────────────────
export const getFaults           = (limit = 50) =>
  get<FaultEvent[]>(`/faults?limit=${limit}`);

// ─── Scenario Control ─────────────────────────────────────────────────────────
export const setScenario         = (scenario: ScenarioId) =>
  post('/simulation/scenario', { scenario });

export const startSimulation     = (scenario: ScenarioId = 'NORMAL_DRIVE') =>
  post('/simulation/start', { scenario });

export const stopSimulation      = () => post('/simulation/stop');

export const startFault          = (fault_id: string) =>
  post('/fault/start', { fault_id });

export const stopFault           = (fault_id: string) =>
  post('/fault/stop', { fault_id });

export const getScenarioStatus   = () => get<ScenarioStatus>('/scenario/status');
