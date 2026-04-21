'use client';

import { useState, useEffect, useRef } from 'react';
import { logsApi } from '@/lib/api';

const DEMO_LOGS = [
  { ts: '14:20:01.442', level: 'INFO', msg: 'Successfully initialized NGINX master process (PID: 1)' },
  { ts: '14:20:01.445', level: 'WARN', msg: 'Potential memory leak detected in worker process 14. Monitoring...' },
  { ts: '14:21:05.112', level: 'ERR!', msg: 'upstream timed out (110: Connection timed out) while connecting to upstream, client: 172.18.0.1' },
  { ts: '14:21:08.891', level: 'DONE', msg: 'Health check passed. Node web-01 reports 100% availability.' },
  { ts: '14:22:00.001', level: 'INFO', msg: 'Received SIGUSR1 signal. Reopening log files...' },
  { ts: '14:22:15.556', level: 'INFO', msg: 'GET /api/v4/nodes/status 200 OK — 42ms' },
  { ts: '14:22:16.120', level: 'INFO', msg: 'POST /api/v4/telemetry/heartbeat 204 No Content — 12ms' },
  { ts: '14:22:45.332', level: 'WARN', msg: 'High latency detected on Redis cluster. Response time: 450ms' },
  { ts: '14:23:01.002', level: 'INFO', msg: 'Garbage collection cycle completed. Reclaimed 142MB.' },
  { ts: '14:23:15.774', level: 'INFO', msg: 'Worker process 12 accepted connection from 172.18.0.5' },
  { ts: '14:23:58.001', level: 'ERR!', msg: 'Failed to resolve upstream "backend-svc": no such host' },
  { ts: '14:24:01.110', level: 'DONE', msg: 'Auto-restart triggered for worker 14. New PID: 89.' },
];

const CONTAINERS = [
  { id: 'web-01',    lines: '1.2k', color: 'bg-[#4cd7f6]',  dot: 'bg-[#4cd7f6]' },
  { id: 'db-01',    lines: '842',  color: 'bg-[#adc6ff]',  dot: 'bg-[#adc6ff]' },
  { id: 'cache-01', lines: '45',   color: 'bg-[#d0bcff]', dot: 'bg-[#d0bcff]' },
];

const LEVEL_STYLE = {
  'INFO': 'text-[#acedff]',
  'WARN': 'text-[#d0bcff]',
  'ERR!': 'text-[#ffb4ab]',
  'DONE': 'text-[#4cd7f6]',
};

