'use client';

import { useState, useEffect } from 'react';
import { CpuChart, MemoryChart, NetworkChart } from '@/components/StatsChart';
import { containersApi, statsApi } from '@/lib/api';

const DEMO_CONTAINERS = [
  { id: 'a3f9c2d1', name: 'web-01' },
  { id: 'b7e3d1f4', name: 'db-01' },
  { id: 'c9f1a4e2', name: 'cache-01' },
];

const CPU_BARS   = [30,45,38,55,72,88,65,50,42,30,25,28,45,60,40,35];
const TIME_TICKS = ['12:00','12:15','12:30','12:45','13:00'];

function generateMockData(mins = 60) {
  const now = Date.now();
  return Array.from({ length: mins }, (_, i) => {
    const t = new Date(now - (mins - i) * 60000);
    return {
      time: `${t.getHours()}:${String(t.getMinutes()).padStart(2,'0')}`,
      cpu: Math.floor(Math.random() * 35 + 5),
      memory: Math.floor(Math.random() * 100 + 80),
      rx: Math.floor(Math.random() * 500 + 50),
      tx: Math.floor(Math.random() * 300 + 20),
    };
  });
}

/* ── resource integrity bar ── */
function IntegrityBar({ label, value, unit, color }) {
  return (
    <div className="space-y-2">
      <div className="flex justify-between text-xs font-mono">
        <span className="text-[#8c909f] uppercase">{label}</span>
        <span className={color}>{value} {unit}</span>
      </div>
      <div className="h-1.5 bg-[#0e0e10] rounded-full overflow-hidden">
        <div
          className={`h-full rounded-full ${color.replace('text-','bg-')}`}
          style={{ width: `${Math.min(parseInt(value) || 15, 100)}%` }}
        />
      </div>
    </div>
  );
}

