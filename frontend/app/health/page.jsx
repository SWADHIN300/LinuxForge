'use client';

import { useState, useEffect } from 'react';
import { healthApi } from '@/lib/api';

/* ── History bar — mimics Stitch check history ── */
function HistoryBar({ bars }) {
  return (
    <div className="flex gap-1.5 items-end h-8">
      {bars.map((healthy, i) => (
        <div
          key={i}
          className={`w-2 rounded-[2px] transition-all ${
            healthy ? 'h-6 bg-[#009eb9]/80' : 'h-2 bg-[#ffb4ab]'
          }`}
        />
      ))}
    </div>
  );
}

/* ── Single health card ── */
function HealthCard({ container }) {
  const { id, name, status, cpu, mem, uptime, latency, errorCode, retries, history } = container;
  const healthy = status === 'healthy' || status === 'running';

  return (
    <div className={`relative bg-[#1c1b1d] rounded-xl p-6 transition-all hover:bg-[#201f22] shadow-2xl ${
      healthy ? 'border-t-2 border-[#adc6ff]' : 'border-2 border-[#ffb4ab]'
    }`}>
      {/* Header */}
      <div className="flex justify-between items-start mb-6">
        <div>
          <span className="font-mono text-xs text-[#adc6ff] mb-1 block">ID: {id}</span>
          <h3 className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4]">{name}</h3>
        </div>
        <div className={`flex items-center gap-2 px-3 py-1 rounded-full border text-[10px] font-bold uppercase tracking-wider ${
          healthy
            ? 'bg-[#4cd7f6]/10 text-[#4cd7f6] border-[#4cd7f6]/20'
            : 'bg-[#ffb4ab]/10 text-[#ffb4ab] border-[#ffb4ab]/20'
        }`}>
          <span className={`w-1.5 h-1.5 rounded-full ${healthy ? 'bg-[#4cd7f6]' : 'bg-[#ffb4ab]'}`} />
          {healthy ? 'Healthy' : 'Unhealthy'}
        </div>
      </div>

      {/* Stats */}
      <div className="grid grid-cols-2 gap-4 mb-6">
        {healthy ? (
          <>
            <div>
              <span className="text-[10px] uppercase tracking-tighter text-[#8c909f] block mb-1">CPU Usage</span>
              <span className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4]">{cpu}%</span>
            </div>
            <div>
              <span className="text-[10px] uppercase tracking-tighter text-[#8c909f] block mb-1">Uptime</span>
              <span className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4]">{uptime}</span>
            </div>
          </>
        ) : (
          <>
            <div>
              <span className="text-[10px] uppercase tracking-tighter text-[#8c909f] block mb-1">Status Code</span>
              <span className="font-['Space_Grotesk'] text-xl font-bold text-[#ffb4ab]">{errorCode}</span>
            </div>
            <div>
              <span className="text-[10px] uppercase tracking-tighter text-[#8c909f] block mb-1">Retries</span>
              <span className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4]">{retries}</span>
            </div>
          </>
        )}
      </div>

      {/* Check history */}
      <div className="mb-6">
        <span className="text-[10px] uppercase tracking-tighter text-[#8c909f] block mb-3">Check History (24h)</span>
        <HistoryBar bars={history} />
      </div>

      {/* Action button */}
      <button className={`w-full py-3 text-sm font-semibold rounded-xl transition-all active:scale-[0.98] ${
        healthy
          ? 'border border-[#424754] hover:bg-[#2a2a2c] text-[#e5e1e4]'
          : 'bg-[#ffb4ab] text-[#690005] font-bold shadow-lg shadow-[#ffb4ab]/10'
      }`}>
        Run Check Now
      </button>
    </div>
  );
}

const DEMO_CONTAINERS = [
  {
    id: '4f2a-991b', name: 'web-01',    status: 'healthy',   cpu: 12.4, uptime: '14d 2h',
    history: [true,true,true,true,true,true,true,true,true,true,true,true,true,true,true],
  },
  {
    id: '88c2-3110', name: 'db-01',     status: 'healthy',   cpu: 64.2, uptime: '14d 2h', latency: '24ms',
    history: [true,true,true,true,true,true,true,true,true,true,true,true,true,true,true],
  },
  {
    id: '0a11-77fe', name: 'cache-01',  status: 'unhealthy', errorCode: 503, retries: 12,
    history: [true,true,true,true,true,false,false,false,true,true,true,false,false,false,false],
  },
];

