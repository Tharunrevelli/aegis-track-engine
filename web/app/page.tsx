'use client';

import React, { useState, useEffect } from 'react';
import RadarScope, { TrackData } from '../components/RadarScope';
import {
  Shield,
  Activity,
  Radio,
  Cpu,
  Play,
  Pause,
  RotateCcw,
  Crosshair,
  AlertTriangle,
  Zap,
  Layers,
  ArrowUpRight
} from 'lucide-react';

interface ScanData {
  scan_index: number;
  time_sec: number;
  latency_us: number;
  detection_count: number;
  confirmed_tracks: number;
  tracks: TrackData[];
}

interface TelemetryRoot {
  scans: ScanData[];
}

export default function Home() {
  const [telemetry, setTelemetry] = useState<TelemetryRoot | null>(null);
  const [currentScanIdx, setCurrentScanIdx] = useState(0);
  const [isPlaying, setIsPlaying] = useState(true);
  const [selectedTrack, setSelectedTrack] = useState<TrackData | null>(null);
  const [playbackSpeed, setPlaybackSpeed] = useState<number>(1);

  // Load telemetry data exported from C++ simulation
  useEffect(() => {
    fetch('/telemetry_data.json')
      .then((res) => res.json())
      .then((data: TelemetryRoot) => {
        setTelemetry(data);
      })
      .catch((err) => console.error('Failed to load telemetry:', err));
  }, []);

  // Playback timer
  useEffect(() => {
    if (!isPlaying || !telemetry) return;
    const interval = setInterval(() => {
      setCurrentScanIdx((prev) => {
        const next = prev + 1;
        return next >= telemetry.scans.length ? 0 : next;
      });
    }, 1000 / playbackSpeed);

    return () => clearInterval(interval);
  }, [isPlaying, telemetry, playbackSpeed]);

  const currentScan: ScanData | null =
    telemetry && telemetry.scans[currentScanIdx] ? telemetry.scans[currentScanIdx] : null;

  // Sync selected track if it exists in current scan
  useEffect(() => {
    if (!currentScan || !selectedTrack) return;
    const updated = currentScan.tracks.find((t) => t.track_id === selectedTrack.track_id);
    if (updated) setSelectedTrack(updated);
  }, [currentScanIdx]);

  return (
    <main className="min-h-screen bg-[#020804] text-slate-100 flex flex-col p-4 md:p-6 lg:p-8">
      {/* Top Header */}
      <header className="flex flex-col md:flex-row md:items-center justify-between border-b border-emerald-900/60 pb-4 mb-6 gap-4">
        <div>
          <div className="flex items-center gap-3">
            <div className="p-2 rounded bg-emerald-950 border border-emerald-500/40 text-emerald-400">
              <Shield className="w-6 h-6" />
            </div>
            <div>
              <h1 className="text-xl md:text-2xl font-bold tracking-wider text-emerald-400 font-mono">
                AEGIS-TRACK // DEFENCE RADAR SENSOR FUSION
              </h1>
              <p className="text-xs text-emerald-600 font-mono">
                PULSE-DOPPLER MULTI-TARGET TRACKING &amp; KINEMATIC STATE ESTIMATION ENGINE [C++14]
              </p>
            </div>
          </div>
        </div>

        <div className="flex items-center gap-4 text-xs font-mono">
          <div className="flex items-center gap-2 bg-emerald-950/80 px-3 py-1.5 rounded border border-emerald-800">
            <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse"></span>
            <span className="text-emerald-300">RADAR ACTIVE (L-BAND)</span>
          </div>
          <div className="bg-slate-900 px-3 py-1.5 rounded border border-slate-700 text-slate-300">
            BEL PSU SPEC PROTOCOL
          </div>
        </div>
      </header>

      {/* Main Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 flex-1">
        {/* Radar Scope View (7 cols) */}
        <div className="lg:col-span-7 flex flex-col items-center justify-center bg-black/60 rounded-xl border border-emerald-900/40 p-4 shadow-xl">
          <div className="w-full flex items-center justify-between mb-2 px-2 text-xs font-mono text-emerald-500">
            <span className="flex items-center gap-1">
              <Radio className="w-3.5 h-3.5 text-emerald-400" /> PPI 360° AZIMUTH SWEEP
            </span>
            <span>POLAR RESOLUTION: 3 MRAD</span>
          </div>

          <RadarScope
            tracks={currentScan ? currentScan.tracks : []}
            selectedTrack={selectedTrack}
            onSelectTrack={setSelectedTrack}
            maxRangeKm={50}
          />

          {/* Timeline & Playback Controls */}
          <div className="w-full mt-4 bg-emerald-950/40 border border-emerald-900/60 rounded-lg p-3">
            <div className="flex items-center justify-between text-xs font-mono text-emerald-400 mb-2">
              <div className="flex items-center gap-2">
                <button
                  onClick={() => setIsPlaying(!isPlaying)}
                  className="p-1.5 rounded bg-emerald-800 hover:bg-emerald-700 text-black font-bold transition flex items-center gap-1"
                >
                  {isPlaying ? <Pause className="w-3.5 h-3.5" /> : <Play className="w-3.5 h-3.5" />}
                  <span>{isPlaying ? 'PAUSE' : 'RESUME'}</span>
                </button>
                <button
                  onClick={() => {
                    setCurrentScanIdx(0);
                    setIsPlaying(true);
                  }}
                  className="p-1.5 rounded bg-emerald-950 hover:bg-emerald-900 border border-emerald-800 text-emerald-300 transition"
                  title="Rewind"
                >
                  <RotateCcw className="w-3.5 h-3.5" />
                </button>
                <div className="flex items-center gap-1 ml-2">
                  {[1, 2, 5].map((spd) => (
                    <button
                      key={spd}
                      onClick={() => setPlaybackSpeed(spd)}
                      className={`px-2 py-0.5 text-[10px] rounded border ${
                        playbackSpeed === spd
                          ? 'bg-emerald-500 text-black border-emerald-400 font-bold'
                          : 'bg-emerald-950 text-emerald-400 border-emerald-900'
                      }`}
                    >
                      {spd}x
                    </button>
                  ))}
                </div>
              </div>

              <span>
                SCAN: {currentScan ? currentScan.scan_index + 1 : 0} /{' '}
                {telemetry ? telemetry.scans.length : 0} | TIME:{' '}
                {currentScan ? currentScan.time_sec.toFixed(1) : 0}s
              </span>
            </div>

            <input
              type="range"
              min={0}
              max={telemetry ? telemetry.scans.length - 1 : 0}
              value={currentScanIdx}
              onChange={(e) => {
                setCurrentScanIdx(Number(e.target.value));
                setIsPlaying(false);
              }}
              className="w-full accent-emerald-500 cursor-pointer h-1.5 bg-emerald-950 rounded-lg"
            />
          </div>
        </div>

        {/* Right Dashboard Metrics & Track Inspector (5 cols) */}
        <div className="lg:col-span-5 flex flex-col gap-4">
          {/* Real-Time Telemetry HUD */}
          <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
            <div className="bg-emerald-950/30 border border-emerald-900/60 p-3 rounded-lg">
              <div className="text-[10px] font-mono text-emerald-500 uppercase">Active Tracks</div>
              <div className="text-xl font-bold font-mono text-emerald-300 mt-1">
                {currentScan ? currentScan.tracks.length : 0}
              </div>
            </div>
            <div className="bg-emerald-950/30 border border-emerald-900/60 p-3 rounded-lg">
              <div className="text-[10px] font-mono text-emerald-500 uppercase">Confirmed</div>
              <div className="text-xl font-bold font-mono text-emerald-400 mt-1">
                {currentScan ? currentScan.confirmed_tracks : 0}
              </div>
            </div>
            <div className="bg-emerald-950/30 border border-emerald-900/60 p-3 rounded-lg">
              <div className="text-[10px] font-mono text-emerald-500 uppercase">C++ Scan Latency</div>
              <div className="text-xl font-bold font-mono text-sky-400 mt-1">
                {currentScan ? currentScan.latency_us.toFixed(1) : '0.5'} <span className="text-xs">μs</span>
              </div>
            </div>
            <div className="bg-emerald-950/30 border border-emerald-900/60 p-3 rounded-lg">
              <div className="text-[10px] font-mono text-emerald-500 uppercase">Throughput</div>
              <div className="text-xl font-bold font-mono text-amber-400 mt-1">
                85K+ <span className="text-[10px]">ops/s</span>
              </div>
            </div>
          </div>

          {/* Selected Target Inspector */}
          <div className="bg-slate-950/80 border border-emerald-900/60 rounded-xl p-4 flex flex-col gap-3">
            <div className="flex items-center justify-between border-b border-emerald-900/40 pb-2">
              <div className="flex items-center gap-2">
                <Crosshair className="w-4 h-4 text-emerald-400" />
                <h2 className="text-sm font-bold font-mono text-emerald-300">
                  {selectedTrack ? `TARGET DOSSIER // T-${selectedTrack.track_id}` : 'TARGET INSPECTOR'}
                </h2>
              </div>
              {selectedTrack && (
                <span
                  className={`text-[10px] font-mono font-bold px-2 py-0.5 rounded ${
                    selectedTrack.iff === 'HOSTILE'
                      ? 'bg-red-950 text-red-400 border border-red-800'
                      : selectedTrack.iff === 'FRIEND'
                      ? 'bg-emerald-950 text-emerald-400 border border-emerald-800'
                      : 'bg-amber-950 text-amber-400 border border-amber-800'
                  }`}
                >
                  IFF: {selectedTrack.iff}
                </span>
              )}
            </div>

            {selectedTrack ? (
              <div className="grid grid-cols-2 gap-3 text-xs font-mono">
                <div className="bg-emerald-950/30 p-2.5 rounded border border-emerald-900/40">
                  <div className="text-slate-400 text-[10px]">Position (X, Y, Z)</div>
                  <div className="text-emerald-300 font-bold mt-1">
                    [{selectedTrack.x.toFixed(0)}, {selectedTrack.y.toFixed(0)}, {selectedTrack.z.toFixed(0)}] m
                  </div>
                </div>
                <div className="bg-emerald-950/30 p-2.5 rounded border border-emerald-900/40">
                  <div className="text-slate-400 text-[10px]">Ground Velocity</div>
                  <div className="text-emerald-300 font-bold mt-1">
                    {selectedTrack.speed.toFixed(1)} m/s ({Math.round(selectedTrack.speed * 3.6)} km/h)
                  </div>
                </div>
                <div className="bg-emerald-950/30 p-2.5 rounded border border-emerald-900/40">
                  <div className="text-slate-400 text-[10px]">Track Status / Hits</div>
                  <div className="text-emerald-300 font-bold mt-1">
                    {selectedTrack.status} ({selectedTrack.hits} hits)
                  </div>
                </div>
                <div className="bg-emerald-950/30 p-2.5 rounded border border-emerald-900/40">
                  <div className="text-slate-400 text-[10px]">Velocity Vector [Vx, Vy]</div>
                  <div className="text-emerald-300 font-bold mt-1">
                    [{selectedTrack.vx.toFixed(1)}, {selectedTrack.vy.toFixed(1)}] m/s
                  </div>
                </div>
              </div>
            ) : (
              <div className="py-6 text-center text-xs font-mono text-slate-500">
                Click any track blip on the PPI scope or table below to lock telemetry.
              </div>
            )}
          </div>

          {/* Active Target Inventory Table */}
          <div className="bg-slate-950/80 border border-emerald-900/60 rounded-xl p-4 flex flex-col flex-1 overflow-hidden">
            <h3 className="text-xs font-bold font-mono text-emerald-400 mb-3 flex items-center gap-1.5">
              <Layers className="w-3.5 h-3.5" /> CONFIRMED RADAR TRACK MATRIX
            </h3>
            <div className="overflow-x-auto overflow-y-auto max-h-56 scrollbar-thin">
              <table className="w-full text-left text-xs font-mono">
                <thead>
                  <tr className="border-b border-emerald-900/60 text-[10px] text-emerald-500">
                    <th className="pb-1.5">ID</th>
                    <th className="pb-1.5">STATUS</th>
                    <th className="pb-1.5">IFF</th>
                    <th className="pb-1.5">ALT</th>
                    <th className="pb-1.5">SPEED</th>
                    <th className="pb-1.5 text-right">ACTION</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-emerald-950">
                  {currentScan &&
                    currentScan.tracks.map((trk) => (
                      <tr
                        key={trk.track_id}
                        onClick={() => setSelectedTrack(trk)}
                        className={`cursor-pointer hover:bg-emerald-950/40 transition ${
                          selectedTrack?.track_id === trk.track_id ? 'bg-emerald-900/30' : ''
                        }`}
                      >
                        <td className="py-2 text-emerald-300 font-bold">T{trk.track_id}</td>
                        <td className="py-2">
                          <span
                            className={`px-1.5 py-0.5 text-[9px] rounded ${
                              trk.status === 'CONFIRMED'
                                ? 'bg-emerald-950 text-emerald-400'
                                : 'bg-amber-950 text-amber-400'
                            }`}
                          >
                            {trk.status}
                          </span>
                        </td>
                        <td className="py-2">
                          <span
                            className={`px-1.5 py-0.5 text-[9px] rounded ${
                              trk.iff === 'HOSTILE'
                                ? 'text-red-400'
                                : trk.iff === 'FRIEND'
                                ? 'text-emerald-400'
                                : 'text-amber-400'
                            }`}
                          >
                            {trk.iff}
                          </span>
                        </td>
                        <td className="py-2 text-slate-300">{Math.round(trk.z)}m</td>
                        <td className="py-2 text-slate-300">{Math.round(trk.speed)} m/s</td>
                        <td className="py-2 text-right">
                          <button
                            onClick={(e) => {
                              e.stopPropagation();
                              setSelectedTrack(trk);
                            }}
                            className="text-[10px] text-sky-400 hover:text-sky-300 underline"
                          >
                            Select
                          </button>
                        </td>
                      </tr>
                    ))}
                </tbody>
              </table>
            </div>
          </div>

          {/* Technical Architecture Badges */}
          <div className="bg-emerald-950/20 border border-emerald-900/40 rounded-lg p-3 text-[11px] font-mono text-emerald-500/90 flex flex-wrap gap-2">
            <span className="bg-emerald-950 px-2 py-0.5 rounded border border-emerald-800">
              6-DOF Kalman Filter
            </span>
            <span className="bg-emerald-950 px-2 py-0.5 rounded border border-emerald-800">
              Kuhn-Munkres Bipartite Match
            </span>
            <span className="bg-emerald-950 px-2 py-0.5 rounded border border-emerald-800">
              Lock-Free Ring Buffer
            </span>
            <span className="bg-emerald-950 px-2 py-0.5 rounded border border-emerald-800">
              Spatial QuadTree
            </span>
          </div>
        </div>
      </div>
    </main>
  );
}
