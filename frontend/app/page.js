'use client';

import { useState, useEffect } from 'react';
import StatCard from '@/components/StatCard';
import StatusDot from '@/components/StatusDot';
import HealthBadge from '@/components/HealthBadge';
import { containersApi, healthApi } from '@/lib/api';

/* ── Demo data ─────────────────────────────────────────────────── */
const DEMO_CONTAINERS = [
  { id: 'a3f9c2d1', name: 'web-01-prod',    status: 'running', image: 'nginx:stable-alpine',  cpu: 2.4,  mem: 128, ports: '80:80, 443:443' },
  { id: 'b7e3d1f4', name: 'db-primary-01',  status: 'running', image: 'postgres:15-alpine',   cpu: 8.1,  mem: 256, ports: '5432:5432' },
  { id: 'c9f1a4e2', name: 'redis-cache',    status: 'running', image: 'redis:7-alpine',        cpu: 0.8,  mem: 64,  ports: '6379:6379' },
  { id: 'd2a8b5c3', name: 'worker-01',      status: 'stopped', image: 'node:20-alpine',        cpu: 0,    mem: 0,   ports: '—' },
];

const ACTIVITY = [
  { time: '08:24 AM', actor: 'SYSTEM',    color: 'bg-[#4cd7f6]', msg: <>New container <span className="text-[#adc6ff] font-mono">redis-cache</span> was successfully initialized.</> },
  { time: 'Yesterday', actor: 'ROOT',     color: 'bg-[#d0bcff]', msg: <>Manual checkpoint <span className="text-[#d0bcff] font-mono">pre-deployment-stable</span> created.</> },
  { time: 'Yesterday', actor: 'SYSTEM',   color: 'bg-[#4cd7f6]', msg: <>Network <span className="text-[#adc6ff] font-mono">vnet-prod-01</span> bridge established.</> },
];

/* ─── Resource bar ──────────────────────────────────────────────── */
function ResourceBar({ label, pct, color }) {
  return (
    <div>
      <div className="flex justify-between items-center mb-2">
        <span className="text-[11px] font-bold font-['Inter'] uppercase tracking-widest text-[#8c909f]">{label}</span>
        <span className={`text-xs font-mono ${color}`}>{pct}%</span>
      </div>
      <div className="h-1.5 w-full bg-[#0e0e10] rounded-full overflow-hidden">
        <div className={`h-full rounded-full transition-all ${color.replace('text-', 'bg-')}`} style={{ width: `${pct}%` }} />
      </div>
    </div>
  );
}

/* ─── Network mini-node ─────────────────────────────────────────── */
function NetNode({ icon, label, primary }) {
  return (
    <div className="flex flex-col items-center gap-2">
      <div className={`h-14 w-14 rounded-xl flex items-center justify-center ${primary ? 'bg-[#2a2a2c] border-2 border-[#adc6ff] shadow-[0_0_20px_#adc6ff20]' : 'bg-[#2a2a2c] border border-[#424754]'}`}>
        <span className={`material-symbols-outlined text-2xl ${primary ? 'text-[#adc6ff]' : 'text-[#8c909f]'}`}>{icon}</span>
      </div>
      <span className="font-mono text-[10px] text-[#8c909f]">{label}</span>
    </div>
  );
}

