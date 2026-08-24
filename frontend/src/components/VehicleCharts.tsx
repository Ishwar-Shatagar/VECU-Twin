// Real-time vehicle data charts using Recharts

import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip,
  ResponsiveContainer, ReferenceLine,
} from 'recharts';
import type { HistoryPoint } from '../types';

interface Props {
  history: HistoryPoint[];
}

const CustomTooltip = ({ active, payload, label }: any) => {
  if (!active || !payload?.length) return null;
  return (
    <div className="bg-surface-800 border border-surface-600 rounded-lg p-2 text-xs">
      {payload.map((p: any) => (
        <div key={p.dataKey} className="flex gap-2">
          <span style={{ color: p.color }}>{p.name}:</span>
          <span className="text-gray-200 font-mono">{p.value?.toFixed?.(1) ?? p.value}</span>
        </div>
      ))}
    </div>
  );
};

function ChartCard({
  title, data, dataKey, color, unit, domain, refLines
}: {
  title: string;
  data: any[];
  dataKey: string;
  color: string;
  unit: string;
  domain?: [number, number];
  refLines?: { y: number; color: string; label: string }[];
}) {
  return (
    <div className="card">
      <div className="card-header">
        <span className="card-title">{title}</span>
        <span className="text-xs text-gray-500">{unit}</span>
      </div>
      <ResponsiveContainer width="100%" height={120}>
        <LineChart data={data} margin={{ top: 4, right: 8, bottom: 0, left: -20 }}>
          <CartesianGrid strokeDasharray="3 3" stroke="#1e2530" />
          <XAxis dataKey="ts" hide />
          <YAxis domain={domain} tick={{ fontSize: 10 }} />
          <Tooltip content={<CustomTooltip />} />
          {refLines?.map(r => (
            <ReferenceLine key={r.label} y={r.y} stroke={r.color}
              strokeDasharray="4 4" strokeOpacity={0.7} />
          ))}
          <Line
            type="monotone"
            dataKey={dataKey}
            stroke={color}
            dot={false}
            strokeWidth={2}
            isAnimationActive={false}
            name={title}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}

export function VehicleCharts({ history }: Props) {
  const data = history.map((h, i) => ({
    ts: i,
    speed_kmh:            h.speed_kmh,
    rpm:                  h.rpm,
    engine_temperature_c: h.engine_temperature_c,
    battery_pct:          h.battery_pct,
  }));

  return (
    <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
      <ChartCard
        title="Speed"
        data={data}
        dataKey="speed_kmh"
        color="#3b82f6"
        unit="km/h"
        domain={[0, 180]}
      />
      <ChartCard
        title="Engine RPM"
        data={data}
        dataKey="rpm"
        color="#f59e0b"
        unit="RPM"
        domain={[0, 8000]}
        refLines={[
          { y: 6000, color: '#f97316', label: 'Warn' },
          { y: 7000, color: '#ef4444', label: 'Crit' },
        ]}
      />
      <ChartCard
        title="Engine Temperature"
        data={data}
        dataKey="engine_temperature_c"
        color="#10b981"
        unit="°C"
        domain={[0, 160]}
        refLines={[
          { y: 110, color: '#f97316', label: 'Warn' },
          { y: 130, color: '#ef4444', label: 'Crit' },
        ]}
      />
      <ChartCard
        title="Battery"
        data={data}
        dataKey="battery_pct"
        color="#8b5cf6"
        unit="%"
        domain={[0, 100]}
        refLines={[
          { y: 20, color: '#f97316', label: 'Low' },
          { y: 10, color: '#ef4444', label: 'Crit' },
        ]}
      />
    </div>
  );
}
