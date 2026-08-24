// Digital Twin status panel

import type { DigitalTwinStatus } from '../types';
import { CheckCircle, AlertTriangle, XCircle, RefreshCw } from 'lucide-react';

interface Props {
  status: DigitalTwinStatus | null;
}

function HealthIcon({ health }: { health: string }) {
  if (health === 'HEALTHY') return <CheckCircle size={16} className="text-emerald-400" />;
  if (health === 'WARNING') return <AlertTriangle size={16} className="text-amber-400" />;
  return <XCircle size={16} className="text-red-400" />;
}

export function DigitalTwinPanel({ status }: Props) {
  if (!status) return (
    <div className="card animate-pulse h-40 bg-surface-700" />
  );

  const health = status.health ?? 'HEALTHY';
  const healthClass =
    health === 'HEALTHY' ? 'badge-healthy' :
    health === 'WARNING'  ? 'badge-warning' : 'badge-critical';

  const glowClass =
    health === 'FAULT' ? 'glow-critical' :
    health === 'WARNING'  ? 'glow-warning' : '';

  return (
    <div className={`card ${glowClass}`}>
      <div className="card-header">
        <div className="flex items-center gap-2">
          <RefreshCw size={14} className="text-accent-blue" />
          <span className="card-title">Digital Twin</span>
        </div>
        <span className={`badge ${healthClass}`}>
          <HealthIcon health={health} />
          <span className="ml-1">{health}</span>
        </span>
      </div>

      <div className="grid grid-cols-2 gap-4">
        <div>
          <div className="data-row">
            <span className="data-label">Synchronized</span>
            <span className={`data-value text-xs ${status.synchronized ? 'text-emerald-400' : 'text-red-400'}`}>
              {status.synchronized ? '✓ YES' : '✗ NO'}
            </span>
          </div>
          <div className="data-row">
            <span className="data-label">Update Age</span>
            <span className="data-value">{status.update_age_ms.toFixed(0)} ms</span>
          </div>
          <div className="data-row">
            <span className="data-label">Frames Processed</span>
            <span className="data-value">{status.frames_processed.toLocaleString()}</span>
          </div>
          <div className="data-row">
            <span className="data-label">Validation Errors</span>
            <span className={`data-value ${status.validation_errors > 0 ? 'text-amber-400' : 'text-emerald-400'}`}>
              {status.validation_errors}
            </span>
          </div>
        </div>

        <div>
          <div className="section-label mb-2">ECU Sync Status</div>
          {Object.entries(status.ecu_sync).map(([ecu, synced]) => (
            <div key={ecu} className="data-row">
              <span className="data-label">{ecu.replace('_ECU', '')}</span>
              <span className={`text-xs font-medium ${synced ? 'text-emerald-400' : 'text-red-400'}`}>
                {synced ? '● SYNC' : '○ LOST'}
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