export default function StatsPage() {
  const [containers, setContainers] = useState(DEMO_CONTAINERS);
  const [selected, setSelected]     = useState(DEMO_CONTAINERS[0]);
  const [timeRange, setTimeRange]   = useState(60);
  const [data, setData]             = useState(() => generateMockData(60));
  const [current, setCurrent]       = useState({ cpu: 12.5, memory: 128, rxRate: 1.2, txRate: 0.8 });

  useEffect(() => {
    containersApi.list()
      .then((res) => {
        const list = Array.isArray(res) ? res : res?.containers || [];
        if (list.length > 0) { setContainers(list); setSelected(list[0]); }
      })
      .catch(() => {});
  }, []);

  useEffect(() => {
    if (!selected) return;
    statsApi.history(selected.id, timeRange)
      .then((res) => {
        if (res?.history && Array.isArray(res.history)) setData(res.history);
        else if (Array.isArray(res)) setData(res);
      })
      .catch(() => setData(generateMockData(timeRange)));

    statsApi.current?.(selected.id)
      .then((res) => { if (res?.cpu != null) setCurrent(res); })
      .catch(() => {});
  }, [selected, timeRange]);

  // SSE live updates
  useEffect(() => {
    if (!selected) return;
    let es;
    try {
      es = statsApi.stream(selected.id);
      if (es) {
        es.onmessage = (e) => {
          try {
            const pt = JSON.parse(e.data);
            setCurrent(pt);
            setData((prev) => [...prev.slice(-120), pt]);
          } catch {}
        };
        es.onerror = () => es?.close();
      }
    } catch {}
    return () => es?.close();
  }, [selected]);

  const RANGES = [{ label:'15m', value:15 },{ label:'1hr', value:60 },{ label:'6hr', value:360 },{ label:'24hr', value:1440 }];

  return (
    <div className="space-y-8 animate-fade-in">

      {/* ── Header ── */}
      <div className="flex flex-col md:flex-row md:items-end justify-between gap-6">
        <div>
          <h1 className="font-['Space_Grotesk'] text-4xl font-bold text-[#e5e1e4] tracking-tight mb-2">
            Operational Metrics
          </h1>
          <div className="flex items-center gap-3 text-[#8c909f] font-mono text-sm">
            <span className="flex items-center gap-1.5 text-[#4cd7f6]">
              <span className="w-2 h-2 rounded-full bg-[#4cd7f6] animate-pulse" />
              Live Engine Feed
            </span>
            <span className="opacity-30">•</span>
            <span>Node: {selected?.name || 'US-EAST-01-A'}</span>
          </div>
        </div>

        {/* Time range pills */}
        <div className="flex p-1 bg-[#1c1b1d] rounded-xl border border-[#424754]/20 shadow-inner">
          {RANGES.map((r) => (
            <button
              key={r.value}
              onClick={() => setTimeRange(r.value)}
              className={`px-4 py-1.5 text-xs font-semibold rounded-lg transition-all ${
                timeRange === r.value
                  ? 'bg-[#2a2a2c] text-[#adc6ff] shadow-sm'
                  : 'text-[#8c909f] hover:text-[#e5e1e4]'
              }`}
            >
              {r.label}
            </button>
          ))}
        </div>
      </div>

      {/* ── Container tabs ── */}
      <div className="flex border-b border-[#424754]/20 gap-8 px-2">
        {containers.map((c) => (
          <button
            key={c.id}
            onClick={() => setSelected(c)}
            className={`pb-4 text-sm font-medium border-b-2 transition-all flex items-center gap-2 ${
              selected?.id === c.id
                ? 'border-[#adc6ff] text-[#adc6ff]'
                : 'border-transparent text-[#8c909f] hover:text-[#e5e1e4]'
            }`}
          >
            <span className="font-mono text-xs opacity-60">[</span>
            {c.name || c.id?.slice(0,8)}
            <span className="font-mono text-xs opacity-60">]</span>
            {selected?.id === c.id && (
              <span className="text-[10px] bg-[#adc6ff]/10 px-1.5 py-0.5 rounded text-[#adc6ff] uppercase">Active</span>
            )}
          </button>
        ))}
      </div>

      {/* ── Top 3 metric cards ── */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        {[
          { label:'CPU Utilization',    value:current.cpu?.toFixed(1) ?? '12.5', unit:'%',    color:'text-[#adc6ff]', icon:'memory',                   sub:<><span className="material-symbols-outlined text-sm">trending_down</span>0.4% lower than avg</> },
          { label:'Memory Commit',      value:current.memory ?? 128,             unit:'MB',   color:'text-[#d0bcff]', icon:'data_usage',                sub:<><span className="material-symbols-outlined text-sm">straight</span>Stable allocation</> },
          { label:'Network Throughput', value:current.rxRate ?? 1.2,             unit:'MB/s', color:'text-[#4cd7f6]', icon:'settings_input_component',  sub:<><span className="material-symbols-outlined text-sm">trending_up</span>Peaked at 4.5 MB/s</> },
        ].map((m) => (
          <div key={m.label} className={`bg-[#1c1b1d] rounded-2xl p-6 border-t-2 ${m.color.replace('text-','border-')} shadow-lg overflow-hidden`}>
            <div className="flex justify-between items-start mb-4">
              <span className="text-xs font-bold text-[#8c909f] uppercase tracking-widest">{m.label}</span>
              <span className={`material-symbols-outlined text-xl ${m.color}`}>{m.icon}</span>
            </div>
            <div className="flex items-baseline gap-2">
              <span className="font-['Space_Grotesk'] text-5xl font-extrabold text-[#e5e1e4]">{m.value}</span>
              <span className={`font-['Space_Grotesk'] text-xl font-medium ${m.color}`}>{m.unit}</span>
            </div>
            <div className={`mt-4 flex items-center gap-2 text-xs ${m.color}`}>{m.sub}</div>
          </div>
        ))}
      </div>

      {/* ── Bento charts grid ── */}
      <div className="grid grid-cols-12 gap-6">

        {/* CPU bar chart (large, 8-col) */}
        <div className="col-span-12 lg:col-span-8 bg-[#1c1b1d] rounded-2xl p-6 border border-[#424754]/10">
          <div className="flex items-center justify-between mb-8">
            <div>
              <h3 className="text-lg font-['Space_Grotesk'] font-semibold text-[#e5e1e4]">CPU Cluster Cycles</h3>
              <p className="text-xs text-[#8c909f]">Core affinity and threading load</p>
            </div>
            <span className="text-[10px] font-mono bg-[#0e0e10] px-2 py-1 rounded border border-[#424754]/20 uppercase text-[#8c909f]">Real-time</span>
          </div>
          <div className="h-64 flex items-end gap-1.5 relative pt-4">
            {/* Grid lines */}
            <div className="absolute inset-0 flex flex-col justify-between pointer-events-none opacity-5">
              {[1,2,3,4].map((i) => <div key={i} className="w-full h-px bg-[#e5e1e4]" />)}
            </div>
            {CPU_BARS.map((h, i) => (
              <div
                key={i}
                className={`flex-1 rounded-t-sm transition-all hover:opacity-100 ${h >= 80 ? 'bg-[#adc6ff]/60 hover:bg-[#adc6ff]' : 'bg-[#adc6ff]/20 hover:bg-[#adc6ff]/40'} relative`}
                style={{ height: `${h}%` }}
              >
                {h >= 80 && (
                  <div className="absolute -top-7 left-1/2 -translate-x-1/2 bg-[#353437] px-2 py-1 rounded text-[10px] font-mono text-[#adc6ff] shadow-xl whitespace-nowrap">
                    {h}%
                  </div>
                )}
              </div>
            ))}
          </div>
          <div className="flex justify-between mt-4 text-[10px] font-mono text-[#8c909f] uppercase tracking-tighter">
            {TIME_TICKS.map((t) => <span key={t}>{t}</span>)}
          </div>
        </div>

        {/* Resource Integrity (4-col) */}
        <div className="col-span-12 lg:col-span-4 bg-[#1c1b1d] rounded-2xl p-6 border border-[#424754]/10 flex flex-col">
          <h3 className="text-lg font-['Space_Grotesk'] font-semibold text-[#e5e1e4] mb-6">Resource Integrity</h3>
          <div className="space-y-6 flex-1">
            <IntegrityBar label="Shared Pool A"  value="42" unit="/ 100 Node" color="text-[#adc6ff]" />
            <IntegrityBar label="I/O Wait State" value="15" unit="0.02 ms"    color="text-[#4cd7f6]" />
            <IntegrityBar label="Cache Pressure" value="68" unit="Optimal"    color="text-[#d0bcff]" />
          </div>
          <div className="mt-8 p-4 rounded-xl bg-[#2a2a2c]/60 backdrop-blur-xl border border-[#adc6ff]/10">
            <div className="flex items-center gap-3">
              <span className="material-symbols-outlined text-[#adc6ff] text-2xl">info</span>
              <div className="text-[11px] leading-relaxed text-[#8c909f]">
                Engine detected a minor <span className="text-[#adc6ff] font-bold">Latency Spill</span> in segment DB-4. Optimization recommended.
              </div>
            </div>
          </div>
        </div>

        {/* Memory pressure SVG chart (6-col) */}
        <div className="col-span-12 lg:col-span-6 bg-[#1c1b1d] rounded-2xl p-6 border border-[#424754]/10">
          <div className="flex items-center justify-between mb-8">
            <div>
              <h3 className="text-lg font-['Space_Grotesk'] font-semibold text-[#e5e1e4]">Memory Pressure</h3>
              <p className="text-xs text-[#8c909f]">Heap vs Stack allocation trace</p>
            </div>
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-[#d0bcff]" />
              <span className="text-[10px] text-[#8c909f] font-mono">RSS</span>
            </div>
          </div>
          <div className="h-48 relative">
            <svg className="w-full h-full" preserveAspectRatio="none" viewBox="0 0 100 100">
              <defs>
                <linearGradient id="memGrad" x1="0%" x2="0%" y1="0%" y2="100%">
                  <stop offset="0%" stopColor="#d0bcff" stopOpacity="0.3" />
                  <stop offset="100%" stopColor="#d0bcff" stopOpacity="0" />
                </linearGradient>
              </defs>
              <path d="M0,80 Q10,70 20,75 T40,60 T60,50 T80,45 T100,30 L100,100 L0,100 Z" fill="url(#memGrad)" />
              <path d="M0,80 Q10,70 20,75 T40,60 T60,50 T80,45 T100,30" fill="none" stroke="#d0bcff" strokeWidth="2" />
            </svg>
          </div>
        </div>

        {/* Network I/O SVG chart (6-col) */}
        <div className="col-span-12 lg:col-span-6 bg-[#1c1b1d] rounded-2xl p-6 border border-[#424754]/10">
          <div className="flex items-center justify-between mb-8">
            <div>
              <h3 className="text-lg font-['Space_Grotesk'] font-semibold text-[#e5e1e4]">Network I/O Flow</h3>
              <p className="text-xs text-[#8c909f]">Packet transmission vs reception</p>
            </div>
            <div className="flex gap-4">
              {[['#4cd7f6','RX'],['#adc6ff','TX']].map(([c,l]) => (
                <div key={l} className="flex items-center gap-2">
                  <span className="w-3 h-0.5 rounded" style={{ backgroundColor: c }} />
                  <span className="text-[10px] text-[#8c909f] font-mono">{l}</span>
                </div>
              ))}
            </div>
          </div>
          <div className="h-48 relative flex items-center">
            <svg className="w-full h-24" preserveAspectRatio="none" viewBox="0 0 100 20">
              <path d="M0,10 L10,12 L20,8 L30,15 L40,5 L50,18 L60,10 L70,12 L80,5 L90,15 L100,10" fill="none" stroke="#4cd7f6" strokeWidth="1.5" />
              <path d="M0,12 L10,15 L20,10 L30,18 L40,8 L50,12 L60,15 L70,8 L80,18 L90,10 L100,15" fill="none" stroke="#adc6ff" strokeDasharray="2,2" strokeWidth="1.5" />
            </svg>
          </div>
        </div>

        {/* Runtime logs stream */}
        <div className="col-span-12 bg-[#0e0e10] border border-[#424754]/10 rounded-2xl overflow-hidden shadow-2xl">
          <div className="px-6 py-4 bg-[#2a2a2c] flex items-center justify-between border-b border-[#424754]/10">
            <div className="flex items-center gap-3">
              <span className="material-symbols-outlined text-sm text-[#adc6ff]">terminal</span>
              <span className="text-xs font-mono font-bold tracking-widest text-[#e5e1e4]">RUNTIME_LOGS_STREAM</span>
            </div>
            <div className="flex gap-2">
              <div className="w-3 h-3 rounded-full bg-[#ffb4ab]/20 border border-[#ffb4ab]/40" />
              <div className="w-3 h-3 rounded-full bg-[#d0bcff]/20 border border-[#d0bcff]/40" />
              <div className="w-3 h-3 rounded-full bg-[#4cd7f6]/20 border border-[#4cd7f6]/40" />
            </div>
          </div>
          <div className="p-6 font-mono text-xs space-y-2 max-h-48 overflow-y-auto bg-black/40 text-zinc-500">
            {[
              { ts:'12:45:01', lvl:'INF', c:'#c2c6d6', msg:<>Container <span className="text-[#adc6ff]">web-01</span> health check passed.</> },
              { ts:'12:45:04', lvl:'INF', c:'#c2c6d6', msg:'Balancer redirected 1.2k req to shard-A.' },
              { ts:'12:45:12', lvl:'WRN', c:'#d0bcff', msg:<>High memory pressure in <span className="text-[#d0bcff]">cache-01</span>.</> },
              { ts:'12:45:18', lvl:'INF', c:'#c2c6d6', msg:'Garbage collection sweep complete. Freed 244MB.' },
              { ts:'12:45:22', lvl:'INF', c:'#c2c6d6', msg:'Node sync handshake successful.' },
              { ts:'12:45:30', lvl:'STT', c:'#8c909f', msg:'Listening for engine pulses...' },
            ].map((l,i) => (
              <p key={i}>
                <span className="text-[#acedff]">[{l.ts}]</span>{' '}
                <span style={{ color: l.c }}>{l.lvl}</span>{' '}
                {l.msg}
              </p>
            ))}
          </div>
        </div>
      </div>

      {/* FAB */}
      <button className="fixed bottom-8 right-8 w-14 h-14 bg-[#adc6ff] text-[#002e6a] rounded-full shadow-2xl flex items-center justify-center transition-transform hover:scale-110 active:scale-90 z-50 font-bold">
        <span className="material-symbols-outlined text-3xl">add</span>
      </button>
    </div>
  );
}
