'use client';

/**
 * StatCard — Stitch design system style
 * accent: 'primary' | 'tertiary' | 'secondary' | 'error' | 'none'
 */
export default function StatCard({ label, value, sub, icon, accent = 'none', pulse = false }) {
  const borderMap = {
    primary:   'border-t-[#adc6ff]',
    tertiary:  'border-t-[#4cd7f6]',
    secondary: 'border-t-[#d0bcff]',
    error:     'border-t-[#ffb4ab]',
    none:      'border-t-[#424754]',
  };

  const valueColorMap = {
    primary:   'text-[#adc6ff]',
    tertiary:  'text-[#4cd7f6]',
    secondary: 'text-[#d0bcff]',
    error:     'text-[#ffb4ab]',
    none:      'text-[#e5e1e4]',
  };

  const subColorMap = {
    primary:   'text-[#adc6ff]',
    tertiary:  'text-[#4cd7f6]',
    secondary: 'text-[#d0bcff]',
    error:     'text-[#ffb4ab]',
    none:      'text-[#8c909f]',
  };

  return (
    <div
      className={`bg-[#1c1b1d] rounded-xl p-6 border-t-2 ${borderMap[accent] ?? borderMap.none} relative overflow-hidden transition-all hover:bg-[#201f22]`}
    >
      {/* Label row */}
      <div className="flex justify-between items-start mb-4">
        <span className="text-[11px] font-bold font-['Inter'] uppercase tracking-widest text-[#8c909f]">
          {label}
        </span>
        {icon && (
          accent === 'tertiary' && pulse ? (
            <span className="h-2 w-2 rounded-full bg-[#4cd7f6] animate-pulse shadow-[0_0_10px_#4cd7f6]" />
          ) : (
            <span className="material-symbols-outlined text-[20px] text-zinc-600">{icon}</span>
          )
        )}
      </div>

      {/* Value */}
      <div className={`font-['Space_Grotesk'] text-5xl font-bold leading-none ${valueColorMap[accent] ?? valueColorMap.none}`}>
        {value}
      </div>

      {/* Sub */}
      {sub && (
        <div className={`mt-2 flex items-center gap-1 text-xs font-mono ${subColorMap[accent] ?? subColorMap.none}`}>
          {sub}
        </div>
      )}
    </div>
  );
}
