// Vehicle Overview — main metric cards grid

import type { VehicleState } from '../types';

interface Props {
  state: VehicleState | null;
}

function MetricCard({
  label, value, unit, color = 'text-gray-100', subtext
}: {
  label: string; value: string | number; unit?: string;
  color?: string; subtext?: string;
}) {
  return (
    <div className="card flex flex-col gap-1">
      <div className="section-label">{label}</div>
      <div className="flex items-baseline gap-1">
        <span className={`text-3xl font-bold tabular-nums ${color}`}>{value}</span>
        {unit && <span className="text-gray-500 text-sm">{unit}</span>}
      </div>
      {subtext && <div className="text-xs text-gray-500 mt-0.5">{subtext}</div>}
    </div>
  );
}

const modeColors: Record<string, string> = {
  IDLE:         'text-gray-400',
  DRIVING:      'text-accent-green',
  ACCELERATING: 'text-brand-400',
  BRAKING:      'text-accent-orange',
  FAULT:        'text-accent-red',
};

export function VehicleOverview({ state }: Props) {
  if (!state) {
    return (
      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-7 gap-3">
        {Array.from({ length: 7 }).map((_, i) => (
          <div key={i} className="card animate-pulse h-20 bg-surface-700" />
        ))}
      </div>
    );
  }

  const mode = state.mode ?? 'IDLE';
  const modeColor = modeColors[mode] ?? 'text-gray-400';

  return (
    <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-7 gap-3">
      <MetricCard
        label="Speed"
        value={state.speed_kmh.toFixed(1)}
        unit="km/h"
        color="text-accent-blue"
      />
      <MetricCard
        label="RPM"
        value={Math.round(state.rpm).toLocaleString()}
        unit="rpm"
        color="text-brand-400"
      />
      <MetricCard
        label="Engine Temp"
        value={state.engine_temperature_c.toFixed(1)}
        unit="°C"
        color={
          state.engine_temperature_c >= 130 ? 'text-accent-red' :
          state.engine_temperature_c >= 110 ? 'text-accent-orange' :
          'text-accent-green'
        }
      />
      <MetricCard
        label="Battery"
        value={state.battery_pct.toFixed(1)}
        unit="%"
        color={
          state.battery_pct <= 10 ? 'text-accent-red' :
          state.battery_pct <= 20 ? 'text-accent-orange' :
          'text-accent-green'
        }
        subtext={`${state.battery_voltage_v.toFixed(0)} V`}
      />
      <MetricCard
        label="Brake"
        value={state.brake_active ? 'ON' : 'OFF'}
        color={state.brake_active ? 'text-accent-red' : 'text-gray-400'}
        subtext={`${state.brake_pressure_bar.toFixed(1)} bar`}
      />
      <MetricCard
        label="Steering"
        value={`${state.steering_angle_deg > 0 ? '+' : ''}${state.steering_angle_deg.toFixed(1)}`}
        unit="°"
        color="text-accent-purple"
      />
      <MetricCard
        label="Mode"
        value={mode}
        color={modeColor}
      />
    </div>
  );
}
