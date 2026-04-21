'use client';

import { useState } from 'react';

const STACKS = [
  { name: 'myapp-stack', status: 'RUNNING', uptime: '14d 02h', network: '2.4 Gb/s', replicas: '08/08', containers: [{ name: 'nginx-gateway-01', icon: 'settings_input_component', version: 'v1.24.0' }, { name: 'postgres-db-replica', icon: 'database', version: 'v15.3' }] },
  { name: 'backend-stack', status: 'STOPPED', containers: [] },
];

export default function StacksPage() {
  const [deployOpen, setDeployOpen] = useState(false);
  const [stackName, setStackName] = useState('new-microservice-cluster');

  return (
    <div className="space-y-8 animate-fade-in">
      <div className="flex justify-between items-end">
        <div>
          <h1 className="font-['Space_Grotesk'] text-5xl font-bold tracking-tighter text-[#e5e1e4]">Stack Manager</h1>
          <p className="text-[#c2c6d6] mt-2 font-mono text-sm">Orchestrating isolated environments across node-01-eu</p>
        </div>
        <button onClick={() => setDeployOpen(true)} className="bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-semibold px-6 py-3 rounded-xl shadow-lg active:scale-95 transition-all flex items-center gap-2">
          <span className="material-symbols-outlined">add</span>Deploy Stack
        </button>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-8">
        {STACKS.map((s) => (
          <section key={s.name} className={`bg-[#1c1b1d] rounded-xl overflow-hidden transition-all border-t-2 ${s.status === 'RUNNING' ? 'border-[#adc6ff] hover:bg-[#2a2a2c]' : 'border-[#424754]/30 opacity-60 grayscale hover:grayscale-0 hover:opacity-100 duration-300'}`}>
            <div className="p-6">
              <div className="flex justify-between items-start mb-6">
                <div className="flex items-center gap-3">
                  <div className={`w-3 h-3 rounded-full ${s.status === 'RUNNING' ? 'bg-[#4cd7f6]' : 'bg-[#424754]'}`} />
                  <h3 className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4]">{s.name}</h3>
                </div>
                <span className={`font-mono text-xs px-2 py-1 rounded ${s.status === 'RUNNING' ? 'text-[#4cd7f6] bg-[#4cd7f6]/10' : 'text-[#8c909f] bg-[#2a2a2c]'}`}>{s.status}</span>
              </div>

              {s.status === 'RUNNING' ? (
                <>
                  <div className="grid grid-cols-3 gap-4 mb-8">
                    {[['UPTIME', s.uptime, 'text-[#adc6ff]'], ['NETWORK', s.network, 'text-[#4cd7f6]'], ['REPLICAS', s.replicas, 'text-[#d0bcff]']].map(([l, v, c]) => (
                      <div key={l} className="bg-[#0e0e10] p-4 rounded-xl">
                        <p className="text-[10px] uppercase tracking-widest text-[#8c909f] mb-1">{l}</p>
                        <p className={`font-mono text-xl font-bold ${c}`}>{v}</p>
                      </div>
                    ))}
                  </div>
                  <div className="space-y-3 mb-8">
                    <p className="text-xs font-bold text-[#8c909f] uppercase tracking-wider">Active Containers</p>
                    {s.containers.map((c) => (
                      <div key={c.name} className="flex items-center justify-between p-3 bg-[#0e0e10] rounded-lg border border-[#424754]/10">
                        <div className="flex items-center gap-3">
                          <span className="material-symbols-outlined text-[#4cd7f6] text-sm">{c.icon}</span>
                          <span className="font-mono text-sm text-[#e5e1e4]">{c.name}</span>
                        </div>
                        <span className="text-[10px] font-mono text-[#8c909f]">{c.version}</span>
                      </div>
                    ))}
                  </div>
                  <div className="flex gap-3">
                    <button className="flex-1 bg-[#353437] text-[#e5e1e4] py-2 rounded-lg font-medium hover:bg-[#424754] transition-colors flex items-center justify-center gap-2">
                      <span className="material-symbols-outlined text-sm">terminal</span>Logs
                    </button>
                    <button className="flex-1 bg-[#ffb4ab]/10 text-[#ffb4ab] py-2 rounded-lg font-medium hover:bg-[#ffb4ab] hover:text-[#690005] transition-all flex items-center justify-center gap-2">
                      <span className="material-symbols-outlined text-sm">stop_circle</span>Stop
                    </button>
                  </div>
                </>
              ) : (
                <>
                  <div className="flex flex-col items-center justify-center py-12 border-2 border-dashed border-[#424754]/20 rounded-xl mb-6">
                    <span className="material-symbols-outlined text-4xl text-[#424754] mb-2">cloud_off</span>
                    <p className="text-sm text-[#8c909f]">Instance Hibernated</p>
                  </div>
                  <div className="flex gap-3">
                    <button className="flex-1 bg-[#adc6ff] text-[#002e6a] py-2 rounded-lg font-medium hover:brightness-110 transition-all flex items-center justify-center gap-2">
                      <span className="material-symbols-outlined text-sm">play_arrow</span>Start Stack
                    </button>
                    <button className="w-12 bg-[#2a2a2c] text-[#e5e1e4] py-2 rounded-lg font-medium hover:bg-[#424754] transition-colors flex items-center justify-center">
                      <span className="material-symbols-outlined text-sm">delete</span>
                    </button>
                  </div>
                </>
              )}
            </div>
          </section>
        ))}
      </div>

      {deployOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-4">
          <div className="absolute inset-0 bg-[#131315]/80 backdrop-blur-md" onClick={() => setDeployOpen(false)} />
          <div className="relative w-full max-w-2xl bg-[#1c1b1d] rounded-2xl shadow-2xl overflow-hidden border border-[#424754]/30">
            <div className="p-6 border-b border-[#424754]/20 flex justify-between items-center">
              <div>
                <h2 className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4]">Deploy New Stack</h2>
                <p className="text-xs text-[#c2c6d6] font-mono">Definition: docker-compose.yml / json</p>
              </div>
              <button onClick={() => setDeployOpen(false)} className="text-[#8c909f] hover:text-[#e5e1e4]"><span className="material-symbols-outlined">close</span></button>
            </div>
            <div className="p-6">
              <div className="mb-4">
                <label className="block text-xs font-bold text-[#8c909f] uppercase tracking-widest mb-2">Stack Identifier</label>
                <input value={stackName} onChange={(e) => setStackName(e.target.value)} className="w-full bg-[#0e0e10] border border-[#424754] rounded-lg px-4 py-3 font-mono text-sm text-[#adc6ff] focus:outline-none focus:border-[#adc6ff]/50 transition-all" />
              </div>
              <div className="mb-6">
                <label className="block text-xs font-bold text-[#8c909f] uppercase tracking-widest mb-2">Configuration (JSON)</label>
                <div className="bg-[#0e0e10] rounded-xl border border-[#424754] p-4 h-48 overflow-auto font-mono text-sm text-[#acedff] whitespace-pre">{`{\n  "version": "3.8",\n  "services": {\n    "web": { "image": "nginx:latest", "ports": ["80:80"] }\n  }\n}`}</div>
              </div>
              <div className="flex items-center gap-4">
                <button className="flex-1 bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-bold py-3 rounded-xl active:scale-95 transition-all">Initialize Deployment</button>
                <button className="px-6 py-3 border border-[#424754] rounded-xl text-[#8c909f] hover:text-[#e5e1e4] hover:bg-[#2a2a2c] transition-all">Validate</button>
              </div>
            </div>
            <div className="bg-[#353437]/50 px-6 py-3 flex items-center justify-between">
              <div className="flex items-center gap-2">
                <span className="material-symbols-outlined text-sm text-[#4cd7f6]">verified</span>
                <span className="text-[10px] text-[#c2c6d6] font-mono">SYNTAX_OK: 2 Services Identified</span>
              </div>
              <span className="text-[10px] text-[#8c909f] font-mono">TARGET: node-primary-cluster</span>
            </div>
          </div>
        </div>
      )}

      <div className="fixed bottom-6 right-6 z-40 bg-[#2a2a2c]/60 backdrop-blur-xl p-4 rounded-2xl border border-[#424754]/20 shadow-2xl w-64">
        <div className="flex items-center justify-between mb-2">
          <span className="text-[10px] font-bold text-[#8c909f] tracking-tighter">GLOBAL LOAD</span>
          <span className="text-xs font-mono text-[#4cd7f6]">22%</span>
        </div>
        <div className="w-full bg-[#0e0e10] h-1.5 rounded-full overflow-hidden">
          <div className="bg-[#4cd7f6] h-full w-[22%]" />
        </div>
        <div className="mt-4 flex items-center justify-between">
          <div><span className="text-[10px] text-[#8c909f]">IOPS</span><p className="text-sm font-mono font-bold text-[#e5e1e4]">12.4k</p></div>
          <div className="text-right"><span className="text-[10px] text-[#8c909f]">NET</span><p className="text-sm font-mono font-bold text-[#adc6ff]">↑ 12MB/s</p></div>
        </div>
      </div>
    </div>
  );
}
