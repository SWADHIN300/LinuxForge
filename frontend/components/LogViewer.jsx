'use client';

import { useEffect, useRef, useState } from 'react';

export default function LogViewer({ logs = [], follow = true, onSearch }) {
  const containerRef = useRef(null);
  const [filter, setFilter] = useState('');

  useEffect(() => {
    if (follow && containerRef.current) {
      containerRef.current.scrollTop = containerRef.current.scrollHeight;
    }
  }, [logs, follow]);

  const filtered = filter
    ? logs.filter((l) => l.toLowerCase().includes(filter.toLowerCase()))
    : logs;

  const colorize = (line) => {
    if (/error/i.test(line)) return 'text-red-400';
    if (/warn/i.test(line)) return 'text-yellow-400';
    if (/info/i.test(line)) return 'text-cyan-400';
    if (/debug/i.test(line)) return 'text-slate-500';
    return 'text-emerald-400';
  };

  const formatLine = (line) => {
    // Highlight timestamp brackets
    const match = line.match(/^(\[[\d:.\-T ]+\])\s*(.*)/);
    if (match) {
      return (
        <>
          <span className="text-slate-500">{match[1]}</span>{' '}
          <span className={colorize(line)}>{match[2]}</span>
        </>
      );
    }
    return <span className={colorize(line)}>{line}</span>;
  };

  return (
    <div className="terminal-window flex flex-col h-full">
      {/* Search bar */}
      <div className="px-3 py-2 border-b border-slate-700/50 flex items-center gap-2">
        <svg className="w-4 h-4 text-slate-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
        </svg>
        <input
          type="text"
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          placeholder="Filter logs..."
          className="flex-1 bg-transparent text-sm text-slate-300 placeholder-slate-600 outline-none font-mono"
        />
        {filter && (
          <span className="text-xs text-slate-500">{filtered.length} matches</span>
        )}
      </div>

      {/* Log output */}
      <div ref={containerRef} className="terminal-body flex-1 overflow-y-auto select-text">
        {filtered.length === 0 ? (
          <div className="text-slate-600 text-center py-8">
            {logs.length === 0 ? 'No logs available' : 'No matching log entries'}
          </div>
        ) : (
          filtered.map((line, i) => (
            <div key={i} className="font-mono text-[13px] leading-6 hover:bg-white/[0.02]">
              <span className="text-slate-600 mr-3 select-none text-xs">{String(i + 1).padStart(4)}</span>
              {formatLine(line)}
            </div>
          ))
        )}
      </div>
    </div>
  );
}
