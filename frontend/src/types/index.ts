// TypeScript types matching Python API schemas

export interface VehicleState {
  speed_kmh: number;
  rpm: number;
  engine_temperature_c: number;
  engine_load_pct: number;
  brake_active: boolean;
  brake_pressure_bar: number;
  battery_pct: number;
  battery_voltage_v: number;
  battery_temperature_c: number;
  battery_charging: boolean;
  steering_angle_deg: number;
  mode: 'IDLE' | 'DRIVING' | 'ACCELERATING' | 'BRAKING' | 'FAULT';
  timestamp: number;
  digital_twin_status: 'HEALTHY' | 'WARNING' | 'FAULT';
}

export interface DigitalTwinStatus {
  health: 'HEALTHY' | 'WARNING' | 'FAULT';
  synchronized: boolean;
  update_age_ms: number;
  frames_processed: number;
  validation_errors: number;
  ecu_sync: Record<string, boolean>;
}

export interface ECUStatus {
  name: string;
  status: 'ONLINE' | 'OFFLINE' | 'FAULT' | 'STARTING' | 'STOPPED';
  message_count: number;
  messages_per_sec: number;
  fault_count: number;
  fault_active: boolean;
  silenced: boolean;
}

export interface CANFrame {
  timestamp: number;
  can_id: string;
  dlc: number;
  data: number[];
  source: string;
  message: string;
}

export interface CANStatistics {
  total_frames: number;
  frames_per_second: number;
  uptime_seconds: number;
  active_ecus: number;
}

export type FaultSeverity = 'NORMAL' | 'WARNING' | 'CRITICAL';

export interface FaultEvent {
  fault_type: string;
  severity: FaultSeverity;
  source_ecu: string;
  timestamp: number;
  description: string;
  current_value: number;
  expected_min: number;
  expected_max: number;
  recommended_action: string;
}

export interface ScenarioStatus {
  active_scenario: string;
  active_faults: string[];
}

export type ScenarioId =
  | 'NONE'
  | 'NORMAL_DRIVE'
  | 'ACCELERATION'
  | 'BRAKING'
  | 'ENGINE_OVERHEAT'
  | 'BRAKE_FAILURE'
  | 'BATTERY_FAULT'
  | 'COMMUNICATION_LOSS'
  | 'SENSOR_STUCK'
  | 'MIXED_FAULT';

export interface HistoryPoint {
  timestamp: number;
  speed_kmh: number;
  rpm: number;
  engine_temperature_c: number;
  battery_pct: number;
}
