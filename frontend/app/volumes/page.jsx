'use client';

import { useState, useEffect } from 'react';
import { containersApi } from '@/lib/api';

const DEMO_VOLUMES = [
  { container: 'web-01', image: 'nginx:latest', icon: 'public', color: 'text-[#adc6ff]', bg: 'bg-[#adc6ff]/10', hostPath: '/mnt/data/web_static', containerPath: '/usr/share/nginx/html', mode: 'RW', size: '2.4 GB' },
  { container: 'db-01',  image: 'postgres:15-alpine', icon: 'database', color: 'text-[#d0bcff]', bg: 'bg-[#d0bcff]/10', hostPath: '/var/lib/postgresql/data', containerPath: '/var/lib/postgresql/data', mode: 'RW', size: '45.8 GB' },
  { container: 'cache-01', image: 'redis:7-bullseye', icon: 'memory', color: 'text-[#4cd7f6]', bg: 'bg-[#4cd7f6]/10', hostPath: '/etc/redis/redis.conf', containerPath: '/usr/local/etc/redis/redis.conf', mode: 'RO', size: '124 KB' },
];

export default function VolumesPage() {
  const [volumes, setVolumes] = useState(DEMO_VOLUMES);

  useEffect(() => {
    containersApi.list()
      .then((res) => {
        const list = Array.isArray(res) ? res : res?.containers || [];
        if (list.length > 0) {
          setVolumes(
            list.flatMap((c) =>
              (c.mounts || []).map((m) => ({
                container: c.name || c.id?.slice(0, 8),
                image: c.image || '',
                icon: 'database',
                color: 'text-[#adc6ff]',
                bg: 'bg-[#adc6ff]/10',
                hostPath: m.source || m.hostPath || '-',
                containerPath: m.destination || m.containerPath || '-',
                mode: m.mode || 'RW',
                size: m.size || '-',
              }))
            )
          );
        }
      })
      .catch(() => {});
  }, []);


  return (
    <div className="space-y-10 animate-fade-in">
      {/* Header */}
      <div className="flex flex-col md:flex-row md:items-end justify-between gap-6">
        <div className="space-y-2">
          <div className="flex items-center gap-3 mb-1">
            <span className="h-1 w-8 bg-[#adc6ff] rounded-full" />
            <span className="text-xs tracking-widest uppercase text-[#8c909f]">Resource Management</span>
          </div>
          <h1 className="font-['Space_Grotesk'] text-5xl font-bold tracking-tight text-[#e5e1e4]">Volume Mounts</h1>
          <p className="text-[#c2c6d6] max-w-xl">Inspect and manage persistent storage across your active simulation nodes.</p>
        </div>
        <div className="flex items-center gap-3">
          <button className="bg-[#1c1b1d] px-4 py-2.5 rounded-xl border border-[#424754]/20 text-[#c2c6d6] flex items-center gap-2 hover:bg-[#2a2a2c] transition-colors">
            <span className="material-symbols-outlined text-sm">filter_list</span>
            <span className="text-sm font-medium">Filter</span>
          </button>
          <button className="bg-gradient-to-br from-[#adc6ff] to-[#4d8eff] px-6 py-2.5 rounded-xl text-[#002e6a] font-bold flex items-center gap-2 shadow-lg shadow-[#adc6ff]/10 active:scale-95 transition-all">
            <span className="material-symbols-outlined text-lg">add_box</span>
            <span className="text-sm">Create Volume</span>
          </button>
        </div>
      </div>

      {/* Stats */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        {[
          { label: 'Total Capacity', value: '1.2', unit: 'TB', icon: 'database', color: 'border-[#adc6ff]', textColor: 'text-[#adc6ff]' },
          { label: 'Active I/O Operations', value: '2,482', unit: 'ops/sec', icon: 'swap_horiz', color: 'border-[#4cd7f6]', textColor: 'text-[#4cd7f6]' },
          { label: 'Health Checks', value: 'Optimal', unit: '', icon: 'verified_user', color: 'border-[#d0bcff]', textColor: 'text-[#d0bcff]' },
        ].map((s) => (
          <div key={s.label} className={`bg-[#1c1b1d] p-6 rounded-xl ${s.color} border-t-2 relative overflow-hidden group`}>
            <div className="absolute top-0 right-0 p-4 opacity-10 group-hover:opacity-20 transition-opacity">
              <span className="material-symbols-outlined text-6xl">{s.icon}</span>
            </div>
            <p className="text-xs uppercase tracking-widest text-[#8c909f] mb-2">{s.label}</p>
            <div className="flex items-baseline gap-2">
              <span className={`font-['Space_Grotesk'] text-4xl font-bold ${s.textColor}`}>{s.value}</span>
              {s.unit && <span className="text-xl font-['Space_Grotesk'] text-[#c2c6d6]">{s.unit}</span>}
              {s.value === 'Optimal' && <span className="w-3 h-3 rounded-full bg-[#d0bcff] animate-pulse shadow-[0_0_12px_rgba(208,188,255,0.5)]" />}
            </div>
          </div>
        ))}
      </div>

      {/* Table */}
      <div className="bg-[#1c1b1d] rounded-xl overflow-hidden border border-[#424754]/10">
        <div className="overflow-x-auto">
          <table className="w-full text-left border-collapse">
            <thead>
              <tr className="bg-[#2a2a2c]/50">
                {['Container','Host Path','Container Path','Mode','Size','Actions'].map((h) => (
                  <th key={h} className="px-6 py-4 text-xs uppercase tracking-widest text-[#8c909f]">{h}</th>
                ))}
              </tr>
            </thead>
            <tbody className="divide-y divide-[#424754]/5">
              {volumes.map((v, i) => (
                <tr key={i} className="hover:bg-[#201f22]/50 transition-colors group">
                  <td className="px-6 py-5">
                    <div className="flex items-center gap-3">
                      <div className={`h-10 w-10 rounded-lg ${v.bg} flex items-center justify-center`}>
                        <span className={`material-symbols-outlined ${v.color}`}>{v.icon}</span>
                      </div>
                      <div>
                        <div className="font-semibold text-[#e5e1e4]">{v.container}</div>
                        <div className="text-xs text-[#8c909f] font-mono">{v.image}</div>
                      </div>
                    </div>
                  </td>
                  <td className="px-6 py-5">
                    <span className="font-mono text-sm text-[#8c909f] bg-[#0e0e10] px-2 py-1 rounded">{v.hostPath}</span>
                  </td>
                  <td className="px-6 py-5">
                    <span className="font-mono text-sm text-[#8c909f] bg-[#0e0e10] px-2 py-1 rounded">{v.containerPath}</span>
                  </td>
                  <td className="px-6 py-5">
                    <span className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-bold ${v.mode === 'RW' ? 'bg-[#009eb9]/20 text-[#4cd7f6] border border-[#4cd7f6]/20' : 'bg-[#2a2a2c] text-[#8c909f] border border-[#424754]/30'}`}>{v.mode}</span>
                  </td>
                  <td className="px-6 py-5 text-sm font-['Space_Grotesk'] font-bold text-[#e5e1e4]">{v.size}</td>
                  <td className="px-6 py-5">
                    <button className="p-2 text-[#8c909f] hover:text-[#adc6ff] transition-colors">
                      <span className="material-symbols-outlined">settings_ethernet</span>
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        <div className="px-6 py-4 bg-[#0e0e10]/50 flex items-center justify-between border-t border-[#424754]/10">
          <div className="text-xs text-[#8c909f] font-mono">Showing {volumes.length} of {volumes.length} volumes detected</div>
          <div className="flex items-center gap-2">
            <button className="p-1 rounded bg-[#2a2a2c] text-[#8c909f] opacity-50 cursor-not-allowed"><span className="material-symbols-outlined text-sm">chevron_left</span></button>
            <button className="p-1 rounded bg-[#2a2a2c] text-[#8c909f] hover:text-white"><span className="material-symbols-outlined text-sm">chevron_right</span></button>
          </div>
        </div>
      </div>

      {/* Terminal log */}
      <div className="bg-[#0e0e10] rounded-xl p-6 border border-[#adc6ff]/10">
        <div className="flex items-center gap-2 mb-4">
          <span className="h-2 w-2 rounded-full bg-[#adc6ff] animate-pulse" />
          <span className="font-mono text-xs uppercase text-[#adc6ff] tracking-tighter">System Log: Storage_Manager</span>
        </div>
        <div className="font-mono text-sm space-y-1 text-[#8c909f]">
          <p><span className="text-[#424754]">[14:22:01]</span> Volume <span className="text-[#adc6ff]">vol_db_01_primary</span> health check passed. Integrity: 100%.</p>
          <p><span className="text-[#424754]">[14:22:05]</span> Snapshotted <span className="text-[#d0bcff]">web-01-root</span> in 45ms.</p>
          <p><span className="text-[#424754]">[14:23:12]</span> Alert: High IOPS detected on <span className="text-[#4cd7f6]">cache-01</span> ephemeral mount.</p>
        </div>
      </div>

      {/* Glass status HUD */}
      <div className="fixed bottom-8 right-8 bg-[#353437]/60 backdrop-blur-xl p-4 rounded-xl border border-[#424754]/30 shadow-2xl flex items-center gap-4 max-w-sm z-50">
        <div className="h-10 w-10 shrink-0 rounded-full bg-[#adc6ff] flex items-center justify-center text-[#002e6a]">
          <span className="material-symbols-outlined">analytics</span>
        </div>
        <div>
          <p className="text-xs font-bold text-[#adc6ff] uppercase">Engine Status</p>
          <p className="text-xs text-[#c2c6d6]">Storage performance is currently at 98% efficiency with zero latency spikes.</p>
        </div>
      </div>
    </div>
  );
}
