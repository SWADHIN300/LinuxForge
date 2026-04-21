'use client';

import { useState } from 'react';

const IMAGES = [
  { name: 'linuxforge-core-kernel', tag: 'v4.2-alpha', size: '12.4 MB', layers: 5, created: '2h ago', expanded: true,
    layerList: [{ cmd: 'FROM alpine:3.18', type: 'Base Image', size: '3.2 MB', border: 'border-[#4cd7f6]', textColor: 'text-[#4cd7f6]' }, { cmd: 'RUN apk add --no-cache bash', type: 'Installation', size: '1.1 MB', border: 'border-[#424754]', textColor: 'text-[#c2c6d6]' }, { cmd: 'COPY . /app', type: 'Application Source', size: '8.1 MB', border: 'border-[#d0bcff]', textColor: 'text-[#d0bcff]' }] },
  { name: 'monitoring-exporter', tag: 'latest', size: '45.0 MB', layers: 3, created: 'Yesterday', expanded: false, layerList: [] },
  { name: 'auth-gateway', tag: 'v1.1.0', size: '8.7 MB', layers: 4, created: '3 days ago', expanded: false, layerList: [] },
];

export default function RegistryPage() {
  const [images, setImages] = useState(IMAGES);
  const [pushOpen, setPushOpen] = useState(false);
  const [imageName, setImageName] = useState('');
  const [tag, setTag] = useState('latest');

  const toggleExpand = (i) => setImages((prev) => prev.map((img, idx) => ({ ...img, expanded: idx === i ? !img.expanded : img.expanded })));

  return (
    <div className="space-y-8 animate-fade-in">
      {/* Header */}
      <div className="flex items-end justify-between">
        <div>
          <h1 className="font-['Space_Grotesk'] text-4xl font-bold tracking-tight text-[#e5e1e4] mb-2">Container Registry</h1>
          <p className="text-[#c2c6d6]">Manage and distribute your forged images across the cluster.</p>
        </div>
        <div className="flex items-center gap-4">
          <div className="relative">
            <span className="material-symbols-outlined absolute left-4 top-1/2 -translate-y-1/2 text-[#8c909f]">search</span>
            <input className="bg-[#201f22] border border-[#424754]/30 rounded-xl pl-12 pr-6 py-3 text-sm focus:outline-none focus:border-[#adc6ff]/50 w-72 transition-all text-[#e5e1e4]" placeholder="Filter registry..." />
          </div>
          <button onClick={() => setPushOpen(true)} className="bg-[#adc6ff] hover:bg-[#adc6ff]/90 text-[#002e6a] px-6 py-3 rounded-xl font-bold flex items-center gap-2 transition-all shadow-lg shadow-[#adc6ff]/10">
            <span className="material-symbols-outlined">upload</span>+ Push Image
          </button>
        </div>
      </div>

      {/* Stats */}
      <div className="grid grid-cols-3 gap-6">
        {[
          { icon: 'inventory_2', val: `${images.length} images`, label: 'Registry Volume', color: 'border-[#adc6ff]', iconColor: 'text-[#adc6ff]', bg: 'bg-[#adc6ff]/10' },
          { icon: 'database', val: '2 repositories', label: 'Storage Nodes', color: 'border-[#d0bcff]', iconColor: 'text-[#d0bcff]', bg: 'bg-[#d0bcff]/10' },
          { icon: 'speed', val: '70MB', label: 'Total Size', color: 'border-[#4cd7f6]', iconColor: 'text-[#4cd7f6]', bg: 'bg-[#4cd7f6]/10' },
        ].map((s) => (
          <div key={s.label} className={`bg-[#1c1b1d] p-6 rounded-xl ${s.color} border-t-2 flex items-center gap-5`}>
            <div className={`h-12 w-12 rounded-lg ${s.bg} flex items-center justify-center`}>
              <span className={`material-symbols-outlined ${s.iconColor}`}>{s.icon}</span>
            </div>
            <div>
              <div className="text-3xl font-['Space_Grotesk'] font-bold text-[#e5e1e4]">{s.val}</div>
              <div className="text-xs text-[#c2c6d6] uppercase tracking-widest font-bold">{s.label}</div>
            </div>
          </div>
        ))}
      </div>

      {/* Table */}
      <div className="bg-[#201f22] rounded-xl overflow-hidden shadow-2xl">
        <table className="w-full text-left border-collapse">
          <thead>
            <tr className="bg-[#2a2a2c]/50 border-b border-[#424754]/20">
              {['Image Name','Tag','Size','Layers','Created','Actions'].map((h) => (
                <th key={h} className="px-6 py-4 text-xs font-bold uppercase tracking-widest text-[#c2c6d6]">{h}</th>
              ))}
            </tr>
          </thead>
          <tbody className="divide-y divide-[#424754]/10">
            {images.map((img, i) => (
              <>
                <tr key={img.name} className="hover:bg-[#1c1b1d] transition-colors cursor-pointer" onClick={() => toggleExpand(i)}>
                  <td className="px-6 py-4">
                    <div className="flex items-center gap-3">
                      <span className={`material-symbols-outlined ${img.expanded ? 'text-[#adc6ff]' : 'text-[#8c909f]'}`}>box</span>
                      <span className="font-semibold text-[#e5e1e4]">{img.name}</span>
                    </div>
                  </td>
                  <td className="px-6 py-4"><span className="font-mono bg-[#353437] px-2 py-1 rounded text-xs text-[#e5e1e4]">{img.tag}</span></td>
                  <td className="px-6 py-4 text-[#c2c6d6]">{img.size}</td>
                  <td className="px-6 py-4 text-[#c2c6d6]">{img.layers} layers</td>
                  <td className="px-6 py-4 text-[#c2c6d6]">{img.created}</td>
                  <td className="px-6 py-4">
                    <button className="p-2 hover:bg-[#353437] rounded-lg transition-colors text-[#8c909f]">
                      <span className="material-symbols-outlined">{img.expanded ? 'expand_less' : 'expand_more'}</span>
                    </button>
                  </td>
                </tr>
                {img.expanded && img.layerList.length > 0 && (
                  <tr key={`${img.name}-layers`} className="bg-[#0e0e10]/50">
                    <td className="px-12 py-6" colSpan={6}>
                      <div className="grid grid-cols-1 gap-3">
                        <div className="text-xs font-bold text-[#adc6ff]/70 uppercase tracking-widest mb-2 flex items-center gap-2">
                          <span className="material-symbols-outlined text-[14px]">view_list</span>Layer Composition
                        </div>
                        {img.layerList.map((layer) => (
                          <div key={layer.cmd} className={`flex items-center justify-between p-3 rounded-lg bg-[#2a2a2c] border-l-2 ${layer.border}`}>
                            <div className="flex items-center gap-4">
                              <code className="text-xs text-[#c2c6d6] font-mono">{layer.cmd}</code>
                              <span className="text-[10px] text-[#8c909f] uppercase tracking-tighter">{layer.type}</span>
                            </div>
                            <span className={`font-mono text-xs ${layer.textColor}`}>{layer.size}</span>
                          </div>
                        ))}
                      </div>
                    </td>
                  </tr>
                )}
              </>
            ))}
          </tbody>
        </table>
      </div>

      {/* Push Modal */}
      {pushOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-4">
          <div className="absolute inset-0 bg-[#131315]/80 backdrop-blur-md" onClick={() => setPushOpen(false)} />
          <div className="relative bg-[#201f22] border border-[#424754]/30 rounded-2xl w-full max-w-xl overflow-hidden shadow-[0_32px_64px_-12px_rgba(173,198,255,0.15)]">
            <div className="p-8">
              <div className="flex items-start justify-between mb-8">
                <div>
                  <h2 className="text-2xl font-bold font-['Space_Grotesk'] mb-1 text-[#e5e1e4]">Push to Registry</h2>
                  <p className="text-[#c2c6d6] text-sm">Upload a new image manifest to the LinuxForge engine.</p>
                </div>
                <button onClick={() => setPushOpen(false)} className="text-[#8c909f] hover:text-[#e5e1e4]"><span className="material-symbols-outlined">close</span></button>
              </div>
              <div className="space-y-6">
                <div>
                  <label className="block text-xs font-bold uppercase tracking-widest text-[#adc6ff] mb-2">Image Repository</label>
                  <div className="relative">
                    <span className="material-symbols-outlined absolute left-4 top-1/2 -translate-y-1/2 text-[#8c909f] text-sm">link</span>
                    <input defaultValue="registry.linuxforge.io/org-alpha/" className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl pl-11 pr-4 py-3 font-mono text-sm text-[#e5e1e4] focus:ring-1 focus:ring-[#adc6ff] focus:border-[#adc6ff] outline-none transition-all" />
                  </div>
                </div>
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="block text-xs font-bold uppercase tracking-widest text-[#adc6ff] mb-2">Image Name</label>
                    <input value={imageName} onChange={(e) => setImageName(e.target.value)} placeholder="e.g. web-app" className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl px-4 py-3 text-sm text-[#e5e1e4] focus:ring-1 focus:ring-[#adc6ff] focus:border-[#adc6ff] outline-none" />
                  </div>
                  <div>
                    <label className="block text-xs font-bold uppercase tracking-widest text-[#adc6ff] mb-2">Tag</label>
                    <input value={tag} onChange={(e) => setTag(e.target.value)} className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl px-4 py-3 text-sm text-[#e5e1e4] font-mono focus:ring-1 focus:ring-[#adc6ff] focus:border-[#adc6ff] outline-none" />
                  </div>
                </div>
                <div className="p-8 border-2 border-dashed border-[#424754]/50 rounded-2xl flex flex-col items-center justify-center bg-[#1c1b1d]/30 hover:border-[#adc6ff]/50 transition-colors cursor-pointer group">
                  <div className="h-12 w-12 rounded-full bg-[#adc6ff]/10 flex items-center justify-center mb-4 group-hover:scale-110 transition-transform">
                    <span className="material-symbols-outlined text-[#adc6ff]">cloud_upload</span>
                  </div>
                  <div className="text-sm font-semibold text-[#e5e1e4] mb-1">Drop image tarball or .forged file</div>
                  <div className="text-xs text-[#c2c6d6]">Maximum upload size: 2GB</div>
                </div>
              </div>
              <div className="flex items-center gap-3 mt-8">
                <button className="flex-1 bg-[#adc6ff] text-[#002e6a] font-bold py-4 rounded-xl shadow-lg shadow-[#adc6ff]/20 hover:brightness-110 active:scale-[0.98] transition-all">Start Upload</button>
                <button onClick={() => setPushOpen(false)} className="px-8 py-4 bg-[#353437] font-bold rounded-xl text-[#e5e1e4] hover:bg-[#424754] transition-colors">Cancel</button>
              </div>
            </div>
            <div className="bg-[#0e0e10] p-4 border-t border-[#424754]/20">
              <div className="flex items-center gap-2 mb-2">
                <div className="h-2 w-2 rounded-full bg-[#4cd7f6] animate-pulse" />
                <span className="text-[10px] font-mono text-[#4cd7f6] uppercase tracking-widest">Ready for Stream</span>
              </div>
              <div className="text-[11px] font-mono text-[#8c909f]">&gt; linuxforge-cli push --registry-target default --force-tls</div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
