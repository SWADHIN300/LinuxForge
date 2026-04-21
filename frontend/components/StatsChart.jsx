'use client';

import {
  LineChart, Line, AreaChart, Area,
  XAxis, YAxis, Tooltip, ResponsiveContainer,
  CartesianGrid,
} from 'recharts';

const CustomTooltip = ({ active, payload, label }) => {
  if (!active || !payload?.length) return null;
  return (
    <div className="bg-[#1e293b] border border-slate-600 rounded-lg px-3 py-2 shadow-xl text-xs">
      <div className="text-slate-400 mb-1">{label}</div>
      {payload.map((p, i) => (
        <div key={i} className="flex items-center gap-2">
          <span className="w-2 h-2 rounded-full" style={{ background: p.color }} />
          <span className="text-slate-300">{p.name}: </span>
          <span className="text-white font-medium">{p.value}{typeof p.value === 'number' ? '%' : ''}</span>
        </div>
      ))}
    </div>
  );
};

export function CpuChart({ data = [] }) {
  return (
    <div className="card p-4">
      <h4 className="text-sm font-semibold text-[var(--text-secondary)] mb-3">CPU Usage</h4>
      <ResponsiveContainer width="100%" height={220}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="3 3" stroke="var(--border-color)" />
          <XAxis
            dataKey="time"
            tick={{ fontSize: 11, fill: 'var(--text-muted)' }}
            axisLine={{ stroke: 'var(--border-color)' }}
          />
          <YAxis
            domain={[0, 100]}
            tick={{ fontSize: 11, fill: 'var(--text-muted)' }}
            axisLine={{ stroke: 'var(--border-color)' }}
            tickFormatter={(v) => `${v}%`}
          />
          <Tooltip content={<CustomTooltip />} />
          <Line
            type="monotone"
            dataKey="cpu"
            stroke="#3366ff"
            strokeWidth={2}
            dot={false}
            name="CPU"
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}

export function MemoryChart({ data = [] }) {
  return (
    <div className="card p-4">
      <h4 className="text-sm font-semibold text-[var(--text-secondary)] mb-3">Memory Usage</h4>
      <ResponsiveContainer width="100%" height={220}>
        <AreaChart data={data}>
          <CartesianGrid strokeDasharray="3 3" stroke="var(--border-color)" />
          <XAxis
            dataKey="time"
            tick={{ fontSize: 11, fill: 'var(--text-muted)' }}
            axisLine={{ stroke: 'var(--border-color)' }}
          />
          <YAxis
            tick={{ fontSize: 11, fill: 'var(--text-muted)' }}
            axisLine={{ stroke: 'var(--border-color)' }}
            tickFormatter={(v) => `${v}MB`}
          />
          <Tooltip content={<CustomTooltip />} />
          <defs>
            <linearGradient id="memGrad" x1="0" y1="0" x2="0" y2="1">
              <stop offset="5%" stopColor="#8b5cf6" stopOpacity={0.3} />
              <stop offset="95%" stopColor="#8b5cf6" stopOpacity={0} />
            </linearGradient>
          </defs>
          <Area
            type="monotone"
            dataKey="memory"
            stroke="#8b5cf6"
            strokeWidth={2}
            fill="url(#memGrad)"
            name="Memory (MB)"
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}

export function NetworkChart({ data = [] }) {
  return (
    <div className="card p-4">
      <h4 className="text-sm font-semibold text-[var(--text-secondary)] mb-3">Network I/O</h4>
      <ResponsiveContainer width="100%" height={220}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="3 3" stroke="var(--border-color)" />
          <XAxis
            dataKey="time"
            tick={{ fontSize: 11, fill: 'var(--text-muted)' }}
            axisLine={{ stroke: 'var(--border-color)' }}
          />
          <YAxis
            tick={{ fontSize: 11, fill: 'var(--text-muted)' }}
            axisLine={{ stroke: 'var(--border-color)' }}
            tickFormatter={(v) => `${v}KB`}
          />
          <Tooltip content={<CustomTooltip />} />
          <Line type="monotone" dataKey="rx" stroke="#10b981" strokeWidth={2} dot={false} name="RX" />
          <Line type="monotone" dataKey="tx" stroke="#06b6d4" strokeWidth={2} dot={false} name="TX" />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}

export function Sparkline({ data = [], dataKey = 'cpu', color = '#3366ff', height = 40 }) {
  return (
    <ResponsiveContainer width="100%" height={height}>
      <LineChart data={data}>
        <Line type="monotone" dataKey={dataKey} stroke={color} strokeWidth={1.5} dot={false} />
      </LineChart>
    </ResponsiveContainer>
  );
}
