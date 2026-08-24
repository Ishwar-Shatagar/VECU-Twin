// System statistics summary bar

import type { CANStatistics, VehicleState, FaultEvent } from '../types';
import { Activity, Cpu, AlertTriangle, Clock } from 'lucide-react';

interface Props {
  stats: CANStatistics | null;
  state: VehicleState | null;
  faults: FaultEvent[];
  lastUpdate: Date | null;
}

function StatItem({ icon, label, value, color = 'text-gray-200' }: {
  icon: React.ReactNode; label: string; value: string; color?: string;
}) {
  return (
    <div className="flex items-center gap-2 px-4 py-2 border-r border-surface-700 last:border-0">
      <div className="text-gray-500">{icon}</div>
      <div>
        <div className="text-xs text-gray-500">{label}</div>
        <div className={`text-sm font-semibold tabular-nums ${color}`}>{value}</div>
      </div>
    </div>
  );
}

function formatUptime(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);
  return h > 0 ? `${h}h ${m}m` : m > 0 ? `${m}m ${s}s` : `${s}s`;
}

export function SystemStats({ stats, state, faults, lastUpdate }: Props) {
  const criticalFaults = faults.filter(f => f.severity === 'CRITICAL').length;
  const activeFaults   = faults.filter(f => f.severity !== 'NORMAL').length;

  return (
    <div className="bg-surface-900 border border-surface-700 rounded-xl flex flex-wrap">
      <StatItem
        icon={<Activity size={14} />}
        label="CAN Messages"
        value={stats?.total_frames?.toLocaleString() ?? '–'}
        color="text-accent-blue"
      />
      <StatItem
        icon={<Activity size={14} />}
        label="Msg/sec"
        value={stats ? `${stats.frames_per_second.toFixed(1)}` : '–'}
        color="text-brand-400"
      />
      <StatItem
        icon={<Cpu size={14} />}
        label="Active ECUs"
        value={stats ? String(stats.active_ecus) : '–'}
        color="text-accent-green"
      />
      <StatItem
        icon={<AlertTriangle size={14} />}
        label="Active Faults"
        value={String(activeFaults)}
        color={criticalFaults > 0 ? 'text-red-400' : activeFaults > 0 ? 'text-amber-400' : 'text-emerald-400'}
      />
      <StatItem
        icon={<Clock size={14} />}
        label="Uptime"
        value={stats ? formatUptime(stats.uptime_seconds) : '–'}
        color="text-gray-300"
      />
      <StatItem
        icon={<Clock size={14} />}
        label="Last Update"
        value={lastUpdate ? lastUpdate.toLocaleTimeString() : '–'}
        color="text-gray-400"
      />
    </div>
  );
}
