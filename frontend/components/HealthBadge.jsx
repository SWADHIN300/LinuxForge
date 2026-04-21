'use client';

const HEALTH_MAP = {
  healthy:   { label: 'Healthy',   dot: 'bg-[#4cd7f6]', text: 'text-[#4cd7f6]', bg: 'bg-[#4cd7f660]/10', border: 'border-[#4cd7f6]/25' },
  running:   { label: 'Running',   dot: 'bg-[#4cd7f6]', text: 'text-[#4cd7f6]', bg: 'bg-[#4cd7f6]/10',   border: 'border-[#4cd7f6]/25' },
  unhealthy: { label: 'Unhealthy', dot: 'bg-[#ffb4ab]', text: 'text-[#ffb4ab]', bg: 'bg-[#ffb4ab]/10',   border: 'border-[#ffb4ab]/25' },
  degraded:  { label: 'Degraded',  dot: 'bg-[#d0bcff]', text: 'text-[#d0bcff]', bg: 'bg-[#d0bcff]/10',   border: 'border-[#d0bcff]/25' },
  stopped:   { label: 'Stopped',   dot: 'bg-[#424754]', text: 'text-[#8c909f]', bg: 'bg-[#424754]/10',   border: 'border-[#424754]/25' },
  unknown:   { label: 'Unknown',   dot: 'bg-[#8c909f]', text: 'text-[#8c909f]', bg: 'bg-[#8c909f]/10',   border: 'border-[#8c909f]/25' },
};

export default function HealthBadge({ status = 'unknown' }) {
  const cfg = HEALTH_MAP[status] ?? HEALTH_MAP.unknown;
  return (
    <span className={`inline-flex items-center gap-1.5 px-2.5 py-0.5 rounded-full text-xs font-semibold border ${cfg.bg} ${cfg.text} ${cfg.border}`}>
      <span className={`h-1.5 w-1.5 rounded-full ${cfg.dot}`} />
      {cfg.label}
    </span>
  );
}