/* ─── Dashboard ─────────────────────────────────────────────────── */
export default function DashboardPage() {
  const [containers, setContainers] = useState(DEMO_CONTAINERS);
  const [healthData, setHealthData]  = useState([]);

  useEffect(() => {
    containersApi.list()
      .then((d) => { if (Array.isArray(d) && d.length > 0) setContainers(d); })
      .catch(() => {});
    healthApi.all()
      .then((d) => { if (Array.isArray(d)) setHealthData(d); })
      .catch(() => {});
  }, []);

  const running = containers.filter((c) => c.status === 'running');
  const stopped = containers.filter((c) => c.status !== 'running');

  return (
    <div className="space-y-8 animate-fade-in">

      {/* ── Page Header ─────────────────────────────────────────── */}
      <header>
        <h1 className="font-['Space_Grotesk'] text-4xl font-bold text-[#e5e1e4] tracking-tight">Dashboard</h1>
        <p className="text-[#c2c6d6] mt-1 text-lg">
          Good morning — {running.length} container{running.length !== 1 ? 's' : ''} running
        </p>
      </header>

      {/* ── Row 1: Primary Stat Cards ───────────────────────────── */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6">
        <StatCard
          label="Running"
          value={String(running.length).padStart(2, '0')}
          sub={<><span className="material-symbols-outlined text-[14px]">trending_up</span>+1 since last hour</>}
          accent="tertiary"
          pulse
        />
        <StatCard
          label="Images"
          value="12"
          sub="4.2 GB used"
          icon="layers"
          accent="none"
        />
        <StatCard
          label="Networks"
          value="04"
          sub="Isolated VNETs"
          icon="lan"
          accent="none"
        />
        <StatCard
          label="Healthy"
          value="100%"
          sub={<><span className="material-symbols-outlined text-[14px]">check_circle</span>No reported issues</>}
          icon="check_circle"
          accent="tertiary"
        />
      </div>

      {/* ── Row 2: Secondary Stats ──────────────────────────────── */}
      <div className="grid grid-cols-1 sm:grid-cols-3 gap-6">
        {[
          { label: 'Stacks',      value: '02 Active',  icon: 'view_agenda', color: 'text-[#adc6ff]' },
          { label: 'Checkpoints', value: '08 Saved',   icon: 'history',     color: 'text-[#d0bcff]' },
          { label: 'Avg CPU',     value: '14.2%',      icon: 'monitoring',  color: 'text-[#ffb4ab]' },
        ].map((s) => (
          <div key={s.label} className="bg-[#1c1b1d] rounded-xl p-6 flex items-center justify-between border border-[#424754]/20">
            <div>
              <span className="text-[11px] font-bold uppercase tracking-widest text-[#8c909f]">{s.label}</span>
              <div className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4] mt-1">{s.value}</div>
            </div>
            <div className="bg-[#2a2a2c] p-3 rounded-lg">
              <span className={`material-symbols-outlined text-[22px] ${s.color}`}>{s.icon}</span>
            </div>
          </div>
        ))}
      </div>

      {/* ── Main 10-col Grid ────────────────────────────────────── */}
      <div className="grid grid-cols-1 lg:grid-cols-10 gap-8">

        {/* Left 60% — Network Overview + Container Table */}
        <div className="lg:col-span-6 space-y-8">
          <section className="bg-[#1c1b1d] rounded-xl p-8 border border-[#424754]/10">
            {/* Section header */}
            <div className="flex justify-between items-center mb-8">
              <h3 className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4]">Network Overview</h3>
              <button className="text-xs font-mono text-[#adc6ff] flex items-center gap-1 hover:underline">
                REFRESH MAP <span className="material-symbols-outlined text-[14px]">refresh</span>
              </button>
            </div>

            {/* Mini topology diagram */}
            <div className="relative h-52 bg-[#0e0e10] rounded-xl overflow-hidden border border-[#424754]/10 mb-8">
              <div className="absolute inset-0 bg-[radial-gradient(circle_at_50%_50%,_#adc6ff08_0%,_transparent_70%)]" />
              {/* SVG connection lines */}
              <svg className="absolute inset-0 w-full h-full pointer-events-none" viewBox="0 0 560 208" preserveAspectRatio="xMidYMid meet">
                <defs>
                  <linearGradient id="dg1" x1="0" y1="0" x2="1" y2="0">
                    <stop offset="0%" stopColor="#adc6ff" stopOpacity="0.1" />
                    <stop offset="50%" stopColor="#adc6ff" stopOpacity="0.6" />
                    <stop offset="100%" stopColor="#adc6ff" stopOpacity="0.1" />
                  </linearGradient>
                  <linearGradient id="dg2" x1="0" y1="0" x2="1" y2="1">
                    <stop offset="0%" stopColor="#4cd7f6" stopOpacity="0.1" />
                    <stop offset="50%" stopColor="#4cd7f6" stopOpacity="0.6" />
                    <stop offset="100%" stopColor="#4cd7f6" stopOpacity="0.1" />
                  </linearGradient>
                  <filter id="ng">
                    <feGaussianBlur stdDeviation="1.5" result="b"/>
                    <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
                  </filter>
                </defs>
                {/* Hub at 280,104 | web-01 at 100,104 | db-01 at 390,52 | cache-01 at 390,156 */}
                <line x1="280" y1="104" x2="100" y2="104" stroke="url(#dg1)" strokeWidth="1.5" strokeDasharray="6 4">
                  <animate attributeName="stroke-dashoffset" from="20" to="0" dur="1.2s" repeatCount="indefinite"/>
                </line>
                <line x1="280" y1="104" x2="390" y2="52" stroke="url(#dg2)" strokeWidth="1.5" strokeDasharray="6 4">
                  <animate attributeName="stroke-dashoffset" from="20" to="0" dur="1.5s" repeatCount="indefinite"/>
                </line>
                <line x1="280" y1="104" x2="390" y2="156" stroke="url(#dg2)" strokeWidth="1.5" strokeDasharray="6 4">
                  <animate attributeName="stroke-dashoffset" from="20" to="0" dur="1.0s" repeatCount="indefinite"/>
                </line>
                {/* Pulsing endpoint dots */}
                {[[100,104,'#adc6ff'],[390,52,'#4cd7f6'],[390,156,'#4cd7f6']].map(([x,y,c],i) => (
                  <circle key={i} cx={x} cy={y} r="3" fill={c} filter="url(#ng)" opacity="0.7">
                    <animate attributeName="r" values="2;4;2" dur={`${1.2+i*0.3}s`} repeatCount="indefinite"/>
                    <animate attributeName="opacity" values="0.4;1;0.4" dur={`${1.2+i*0.3}s`} repeatCount="indefinite"/>
                  </circle>
                ))}
              </svg>

              {/* Nodes as absolutely positioned HTML */}
              {/* web-01 at ~18% left, center */}
              <div className="absolute top-1/2 -translate-y-1/2" style={{ left: '14%' }}>
                <NetNode icon="language" label="web-01" primary />
              </div>
              {/* Hub at center */}
              <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2">
                <div className="h-12 w-12 rounded-full bg-[#2a2a2c] border-2 border-[#adc6ff]/40 flex flex-col items-center justify-center shadow-[0_0_20px_rgba(173,198,255,0.12)]">
                  <span className="material-symbols-outlined text-lg text-[#adc6ff]">hub</span>
                </div>
                <span className="font-mono text-[9px] text-[#8c909f] block text-center mt-1">bridge-0</span>
              </div>
              {/* db-01 at ~70% left, 25% top */}
              <div className="absolute" style={{ left: '70%', top: '25%', transform: 'translate(-50%, -50%)' }}>
                <NetNode icon="database" label="db-01" />
              </div>
              {/* cache-01 at ~70% left, 75% top */}
              <div className="absolute" style={{ left: '70%', top: '75%', transform: 'translate(-50%, -50%)' }}>
                <NetNode icon="memory" label="cache-01" />
              </div>
            </div>

            {/* Running containers table */}
            <div>
              <h4 className="text-[11px] font-bold uppercase tracking-widest text-[#8c909f] mb-4">Running Containers</h4>
              <div className="overflow-x-auto">
                <table className="w-full text-left">
                  <thead>
                    <tr className="text-[11px] text-[#8c909f] border-b border-[#353437] uppercase tracking-widest">
                      <th className="py-3 font-medium">Name</th>
                      <th className="py-3 font-medium">Image</th>
                      <th className="py-3 font-medium">Ports</th>
                      <th className="py-3 font-medium">CPU</th>
                      <th className="py-3 font-medium text-right">Action</th>
                    </tr>
                  </thead>
                  <tbody className="text-sm">
                    {running.map((c) => (
                      <tr key={c.id} className="group hover:bg-[#2a2a2c] transition-colors">
                        <td className="py-4 font-mono text-[#adc6ff] flex items-center gap-2">
                          <StatusDot status="running" />
                          {c.name}
                        </td>
                        <td className="py-4 text-[#8c909f]">{c.image}</td>
                        <td className="py-4 font-mono text-xs text-[#c2c6d6]">{c.ports || '—'}</td>
                        <td className="py-4 font-mono text-[#c2c6d6]">{c.cpu}%</td>
                        <td className="py-4 text-right">
                          <button className="p-1 text-[#8c909f] hover:text-[#adc6ff] transition-colors">
                            <span className="material-symbols-outlined text-[18px]">terminal</span>
                          </button>
                        </td>
                      </tr>
                    ))}
                    {stopped.map((c) => (
                      <tr key={c.id} className="opacity-40">
                        <td className="py-4 font-mono text-[#8c909f] flex items-center gap-2">
                          <StatusDot status="stopped" />
                          {c.name}
                        </td>
                        <td className="py-4 text-[#8c909f]">{c.image}</td>
                        <td className="py-4 font-mono text-xs text-[#8c909f]">{c.ports || '—'}</td>
                        <td className="py-4 font-mono text-[#8c909f]">—</td>
                        <td className="py-4 text-right">
                          <span className="badge badge-gray">Stopped</span>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </section>
        </div>

        {/* Right 40% — Activity + Resources */}
        <div className="lg:col-span-4 space-y-8">

          {/* Recent Activity */}
          <section className="bg-[#1c1b1d] rounded-xl p-8 border border-[#424754]/10">
            <h3 className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4] mb-6">Recent Activity</h3>
            <div className="space-y-6">
              {ACTIVITY.map((a, i) => (
                <div key={i} className={`relative pl-8 ${i < ACTIVITY.length - 1 ? 'pb-6 border-l border-[#353437]' : 'border-l border-[#353437]'}`}>
                  <div className={`absolute left-[-5px] top-0 h-2 w-2 rounded-full ${a.color}`} />
                  <div className="text-[11px] font-mono text-[#8c909f] mb-1">{a.time} — {a.actor}</div>
                  <p className="text-sm text-[#e5e1e4] leading-snug">{a.msg}</p>
                </div>
              ))}
            </div>
          </section>

          {/* Resource Summary */}
          <section className="bg-[#1c1b1d] rounded-xl p-8 border border-[#424754]/10">
            <h3 className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4] mb-6">Resource Summary</h3>
            <div className="space-y-6">
              <ResourceBar label="CPU Allocation" pct={14.2} color="text-[#adc6ff]" />
              <ResourceBar label="Memory Load"    pct={42.8} color="text-[#4cd7f6]" />
              <ResourceBar label="Disk I/O"       pct={61.5} color="text-[#d0bcff]" />
            </div>

            {/* Cluster stats footer */}
            <div className="mt-8 pt-6 border-t border-[#353437] flex gap-4">
              {[['32','Nodes'],['128','Cores'],['512 GB','RAM']].map(([v, l]) => (
                <div key={l} className="flex-1 text-center">
                  <div className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4]">{v}</div>
                  <div className="text-[10px] uppercase tracking-widest text-[#8c909f] font-['Inter'] mt-1">{l}</div>
                </div>
              ))}
            </div>
          </section>

          {/* Health summary cards */}
          <section className="bg-[#1c1b1d] rounded-xl p-8 border border-[#424754]/10">
            <h3 className="font-['Space_Grotesk'] text-xl font-bold text-[#e5e1e4] mb-5">Health Status</h3>
            <div className="space-y-2">
              {(healthData.length > 0 ? healthData : running).map((c, i) => (
                <div key={i} className="flex items-center justify-between px-3 py-2.5 rounded-lg bg-[#201f22]">
                  <span className="text-sm font-mono text-[#adc6ff]">{c.name || c.id}</span>
                  <HealthBadge status={c.health_status || c.status || 'healthy'} />
                </div>
              ))}
            </div>
          </section>

        </div>
      </div>
    </div>
  );
}
