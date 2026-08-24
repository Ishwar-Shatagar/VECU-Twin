import React from 'react';
import { useVehicleData } from './hooks/useVehicleData';
import { VehicleOverview } from './components/VehicleOverview';
import { DigitalTwinPanel } from './components/DigitalTwinPanel';
import { ECUPanel } from './components/ECUPanel';
import { VehicleCharts } from './components/VehicleCharts';
import { CANMonitor } from './components/CANMonitor';
import { FaultMonitor } from './components/FaultMonitor';
import { ScenarioControl } from './components/ScenarioControl';
import { SystemStats } from './components/SystemStats';
import { Radio, Car, ShieldAlert, Cpu, Activity } from 'lucide-react';

export function App() {
  const {
    state,
    twinStatus,
    ecus,
    canFrames,
    canStats,
    faults,
    scenarioStatus,
    history,
    connected,
    lastUpdate,
  } = useVehicleData();

  return (
    <div className="min-h-screen bg-surface-950 text-gray-100 flex flex-col">
      {/* Top Header */}
      <header className="bg-surface-900 border-b border-surface-700 px-6 py-3.5 flex items-center justify-between sticky top-0 z-50 backdrop-blur">
        <div className="flex items-center gap-3">
          <div className="bg-brand-500/20 text-brand-400 p-2 rounded-lg border border-brand-500/30">
            <Car size={22} className="animate-pulse-slow" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <h1 className="font-bold text-lg tracking-tight bg-gradient-to-r from-gray-100 via-gray-200 to-brand-400 bg-clip-text text-transparent">
                VECU-Twin
              </h1>
              <span className="text-xs px-2 py-0.5 rounded bg-surface-800 border border-surface-600 text-gray-400 font-mono">
                v1.0.0
              </span>
            </div>
            <p className="text-xs text-gray-400">
              Virtual ECU & Digital Twin Vehicle Simulator
            </p>
          </div>
        </div>

        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2 text-xs">
            <span
              className={`inline-block w-2.5 h-2.5 rounded-full ${
                connected ? 'bg-emerald-500 shadow-[0_0_8px_#10b981]' : 'bg-red-500 shadow-[0_0_8px_#ef4444]'
              }`}
            />
            <span className="font-medium text-gray-300">
              {connected ? 'LIVE TELEMETRY' : 'OFFLINE'}
            </span>
          </div>

          <div className="hidden sm:flex items-center gap-1.5 px-3 py-1 bg-surface-800 border border-surface-700 rounded-lg text-xs font-mono text-gray-400">
            <Radio size={13} className={connected ? 'text-emerald-400' : 'text-gray-500'} />
            <span>IPC Stream</span>
          </div>
        </div>
      </header>

      {/* Main Dashboard Body */}
      <main className="flex-1 p-4 md:p-6 space-y-5 max-w-[1600px] mx-auto w-full">
        {/* System Overview Bar */}
        <SystemStats
          stats={canStats}
          state={state}
          faults={faults}
          lastUpdate={lastUpdate}
        />

        {/* Primary Gauges & Metrics */}
        <section>
          <div className="flex items-center gap-2 mb-2">
            <Activity size={16} className="text-accent-blue" />
            <h2 className="text-sm font-semibold text-gray-300 uppercase tracking-wider">
              Vehicle Telemetry
            </h2>
          </div>
          <VehicleOverview state={state} />
        </section>

        {/* Middle Row: Digital Twin & Scenario Controller */}
        <div className="grid grid-cols-1 lg:grid-cols-12 gap-5">
          <div className="lg:col-span-6">
            <DigitalTwinPanel status={twinStatus} />
          </div>
          <div className="lg:col-span-6">
            <ScenarioControl status={scenarioStatus} />
          </div>
        </div>

        {/* ECU Subsystems */}
        <section>
          <div className="flex items-center gap-2 mb-2">
            <Cpu size={16} className="text-accent-green" />
            <h2 className="text-sm font-semibold text-gray-300 uppercase tracking-wider">
              Virtual ECU Subsystems
            </h2>
          </div>
          <ECUPanel ecus={ecus} />
        </section>

        {/* Telemetry Waveforms */}
        <section>
          <div className="flex items-center gap-2 mb-2">
            <Activity size={16} className="text-brand-400" />
            <h2 className="text-sm font-semibold text-gray-300 uppercase tracking-wider">
              Real-time Waveforms
            </h2>
          </div>
          <VehicleCharts history={history} />
        </section>

        {/* Lower Row: CAN Bus Stream & Fault Detector Logs */}
        <div className="grid grid-cols-1 lg:grid-cols-12 gap-5">
          <div className="lg:col-span-7">
            <CANMonitor frames={canFrames} stats={canStats} />
          </div>
          <div className="lg:col-span-5">
            <FaultMonitor faults={faults} />
          </div>
        </div>
      </main>

      {/* Footer */}
      <footer className="bg-surface-900 border-t border-surface-700 px-6 py-3 text-xs text-gray-500 flex flex-col sm:flex-row items-center justify-between gap-2">
        <div>
          VECU-Twin • Embedded & Automotive Digital Twin Simulation Portfolio
        </div>
        <div className="font-mono text-gray-400">
          C++17 • Python 3.11 • React • Software-Only CAN Abstraction
        </div>
      </footer>
    </div>
  );
}

export default App;
