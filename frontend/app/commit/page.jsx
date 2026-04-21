'use client';

import { useState } from 'react';

const CONTAINERS = [
  { id: 'a3f9c2', name: 'web-01',      image: 'nginx:alpine-stable', status: 'running', cpuPct: 64 },
  { id: 'b8e1d4', name: 'db-primary',  image: 'postgres:15-image',   status: 'idle',    cpuPct: 12 },
  { id: 'c9f0a1', name: 'redis-cache', image: 'redis:7-alpine',      status: 'running', cpuPct: 38 },
];

const HISTORY = [
  { tag: 'myapp:v2',       source: 'web-01',    time: 'Just now',    author: 'admin_root',   hash: 'sha256:a3f9...c2' },
  { tag: 'db-backup:daily', source: 'db-primary', time: '2 hours ago', author: 'sys_worker_01', hash: 'sha256:b8e1...d4' },
  { tag: 'myapp:v1.9-beta', source: 'web-01',    time: '5 hours ago', author: 'admin_root',   hash: 'sha256:d91c...f0' },
];

const PIPELINE_STEPS = [
  { label: 'Capturing container filesystem delta', time: '0.4s' },
  { label: 'Extracting layer signatures',          time: '1.2s' },
  { label: 'Merging filesystem layers',            time: '0.8s' },
  { label: 'Pushing to Registry: myapp:v2',        time: '3.1s' },
];

