'use client';

import { useState } from 'react';

const PROFILES = [
  { name: 'default', icon: 'verified_user', desc: 'Balanced hardening profile for general purpose workloads. Standard Docker seccomp rules.', syscalls: 312, usage: 84, active: true, badge: { text: 'DEFAULT', cls: 'bg-[#adc6ff] text-[#002e6a]' } },
  { name: 'strict',  icon: 'lock_person',   desc: 'High-security profile. Disables network creation and kernel module loading. Recommended for untrusted code.', syscalls: 456, usage: 12, active: false, badge: null },
  { name: 'none',    icon: 'warning',       desc: 'Unconfined profile. No kernel filters applied. Use only for system-level debugging or performance profiling.', syscalls: 0, usage: 4, active: false, badge: { text: 'UNSAFE', cls: 'bg-[#ffb4ab]/20 text-[#ffb4ab]' } },
];

const BLOCKED_SYSCALLS = ['reboot','mount','unmount','swapon','swapoff','sethostname','setdomainname','kexec_load','init_module'];

const ASSIGNMENTS = [
  { id: '6f8a2c1e4d3b', app: 'auth-service-api',  image: 'forge/auth:v2.1',        profile: 'default', profileColor: 'bg-[#adc6ff]/10 text-[#adc6ff] border-[#adc6ff]/20', status: 'Protected', statusColor: 'bg-[#4cd7f6]' },
  { id: 'a1b2c3d4e5f6', app: 'user-db-primary',   image: 'forge/postgres:14',       profile: 'strict',  profileColor: 'bg-[#d0bcff]/10 text-[#d0bcff] border-[#d0bcff]/20',  status: 'Hardened', statusColor: 'bg-[#4cd7f6]' },
  { id: '9z8y7x6w5v4u', app: 'legacy-export-tool', image: 'custom/exporter:old',    profile: 'none',    profileColor: 'bg-[#ffb4ab]/10 text-[#ffb4ab] border-[#ffb4ab]/20',   status: 'Exposed',  statusColor: 'bg-[#ffb4ab]' },
];