export default function LogsPage() {
  const [active, setActive]   = useState('web-01');
  const [filter, setFilter]   = useState('');
  const [follow, setFollow]   = useState(true);
  const [logs, setLogs]       = useState(DEMO_LOGS);
  const bottomRef             = useRef(null);

  useEffect(() => {
    // logsApi.stream returns an EventSource (SSE), not a Promise
    let es;
    try {
      es = logsApi.stream?.(active);
      if (es && typeof es.addEventListener === 'function') {
        es.addEventListener('message', (e) => {
          try {
            const line = JSON.parse(e.data);
            setLogs((prev) => [...prev.slice(-500), line]);
          } catch { /* ignore parse errors */ }
        });
        es.onerror = () => es?.close();
      } else if (es && typeof es.then === 'function') {
        // fallback if it IS a promise (older API)
        es.then((d) => { if (Array.isArray(d) && d.length > 0) setLogs(d); }).catch(() => {});
        es = null; // don't close
      }
    } catch { /* stream not available */ }
    return () => es?.close?.();
  }, [active]);

  useEffect(() => {
    if (follow) bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logs, follow]);

  const filtered = logs.filter((l) =>
    !filter || l.msg?.toLowerCase().includes(filter.toLowerCase()) || l.level?.includes(filter.toUpperCase())
  );

  const errCount  = logs.filter((l) => l.level === 'ERR!').length;
  const warnCount = logs.filter((l) => l.level === 'WARN').length;

  return (
    <div className="flex h-[calc(100vh-4rem)] -m-8 overflow-hidden">

      {/* ── Container sidebar ─────────────────────────────────── */}
      <aside className="w-56 shrink-0 bg-[#131315] border-r border-[#424754]/20 flex flex-col">
        <div className="px-5 pt-6 pb-4">
          <div className="text-[10px] font-bold uppercase tracking-[0.2em] text-[#8c909f] mb-3">Active Nodes</div>
          <div className="space-y-1">
            {CONTAINERS.map((c) => (
              <button
                key={c.id}
                onClick={() => setActive(c.id)}
                className={`w-full flex items-center justify-between p-3 rounded-l-lg transition-all text-left ${
                  active === c.id
                    ? 'bg-[#1c1b1d] text-[#adc6ff] border-r-2 border-[#adc6ff] font-semibold'
                    : 'text-zinc-500 hover:bg-[#1c1b1d] hover:text-zinc-200'
                }`}
              >
                <div className="flex items-center gap-2.5">
                  <span className={`h-2 w-2 rounded-full ${c.dot} ${active === c.id ? 'animate-pulse' : ''}`} />
                  <span className="font-mono text-sm">{c.id}</span>
                </div>
                <span className="text-[10px] opacity-50 font-mono">{c.lines}</span>
              </button>
            ))}
          </div>
        </div>
        <div className="mt-auto p-4 border-t border-[#353437] space-y-1">
          <a href="#" className="lf-nav-item text-sm">
            <span className="material-symbols-outlined text-[18px]">menu_book</span>
            Documentation
          </a>
          <a href="#" className="lf-nav-item text-sm">
            <span className="material-symbols-outlined text-[18px]">support_agent</span>
            Support
          </a>
        </div>
      </aside>

      {/* ── Main log area ──────────────────────────────────────── */}
      <main className="flex-1 flex flex-col bg-[#131315] min-w-0">

        {/* Header bar */}
        <div className="h-14 shrink-0 flex items-center justify-between px-6 bg-[#1c1b1d] border-b border-[#424754]/10">
          <div className="flex items-center gap-4">
            <div className="flex items-center gap-2">
              <span className="material-symbols-outlined text-[#adc6ff] text-xl">terminal</span>
              <h1 className="font-['Space_Grotesk'] font-bold text-lg tracking-tight text-[#e5e1e4]">{active}</h1>
            </div>
            <div className="h-4 w-px bg-[#424754]/50" />
            {/* Follow toggle */}
            <button
              onClick={() => setFollow((f) => !f)}
              className="flex items-center gap-2 px-3 py-1 bg-[#0e0e10] rounded-lg border border-[#424754]/20"
            >
              <span className="text-[10px] font-bold text-zinc-500 uppercase tracking-widest">Follow</span>
              <div className={`w-8 h-4 rounded-full relative transition-colors ${follow ? 'bg-[#adc6ff]/20' : 'bg-[#353437]'}`}>
                <div className={`absolute top-0.5 w-3 h-3 rounded-full transition-all ${follow ? 'right-0.5 bg-[#adc6ff]' : 'left-0.5 bg-zinc-600'}`} />
              </div>
            </button>
          </div>

          <div className="flex items-center gap-3">
            <div className="flex items-center gap-2 bg-[#353437]/40 px-3 py-1.5 rounded-lg text-xs font-medium text-[#c2c6d6] cursor-pointer hover:bg-[#353437] transition-colors">
              <span>Last 1 hour</span>
              <span className="material-symbols-outlined text-sm">expand_more</span>
            </div>
            <button
              onClick={() => setLogs([])}
              className="flex items-center gap-2 px-4 py-1.5 text-xs font-bold text-[#ffb4ab] bg-[#ffb4ab]/10 hover:bg-[#ffb4ab]/20 rounded-lg transition-all active:scale-95"
            >
              <span className="material-symbols-outlined text-sm">delete_sweep</span>
              Clear
            </button>
          </div>
        </div>

        {/* Terminal canvas */}
        <div className="flex-1 overflow-y-auto p-6 font-mono text-[13px] leading-relaxed bg-[#0e0e10]">
          <div className="space-y-1">
            {filtered.map((l, i) => (
              <div key={i} className="flex gap-4 group hover:bg-white/5 px-2 -mx-2 rounded transition-colors">
                <span className="text-zinc-700 shrink-0 select-none">{l.ts || l.timestamp}</span>
                <span className={`font-bold shrink-0 ${LEVEL_STYLE[l.level] ?? 'text-zinc-400'}`}>[{l.level}]</span>
                <span className={l.level === 'ERR!' ? 'text-[#ffdad6]' : 'text-zinc-300'}>{l.msg || l.message}</span>
              </div>
            ))}
            {/* Blinking cursor */}
            <div className="flex items-center gap-2 mt-1">
              <span className="text-[#adc6ff] opacity-30 select-none font-mono text-xs">►</span>
              <div className="w-2 h-4 bg-[#adc6ff] animate-pulse opacity-60" />
            </div>
            <div ref={bottomRef} />
          </div>
        </div>

        {/* Footer filter bar */}
        <footer className="h-20 shrink-0 bg-[#201f22] border-t border-[#424754]/10 px-6 flex items-center gap-6">
          <div className="flex-1 relative flex items-center group">
            <span className="material-symbols-outlined absolute left-4 text-zinc-500 group-focus-within:text-[#adc6ff] transition-colors">search</span>
            <input
              value={filter}
              onChange={(e) => setFilter(e.target.value)}
              className="w-full bg-[#0e0e10] border border-[#424754]/20 rounded-xl pl-12 pr-4 py-3 text-sm font-mono focus:ring-1 focus:ring-[#adc6ff]/50 focus:border-[#adc6ff]/50 transition-all outline-none text-[#e5e1e4] placeholder:text-zinc-600"
              placeholder="Filter logs by keyword, ID, or level..."
            />
          </div>
          <div className="flex items-center gap-4 text-zinc-500 text-xs font-mono shrink-0">
            <div className="flex items-center gap-2">
              <span className="h-2 w-2 rounded-full bg-[#4cd7f6]" />
              {filtered.length} Lines
            </div>
            <div className="flex items-center gap-2">
              <span className="h-2 w-2 rounded-full bg-[#ffb4ab]" />
              {errCount} Errors
            </div>
            <div className="flex items-center gap-2">
              <span className="h-2 w-2 rounded-full bg-[#d0bcff]" />
              {warnCount} Warnings
            </div>
          </div>
        </footer>
      </main>
    </div>
  );
}