export default function CommitPage() {
  const [selected, setSelected] = useState(CONTAINERS[0]);
  const [imageName, setImageName] = useState('myapp');
  const [tag, setTag] = useState('v2');
  const [message, setMessage] = useState('');
  const [committed, setCommitted] = useState(false);
  const [loading, setLoading] = useState(false);
  const [step, setStep] = useState(4); // all done for demo

  const handleCommit = async () => {
    setLoading(true);
    setCommitted(false);
    setStep(0);
    for (let i = 0; i < PIPELINE_STEPS.length; i++) {
      await new Promise((r) => setTimeout(r, 600));
      setStep(i + 1);
    }
    setLoading(false);
    setCommitted(true);
  };

  return (
    <div className="space-y-10 animate-fade-in">
      {/* Header */}
      <div>
        <h1 className="font-['Space_Grotesk'] text-4xl font-bold text-[#e5e1e4] tracking-tight mb-2">Commit Snapshot</h1>
        <p className="text-[#c2c6d6] max-w-2xl">Capture the current state of a running container and promote it to a new image version in your registry.</p>
      </div>

      {/* Two column layout */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
        {/* Container selector */}
        <div className="lg:col-span-4 space-y-4">
          <div className="flex items-center justify-between mb-4">
            <h2 className="font-['Space_Grotesk'] text-sm uppercase tracking-widest text-[#8c909f]">Select Container</h2>
            <span className="text-xs font-mono text-[#adc6ff] bg-[#adc6ff]/10 px-2 py-0.5 rounded">{CONTAINERS.filter((c) => c.status === 'running').length} ACTIVE</span>
          </div>
          {CONTAINERS.map((c) => (
            <div key={c.id} onClick={() => setSelected(c)} className={`group relative p-4 rounded-xl cursor-pointer transition-all border-2 ${selected.id === c.id ? 'bg-[#1c1b1d] border-[#adc6ff] ring-4 ring-[#adc6ff]/5' : 'bg-[#201f22] border-transparent hover:border-[#424754]/30'}`}>
              <div className="flex justify-between items-start mb-2">
                <span className={`font-mono text-xs ${selected.id === c.id ? 'text-[#adc6ff]' : 'text-[#424754]'}`}>{c.id}</span>
                {c.status === 'running' && <span className="flex h-2 w-2 rounded-full bg-[#4cd7f6] shadow-[0_0_8px_rgba(76,215,246,0.6)]" />}
                {c.status !== 'running' && <span className="flex h-2 w-2 rounded-full bg-[#424754]" />}
              </div>
              <h3 className={`font-bold mb-1 ${selected.id === c.id ? 'text-white' : 'text-[#c2c6d6]'}`}>{c.name}</h3>
              <p className={`text-xs font-mono ${selected.id === c.id ? 'text-[#c2c6d6]' : 'text-[#424754]'}`}>{c.image}</p>
              <div className="mt-4 flex items-center gap-3">
                <div className="h-1 w-full bg-[#353437] rounded-full overflow-hidden">
                  <div className="h-full bg-[#adc6ff]" style={{ width: `${c.cpuPct}%` }} />
                </div>
                <span className="text-[10px] font-mono text-[#8c909f]">{c.cpuPct}%</span>
              </div>
            </div>
          ))}
        </div>

        {/* Commit form + pipeline */}
        <div className="lg:col-span-8 flex flex-col gap-8">
          <div className="bg-[#1c1b1d] rounded-xl overflow-hidden border-t-2 border-[#adc6ff] shadow-2xl">
            <div className="p-6 border-b border-white/5 bg-[#2a2a2c]/30">
              <div className="flex items-center gap-3">
                <span className="material-symbols-outlined text-[#adc6ff]">history_edu</span>
                <h2 className="font-['Space_Grotesk'] text-xl font-bold text-white">Commit Details: {selected.name} ({selected.id})</h2>
              </div>
            </div>
            <div className="p-8 space-y-6">
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div className="space-y-2">
                  <label className="block text-xs font-mono uppercase tracking-widest text-[#8c909f]">Target Image Name</label>
                  <input value={imageName} onChange={(e) => setImageName(e.target.value)} className="w-full bg-[#0e0e10] border border-[#424754]/30 rounded-lg px-4 py-3 text-[#e5e1e4] focus:border-[#adc6ff] focus:ring-1 focus:ring-[#adc6ff] outline-none transition-all font-mono" />
                </div>
                <div className="space-y-2">
                  <label className="block text-xs font-mono uppercase tracking-widest text-[#8c909f]">Target Tag</label>
                  <input value={tag} onChange={(e) => setTag(e.target.value)} className="w-full bg-[#0e0e10] border border-[#424754]/30 rounded-lg px-4 py-3 text-[#e5e1e4] focus:border-[#adc6ff] focus:ring-1 focus:ring-[#adc6ff] outline-none transition-all font-mono" />
                </div>
              </div>
              <div className="space-y-2">
                <label className="block text-xs font-mono uppercase tracking-widest text-[#8c909f]">Commit Message</label>
                <textarea value={message} onChange={(e) => setMessage(e.target.value)} rows={3} placeholder="Describe changes made to this container instance..." className="w-full bg-[#0e0e10] border border-[#424754]/30 rounded-lg px-4 py-3 text-[#e5e1e4] focus:border-[#adc6ff] focus:ring-1 focus:ring-[#adc6ff] outline-none transition-all resize-none" />
              </div>
              {committed && (
                <div className="bg-[#009eb9]/10 border border-[#4cd7f6]/20 rounded-xl p-4 flex items-center gap-4">
                  <div className="h-10 w-10 rounded-full bg-[#4cd7f6]/20 flex items-center justify-center text-[#4cd7f6]">
                    <span className="material-symbols-outlined">check_circle</span>
                  </div>
                  <div>
                    <p className="text-white font-bold text-sm">✅ Committed {selected.id} → {imageName}:{tag}</p>
                    <p className="text-[#8c909f] text-xs mt-0.5">Snapshot successfully pushed to the internal forge registry.</p>
                  </div>
                </div>
              )}
              <button onClick={handleCommit} disabled={loading} className="w-full py-4 bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-bold rounded-xl shadow-lg shadow-[#adc6ff]/20 hover:brightness-110 active:scale-[0.99] transition-all flex items-center justify-center gap-2 disabled:opacity-60">
                <span className="material-symbols-outlined">{loading ? 'sync' : 'rocket_launch'}</span>
                {loading ? 'Forging...' : 'Execute Forge Commit'}
              </button>
            </div>
          </div>

          {/* Pipeline steps */}
          <div className="bg-[#1c1b1d] rounded-xl p-6 border border-white/5">
            <h3 className="font-['Space_Grotesk'] text-sm uppercase tracking-widest text-[#8c909f] mb-6">Engine Process Pipeline</h3>
            <div className="space-y-4">
              {PIPELINE_STEPS.map((s, i) => (
                <div key={s.label} className="flex items-center gap-4">
                  <div className="relative flex flex-col items-center">
                    <div className={`h-8 w-8 rounded-full border flex items-center justify-center transition-all ${i < step ? 'bg-[#4cd7f6]/20 border-[#4cd7f6]' : 'bg-[#2a2a2c] border-[#424754]'}`}>
                      {i < step ? <span className="material-symbols-outlined text-sm text-[#4cd7f6]">check</span> : <span className="text-xs text-[#8c909f]">{i + 1}</span>}
                    </div>
                    {i < PIPELINE_STEPS.length - 1 && <div className={`h-4 w-px transition-all ${i < step - 1 ? 'bg-[#4cd7f6]/50' : 'bg-[#424754]/30'}`} />}
                  </div>
                  <span className={`text-sm font-medium transition-all ${i < step ? 'text-white' : 'text-[#8c909f]'}`}>{s.label}</span>
                  {i < step && <span className="ml-auto font-mono text-[10px] text-[#424754]">{s.time}</span>}
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* Commit history */}
      <div className="space-y-6">
        <div className="flex items-center justify-between">
          <h2 className="font-['Space_Grotesk'] text-2xl font-bold text-white">Commit History</h2>
          <button className="text-[#adc6ff] text-sm flex items-center gap-2 hover:underline">
            <span className="material-symbols-outlined text-sm">filter_list</span>Filter Registry
          </button>
        </div>
        <div className="bg-[#1c1b1d] rounded-xl overflow-hidden border border-white/5">
          <table className="w-full text-left">
            <thead>
              <tr className="bg-[#2a2a2c]/50 border-b border-white/5">
                {['Image:Tag','Source Node','Timestamp','Author','Hash'].map((h) => (
                  <th key={h} className="px-6 py-4 text-xs font-mono uppercase tracking-widest text-[#8c909f]">{h}</th>
                ))}
              </tr>
            </thead>
            <tbody className="divide-y divide-white/5">
              {HISTORY.map((r, i) => (
                <tr key={r.tag} className="hover:bg-white/[0.02] transition-colors group">
                  <td className="px-6 py-4">
                    <div className="flex items-center gap-3">
                      <div className={`h-8 w-8 rounded flex items-center justify-center ${i === 0 ? 'bg-[#adc6ff]/10 text-[#adc6ff]' : 'bg-[#2a2a2c] text-[#8c909f]'}`}>
                        <span className="material-symbols-outlined text-sm">layers</span>
                      </div>
                      <span className="font-mono text-sm text-white">{r.tag}</span>
                    </div>
                  </td>
                  <td className="px-6 py-4 text-sm text-[#c2c6d6] font-mono">{r.source}</td>
                  <td className="px-6 py-4 text-sm text-[#8c909f]">{r.time}</td>
                  <td className="px-6 py-4">
                    <div className="flex items-center gap-2">
                      <div className="h-5 w-5 rounded-full bg-[#2a2a2c] border border-[#424754]" />
                      <span className="text-sm text-[#c2c6d6]">{r.author}</span>
                    </div>
                  </td>
                  <td className="px-6 py-4 font-mono text-xs text-[#424754] group-hover:text-[#adc6ff] transition-colors uppercase">{r.hash}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
