'use client';

import React, { useRef, useEffect, useState } from 'react';

export interface TrackData {
  track_id: number;
  status: string;
  iff: string;
  x: number;
  y: number;
  z: number;
  vx: number;
  vy: number;
  vz: number;
  speed: number;
  hits: number;
  misses: number;
}

interface RadarScopeProps {
  tracks: TrackData[];
  selectedTrack: TrackData | null;
  onSelectTrack: (track: TrackData | null) => void;
  maxRangeKm?: number;
}

export default function RadarScope({
  tracks,
  selectedTrack,
  onSelectTrack,
  maxRangeKm = 50,
}: RadarScopeProps) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [sweepAngle, setSweepAngle] = useState(0);

  // Animate sweeping beam
  useEffect(() => {
    let animId: number;
    const animate = () => {
      setSweepAngle((prev) => (prev + 0.035) % (Math.PI * 2));
      animId = requestAnimationFrame(animate);
    };
    animId = requestAnimationFrame(animate);
    return () => cancelAnimationFrame(animId);
  }, []);

  // Render canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = canvas.width;
    const height = canvas.height;
    const cx = width / 2;
    const cy = height / 2;
    const radius = Math.min(cx, cy) - 20;

    // Clear background
    ctx.fillStyle = '#020b06';
    ctx.fillRect(0, 0, width, height);

    // Range rings
    ctx.lineWidth = 1;
    const rings = 5;
    for (let i = 1; i <= rings; ++i) {
      const r = (radius / rings) * i;
      ctx.strokeStyle = i === rings ? '#10b981' : '#044229';
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, Math.PI * 2);
      ctx.stroke();

      // Range text
      ctx.fillStyle = '#059669';
      ctx.font = '10px monospace';
      ctx.fillText(`${(maxRangeKm / rings) * i} km`, cx + 4, cy - r + 12);
    }

    // Azimuth lines (every 30 degrees)
    for (let deg = 0; deg < 360; deg += 30) {
      const rad = (deg * Math.PI) / 180;
      ctx.strokeStyle = deg % 90 === 0 ? '#05613c' : '#033320';
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(cx + radius * Math.sin(rad), cy - radius * Math.cos(rad));
      ctx.stroke();

      // Degree label
      if (deg % 30 === 0) {
        const lx = cx + (radius + 12) * Math.sin(rad);
        const ly = cy - (radius + 12) * Math.cos(rad);
        ctx.fillStyle = '#10b981';
        ctx.font = '9px monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(`${deg}°`, lx, ly);
      }
    }

    // Rotating Radar Beam with Phosphor trailing fade
    const trailArcs = 25;
    for (let i = 0; i < trailArcs; ++i) {
      const a = sweepAngle - (i * 0.02);
      const alpha = (1 - i / trailArcs) * 0.18;
      ctx.fillStyle = `rgba(16, 185, 129, ${alpha})`;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.arc(cx, cy, radius, a - 0.02, a, false);
      ctx.lineTo(cx, cy);
      ctx.fill();
    }

    // Primary sweep line
    ctx.strokeStyle = '#34d399';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + radius * Math.sin(sweepAngle), cy - radius * Math.cos(sweepAngle));
    ctx.stroke();

    // Scale conversion: meters to pixels
    const maxRangeMeters = maxRangeKm * 1000;
    const scale = radius / maxRangeMeters;

    // Render Active Tracks
    tracks.forEach((track) => {
      const px = cx + track.x * scale;
      const py = cy - track.y * scale; // Inverted Y for screen coordinates

      // Check if inside scope
      const distFromCenter = Math.sqrt((px - cx) ** 2 + (py - cy) ** 2);
      if (distFromCenter > radius) return;

      const isSelected = selectedTrack?.track_id === track.track_id;
      const isConfirmed = track.status === 'CONFIRMED';
      const isCoasting = track.status === 'COASTING';

      // Colors based on IFF
      let color = '#34d399'; // Friend
      if (track.iff === 'HOSTILE') color = '#ef4444';
      if (track.iff === 'UNKNOWN') color = '#f59e0b';

      ctx.save();
      ctx.translate(px, py);

      // Selection ring
      if (isSelected) {
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 1.5;
        ctx.setLineDash([3, 3]);
        ctx.beginPath();
        ctx.arc(0, 0, 16, 0, Math.PI * 2);
        ctx.stroke();
        ctx.setLineDash([]);
      }

      // Target Symbol
      ctx.fillStyle = color;
      ctx.strokeStyle = color;
      ctx.lineWidth = 1.5;

      if (track.iff === 'HOSTILE') {
        // Red Diamond
        ctx.beginPath();
        ctx.moveTo(0, -7);
        ctx.lineTo(7, 0);
        ctx.lineTo(0, 7);
        ctx.lineTo(-7, 0);
        ctx.closePath();
        if (isConfirmed) ctx.fill();
        else ctx.stroke();
      } else if (track.iff === 'FRIEND') {
        // Green Triangle
        ctx.beginPath();
        ctx.moveTo(0, -8);
        ctx.lineTo(7, 6);
        ctx.lineTo(-7, 6);
        ctx.closePath();
        if (isConfirmed) ctx.fill();
        else ctx.stroke();
      } else {
        // Amber Square
        if (isConfirmed) ctx.fillRect(-5, -5, 10, 10);
        else ctx.strokeRect(-5, -5, 10, 10);
      }

      // Velocity Vector (30 seconds projected vector)
      const leadTime = 25; // seconds
      const vx_px = track.vx * scale * leadTime;
      const vy_px = -track.vy * scale * leadTime;

      ctx.strokeStyle = color;
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      ctx.moveTo(0, 0);
      ctx.lineTo(vx_px, vy_px);
      ctx.stroke();

      // Track Tag text
      ctx.font = '10px monospace';
      ctx.fillStyle = isSelected ? '#38bdf8' : '#e2e8f0';
      ctx.textAlign = 'left';
      ctx.fillText(`T${track.track_id}`, 10, -5);
      ctx.fillStyle = '#94a3b8';
      ctx.fillText(`FL${Math.round(track.z / 100)} ${Math.round(track.speed)}m/s`, 10, 7);

      if (isCoasting) {
        ctx.fillStyle = '#f59e0b';
        ctx.fillText('[COAST]', 10, 18);
      }

      ctx.restore();
    });

  }, [tracks, selectedTrack, sweepAngle, maxRangeKm]);

  // Click on canvas to select track
  const handleCanvasClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const clickX = e.clientX - rect.left;
    const clickY = e.clientY - rect.top;

    const cx = canvas.width / 2;
    const cy = canvas.height / 2;
    const radius = Math.min(cx, cy) - 20;
    const maxRangeMeters = maxRangeKm * 1000;
    const scale = radius / maxRangeMeters;

    let closestTrack: TrackData | null = null;
    let minDistance = 20; // 20px click tolerance

    tracks.forEach((track) => {
      const px = cx + track.x * scale;
      const py = cy - track.y * scale;
      const dist = Math.hypot(clickX - px, clickY - py);
      if (dist < minDistance) {
        minDistance = dist;
        closestTrack = track;
      }
    });

    onSelectTrack(closestTrack);
  };

  return (
    <div className="relative flex flex-col items-center">
      <canvas
        ref={canvasRef}
        width={600}
        height={600}
        onClick={handleCanvasClick}
        className="rounded-full border border-emerald-900/60 shadow-[0_0_50px_rgba(16,185,129,0.15)] cursor-crosshair bg-black"
      />
      <div className="absolute top-4 left-4 text-xs font-mono text-emerald-400 bg-emerald-950/60 px-2 py-1 rounded border border-emerald-800">
        RANGE: {maxRangeKm} KM | POLAR PPI
      </div>
      <div className="absolute bottom-4 right-4 text-xs font-mono text-emerald-400/80 bg-emerald-950/60 px-2 py-1 rounded border border-emerald-800">
        C++14 ENGINE STREAM
      </div>
    </div>
  );
}
