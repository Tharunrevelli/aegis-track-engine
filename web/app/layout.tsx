import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'AEGIS-TRACK | Tactical Radar Target Tracking & Sensor Fusion',
  description: 'Real-time Pulse-Doppler Radar Multi-Target Tracker with 6-DOF Kalman Filter & Hungarian Association',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="antialiased bg-[#020704] text-slate-100 min-h-screen">
        {children}
      </body>
    </html>
  );
}
