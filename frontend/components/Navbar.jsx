'use client';

import { useState, useEffect } from 'react';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import NotificationBell from './NotificationBell';

/* ─── Nav items ─────────────────────────────────────────────────── */
const NAV_ITEMS = [
  { href: '/',            icon: 'dashboard',   label: 'Dashboard' },
  { href: '/logs',        icon: 'terminal',    label: 'Logs Viewer' },
  { href: '/stats',       icon: 'monitoring',  label: 'Performance' },
  { href: '/health',      icon: 'favorite',    label: 'Health Monitor' },
  { href: '/volumes',     icon: 'storage',     label: 'Volume Mounts' },
  { href: '/stacks',      icon: 'view_agenda', label: 'Stack Manager' },
  { href: '/network',     icon: 'hub',         label: 'Network Topology' },
  { href: '/registry',    icon: 'inventory_2', label: 'Container Registry' },
  { href: '/security',    icon: 'security',    label: 'Security Profiles' },
  { href: '/dns',         icon: 'dns',         label: 'Container DNS' },
  { href: '/checkpoints', icon: 'history',     label: 'Checkpoints' },
  { href: '/commit',      icon: 'commit',      label: 'Container Commit' },
];

const EXPANDED_W  = 256; // px
const COLLAPSED_W = 64;  // px

/* ─── Single nav link — hides label when collapsed ───────────────── */
function SideNavLink({ href, icon, label, active, collapsed }) {
  return (
    <Link
      href={href}
      title={collapsed ? label : undefined}
      className={`lf-nav-item${active ? ' active' : ''} ${collapsed ? 'justify-center px-0' : ''}`}
      style={{ transition: 'padding 0.25s ease' }}
    >
      <span
        className="material-symbols-outlined text-[20px] shrink-0"
        style={{ fontVariationSettings: active ? "'FILL' 1" : "'FILL' 0" }}
      >
        {icon}
      </span>
      {/* Label fades out as sidebar collapses */}
      <span
        className="font-['Inter'] font-medium text-sm whitespace-nowrap overflow-hidden"
        style={{
          maxWidth: collapsed ? '0px' : '180px',
          opacity: collapsed ? 0 : 1,
          transition: 'max-width 0.25s ease, opacity 0.2s ease',
          pointerEvents: collapsed ? 'none' : 'auto',
        }}
      >
        {label}
      </span>
    </Link>
  );
}