const AUDIT_LOGS = [
  { level: 'INFO', time: '14:31:45', msg: 'Node engine handshake successful on port 8080.' },
  { level: 'SAVE', time: '14:31:50', msg: 'Checkpoint "snapshot_v1.0" successfully stored in persistent storage.' },
  { level: 'ERR ', time: '14:32:00', msg: 'Container cache-01 health probe failed: Connection refused (timeout).' },
  { level: 'INFO', time: '14:32:05', msg: 'Re-initializing network interface for node cluster C3.' },
];

const LOG_COLOR = { 'INFO':'text-[#4cd7f6]/70', 'SAVE':'text-[#d0bcff]/70', 'ERR ':'text-[#ffb4ab]/70' };

export default function HealthPage() {
  const [containers, setContainers] = useState(DEMO_CONTAINERS);

  useEffect(() => {
    healthApi.all()
      .then((d) => {
        if (Array.isArray(d) && d.length > 0) {
          setContainers(d.map((c) => ({
            id: c.id?.slice(0, 9) || '—',
            name: c.name || c.id,
            status: c.health_status || c.status,
            cpu: c.cpu || 0,
            uptime: c.uptime || '—',
            errorCode: 503,
            retries: 0,
            history: Array(15).fill(c.status === 'running'),
          })));
        }
      })
      .catch(() => {});
  }, []);

  const healthy   = containers.filter((c) => c.status === 'healthy' || c.status === 'running').length;
  const unhealthy = containers.length - healthy;

  return (
    <div className="space-y-10 animate-fade-in">

      {/* ── Glow decorations ── */}
      <div className="fixed top-0 right-0 w-[500px] h-[500px] bg-[#adc6ff]/5 blur-[120px] rounded-full -z-10 pointer-events-none" />
      <div className="fixed bottom-0 left-0 w-[300px] h-[300px] bg-[#d0bcff]/5 blur-[100px] rounded-full -z-10 pointer-events-none" />

      {/* ── Header ── */}
      <header className="flex flex-col gap-2">
        <div className="flex items-center gap-3 mb-1">
          <span className="w-2 h-2 rounded-full bg-[#009eb9] relative">
            <span className="absolute inset-0 w-full h-full rounded-full bg-[#009eb9] animate-ping opacity-30" />
          </span>
          <span className="text-xs uppercase tracking-widest text-[#8c909f] font-bold">
            System Status: {unhealthy === 0 ? 'Nominal' : `${unhealthy} Issue${unhealthy > 1 ? 's' : ''}`}
          </span>
        </div>
        <h1 className="font-['Space_Grotesk'] text-5xl font-bold tracking-tight text-[#e5e1e4]">Health Monitor</h1>
        <p className="text-[#c2c6d6] text-lg">
          {unhealthy === 0
            ? 'All containers healthy within the simulation cluster.'
            : `${healthy} healthy, ${unhealthy} unhealthy — intervention may be required.`}
        </p>
      </header>

      {/* ── Container cards ── */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {containers.map((c) => (
          <HealthCard key={c.id} container={c} />
        ))}
      </div>

      {/* ── Audit log ── */}
      <section className="bg-[#0e0e10] rounded-xl p-6 border border-[#424754]/10">
        <div className="flex items-center justify-between mb-6">
          <div className="flex items-center gap-3">
            <span className="material-symbols-outlined text-[#adc6ff]">terminal</span>
            <h2 className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4]">System Health Logs</h2>
          </div>
          <span className="font-mono text-[10px] text-[#8c909f]">UTC-0 {new Date().toISOString().slice(0,19).replace('T',' ')}</span>
        </div>
        <div className="font-mono text-xs space-y-2 text-[#8c909f] leading-relaxed">
          {AUDIT_LOGS.map((l, i) => (
            <div key={i} className="flex gap-4">
              <span className={LOG_COLOR[l.level] ?? 'text-zinc-500'}>[{l.level}]</span>
              <span className="text-zinc-700">{l.time}</span>
              <span className={l.level === 'ERR ' ? 'text-[#e5e1e4] font-bold' : ''}>{l.msg}</span>
            </div>
          ))}
        </div>
      </section>
    </div>
  );
}
