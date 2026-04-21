'use client';

import { useState } from 'react';

const CHECKPOINTS = [
  { name: 'before-update', source: 'web-01', created: '2023-10-24 14:20:01', size: '1.2 GB', memState: '512MB', diskDelta: '+142 MB', tags: ['us-east-1','amd64','kvm-v4'] },
  { name: 'pre-migration',  source: 'db-01',  created: '2023-10-23 09:12:44', size: '3.0 GB', memState: '1.2GB',  diskDelta: '+380 MB', tags: ['us-west-2','arm64'] },
];

export default function CheckpointsPage() {
  const [expanded, setExpanded] = useState(0);
  const [createOpen, setCreateOpen] = useState(false);
  const [restoreOpen, setRestoreOpen] = useState(null);
  const [memDump, setMemDump] = useState(true);

  return (
    <div className="space-y-8 animate-fade-in">
      {/* Header */}
      <div className="flex items-end justify-between">
        <div>
          <h1 className="font-['Space_Grotesk'] text-5xl font-bold text-[#e5e1e4] tracking-tight mb-2">Checkpoints</h1>
          <p className="text-[#c2c6d6] max-w-lg">Point-in-time snapshots of container state for rapid recovery and migration testing.</p>
        </div>
        <button onClick={() => setCreateOpen(true)} className="bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-semibold px-6 py-3 rounded-xl flex items-center gap-2 active:scale-95 transition-transform">
          <span className="material-symbols-outlined">add</span>Create Checkpoint
        </button>
      </div>

      {/* Stats bento */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        <div className="bg-[#1c1b1d] p-6 rounded-xl border-t-2 border-[#adc6ff]">
          <p className="text-xs uppercase tracking-widest text-[#8c909f] mb-2">Active States</p>
          <p className="text-4xl font-['Space_Grotesk'] font-bold text-[#adc6ff]">{CHECKPOINTS.length}</p>
        </div>
        <div className="bg-[#1c1b1d] p-6 rounded-xl">
          <p className="text-xs uppercase tracking-widest text-[#8c909f] mb-2">Storage Used</p>
          <p className="text-4xl font-['Space_Grotesk'] font-bold text-[#d0bcff]">4.2 GB</p>
        </div>
        <div className="bg-[#1c1b1d] p-6 rounded-xl md:col-span-2 relative overflow-hidden">
          <div className="relative z-10">
            <p className="text-xs uppercase tracking-widest text-[#8c909f] mb-2">Health Index</p>
            <div className="flex items-baseline gap-3">
              <p className="text-4xl font-['Space_Grotesk'] font-bold text-[#4cd7f6]">99.8%</p>
              <span className="flex items-center gap-1 text-[#4cd7f6] text-sm">
                <span className="w-2 h-2 rounded-full bg-[#4cd7f6] animate-pulse" />Stable
              </span>
            </div>
          </div>
          <div className="absolute right-0 bottom-0 opacity-10"><span className="material-symbols-outlined text-9xl">query_stats</span></div>
        </div>
      </div>

      {/* Checkpoint table */}
      <div className="bg-[#1c1b1d] rounded-xl overflow-hidden border border-[#424754]/10">
        <table className="w-full text-left border-collapse">
          <thead>
            <tr className="bg-[#353437]/30">
              {['Name','Source','Created','Size','Actions'].map((h, i) => (
                <th key={h} className={`px-6 py-4 text-xs uppercase tracking-widest text-[#8c909f] font-medium ${i === 4 ? 'text-right' : ''}`}>{h}</th>
              ))}
            </tr>
          </thead>
          <tbody className="divide-y divide-[#424754]/10">
            {CHECKPOINTS.map((cp, i) => (
              <>
                <tr key={cp.name} className={`transition-colors ${expanded === i ? 'bg-[#2a2a2c]/40' : 'hover:bg-[#2a2a2c] cursor-pointer'}`} onClick={() => setExpanded(expanded === i ? -1 : i)}>
                  <td className="px-6 py-4">
                    <div className="flex items-center gap-3">
                      <span className={`material-symbols-outlined ${expanded === i ? 'text-[#adc6ff]' : 'text-[#8c909f]'}`}>bookmark</span>
                      <span className="font-medium text-[#e5e1e4]">{cp.name}</span>
                    </div>
                  </td>
                  <td className="px-6 py-4"><span className="font-mono text-sm bg-[#353437] px-2 py-1 rounded text-[#e5e1e4]">{cp.source}</span></td>
                  <td className="px-6 py-4 text-sm text-[#8c909f]">{cp.created}</td>
                  <td className="px-6 py-4 text-sm font-medium text-[#e5e1e4]">{cp.size}</td>
                  <td className="px-6 py-4 text-right">
                    <button onClick={(e) => { e.stopPropagation(); setRestoreOpen(cp); }} className="p-2 hover:bg-[#353437] rounded-lg transition-colors text-[#8c909f] hover:text-white"><span className="material-symbols-outlined">restore</span></button>
                    <button className="p-2 hover:bg-[#353437] rounded-lg transition-colors text-[#8c909f] hover:text-white"><span className="material-symbols-outlined">more_vert</span></button>
                  </td>
                </tr>
                {expanded === i && (
                  <tr key={`${cp.name}-detail`} className="bg-[#2a2a2c]/20">
                    <td className="px-6 py-8" colSpan={5}>
                      <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
                        <div className="space-y-4">
                          <h4 className="text-xs font-['Space_Grotesk'] uppercase tracking-widest text-[#8c909f]">Memory State</h4>
                          <div className="flex items-center gap-4">
                            <div className="w-16 h-16 rounded-full border-4 border-[#424754] flex items-center justify-center">
                              <span className="text-xs font-bold text-[#e5e1e4]">{cp.memState}</span>
                            </div>
                            <div className="space-y-1">
                              <p className="text-sm font-medium text-[#e5e1e4]">Heap Allocated</p>
                              <p className="text-xs text-[#8c909f]">Page tables preserved</p>
                            </div>
                          </div>
                        </div>
                        <div className="space-y-4">
                          <h4 className="text-xs font-['Space_Grotesk'] uppercase tracking-widest text-[#8c909f]">Disk Footprint</h4>
                          <div className="space-y-2">
                            <div className="flex justify-between text-xs">
                              <span className="text-[#e5e1e4]">Delta Change</span>
                              <span className="text-[#d0bcff]">{cp.diskDelta}</span>
                            </div>
                            <div className="h-1 bg-[#353437] rounded-full overflow-hidden">
                              <div className="h-full bg-[#d0bcff] w-1/3" />
                            </div>
                            <p className="text-[10px] text-[#8c909f] italic">Snapshot taken in &apos;Consistent&apos; mode</p>
                          </div>
                        </div>
                        <div className="space-y-4">
                          <h4 className="text-xs font-['Space_Grotesk'] uppercase tracking-widest text-[#8c909f]">Node Affinity</h4>
                          <div className="flex flex-wrap gap-2">
                            {cp.tags.map((t) => <span key={t} className="text-[10px] bg-[#4cd7f6]/10 text-[#4cd7f6] px-2 py-0.5 rounded border border-[#4cd7f6]/20">{t}</span>)}
                          </div>
                        </div>
                      </div>
                    </td>
                  </tr>
                )}
              </>
            ))}
          </tbody>
        </table>
      </div>

      {/* Create Modal */}
      {createOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-8 bg-black/60 backdrop-blur-md">
          <div className="bg-[#1c1b1d] rounded-xl border border-[#424754] shadow-2xl overflow-hidden w-full max-w-md">
            <div className="p-6 border-b border-[#424754]/10">
              <h3 className="text-xl font-['Space_Grotesk'] font-bold text-[#e5e1e4]">Create New Checkpoint</h3>
              <p className="text-sm text-[#8c909f]">Capture the current execution state of a node.</p>
            </div>
            <div className="p-6 space-y-6">
              <div className="space-y-2">
                <label className="text-xs uppercase tracking-widest text-[#8c909f] font-bold">Target Instance</label>
                <select className="w-full bg-[#0e0e10] border border-[#424754] rounded-lg text-sm text-[#e5e1e4] focus:ring-[#adc6ff] focus:border-[#adc6ff] py-3 px-4 outline-none">
                  <option>web-01 (Active)</option><option>db-01 (Idle)</option><option>cache-alpha (Active)</option>
                </select>
              </div>
              <div className="space-y-2">
                <label className="text-xs uppercase tracking-widest text-[#8c909f] font-bold">Checkpoint Label</label>
                <input className="w-full bg-[#0e0e10] border border-[#424754] rounded-lg text-sm text-[#e5e1e4] focus:ring-[#adc6ff] focus:border-[#adc6ff] py-3 px-4 outline-none" placeholder="e.g. final-release-v2" />
              </div>
              <div className="flex items-center gap-3 p-4 bg-[#353437]/30 rounded-lg">
                <div className="flex-1">
                  <p className="text-sm font-medium text-[#e5e1e4]">Memory Dump</p>
                  <p className="text-xs text-[#8c909f]">Include RAM state for instant resumption.</p>
                </div>
                <button onClick={() => setMemDump(!memDump)} className={`w-12 h-6 rounded-full relative transition-colors ${memDump ? 'bg-[#adc6ff]' : 'bg-[#424754]'}`}>
                  <div className={`absolute top-1 w-4 h-4 bg-white rounded-full transition-all ${memDump ? 'right-1' : 'left-1'}`} />
                </button>
              </div>
            </div>
            <div className="p-6 bg-[#2a2a2c]/30 flex justify-end gap-3">
              <button onClick={() => setCreateOpen(false)} className="px-4 py-2 text-sm font-medium text-[#8c909f] hover:text-white transition-colors">Cancel</button>
              <button className="bg-[#adc6ff] text-[#002e6a] font-bold px-6 py-2 rounded-lg text-sm">Initialize Forge</button>
            </div>
          </div>
        </div>
      )}

      {/* Restore Confirm Modal */}
      {restoreOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-8 bg-black/60 backdrop-blur-md">
          <div className="bg-[#1c1b1d] rounded-xl border border-[#ffb4ab]/30 shadow-2xl overflow-hidden w-full max-w-md">
            <div className="p-6 bg-[#93000a]/10 border-b border-[#ffb4ab]/10 flex items-start gap-4">
              <div className="w-12 h-12 rounded-full bg-[#93000a] flex items-center justify-center shrink-0">
                <span className="material-symbols-outlined text-[#ffb4ab]" style={{ fontVariationSettings: "'FILL' 1" }}>warning</span>
              </div>
              <div>
                <h3 className="text-xl font-['Space_Grotesk'] font-bold text-[#ffb4ab]">Confirm Restoration</h3>
                <p className="text-sm text-[#8c909f]">Restoring <span className="text-[#e5e1e4] font-bold">{restoreOpen.name}</span> will overwrite the current state of <span className="text-[#e5e1e4] font-bold">{restoreOpen.source}</span>.</p>
              </div>
            </div>
            <div className="p-6 space-y-4">
              <div className="p-4 bg-[#0e0e10] rounded-lg border border-[#424754]/10">
                <div className="flex justify-between mb-4"><span className="text-xs text-[#8c909f]">Current Uptime</span><span className="text-xs font-mono text-[#e5e1e4]">14d 02h 11m</span></div>
                <div className="flex justify-between"><span className="text-xs text-[#8c909f]">Unsaved Data</span><span className="text-xs text-[#ffb4ab]">Warning: 12.4MB volatile</span></div>
              </div>
              <p className="text-xs text-[#8c909f] leading-relaxed">This action is irreversible. The current live node will be halted, its memory flushed, and replaced with the checkpoint state.</p>
            </div>
            <div className="p-6 bg-[#2a2a2c]/30 flex flex-col gap-3">
              <button className="w-full bg-[#ffb4ab] text-[#690005] font-bold py-3 rounded-lg flex items-center justify-center gap-2">
                <span className="material-symbols-outlined text-sm">settings_backup_restore</span>Confirm State Overwrite
              </button>
              <button onClick={() => setRestoreOpen(null)} className="w-full py-2 text-sm font-medium text-[#8c909f] hover:text-white transition-colors">Abort Restoration</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