/* ─── Main shell ────────────────────────────────────────────────── */
export default function Sidebar() {
  const pathname = usePathname();
  const [mobileOpen, setMobileOpen] = useState(false);
  const [collapsed, setCollapsed] = useState(false);

  /* Persist + sync CSS variable so lf-main shifts accordingly */
  useEffect(() => {
    const saved = localStorage.getItem('lf-sidebar-collapsed');
    if (saved === 'true') setCollapsed(true);
  }, []);

  useEffect(() => {
    const w = collapsed ? COLLAPSED_W : EXPANDED_W;
    document.documentElement.style.setProperty('--sidebar-w', `${w}px`);
    localStorage.setItem('lf-sidebar-collapsed', String(collapsed));
  }, [collapsed]);

  const toggle = () => setCollapsed((c) => !c);

  return (
    <>
      {/* ── Fixed Top Header ──────────────────────────────────────── */}
      <header className="lf-header">
        <div className="flex items-center gap-6">
          {/* Mobile hamburger */}
          <button
            className="md:hidden p-1 text-[#8c909f] hover:text-[#adc6ff] transition-colors"
            onClick={() => setMobileOpen((o) => !o)}
            aria-label="Toggle menu"
          >
            <span className="material-symbols-outlined">
              {mobileOpen ? 'close' : 'menu'}
            </span>
          </button>

          {/* Logo */}
          <Link href="/" className="flex items-center gap-2 group">
            <span className="font-['Space_Grotesk'] text-xl font-bold tracking-tight text-[#adc6ff] uppercase">
              LinuxForge
            </span>
          </Link>

          {/* Top quick links (desktop) */}
          <nav className="hidden lg:flex items-center gap-1">
            {['/', '/stats', '/network'].map((href) => {
              const item = NAV_ITEMS.find((n) => n.href === href);
              if (!item) return null;
              const active = pathname === href;
              return (
                <Link
                  key={href}
                  href={href}
                  className={`text-sm px-3 py-5 transition-all border-b-2 ${
                    active
                      ? 'text-[#adc6ff] border-[#adc6ff]'
                      : 'text-zinc-400 border-transparent hover:text-white hover:bg-white/5'
                  }`}
                >
                  {item.label.split(' ')[0]}
                </Link>
              );
            })}
          </nav>
        </div>

        {/* Right: bell + version + avatar */}
        <div className="flex items-center gap-3">
          <NotificationBell />
          <span className="hidden sm:block font-mono text-[10px] text-zinc-600 tracking-widest">
            v4.2.0
          </span>
          <div className="h-8 w-8 rounded-full bg-[#2a2a2c] border border-[#424754] flex items-center justify-center">
            <span className="material-symbols-outlined text-[18px] text-[#adc6ff]">person</span>
          </div>
        </div>
      </header>

      {/* ── Fixed Left Sidebar (desktop) ──────────────────────────── */}
      <aside
        className="lf-sidebar hidden md:flex overflow-hidden"
        style={{
          width: collapsed ? `${COLLAPSED_W}px` : `${EXPANDED_W}px`,
          transition: 'width 0.25s cubic-bezier(0.4,0,0.2,1)',
        }}
      >
        {/* Engine label + collapse toggle */}
        <div
          className={`flex items-center py-4 mb-2 ${collapsed ? 'justify-center px-0' : 'justify-between px-2'}`}
          style={{ transition: 'padding 0.25s ease' }}
        >
          <div
            className="overflow-hidden"
            style={{
              maxWidth: collapsed ? '0px' : '160px',
              opacity: collapsed ? 0 : 1,
              transition: 'max-width 0.25s ease, opacity 0.2s ease',
            }}
          >
            <h2 className="font-['Space_Grotesk'] font-bold text-[#adc6ff] text-base leading-tight whitespace-nowrap">
              Sim Engine
            </h2>
            <p className="text-zinc-600 text-[11px] font-mono mt-0.5">v4.2.0-stable</p>
          </div>

          {/* Toggle button */}
          <button
            onClick={toggle}
            title={collapsed ? 'Expand sidebar' : 'Collapse sidebar'}
            className="h-7 w-7 rounded-lg bg-[#2a2a2c] hover:bg-[#353437] border border-[#424754]/30 flex items-center justify-center text-[#8c909f] hover:text-[#adc6ff] transition-all active:scale-90 shrink-0"
          >
            <span
              className="material-symbols-outlined text-[16px]"
              style={{
                transform: collapsed ? 'rotate(0deg)' : 'rotate(180deg)',
                transition: 'transform 0.25s ease',
                display: 'block',
              }}
            >
              chevron_right
            </span>
          </button>
        </div>

        {/* Nav links */}
        <nav className="flex-1 space-y-0.5">
          {NAV_ITEMS.map((item) => (
            <SideNavLink
              key={item.href}
              href={item.href}
              icon={item.icon}
              label={item.label}
              active={pathname === item.href}
              collapsed={collapsed}
            />
          ))}
        </nav>

        {/* New Instance CTA */}
        <button
          className="mt-4 w-full bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-semibold rounded-xl text-sm flex items-center justify-center gap-2 active:scale-95 transition-all hover:shadow-[0_0_20px_rgba(173,198,255,0.25)] overflow-hidden"
          style={{
            padding: collapsed ? '0.75rem 0' : '0.75rem 1rem',
            transition: 'padding 0.25s ease',
          }}
          title={collapsed ? 'New Instance' : undefined}
        >
          <span className="material-symbols-outlined text-[18px] shrink-0">add</span>
          <span
            className="whitespace-nowrap overflow-hidden"
            style={{
              maxWidth: collapsed ? '0px' : '120px',
              opacity: collapsed ? 0 : 1,
              transition: 'max-width 0.25s ease, opacity 0.2s ease',
            }}
          >
            New Instance
          </span>
        </button>

        {/* Bottom links */}
        <div className="mt-4 pt-4 border-t border-[#353437] space-y-0.5">
          {[
            { icon: 'menu_book', label: 'Documentation' },
            { icon: 'support_agent', label: 'Support' },
          ].map(({ icon, label }) => (
            <a
              key={label}
              href="#"
              title={collapsed ? label : undefined}
              className={`lf-nav-item ${collapsed ? 'justify-center px-0' : ''}`}
              style={{ transition: 'padding 0.25s ease' }}
            >
              <span className="material-symbols-outlined text-[20px] shrink-0">{icon}</span>
              <span
                className="font-['Inter'] font-medium text-sm whitespace-nowrap overflow-hidden"
                style={{
                  maxWidth: collapsed ? '0px' : '180px',
                  opacity: collapsed ? 0 : 1,
                  transition: 'max-width 0.25s ease, opacity 0.2s ease',
                }}
              >
                {label}
              </span>
            </a>
          ))}
        </div>
      </aside>

      {/* ── Mobile Drawer ─────────────────────────────────────────── */}
      {mobileOpen && (
        <>
          <div
            className="fixed inset-0 bg-black/60 z-30 md:hidden"
            onClick={() => setMobileOpen(false)}
          />
          <aside className="fixed left-0 top-16 bottom-0 w-64 z-40 bg-[#131315] border-r border-[#353437] flex flex-col p-4 gap-1 overflow-y-auto md:hidden animate-slide-down">
            <div className="px-2 py-3 mb-2">
              <h2 className="font-['Space_Grotesk'] font-bold text-[#adc6ff] text-base">Sim Engine</h2>
              <p className="text-zinc-600 text-[11px] font-mono mt-0.5">v4.2.0-stable</p>
            </div>
            <nav className="flex-1 space-y-0.5">
              {NAV_ITEMS.map((item) => (
                <div key={item.href} onClick={() => setMobileOpen(false)}>
                  <SideNavLink
                    href={item.href}
                    icon={item.icon}
                    label={item.label}
                    active={pathname === item.href}
                    collapsed={false}
                  />
                </div>
              ))}
            </nav>
            <button className="mt-4 w-full px-4 py-3 bg-gradient-to-r from-[#adc6ff] to-[#4d8eff] text-[#002e6a] font-semibold rounded-xl text-sm flex items-center justify-center gap-2 active:scale-95 transition-all">
              <span className="material-symbols-outlined text-[18px]">add</span>
              New Instance
            </button>
          </aside>
        </>
      )}
    </>
  );
}
