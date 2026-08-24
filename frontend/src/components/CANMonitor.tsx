// CAN Bus live message monitor

import type { CANFrame, CANStatistics } from '../types';

interface Props {
  frames: CANFrame[];
  stats: CANStatistics | null;
}

const ECU_COLORS: Record<string, string> = {
  ENGINE_ECU:  'text-brand-400',
  BRAKE_ECU:   'text-accent-red',
  BATTERY_ECU: 'text-accent-purple',
  STEERING_ECU: 'text-accent-blue',
  GATEWAY_ECU: 'text-accent-green',
};

function formatBytes(data: number[]): string {
  return data.map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
}

function formatTime(ts: number): string {
  return new Date(ts * 1000).toLocaleTimeString('en-US', {
    hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
}

export function CANMonitor({ frames, stats }: Props) {
  return (
    <div className="card">
      <div className="card-header">
        <span className="card-title">CAN Bus Monitor</span>
        {stats && (
          <div className="flex gap-4 text-xs text-gray-400">
            <span>{stats.total_frames.toLocaleString()} frames</span>
            <span className="text-accent-blue">{stats.frames_per_second.toFixed(1)} msg/s</span>
            <span>↑ {stats.active_ecus} ECUs</span>
          </div>
        )}
      </div>

      <div className="overflow-x-auto">
        <table className="w-full text-xs">
          <thead>
            <tr className="border-b border-surface-700">
              <th className="text-left py-1.5 px-2 text-gray-500 font-medium w-24">Time</th>
              <th className="text-left py-1.5 px-2 text-gray-500 font-medium w-16">ID</th>
              <th className="text-left py-1.5 px-2 text-gray-500 font-medium">Source</th>
              <th className="text-left py-1.5 px-2 text-gray-500 font-medium">Message</th>
              <th className="text-left py-1.5 px-2 text-gray-500 font-medium w-8">DLC</th>
              <th className="text-left py-1.5 px-2 text-gray-500 font-medium">Payload (hex)</th>
            </tr>
          </thead>
          <tbody>
            {frames.slice(0, 20).map((f, i) => (
              <tr
                key={`${f.timestamp}-${i}`}
                className="border-b border-surface-700/50 hover:bg-surface-700/30 transition-colors"
              >
                <td className="py-1 px-2 font-mono text-gray-500">{formatTime(f.timestamp)}</td>
                <td className="py-1 px-2 font-mono text-brand-400">{f.can_id}</td>
                <td className={`py-1 px-2 font-medium ${ECU_COLORS[f.source] ?? 'text-gray-400'}`}>
                  {f.source?.replace('_ECU', '')}
                </td>
                <td className="py-1 px-2 text-gray-300">{f.message}</td>
                <td className="py-1 px-2 text-center text-gray-500">{f.dlc}</td>
                <td className="py-1 px-2 font-mono text-gray-500 text-xs">
                  {formatBytes(f.data ?? [])}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        {frames.length === 0 && (
          <div className="text-center py-8 text-gray-600 text-sm">
            Waiting for CAN frames...
          </div>
        )}
      </div>
    </div>
  );
}
