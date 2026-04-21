'use client';

import { useState } from 'react';

const NODES = [
  { id: 'web-01',   ip: '172.18.0.2', icon: 'public',   color: 'text-[#adc6ff]', border: 'border-[#adc6ff]', bg: 'bg-[#adc6ff]/10', status: 'running', top: '28%', left: '28%' },
  { id: 'db-01',    ip: '172.18.0.3', icon: 'database',  color: 'text-[#d0bcff]', border: 'border-[#d0bcff]', bg: 'bg-[#d0bcff]/10', status: 'running', top: '28%', left: '68%' },
  { id: 'cache-01', ip: '172.18.0.4', icon: 'memory',    color: 'text-[#4cd7f6]', border: 'border-[#4cd7f6]', bg: 'bg-[#4cd7f6]/10', status: 'running', top: '76%', left: '48%' },
];

export default function NetworkPage() {
  const [selected, setSelected] = useState(NODES[0]);
  const [connectOpen, setConnectOpen] = useState(false);

  return (
    <div className="space-y-8 animate-fade-in">
      {/* Header */}
      <div className="flex items-end justify-between">
        <div>
          <h1 className="font-['Space_Grotesk'] text-4xl font-bold tracking-tight text-[#e5e1e4]">Container Network</h1>
          <p className="text-[#c2c6d6] text-sm mt-1">Real-time topology visualization and orchestration</p>
        </div>
        <div className="flex gap-4">
          <div className="flex flex-col items-end">
            <span className="text-[10px] uppercase tracking-widest text-[#8c909f] font-bold">Active Throughput</span>
            <span className="font-mono text-xl text-[#4cd7f6]">2.4 GB/s</span>
          </div>
          <button onClick={() => setConnectOpen(true)} className="bg-[#adc6ff] hover:bg-[#4d8eff] text-[#002e6a] px-6 py-2 rounded-xl font-bold text-sm shadow-lg flex items-center gap-2 transition-all active:scale-95">
            <span className="material-symbols-outlined text-sm">add_circle</span>Connect Containers
          </button>
        </div>
      </div>

      <div className="grid grid-cols-12 gap-6">
        {/* Topology canvas */}
        <div className="col-span-12 lg:col-span-8">
          <div className="bg-[#0e0e10] rounded-2xl border border-[#424754]/10 overflow-hidden">
            <div className="relative w-full h-[480px] flex items-center justify-center">
              {/* SVG lines — use fixed viewBox so coordinates are deterministic */}
              <svg
                className="absolute inset-0 w-full h-full pointer-events-none"
                viewBox="0 0 800 480"
                preserveAspectRatio="xMidYMid meet"
              >
                <defs>
                  <linearGradient id="lg1" x1="0" y1="0" x2="1" y2="1" gradientUnits="objectBoundingBox">
                    <stop offset="0%"   stopColor="#adc6ff" stopOpacity="0.1" />
                    <stop offset="50%"  stopColor="#adc6ff" stopOpacity="0.5" />
                    <stop offset="100%" stopColor="#adc6ff" stopOpacity="0.1" />
                  </linearGradient>
                  <filter id="glow">
                    <feGaussianBlur stdDeviation="2" result="blur" />
                    <feMerge><feMergeNode in="blur"/><feMergeNode in="SourceGraphic"/></feMerge>
                  </filter>
                </defs>
                {/* Bridge center at 400,240 | web-01 at 224,134 | db-01 at 544,134 | cache-01 at 384,365 */}
                <line x1="400" y1="240" x2="224" y2="134" stroke="url(#lg1)" strokeWidth="2" strokeDasharray="8 5">
                  <animate attributeName="stroke-dashoffset" from="0" to="-26" dur="1.2s" repeatCount="indefinite" />
                </line>
                <line x1="400" y1="240" x2="544" y2="134" stroke="url(#lg1)" strokeWidth="2" strokeDasharray="8 5">
                  <animate attributeName="stroke-dashoffset" from="0" to="-26" dur="1.4s" repeatCount="indefinite" />
                </line>
                <line x1="400" y1="240" x2="384" y2="365" stroke="url(#lg1)" strokeWidth="2" strokeDasharray="8 5">
                  <animate attributeName="stroke-dashoffset" from="0" to="-26" dur="1.0s" repeatCount="indefinite" />
                </line>
                {/* Endpoint glow dots */}
                {[[224,134],[544,134],[384,365]].map(([x,y],i) => (
                  <circle key={i} cx={x} cy={y} r="4" fill="#adc6ff" opacity="0.6" filter="url(#glow)">
                    <animate attributeName="r" values="3;5;3" dur={`${1.2+i*0.2}s`} repeatCount="indefinite" />
                    <animate attributeName="opacity" values="0.4;0.9;0.4" dur={`${1.2+i*0.2}s`} repeatCount="indefinite" />
                  </circle>
                ))}
              </svg>

              {/* Bridge center */}
              <div className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 z-10">
                <div className="w-24 h-24 rounded-full bg-[#2a2a2c] border-2 border-[#adc6ff]/40 flex flex-col items-center justify-center shadow-[0_0_32px_rgba(173,198,255,0.08)] group cursor-pointer hover:border-[#adc6ff] transition-all">
                  <span className="material-symbols-outlined text-3xl text-[#adc6ff]">hub</span>
                  <span className="text-[10px] font-bold uppercase tracking-tighter mt-1 text-[#e5e1e4]">Bridge-0</span>
                </div>
              </div>

              {/* Container nodes */}
              {NODES.map((n) => (
                <div key={n.id} className="absolute z-10 -translate-x-1/2 -translate-y-1/2" style={{ top: n.top, left: n.left }}>
                  <div onClick={() => setSelected(n)} className={`p-4 bg-[#1c1b1d] ${n.border} border-t-2 rounded-xl flex items-center gap-4 shadow-[0_0_32px_rgba(173,198,255,0.08)] cursor-pointer transition-all hover:opacity-100 ${selected?.id === n.id ? 'ring-2 ring-[#adc6ff]/50 opacity-100' : 'opacity-75'}`}>
                    <div className={`w-10 h-10 rounded-lg ${n.bg} flex items-center justify-center`}>
                      <span className={`material-symbols-outlined ${n.color}`}>{n.icon}</span>
                    </div>
                    <div>
                      <div className="text-sm font-bold text-[#e5e1e4]">{n.id}</div>
                      <div className="text-[10px] font-mono text-[#c2c6d6]">{n.ip}</div>
                    </div>
                    <div className="ml-2 w-2 h-2 rounded-full bg-[#4cd7f6] animate-pulse shadow-[0_0_8px_#4cd7f6]" />
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Details panel */}
        <div className="col-span-12 lg:col-span-4 bg-[#201f22] rounded-2xl border border-[#424754]/10 p-6 overflow-y-auto">
          <div className="flex items-center justify-between mb-8">
            <h3 className="font-['Space_Grotesk'] font-bold text-lg text-[#e5e1e4]">Container Details</h3>
          </div>
          {selected && (
            <>
              <div className="flex flex-col items-center text-center mb-8">
                <div className={`w-20 h-20 rounded-2xl ${selected.bg} flex items-center justify-center mb-4 border ${selected.border.replace('border-','border-').replace('/40','')}/20`}>
                  <span className={`material-symbols-outlined text-4xl ${selected.color}`}>{selected.icon}</span>
                </div>
                <h4 className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4]">{selected.id}</h4>
                <span className="px-2 py-0.5 bg-[#009eb9]/20 text-[#4cd7f6] text-[10px] font-bold rounded uppercase tracking-widest mt-2">Running</span>
              </div>
              <div className="space-y-6">
                <div className="bg-[#1c1b1d] p-4 rounded-xl">
                  <label className="text-[10px] uppercase font-bold tracking-widest text-[#8c909f] mb-1 block">Network Configuration</label>
                  {[['IP Address', selected.ip, 'text-[#adc6ff]'], ['MAC Address', '02:42:ac:12:00:02', 'text-[#e5e1e4]'], ['Network Mode', 'Bridge', 'text-[#e5e1e4]']].map(([k, v, c]) => (
                    <div key={k} className="flex justify-between items-center py-2 border-b border-[#424754]/10 last:border-0">
                      <span className="text-sm text-[#c2c6d6]">{k}</span>
                      <span className={`text-sm font-mono ${c}`}>{v}</span>
                    </div>
                  ))}
                </div>
                <div>
                  <label className="text-[10px] uppercase font-bold tracking-widest text-[#8c909f] mb-3 block px-1">Active Connections (2)</label>
                  <div className="space-y-2">
                    {NODES.filter((n) => n.id !== selected.id).map((n) => (
                      <div key={n.id} className="flex items-center justify-between p-3 bg-[#2a2a2c] rounded-lg hover:bg-[#353437] transition-colors cursor-pointer group">
                        <div className="flex items-center gap-3">
                          <span className={`material-symbols-outlined ${n.color} text-sm`}>{n.icon}</span>
                          <span className="text-sm font-medium text-[#e5e1e4]">{n.id}</span>
                        </div>
                        <span className="material-symbols-outlined text-[#424754] group-hover:text-[#adc6ff] transition-colors">arrow_forward</span>
                      </div>
                    ))}
                  </div>
                </div>
                {/* Live traffic mini bars */}
                <div className="bg-[#1c1b1d] p-4 rounded-xl">
                  <label className="text-[10px] uppercase font-bold tracking-widest text-[#8c909f] mb-3 block">Live Traffic</label>
                  <div className="h-16 flex items-end gap-1">
                    {[40,60,30,80,50,90,45].map((h, i) => (
                      <div key={i} className="flex-1 rounded-t-sm" style={{ height: `${h}%`, backgroundColor: `rgba(173,198,255,${h > 70 ? 0.5 : 0.2})` }} />
                    ))}
                  </div>
                </div>
              </div>
            </>
          )}
        </div>
      </div>

      {/* Connect modal */}
      {connectOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-6">
          <div className="absolute inset-0 bg-[#131315]/60 backdrop-blur-md" onClick={() => setConnectOpen(false)} />
          <div className="relative w-full max-w-lg bg-[#2a2a2c] rounded-[2rem] p-8 shadow-2xl border border-[#424754]/30 shadow-[0_0_32px_rgba(173,198,255,0.08)]">
            <div className="flex justify-between items-start mb-8">
              <div>
                <h2 className="font-['Space_Grotesk'] text-3xl font-bold text-[#e5e1e4]">Connect Containers</h2>
                <p className="text-[#c2c6d6] text-sm">Define a new network bridge between nodes</p>
              </div>
              <button onClick={() => setConnectOpen(false)} className="p-2 hover:bg-[#353437] rounded-full text-[#c2c6d6]"><span className="material-symbols-outlined">close</span></button>
            </div>
            <div className="space-y-6">
              <div className="space-y-2">
                <label className="text-xs font-bold uppercase tracking-widest text-[#adc6ff]">Source Node</label>
                <select className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl py-3 px-4 text-[#e5e1e4] focus:outline-none focus:border-[#adc6ff] transition-all">
                  {NODES.map((n) => <option key={n.id}>{n.id} ({n.ip})</option>)}
                </select>
              </div>
              <div className="flex justify-center -my-2 relative z-10">
                <div className="bg-[#adc6ff] p-2 rounded-full shadow-lg"><span className="material-symbols-outlined text-[#002e6a]">link</span></div>
              </div>
              <div className="space-y-2">
                <label className="text-xs font-bold uppercase tracking-widest text-[#adc6ff]">Destination Node</label>
                <select className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl py-3 px-4 text-[#e5e1e4] focus:outline-none focus:border-[#adc6ff] transition-all">
                  {NODES.slice().reverse().map((n) => <option key={n.id}>{n.id} ({n.ip})</option>)}
                </select>
              </div>
              <div className="pt-4 flex gap-3">
                <button onClick={() => setConnectOpen(false)} className="flex-1 py-4 text-sm font-bold text-[#c2c6d6] hover:text-[#e5e1e4] transition-colors">Discard</button>
                <button className="flex-[2] py-4 bg-[#adc6ff] text-[#002e6a] rounded-2xl font-bold shadow-xl hover:bg-[#4d8eff] active:scale-95 transition-all">Establish Connection</button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
