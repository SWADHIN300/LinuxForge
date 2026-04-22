'use client';

import { useEffect, useMemo, useState } from 'react';
import { stacksApi } from '@/lib/api';

function timeAgo(value) {
  if (!value) return 'Unknown';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return value;

  const diffMins = Math.max(1, Math.floor((Date.now() - date.getTime()) / 60000));
  if (diffMins < 60) return `${diffMins}m ago`;
  const diffHours = Math.floor(diffMins / 60);
  if (diffHours < 24) return `${diffHours}h ago`;
  const diffDays = Math.floor(diffHours / 24);
  return `${diffDays}d ago`;
}

export default function StacksPage() {
  const [stacks, setStacks] = useState([]);
  const [deployOpen, setDeployOpen] = useState(false);
  const [stackFile, setStackFile] = useState('stacks/test.json');
  const [loading, setLoading] = useState(true);
  const [deploying, setDeploying] = useState(false);
  const [notice, setNotice] = useState('');
  const [error, setError] = useState('');

  const loadStacks = async () => {
    setLoading(true);
    setError('');
    try {
      const list = await stacksApi.list();
      const items = Array.isArray(list) ? list : [];
      const detailed = await Promise.all(items.map(async (stack) => {
        try {
          const status = await stacksApi.get(stack.name);
          return {
            ...stack,
            containers: Array.isArray(status?.containers) ? status.containers : [],
          };
        } catch {
          return {
            ...stack,
            containers: [],
          };
        }
      }));
      setStacks(detailed);
    } catch (err) {
      setError(err.message || 'Failed to load stack state.');
      setStacks([]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadStacks();
  }, []);

  const runningCount = useMemo(() => stacks.filter((stack) => stack.status === 'running').length, [stacks]);

  const handleDeploy = async () => {
    setError('');
    setNotice('');

    if (!stackFile.trim()) {
      setError('Stack file path is required.');
      return;
    }

    setDeploying(true);
    try {
      const result = await stacksApi.up(stackFile.trim());
      await loadStacks();
      setNotice(`Stack ${result.name || stackFile} deployed successfully.`);
      setDeployOpen(false);
    } catch (err) {
      setError(err.message || 'Stack deployment failed.');
    } finally {
      setDeploying(false);
    }
  };

  const handleStop = async (stackName) => {
    setError('');
    setNotice('');
    try {
      await stacksApi.down(stackName);
      await loadStacks();
      setNotice(`Stack ${stackName} stopped.`);
    } catch (err) {
      setError(err.message || `Failed to stop ${stackName}.`);
    }
  };

  return (
    <div className="space-y-8 animate-fade-in">
      <div className="flex justify-between items-end gap-6">
        <div>
          <h1 className="font-['Space_Grotesk'] text-5xl font-bold tracking-tighter text-[#e5e1e4]">Stack Manager</h1>
          <p className="text-[#c2c6d6] mt-2 font-mono text-sm">Deploy multi-container applications from stack definition files.</p>
        </div>
        <button onClick={() => setDeployOpen(true)} className="bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-semibold px-6 py-3 rounded-xl shadow-lg active:scale-95 transition-all flex items-center gap-2">
          <span className="material-symbols-outlined">add</span>Deploy Stack
        </button>
      </div>

      {(error || notice) && (
        <div className={`rounded-xl border px-5 py-4 text-sm ${error ? 'border-[#ffb4ab]/30 bg-[#93000a]/10 text-[#ffdad6]' : 'border-[#4cd7f6]/20 bg-[#009eb9]/10 text-[#e6f7ff]'}`}>
          {error || notice}
        </div>
      )}

      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        {[
          ['Active Stacks', String(runningCount).padStart(2, '0'), 'text-[#adc6ff]'],
          ['Tracked Definitions', String(stacks.length).padStart(2, '0'), 'text-[#4cd7f6]'],
          ['Recommended Demo', 'stacks/test.json', 'text-[#d0bcff]'],
        ].map(([label, value, color]) => (
          <div key={label} className="bg-[#1c1b1d] rounded-xl p-6 border border-[#424754]/20">
            <div className="text-[11px] font-bold uppercase tracking-widest text-[#8c909f]">{label}</div>
            <div className={`mt-2 font-['Space_Grotesk'] text-2xl font-bold ${color}`}>{value}</div>
          </div>
        ))}
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-8">
        {loading && (
          <div className="lg:col-span-2 bg-[#1c1b1d] rounded-xl p-10 text-center text-[#8c909f]">
            Loading stack state...
          </div>
        )}

        {!loading && stacks.length === 0 && (
          <div className="lg:col-span-2 bg-[#1c1b1d] rounded-xl p-10 text-center text-[#8c909f]">
            No stacks found. Deploy one from a stack file to get started.
          </div>
        )}

        {!loading && stacks.map((stack) => {
          const isRunning = stack.status === 'running';
          const containers = Array.isArray(stack.containers) ? stack.containers : [];
          return (
            <section key={stack.name} className={`bg-[#1c1b1d] rounded-xl overflow-hidden transition-all border-t-2 ${isRunning ? 'border-[#adc6ff] hover:bg-[#2a2a2c]' : 'border-[#424754]/30 opacity-80 hover:opacity-100'}`}>
              <div className="p-6">
                <div className="flex justify-between items-start mb-6">
                  <div className="flex items-center gap-3">
                    <div className={`w-3 h-3 rounded-full ${isRunning ? 'bg-[#4cd7f6]' : 'bg-[#424754]'}`} />
                    <h3 className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4]">{stack.name}</h3>
                  </div>
                  <span className={`font-mono text-xs px-2 py-1 rounded ${isRunning ? 'text-[#4cd7f6] bg-[#4cd7f6]/10' : 'text-[#8c909f] bg-[#2a2a2c]'}`}>{String(stack.status || 'unknown').toUpperCase()}</span>
                </div>

                <div className="grid grid-cols-3 gap-4 mb-8">
                  {[
                    ['UPTIME', timeAgo(stack.created_at), 'text-[#adc6ff]'],
                    ['SOURCE', stack.source_file || '—', 'text-[#4cd7f6]'],
                    ['CONTAINERS', `${containers.length}`, 'text-[#d0bcff]'],
                  ].map(([label, value, color]) => (
                    <div key={label} className="bg-[#0e0e10] p-4 rounded-xl">
                      <p className="text-[10px] uppercase tracking-widest text-[#8c909f] mb-1">{label}</p>
                      <p className={`font-mono text-sm font-bold break-all ${color}`}>{value}</p>
                    </div>
                  ))}
                </div>

                <div className="space-y-3 mb-8">
                  <p className="text-xs font-bold text-[#8c909f] uppercase tracking-wider">Active Containers</p>
                  {containers.length === 0 && (
                    <div className="p-3 bg-[#0e0e10] rounded-lg border border-[#424754]/10 text-sm text-[#8c909f]">
                      No container details available for this stack yet.
                    </div>
                  )}
                  {containers.map((container) => (
                    <div key={container.id || container.name} className="flex items-center justify-between p-3 bg-[#0e0e10] rounded-lg border border-[#424754]/10">
                      <div className="flex items-center gap-3">
                        <span className={`material-symbols-outlined text-sm ${container.status === 'running' ? 'text-[#4cd7f6]' : 'text-[#8c909f]'}`}>deployed_code</span>
                        <div>
                          <span className="font-mono text-sm text-[#e5e1e4] block">{container.name}</span>
                          <span className="text-[10px] font-mono text-[#8c909f]">{container.id}</span>
                        </div>
                      </div>
                      <span className={`text-[10px] font-mono ${container.status === 'running' ? 'text-[#4cd7f6]' : 'text-[#8c909f]'}`}>{container.status}</span>
                    </div>
                  ))}
                </div>

                <div className="flex gap-3">
                  <button onClick={() => setStackFile(stack.source_file || stack.name)} className="flex-1 bg-[#353437] text-[#e5e1e4] py-2 rounded-lg font-medium hover:bg-[#424754] transition-colors flex items-center justify-center gap-2">
                    <span className="material-symbols-outlined text-sm">description</span>Use Definition
                  </button>
                  <button onClick={() => handleStop(stack.name)} className="flex-1 bg-[#ffb4ab]/10 text-[#ffb4ab] py-2 rounded-lg font-medium hover:bg-[#ffb4ab] hover:text-[#690005] transition-all flex items-center justify-center gap-2">
                    <span className="material-symbols-outlined text-sm">stop_circle</span>Stop
                  </button>
                </div>
              </div>
            </section>
          );
        })}
      </div>

      {deployOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-4">
          <div className="absolute inset-0 bg-[#131315]/80 backdrop-blur-md" onClick={() => setDeployOpen(false)} />
          <div className="relative w-full max-w-2xl bg-[#1c1b1d] rounded-2xl shadow-2xl overflow-hidden border border-[#424754]/30">
            <div className="p-6 border-b border-[#424754]/20 flex justify-between items-center">
              <div>
                <h2 className="font-['Space_Grotesk'] text-2xl font-bold text-[#e5e1e4]">Deploy New Stack</h2>
                <p className="text-xs text-[#c2c6d6] font-mono">Point this to a JSON stack file such as `stacks/test.json`.</p>
              </div>
              <button onClick={() => setDeployOpen(false)} className="text-[#8c909f] hover:text-[#e5e1e4]"><span className="material-symbols-outlined">close</span></button>
            </div>
            <div className="p-6 space-y-5">
              <div>
                <label className="block text-xs font-bold text-[#8c909f] uppercase tracking-widest mb-2">Stack File Path</label>
                <input value={stackFile} onChange={(e) => setStackFile(e.target.value)} className="w-full bg-[#0e0e10] border border-[#424754] rounded-lg px-4 py-3 font-mono text-sm text-[#adc6ff] focus:outline-none focus:border-[#adc6ff]/50 transition-all" />
              </div>
              <div className="bg-[#0e0e10] rounded-xl border border-[#424754] p-4 h-48 overflow-auto font-mono text-sm text-[#acedff] whitespace-pre-wrap">{`{\n  "name": "test-stack",\n  "containers": [\n    { "name": "web", "image": "alpine:3.18" },\n    { "name": "db", "image": "alpine:3.18" }\n  ],\n  "network": { "connect_all": true }\n}`}</div>
              <div className="flex items-center gap-4">
                <button onClick={handleDeploy} disabled={deploying} className="flex-1 bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-bold py-3 rounded-xl active:scale-95 transition-all disabled:opacity-60">
                  {deploying ? 'Deploying...' : 'Initialize Deployment'}
                </button>
                <button onClick={() => setStackFile('stacks/test.json')} className="px-6 py-3 border border-[#424754] rounded-xl text-[#8c909f] hover:text-[#e5e1e4] hover:bg-[#2a2a2c] transition-all">Use Example</button>
              </div>
            </div>
            <div className="bg-[#353437]/50 px-6 py-3 flex items-center justify-between">
              <div className="flex items-center gap-2">
                <span className="material-symbols-outlined text-sm text-[#4cd7f6]">verified</span>
                <span className="text-[10px] text-[#c2c6d6] font-mono">BACKEND ROUTE: /api/stacks/up</span>
              </div>
              <span className="text-[10px] text-[#8c909f] font-mono">TARGET: local simulator engine</span>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
