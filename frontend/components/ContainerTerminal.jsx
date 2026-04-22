'use client';

import { useEffect, useRef } from 'react';

export default function ContainerTerminal({ containerId, open, onClose }) {
  const termRef = useRef(null);
  const termInstance = useRef(null);
  const wsRef = useRef(null);

  useEffect(() => {
    if (!open || !containerId) return;

    let term, fitAddon, ws;

    const init = async () => {
      const { Terminal } = await import('@xterm/xterm');
      const { FitAddon } = await import('@xterm/addon-fit');
      await import('@xterm/xterm/css/xterm.css');

      term = new Terminal({
        cursorBlink: true,
        fontSize: 13,
        fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
        theme: {
          background: '#0a0e17',
          foreground: '#e2e8f0',
          cursor: '#3366ff',
          cursorAccent: '#0a0e17',
          selectionBackground: '#3366ff44',
          black: '#0a0e17',
          red: '#ef4444',
          green: '#10b981',
          yellow: '#f59e0b',
          blue: '#3366ff',
          magenta: '#8b5cf6',
          cyan: '#06b6d4',
          white: '#e2e8f0',
        },
        allowProposedApi: true,
      });

      fitAddon = new FitAddon();
      term.loadAddon(fitAddon);
      term.open(termRef.current);
      fitAddon.fit();
      termInstance.current = term;

      term.writeln('\x1b[1;36m╔══════════════════════════════════════╗\x1b[0m');
      term.writeln('\x1b[1;36m║\x1b[0m  🐳 Container Terminal                \x1b[1;36m║\x1b[0m');
      term.writeln(`\x1b[1;36m║\x1b[0m  ID: ${containerId.slice(0, 12).padEnd(30)}\x1b[1;36m║\x1b[0m`);
      term.writeln('\x1b[1;36m╚══════════════════════════════════════╝\x1b[0m');
      term.writeln('');

      // Attempt WebSocket connection
      try {
        ws = new WebSocket(`ws://localhost:5000/terminal/${containerId}`);
        wsRef.current = ws;

        ws.onopen = () => {
          term.writeln('\x1b[32mConnected to container shell.\x1b[0m');
          term.writeln('');
        };

        ws.onmessage = (e) => term.write(e.data);

        ws.onerror = () => {
          term.writeln('\x1b[33mWebSocket not available — running in demo mode.\x1b[0m');
          term.writeln('\x1b[90mTo enable live terminal, start the WebSocket server on port 5000.\x1b[0m');
          term.writeln('');
          setupDemo(term);
        };

        ws.onclose = () => {
          term.writeln('');
          term.writeln('\x1b[31mConnection closed.\x1b[0m');
        };

        term.onData((data) => {
          if (ws.readyState === WebSocket.OPEN) ws.send(data);
        });
      } catch {
        term.writeln('\x1b[33mWebSocket not available — running in demo mode.\x1b[0m');
        setupDemo(term);
      }

      const resizeObs = new ResizeObserver(() => fitAddon.fit());
      resizeObs.observe(termRef.current);

      return () => resizeObs.disconnect();
    };

    init();

    return () => {
      if (wsRef.current) wsRef.current.close();
      if (termInstance.current) termInstance.current.dispose();
    };
  }, [open, containerId]);

  const setupDemo = (term) => {
    let line = '';
    term.write('\x1b[1;32mroot@container\x1b[0m:\x1b[1;34m~\x1b[0m# ');
    term.onData((data) => {
      if (data === '\r') {
        term.writeln('');
        if (line.trim() === 'ls') {
          term.writeln('bin  dev  etc  home  lib  proc  root  sys  tmp  usr  var');
        } else if (line.trim() === 'whoami') {
          term.writeln('root');
        } else if (line.trim() === 'hostname') {
          term.writeln(containerId.slice(0, 12));
        } else if (line.trim() === 'exit') {
          term.writeln('logout');
          onClose?.();
          return;
        } else if (line.trim()) {
          term.writeln(`sh: ${line.trim()}: command simulated`);
        }
        line = '';
        term.write('\x1b[1;32mroot@container\x1b[0m:\x1b[1;34m~\x1b[0m# ');
      } else if (data === '\x7f') {
        if (line.length > 0) {
          line = line.slice(0, -1);
          term.write('\b \b');
        }
      } else {
        line += data;
        term.write(data);
      }
    });
  };

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4">
      <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" onClick={onClose} />
      <div className="relative w-full max-w-4xl h-[500px] bg-[#0a0e17] border border-slate-700 rounded-2xl shadow-2xl overflow-hidden animate-slide-up">
        {/* Title bar */}
        <div className="flex items-center justify-between px-4 py-2.5 bg-[#111827] border-b border-slate-700">
          <div className="flex items-center gap-3">
            <div className="flex gap-1.5">
              <div className="w-3 h-3 rounded-full bg-red-500 cursor-pointer hover:brightness-110" onClick={onClose} />
              <div className="w-3 h-3 rounded-full bg-yellow-500" />
              <div className="w-3 h-3 rounded-full bg-green-500" />
            </div>
            <span className="text-xs text-slate-400 font-mono">
              terminal — {containerId?.slice(0, 12)}
            </span>
          </div>
          <button
            onClick={onClose}
            className="text-slate-500 hover:text-slate-300 transition-colors text-sm"
          >
            ✕
          </button>
        </div>
        {/* Terminal */}
        <div ref={termRef} className="w-full h-[calc(100%-40px)]" />
      </div>
    </div>
  );
}
