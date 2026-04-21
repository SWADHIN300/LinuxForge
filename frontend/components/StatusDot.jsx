'use client';

const STATUS_MAP = {
  running: { color: 'bg-[#4cd7f6]',  glow: 'shadow-[0_0_8px_#4cd7f6]',  pulse: true  },
  healthy: { color: 'bg-[#4cd7f6]',  glow: 'shadow-[0_0_8px_#4cd7f6]',  pulse: false },
  stopped: { color: 'bg-[#424754]',  glow: '',                            pulse: false },
  paused:  { color: 'bg-[#d0bcff]',  glow: 'shadow-[0_0_8px_#d0bcff]',  pulse: false },
  error:   { color: 'bg-[#ffb4ab]',  glow: 'shadow-[0_0_8px_#ffb4ab]',  pulse: true  },
  exited:  { color: 'bg-[#8c909f]',  glow: '',                            pulse: false },
};

export default function StatusDot({ status = 'stopped', size = 'sm' }) {
  const cfg = STATUS_MAP[status] ?? STATUS_MAP.stopped;
  const sz  = size === 'lg' ? 'h-3 w-3' : 'h-2 w-2';

  return (
    <span
      className={`inline-block rounded-full ${sz} ${cfg.color} ${cfg.glow} ${cfg.pulse ? 'animate-pulse' : ''}`}
      title={status}
    />
  );
}
