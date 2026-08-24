// Scenario control panel

import { useState } from 'react';
import type { ScenarioId, ScenarioStatus } from '../types';
import { setScenario } from '../services/api';
import { Play, Square } from 'lucide-react';

interface Props {
  status: ScenarioStatus | null;
  onScenarioChange?: () => void;
}

interface ScenarioOption {
  id: ScenarioId;
  label: string;
  description: string;
  type: 'normal' | 'fault';
  icon: string;
}

const SCENARIOS: ScenarioOption[] = [
  { id: 'NONE',             label: 'Stopped',          description: 'No active scenario',                     type: 'normal', icon: '⏹' },
  { id: 'NORMAL_DRIVE',     label: 'Normal Drive',      description: 'Stable cruise at 60 km/h',               type: 'normal', icon: '🚗' },
  { id: 'ACCELERATION',     label: 'Acceleration',      description: 'Gradual speed and RPM increase',         type: 'normal', icon: '🏎' },
  { id: 'BRAKING',          label: 'Braking',           description: 'Active braking, speed decreases',        type: 'normal', icon: '🛑' },
  { id: 'ENGINE_OVERHEAT',  label: 'Engine Overheat',   description: 'Temperature rises → CRITICAL fault',     type: 'fault',  icon: '🌡' },
  { id: 'BRAKE_FAILURE',    label: 'Brake Failure',     description: 'Brake active, pressure fails to build',  type: 'fault',  icon: '⚠️' },
  { id: 'BATTERY_FAULT',    label: 'Battery Fault',     description: 'Rapid SOC drain, voltage drops',         type: 'fault',  icon: '🔋' },
  { id: 'COMMUNICATION_LOSS', label: 'Comm Loss',       description: 'ENGINE_ECU stops communicating',         type: 'fault',  icon: '📡' },
  { id: 'SENSOR_STUCK',     label: 'Sensor Stuck',      description: 'RPM sensor freezes on fixed value',      type: 'fault',  icon: '🔒' },
  { id: 'MIXED_FAULT',      label: 'Mixed Fault',       description: 'Engine overheat + battery degradation',  type: 'fault',  icon: '💥' },
];

export function ScenarioControl({ status, onScenarioChange }: Props) {
  const [loading, setLoading] = useState<ScenarioId | null>(null);
  const active = status?.active_scenario ?? 'NONE';

  const handleSelect = async (scenarioId: ScenarioId) => {
    setLoading(scenarioId);
    try {
      await setScenario(scenarioId);
      onScenarioChange?.();
    } catch (e) {
      console.error('Scenario change failed:', e);
    } finally {
      setLoading(null);
    }
  };

  const normalScenarios = SCENARIOS.filter(s => s.type === 'normal');
  const faultScenarios  = SCENARIOS.filter(s => s.type === 'fault');

  return (
    <div className="card">
      <div className="card-header">
        <span className="card-title">Scenario Control</span>
        {active !== 'NONE' && (
          <span className="badge badge-warning">
            <Play size={10} className="mr-1" />
            {active.replace(/_/g, ' ')}
          </span>
        )}
      </div>

      <div className="space-y-3">
        <div>
          <div className="section-label">Normal Scenarios</div>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-2">
            {normalScenarios.map(s => (
              <button
                key={s.id}
                id={`scenario-${s.id}`}
                onClick={() => handleSelect(s.id)}
                disabled={loading !== null}
                title={s.description}
                className={`flex flex-col items-center gap-1 p-2 rounded-lg border text-xs font-medium
                  transition-all duration-150 cursor-pointer
                  ${active === s.id
                    ? 'bg-brand-500/30 border-brand-400 text-brand-300'
                    : 'bg-surface-700 border-surface-600 text-gray-400 hover:border-brand-500/50 hover:text-gray-300'
                  }
                  ${loading === s.id ? 'opacity-50' : ''}
                `}
              >
                <span className="text-lg">{s.icon}</span>
                <span>{s.label}</span>
              </button>
            ))}
          </div>
        </div>

        <div>
          <div className="section-label text-red-400/70">Fault Injection</div>
          <div className="grid grid-cols-2 md:grid-cols-3 gap-2">
            {faultScenarios.map(s => (
              <button
                key={s.id}
                id={`scenario-${s.id}`}
                onClick={() => handleSelect(s.id)}
                disabled={loading !== null}
                title={s.description}
                className={`flex flex-col items-center gap-1 p-2 rounded-lg border text-xs font-medium
                  transition-all duration-150 cursor-pointer
                  ${active === s.id
                    ? 'bg-red-900/40 border-red-700 text-red-300 glow-critical'
                    : 'bg-surface-700 border-surface-600 text-gray-400 hover:border-red-700/50 hover:text-gray-300'
                  }
                  ${loading === s.id ? 'opacity-50' : ''}
                `}
              >
                <span className="text-lg">{s.icon}</span>
                <span className="text-center leading-tight">{s.label}</span>
              </button>
            ))}
          </div>
        </div>

        {status?.active_faults && status.active_faults.length > 0 && (
          <div className="mt-2 p-2 bg-red-950/30 border border-red-900/30 rounded-lg">
            <div className="text-xs text-red-400 font-medium mb-1">Active Faults:</div>
            <div className="flex flex-wrap gap-1">
              {status.active_faults.map(f => (
                <span key={f} className="badge badge-critical text-xs">{f}</span>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
