'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import { registryApi } from '@/lib/api';

function formatCreated(value) {
  if (!value) return 'Unknown';

  const created = new Date(value);
  if (Number.isNaN(created.getTime())) return value;

  const diffMs = Date.now() - created.getTime();
  const diffMins = Math.max(1, Math.floor(diffMs / 60000));

  if (diffMins < 60) return `${diffMins}m ago`;

  const diffHours = Math.floor(diffMins / 60);
  if (diffHours < 24) return `${diffHours}h ago`;

  const diffDays = Math.floor(diffHours / 24);
  if (diffDays < 7) return `${diffDays}d ago`;

  return created.toLocaleDateString();
}

function normalizeImage(image, idx = 0) {
  return {
    id: image.id || `${image.name || 'image'}-${image.tag || idx}`,
    name: image.name || 'unnamed',
    tag: image.tag || 'latest',
    size: image.size || '—',
    layers: Number(image.layers) || 0,
    created: formatCreated(image.created_at),
    createdAt: image.created_at || '',
    runtime: image.runtime || 'linux',
    cmd: image.cmd || '/bin/sh',
    digest: image.digest || '',
    description: image.description || '',
    source: image.committed_from || image.rootfs_path || '',
    expanded: idx === 0,
  };
}

export default function RegistryPage() {
  const [images, setImages] = useState([]);
  const [pushOpen, setPushOpen] = useState(false);
  const [mode, setMode] = useState('upload');
  const [imageName, setImageName] = useState('');
  const [tag, setTag] = useState('latest');
  const [contextDir, setContextDir] = useState('examples/node-app');
  const [buildCommand, setBuildCommand] = useState('');
  const [nodeRuntime, setNodeRuntime] = useState(true);
  const [filter, setFilter] = useState('');
  const [selectedFile, setSelectedFile] = useState(null);
  const [loading, setLoading] = useState(true);
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const fileInputRef = useRef(null);

  const loadImages = async () => {
    setLoading(true);
    setError('');
    try {
      const data = await registryApi.list();
      const list = Array.isArray(data) ? data : data?.images || [];
      setImages(list.map((image, idx) => normalizeImage(image, idx)));
    } catch (err) {
      setError(err.message || 'Failed to load registry images.');
      setImages([]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadImages();
  }, []);

  const filteredImages = useMemo(() => {
    const query = filter.trim().toLowerCase();
    if (!query) return images;
    return images.filter((img) =>
      [img.name, img.tag, img.runtime, img.description, img.cmd]
        .filter(Boolean)
        .some((value) => value.toLowerCase().includes(query))
    );
  }, [filter, images]);

  const totalSize = useMemo(() => {
    return images.reduce((count, image) => {
      const match = String(image.size).match(/([\d.]+)/);
      return count + (match ? Number(match[1]) : 0);
    }, 0);
  }, [images]);

  const repositories = useMemo(() => {
    return new Set(images.map((image) => image.name)).size;
  }, [images]);

  const toggleExpand = (id) => {
    setImages((prev) =>
      prev.map((img) => (img.id === id ? { ...img, expanded: !img.expanded } : img))
    );
  };

  const resetModal = () => {
    setPushOpen(false);
    setMode('upload');
    setImageName('');
    setTag('latest');
    setContextDir('examples/node-app');
    setBuildCommand('');
    setNodeRuntime(true);
    setSelectedFile(null);
    setUploading(false);
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  const handleUpload = async () => {
    const trimmedName = imageName.trim();
    const trimmedTag = tag.trim() || 'latest';

    setError('');
    setSuccess('');

    if (!trimmedName) {
      setError('Image name is required.');
      return;
    }

    if (!selectedFile) {
      setError('Choose a tar.gz archive before uploading.');
      return;
    }

    setUploading(true);
    try {
      await registryApi.upload(selectedFile, `${trimmedName}:${trimmedTag}`);
      await loadImages();
      setSuccess(`Imported ${trimmedName}:${trimmedTag} into the local registry.`);
      resetModal();
    } catch (err) {
      setError(err.message || 'Image upload failed.');
      setUploading(false);
    }
  };

  const handleBuild = async () => {
    const trimmedName = imageName.trim();
    const trimmedTag = tag.trim() || 'latest';
    const trimmedContext = contextDir.trim();

    setError('');
    setSuccess('');

    if (!trimmedName) {
      setError('Image name is required.');
      return;
    }

    if (!trimmedContext) {
      setError('Context directory is required.');
      return;
    }

    setUploading(true);
    try {
      await registryApi.build({
        image: `${trimmedName}:${trimmedTag}`,
        contextDir: trimmedContext,
        cmd: buildCommand.trim(),
        node: nodeRuntime,
      });
      await loadImages();
      setSuccess(`Built ${trimmedName}:${trimmedTag} from ${trimmedContext}.`);
      resetModal();
    } catch (err) {
      setError(err.message || 'Image build failed.');
      setUploading(false);
    }
  };

  return (
    <div className="space-y-8 animate-fade-in">
      <div className="flex items-end justify-between gap-4">
        <div>
          <h1 className="font-['Space_Grotesk'] text-4xl font-bold tracking-tight text-[#e5e1e4] mb-2">Container Registry</h1>
          <p className="text-[#c2c6d6]">Manage and distribute your forged images across the cluster.</p>
        </div>
        <div className="flex items-center gap-4">
          <div className="relative">
            <span className="material-symbols-outlined absolute left-4 top-1/2 -translate-y-1/2 text-[#8c909f]">search</span>
            <input
              value={filter}
              onChange={(e) => setFilter(e.target.value)}
              className="bg-[#201f22] border border-[#424754]/30 rounded-xl pl-12 pr-6 py-3 text-sm focus:outline-none focus:border-[#adc6ff]/50 w-72 transition-all text-[#e5e1e4]"
              placeholder="Filter registry..."
            />
          </div>
          <button onClick={() => setPushOpen(true)} className="bg-[#adc6ff] hover:bg-[#adc6ff]/90 text-[#002e6a] px-6 py-3 rounded-xl font-bold flex items-center gap-2 transition-all shadow-lg shadow-[#adc6ff]/10">
            <span className="material-symbols-outlined">add_box</span>+ Create Image
          </button>
        </div>
      </div>

      {(error || success) && (
        <div className={`rounded-xl border px-5 py-4 text-sm ${error ? 'border-[#ffb4ab]/30 bg-[#93000a]/10 text-[#ffdad6]' : 'border-[#4cd7f6]/20 bg-[#009eb9]/10 text-[#e6f7ff]'}`}>
          {error || success}
        </div>
      )}

      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        {[
          { icon: 'inventory_2', val: `${images.length} images`, label: 'Registry Volume', color: 'border-[#adc6ff]', iconColor: 'text-[#adc6ff]', bg: 'bg-[#adc6ff]/10' },
          { icon: 'database', val: `${repositories} repositories`, label: 'Storage Nodes', color: 'border-[#d0bcff]', iconColor: 'text-[#d0bcff]', bg: 'bg-[#d0bcff]/10' },
          { icon: 'speed', val: `${totalSize.toFixed(1)}+`, label: 'Total Size', suffix: 'units', color: 'border-[#4cd7f6]', iconColor: 'text-[#4cd7f6]', bg: 'bg-[#4cd7f6]/10' },
        ].map((s) => (
          <div key={s.label} className={`bg-[#1c1b1d] p-6 rounded-xl ${s.color} border-t-2 flex items-center gap-5`}>
            <div className={`h-12 w-12 rounded-lg ${s.bg} flex items-center justify-center`}>
              <span className={`material-symbols-outlined ${s.iconColor}`}>{s.icon}</span>
            </div>
            <div>
              <div className="text-3xl font-['Space_Grotesk'] font-bold text-[#e5e1e4]">{s.val}</div>
              <div className="text-xs text-[#c2c6d6] uppercase tracking-widest font-bold">{s.label}{s.suffix ? ` • ${s.suffix}` : ''}</div>
            </div>
          </div>
        ))}
      </div>

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
            {loading && (
              <tr>
                <td colSpan={6} className="px-6 py-10 text-center text-[#8c909f]">Loading registry images...</td>
              </tr>
            )}

            {!loading && filteredImages.length === 0 && (
              <tr>
                <td colSpan={6} className="px-6 py-10 text-center text-[#8c909f]">No registry images found.</td>
              </tr>
            )}

            {!loading && filteredImages.map((img) => (
              <FragmentRow key={img.id} image={img} onToggle={() => toggleExpand(img.id)} />
            ))}
          </tbody>
        </table>
      </div>

      {pushOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-4">
          <div className="absolute inset-0 bg-[#131315]/80 backdrop-blur-md" onClick={resetModal} />
          <div className="relative bg-[#201f22] border border-[#424754]/30 rounded-2xl w-full max-w-xl overflow-hidden shadow-[0_32px_64px_-12px_rgba(173,198,255,0.15)]">
            <div className="p-8">
              <div className="flex items-start justify-between mb-8">
                <div>
                  <h2 className="text-2xl font-bold font-['Space_Grotesk'] mb-1 text-[#e5e1e4]">Create Image</h2>
                  <p className="text-[#c2c6d6] text-sm">Build from a local context folder or import a rootfs tarball.</p>
                </div>
                <button onClick={resetModal} className="text-[#8c909f] hover:text-[#e5e1e4]"><span className="material-symbols-outlined">close</span></button>
              </div>
              <div className="mb-6 flex gap-2 rounded-xl bg-[#131315] p-1">
                {[
                  { id: 'upload', label: 'Import Tarball' },
                  { id: 'build', label: 'Build From Context' },
                ].map((option) => (
                  <button
                    key={option.id}
                    onClick={() => setMode(option.id)}
                    className={`flex-1 rounded-lg px-4 py-2 text-sm font-semibold transition-all ${mode === option.id ? 'bg-[#adc6ff] text-[#002e6a]' : 'text-[#8c909f] hover:text-[#e5e1e4]'}`}
                  >
                    {option.label}
                  </button>
                ))}
              </div>
              <div className="space-y-6">
                <div>
                  <label className="block text-xs font-bold uppercase tracking-widest text-[#adc6ff] mb-2">Image Repository</label>
                  <div className="relative">
                    <span className="material-symbols-outlined absolute left-4 top-1/2 -translate-y-1/2 text-[#8c909f] text-sm">link</span>
                    <input defaultValue="registry.linuxforge.local/" readOnly className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl pl-11 pr-4 py-3 font-mono text-sm text-[#8c909f] outline-none" />
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
                {mode === 'upload' ? (
                  <>
                    <button
                      type="button"
                      onClick={() => fileInputRef.current?.click()}
                      className="w-full p-8 border-2 border-dashed border-[#424754]/50 rounded-2xl flex flex-col items-center justify-center bg-[#1c1b1d]/30 hover:border-[#adc6ff]/50 transition-colors cursor-pointer group"
                    >
                      <div className="h-12 w-12 rounded-full bg-[#adc6ff]/10 flex items-center justify-center mb-4 group-hover:scale-110 transition-transform">
                        <span className="material-symbols-outlined text-[#adc6ff]">cloud_upload</span>
                      </div>
                      <div className="text-sm font-semibold text-[#e5e1e4] mb-1">
                        {selectedFile ? selectedFile.name : 'Choose image tarball'}
                      </div>
                      <div className="text-xs text-[#c2c6d6]">Supported: `.tar.gz` root filesystem archives</div>
                    </button>
                    <input
                      ref={fileInputRef}
                      type="file"
                      accept=".tar,.gz,.tgz,.tar.gz"
                      className="hidden"
                      onChange={(e) => setSelectedFile(e.target.files?.[0] || null)}
                    />
                  </>
                ) : (
                  <div className="space-y-4 rounded-2xl border border-[#424754]/30 bg-[#1c1b1d]/40 p-5">
                    <div>
                      <label className="block text-xs font-bold uppercase tracking-widest text-[#adc6ff] mb-2">Context Directory</label>
                      <input value={contextDir} onChange={(e) => setContextDir(e.target.value)} placeholder="e.g. examples/node-app" className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl px-4 py-3 text-sm text-[#e5e1e4] focus:ring-1 focus:ring-[#adc6ff] focus:border-[#adc6ff] outline-none font-mono" />
                    </div>
                    <div>
                      <label className="block text-xs font-bold uppercase tracking-widest text-[#adc6ff] mb-2">Start Command</label>
                      <input value={buildCommand} onChange={(e) => setBuildCommand(e.target.value)} placeholder="Optional override, e.g. npm start" className="w-full bg-[#0e0e10] border border-[#424754] rounded-xl px-4 py-3 text-sm text-[#e5e1e4] focus:ring-1 focus:ring-[#adc6ff] focus:border-[#adc6ff] outline-none font-mono" />
                    </div>
                    <button
                      type="button"
                      onClick={() => setNodeRuntime((prev) => !prev)}
                      className={`flex items-center justify-between rounded-xl border px-4 py-3 text-sm transition-all ${nodeRuntime ? 'border-[#4cd7f6]/40 bg-[#4cd7f6]/10 text-[#e6f7ff]' : 'border-[#424754] bg-[#0e0e10] text-[#8c909f]'}`}
                    >
                      <span>Use Node runtime hint</span>
                      <span className="font-mono text-xs">{nodeRuntime ? 'ON' : 'OFF'}</span>
                    </button>
                  </div>
                )}
              </div>
              <div className="flex items-center gap-3 mt-8">
                <button
                  onClick={mode === 'upload' ? handleUpload : handleBuild}
                  disabled={uploading}
                  className="flex-1 bg-[#adc6ff] text-[#002e6a] font-bold py-4 rounded-xl shadow-lg shadow-[#adc6ff]/20 hover:brightness-110 active:scale-[0.98] transition-all disabled:opacity-60"
                >
                  {uploading ? (mode === 'upload' ? 'Uploading...' : 'Building...') : (mode === 'upload' ? 'Start Upload' : 'Build Image')}
                </button>
                <button onClick={resetModal} disabled={uploading} className="px-8 py-4 bg-[#353437] font-bold rounded-xl text-[#e5e1e4] hover:bg-[#424754] transition-colors disabled:opacity-50">Cancel</button>
              </div>
            </div>
            <div className="bg-[#0e0e10] p-4 border-t border-[#424754]/20">
              <div className="flex items-center gap-2 mb-2">
                <div className="h-2 w-2 rounded-full bg-[#4cd7f6] animate-pulse" />
                <span className="text-[10px] font-mono text-[#4cd7f6] uppercase tracking-widest">{mode === 'upload' ? 'Ready for Import' : 'Ready for Build'}</span>
              </div>
              <div className="text-[11px] font-mono text-[#8c909f]">
                {mode === 'upload'
                  ? '> mycontainer import archive.tar.gz image:tag'
                  : '> mycontainer image build ./context image:tag --node'}
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

function FragmentRow({ image, onToggle }) {
  return (
    <>
      <tr className="hover:bg-[#1c1b1d] transition-colors cursor-pointer" onClick={onToggle}>
        <td className="px-6 py-4">
          <div className="flex items-center gap-3">
            <span className={`material-symbols-outlined ${image.expanded ? 'text-[#adc6ff]' : 'text-[#8c909f]'}`}>box</span>
            <span className="font-semibold text-[#e5e1e4]">{image.name}</span>
          </div>
        </td>
        <td className="px-6 py-4"><span className="font-mono bg-[#353437] px-2 py-1 rounded text-xs text-[#e5e1e4]">{image.tag}</span></td>
        <td className="px-6 py-4 text-[#c2c6d6]">{image.size}</td>
        <td className="px-6 py-4 text-[#c2c6d6]">{image.layers} layers</td>
        <td className="px-6 py-4 text-[#c2c6d6]">{image.created}</td>
        <td className="px-6 py-4">
          <button className="p-2 hover:bg-[#353437] rounded-lg transition-colors text-[#8c909f]">
            <span className="material-symbols-outlined">{image.expanded ? 'expand_less' : 'expand_more'}</span>
          </button>
        </td>
      </tr>
      {image.expanded && (
        <tr className="bg-[#0e0e10]/50">
          <td className="px-12 py-6" colSpan={6}>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div className="p-4 rounded-lg bg-[#2a2a2c] border-l-2 border-[#adc6ff]">
                <div className="text-[10px] text-[#8c909f] uppercase tracking-widest mb-2">Runtime</div>
                <div className="text-sm text-[#e5e1e4] font-mono">{image.runtime}</div>
              </div>
              <div className="p-4 rounded-lg bg-[#2a2a2c] border-l-2 border-[#4cd7f6]">
                <div className="text-[10px] text-[#8c909f] uppercase tracking-widest mb-2">Command</div>
                <div className="text-sm text-[#e5e1e4] font-mono break-all">{image.cmd}</div>
              </div>
              <div className="p-4 rounded-lg bg-[#2a2a2c] border-l-2 border-[#d0bcff]">
                <div className="text-[10px] text-[#8c909f] uppercase tracking-widest mb-2">Source</div>
                <div className="text-sm text-[#e5e1e4] font-mono break-all">{image.source || 'Direct import'}</div>
              </div>
              <div className="p-4 rounded-lg bg-[#2a2a2c] border-l-2 border-[#ffcf99]">
                <div className="text-[10px] text-[#8c909f] uppercase tracking-widest mb-2">Digest</div>
                <div className="text-sm text-[#e5e1e4] font-mono break-all">{image.digest || 'Not available'}</div>
              </div>
              {image.description && (
                <div className="p-4 rounded-lg bg-[#2a2a2c] border-l-2 border-[#8ce9a0] md:col-span-2">
                  <div className="text-[10px] text-[#8c909f] uppercase tracking-widest mb-2">Description</div>
                  <div className="text-sm text-[#e5e1e4]">{image.description}</div>
                </div>
              )}
            </div>
          </td>
        </tr>
      )}
    </>
  );
}
