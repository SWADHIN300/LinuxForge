'use client';

import StatusDot from './StatusDot';

export default function StackCard({ stack, onViewLogs, onStop }) {
  const name = stack.name || 'unnamed-stack';
  const status = stack.status || 'running';
  const containers = stack.containers || [];
  const uptime = stack.uptime || 'N/A';

  return (
    <div className="card p-5 flex flex-col gap-3 animate-fade-in">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <span className="text-xl">📦</span>
          <span className="font-semibold text-[var(--text-primary)]">{name}</span>
        </div>
        <span className={`badge ${status === 'running' ? 'badge-green' : 'badge-red'}`}>
          <StatusDot status={status} size="xs" />
          {status}
        </span>
      </div>

      {/* Containers */}
      <div className="space-y-1.5">
        <div className="text-xs font-medium text-[var(--text-muted)] uppercase tracking-wider">
          Containers
        </div>
        {containers.length > 0 ? (
          containers.map((c, i) => (
            <div
              key={i}
              className="flex items-center justify-between text-sm py-1 px-2 rounded-lg hover:bg-[var(--bg-secondary)] transition-colors"
            >
              <div className="flex items-center gap-2">
                <StatusDot status={c.status || 'running'} size="xs" />
                <span className="text-[var(--text-primary)] font-medium">{c.name || c.id}</span>
              </div>
              <div className="flex items-center gap-3 text-xs text-[var(--text-muted)]">
                <span className="font-mono">{c.ip || '—'}</span>
                {c.cpu != null && <span>CPU:{c.cpu}%</span>}
              </div>
            </div>
          ))
        ) : (
          <div className="text-sm text-[var(--text-muted)]">No containers</div>
        )}
      </div>

      {/* Metadata */}
      <div className="flex items-center gap-4 text-xs text-[var(--text-muted)] border-t border-[var(--border-color)] pt-3">
        <span>⏱ Uptime: {uptime}</span>
      </div>

      {/* Actions */}
      <div className="flex gap-2 pt-1">
        <button
          onClick={() => onViewLogs?.(name)}
          className="btn-secondary text-xs py-1.5 px-3"
        >
          📋 View Logs
        </button>
        <button
          onClick={() => onStop?.(name)}
          className="btn-danger text-xs py-1.5 px-3"
        >
          ⏹ Stop Stack
        </button>
      </div>
    </div>
  );
}
