// Fault Monitor — live fault event list with severity badges

import type { FaultEvent, FaultSeverity } from '../types';
import { AlertTriangle, AlertCircle, Info } from 'lucide-react';

interface Props {
  faults: FaultEvent[];
}

function SeverityIcon({ severity }: { severity: FaultSeverity }) {
  if (severity === 'CRITICAL') return <AlertCircle size={14} className="text-red-400 shrink-0" />;
  if (severity === 'WARNING')  return <AlertTriangle size={14} className="text-amber-400 shrink-0" />;
  return <Info size={14} className="text-gray-400 shrink-0" />;
}

function FaultRow({ fault }: { fault: FaultEvent }) {
  const badgeClass =
    fault.severity === 'CRITICAL' ? 'badge-critical' :
    fault.severity === 'WARNING'  ? 'badge-warning' : 'badge-offline';

  const rowGlow =
    fault.severity === 'CRITICAL' ? 'bg-red-950/20 border-red-900/30' :
    fault.severity === 'WARNING'  ? 'bg-amber-950/20 border-amber-900/20' :
    'border-surface-700/50';

  return (
    <div className={`flex items-start gap-3 p-2.5 rounded-lg border mb-2 ${rowGlow}`}>
      <SeverityIcon severity={fault.severity} />
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-2 flex-wrap">
          <span className={`badge ${badgeClass}`}>{fault.severity}</span>
          <span className="text-sm font-semibold text-gray-200">{fault.fault_type}</span>
          <span className="text-xs text-gray-500">{fault.source_ecu?.replace('_ECU', '')}</span>
        </div>
        <div className="text-xs text-gray-400 mt-0.5">{fault.description}</div>
        <div className="text-xs text-gray-500 mt-0.5">
          Value: <span className="text-gray-300 font-mono">{fault.current_value?.toFixed(1)}</span>
          {' '}| Range: <span className="text-gray-300 font-mono">
            {fault.expected_min?.toFixed(0)}–{fault.expected_max?.toFixed(0)}
          </span>
        </div>
        <div className="text-xs text-accent-blue mt-0.5 italic">{fault.recommended_action}</div>
      </div>
      <div className="text-xs text-gray-600 shrink-0">
        {fault.timestamp ? new Date(fault.timestamp * 1000).toLocaleTimeString() : ''}
      </div>
    </div>
  );
}

export function FaultMonitor({ faults }: Props) {
  const criticalCount = faults.filter(f => f.severity === 'CRITICAL').length;
  const warningCount  = faults.filter(f => f.severity === 'WARNING').length;

  return (
    <div className="card">
      <div className="card-header">
        <span className="card-title">Fault Monitor</span>
        <div className="flex gap-2">
          {criticalCount > 0 && (
            <span className="badge badge-critical">{criticalCount} CRITICAL</span>
          )}
          {warningCount > 0 && (
            <span className="badge badge-warning">{warningCount} WARNING</span>
          )}
          {faults.length === 0 && (
            <span className="badge badge-healthy">NO FAULTS</span>
          )}
        </div>
      </div>

      <div className="max-h-72 overflow-y-auto pr-1">
        {faults.length === 0 ? (
          <div className="text-center py-8 text-gray-600 text-sm">
            No fault events detected
          </div>
        ) : (
          faults.slice(0, 15).map((f, i) => (
            <FaultRow key={`${f.fault_type}-${f.timestamp}-${i}`} fault={f} />
          ))
        )}
      </div>
    </div>
  );
}
