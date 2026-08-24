// ECU status cards grid

import type { ECUStatus } from '../types';
import { Cpu, AlertCircle, WifiOff } from 'lucide-react';

interface Props {
  ecus: ECUStatus[];
}

const ECU_ICONS: Record<string, string> = {
  ENGINE_ECU:  '⚙️',
  BRAKE_ECU:   '🛑',
  BATTERY_ECU: '🔋',
  STEERING_ECU: '🔄',
  GATEWAY_ECU: '🌐',
};

function ECUCard({ ecu }: { ecu: ECUStatus }) {
  const statusClass =
    ecu.status === 'ONLINE'  ? 'text-emerald-400' :
    ecu.status === 'FAULT'   ? 'text-red-400' :
    'text-gray-500';

  const badgeClass =
    ecu.fault_active ? 'badge-critical' :
    ecu.silenced     ? 'badge-warning' :
    ecu.status === 'ONLINE' ? 'badge-healthy' : 'badge-offline';

  return (
    <div className={`card ${ecu.fault_active ? 'glow-critical' : ''}`}>
      <div className="flex items-start justify-between mb-3">
        <div className="flex items-center gap-2">
          <span className="text-lg">{ECU_ICONS[ecu.name] ?? '📦'}</span>
          <div>
            <div className="text-sm font-semibold text-gray-200">
              {ecu.name.replace('_ECU', '')}
            </div>
            <div className={`text-xs font-mono ${statusClass}`}>{ecu.status}</div>
          </div>
        </div>
        <span className={`badge ${badgeClass}`}>
          {ecu.fault_active ? '⚠ FAULT' : ecu.silenced ? '✗ SILENT' : ecu.status}
        </span>
      </div>

      <div className="space-y-1">
        <div className="data-row">
          <span className="data-label">Messages</span>
          <span className="data-value">{ecu.message_count.toLocaleString()}</span>
        </div>
        <div className="data-row">
          <span className="data-label">Rate</span>
          <span className="data-value">{ecu.messages_per_sec.toFixed(1)} msg/s</span>
        </div>
        <div className="data-row">
          <span className="data-label">Faults</span>
          <span className={`data-value ${ecu.fault_count > 0 ? 'text-red-400' : ''}`}>
            {ecu.fault_count}
          </span>
        </div>
      </div>
    </div>
  );
}

export function ECUPanel({ ecus }: Props) {
  if (ecus.length === 0) {
    return (
      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-5 gap-3">
        {Array.from({ length: 5 }).map((_, i) => (
          <div key={i} className="card animate-pulse h-32 bg-surface-700" />
        ))}
      </div>
    );
  }

  return (
    <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-5 gap-3">
      {ecus.map(ecu => <ECUCard key={ecu.name} ecu={ecu} />)}
    </div>
  );
}