export default function SecurityPage() {
  const [selected, setSelected] = useState(PROFILES[0]);

  return (
    <div className="space-y-8 animate-fade-in">
      {/* Header */}
      <div className="flex flex-col md:flex-row md:items-end justify-between gap-6">
        <div>
          <div className="flex items-center gap-3 mb-2 text-[#4cd7f6]">
            <span className="material-symbols-outlined">security</span>
            <span className="font-mono text-xs tracking-widest uppercase">Kernel Shielding Protocol</span>
          </div>
          <h1 className="font-['Space_Grotesk'] text-5xl font-bold text-[#e5e1e4] tracking-tight">Security Profiles</h1>
          <p className="text-[#c2c6d6] mt-2 max-w-lg">Manage Seccomp and AppArmor profiles to harden container execution boundaries.</p>
        </div>
        <div className="flex items-center gap-4">
          <div className="bg-[#1c1b1d] p-4 rounded-xl border-t-2 border-[#adc6ff]">
            <span className="block text-xs uppercase tracking-tighter text-[#8c909f] font-bold mb-1">Active Nodes</span>
            <span className="text-2xl font-['Space_Grotesk'] font-bold text-[#adc6ff]">12 / 12</span>
          </div>
          <div className="bg-[#1c1b1d] p-4 rounded-xl border-t-2 border-[#4cd7f6]">
            <span className="block text-xs uppercase tracking-tighter text-[#8c909f] font-bold mb-1">Violation Alerts</span>
            <span className="text-2xl font-['Space_Grotesk'] font-bold text-[#4cd7f6]">0</span>
          </div>
        </div>
      </div>

      {/* Main grid */}
      <div className="grid grid-cols-12 gap-6 items-start">
        {/* Profiles list */}
        <section className="col-span-12 lg:col-span-4 flex flex-col gap-4">
          <div className="flex items-center justify-between mb-2">
            <h3 className="text-sm font-bold uppercase tracking-widest text-[#8c909f]">Available Profiles</h3>
            <button className="text-[#adc6ff] text-xs flex items-center gap-1 hover:underline">
              <span className="material-symbols-outlined text-sm">add</span>Create New
            </button>
          </div>
          {PROFILES.map((p) => (
            <div key={p.name} onClick={() => setSelected(p)} className={`p-5 rounded-xl border-t-2 cursor-pointer transition-all hover:translate-x-1 ${selected.name === p.name ? 'bg-[#2a2a2c] border-[#adc6ff]' : 'bg-[#1c1b1d] border-[#424754]/30 hover:bg-[#2a2a2c]'}`}>
              <div className="flex items-start justify-between mb-4">
                <div className={`h-10 w-10 rounded-lg flex items-center justify-center ${selected.name === p.name ? 'bg-[#adc6ff]/10 text-[#adc6ff]' : 'bg-[#2a2a2c] text-[#8c909f]'}`}>
                  <span className="material-symbols-outlined">{p.icon}</span>
                </div>
                {p.badge && <span className={`px-2 py-0.5 rounded text-[10px] font-bold uppercase ${p.badge.cls}`}>{p.badge.text}</span>}
              </div>
              <h4 className="text-lg font-['Space_Grotesk'] font-semibold text-[#e5e1e4]">{p.name}</h4>
              <p className="text-sm text-[#c2c6d6] mt-1">{p.desc}</p>
              <div className="mt-4 flex items-center gap-4 text-xs font-mono text-[#8c909f]">
                <span>Syscalls: {p.syscalls} blocked</span>
                <span>Usage: {p.usage}%</span>
              </div>
            </div>
          ))}
        </section>

        {/* Detail panel */}
        <section className="col-span-12 lg:col-span-8 flex flex-col gap-6">
          {/* Syscall visualization */}
          <div className="bg-[#131315] p-8 rounded-xl border-t-2 border-[#adc6ff] relative overflow-hidden">
            <div className="absolute top-0 right-0 p-8 opacity-5">
              <span className="material-symbols-outlined text-[120px]">filter_list</span>
            </div>
            <div className="relative z-10">
              <div className="flex items-center gap-3 mb-6">
                <span className="text-xs font-bold uppercase tracking-[0.2em] text-[#adc6ff]">Security Analysis</span>
                <div className="h-px flex-grow bg-[#424754]/30" />
              </div>
              <h2 className="text-3xl font-['Space_Grotesk'] font-bold text-[#e5e1e4] mb-2">
                Blocked Syscalls: <span className="text-[#adc6ff]">{selected.name}</span>
              </h2>
              <p className="text-[#c2c6d6] mb-8 max-w-md">Filter definition active on kernel 5.15.0-generic. Preventing unauthorized execution of sensitive operations.</p>
              <div className="flex flex-wrap gap-2">
                {selected.syscalls > 0 ? BLOCKED_SYSCALLS.map((sc) => (
                  <div key={sc} className="px-4 py-2 bg-[#2a2a2c] rounded-lg border border-[#424754]/20 flex items-center gap-2 hover:border-[#adc6ff]/50 transition-colors">
                    <span className="h-2 w-2 rounded-full bg-[#ffb4ab] animate-pulse" />
                    <span className="font-mono text-sm text-[#e5e1e4]">{sc}</span>
                    <span className="material-symbols-outlined text-xs text-[#8c909f]">info</span>
                  </div>
                )) : (
                  <div className="px-4 py-2 bg-[#ffb4ab]/10 rounded-lg border border-[#ffb4ab]/20 flex items-center gap-2">
                    <span className="h-2 w-2 rounded-full bg-[#ffb4ab]" />
                    <span className="font-mono text-sm text-[#ffb4ab]">No syscalls blocked — UNSAFE</span>
                  </div>
                )}
                {selected.syscalls > 9 && <div className="px-4 py-2 bg-[#2a2a2c] rounded-lg border border-[#424754]/20 text-[#8c909f] italic text-xs">+ {selected.syscalls - 9} more...</div>}
              </div>
            </div>
          </div>

          {/* Assignments table */}
          <div className="bg-[#1c1b1d] rounded-xl overflow-hidden shadow-2xl">
            <div className="p-6 border-b border-[#424754]/10 flex items-center justify-between">
              <h3 className="text-sm font-bold uppercase tracking-widest text-[#e5e1e4]">Container Assignments</h3>
              <select className="bg-[#353437] border-none rounded text-xs px-2 py-1 focus:ring-1 focus:ring-[#adc6ff] text-[#e5e1e4]">
                <option>All Profiles</option>
                <option>default</option>
                <option>strict</option>
              </select>
            </div>
            <div className="overflow-x-auto">
              <table className="w-full text-left border-collapse">
                <thead>
                  <tr className="bg-[#0e0e10]/50">
                    {['Container ID','Application','Active Profile','Runtime Status','Actions'].map((h, i) => (
                      <th key={h} className={`px-6 py-4 text-xs font-bold uppercase tracking-tighter text-[#8c909f] ${i === 4 ? 'text-right' : ''}`}>{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody className="divide-y divide-[#424754]/10">
                  {ASSIGNMENTS.map((a) => (
                    <tr key={a.id} className="hover:bg-[#2a2a2c]/50 transition-colors">
                      <td className="px-6 py-4 font-mono text-xs text-[#adc6ff]">{a.id}</td>
                      <td className="px-6 py-4">
                        <div className="flex flex-col">
                          <span className="text-sm font-semibold text-[#e5e1e4]">{a.app}</span>
                          <span className="text-[10px] text-[#8c909f]">image: {a.image}</span>
                        </div>
                      </td>
                      <td className="px-6 py-4">
                        <span className={`text-xs font-semibold px-2 py-1 rounded-lg border ${a.profileColor}`}>{a.profile}</span>
                      </td>
                      <td className="px-6 py-4">
                        <div className="flex items-center gap-2">
                          <span className={`h-2 w-2 rounded-full ${a.statusColor}`} />
                          <span className="text-xs text-[#e5e1e4]">{a.status}</span>
                        </div>
                      </td>
                      <td className="px-6 py-4 text-right">
                        <button className="material-symbols-outlined text-[#8c909f] hover:text-[#adc6ff] transition-colors">edit_square</button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </section>
      </div>
    </div>
  );
}
