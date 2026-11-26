import React, { useState, useMemo, useEffect } from 'react';
import { marked } from 'marked';
import {
  X,
  Code2,
  Play,
  Square,
  Save,
  Trash2,
  Cpu,
  FileText,
  Sliders,
  ArrowLeftRight,
  Copy,
  Share2
} from 'lucide-react';
import omnilynxLogo from '../assets/omnilynx-logo.png';

export default function ModDetailsModal({
  mod,
  lang = 'ar',
  onClose,
  onSaveSettings,
  onEditCode,
  onDelete,
  onSwap,
  onDuplicate,
  onShare,
  onCompileAndInject,
  onEject
}) {
  const [tab, setTab] = useState(mod.parsed.settingsSchema.length > 0 ? 'settings' : 'details');
  const [translatedReadme, setTranslatedReadme] = useState('');
  const [translatedDesc, setTranslatedDesc] = useState('');

  useEffect(() => {
    let active = true;
    if (lang === 'ar') {
      if (mod.parsed.readme) {
        window.wendoAPI?.translateText({ text: mod.parsed.readme, targetLang: 'ar' }).then((res) => {
          if (active && res) setTranslatedReadme(res);
        });
      }
      if (mod.parsed.meta.description) {
        window.wendoAPI?.translateText({ text: mod.parsed.meta.description, targetLang: 'ar' }).then((res) => {
          if (active && res) setTranslatedDesc(res);
        });
      }
    } else {
      setTranslatedReadme('');
      setTranslatedDesc('');
    }
    return () => {
      active = false;
    };
  }, [lang, mod.id, mod.parsed.readme, mod.parsed.meta.description]);
  const [formValues, setFormValues] = useState(() => {
    const initial = { ...(mod.settings || {}) };
    for (const field of mod.parsed.settingsSchema) {
      if (!(field.key in initial)) {
        initial[field.key] = field.defaultValue ?? '';
      }
    }
    return initial;
  });
  const [targetProc, setTargetProc] = useState(mod.parsed.meta.include?.[0] || 'notepad.exe');
  const [statusMsg, setStatusMsg] = useState('');
  const [busy, setBusy] = useState(false);

  const sanitizeLinks = (str = '') =>
    str
      .replace(/https?:\/\/(?:www\.)?windhawk\.net[^\s)]*/gi, 'https://github.com/PyDevMedo/Omnilynx')
      .replace(/https?:\/\/github\.com\/ramensoftware\/windhawk[^\s)]*/gi, 'https://github.com/PyDevMedo/Omnilynx')
      .replace(/https?:\/\/github\.com\/(?!PyDevMedo\/Omnilynx)[^\s)]+(?<!\.(?:png|jpg|jpeg|gif|webp))/gi, 'https://github.com/PyDevMedo/Omnilynx')
      .replace(/windhawk/gi, 'Omnilynx');

  const readmeHtml = useMemo(() => {
    const textToRender =
      lang === 'ar' && translatedReadme
        ? translatedReadme
        : mod.parsed.readme || '*No README provided for this mod.*';
    return marked.parse(sanitizeLinks(textToRender));
  }, [mod.parsed.readme, lang, translatedReadme]);

  const handleSave = async () => {
    setBusy(true);
    const res = await onSaveSettings(mod.id, formValues, mod);
    setStatusMsg(res?.message || 'Settings saved & applied!');
    setBusy(false);
  };

  const handleInject = async () => {
    setBusy(true);
    setStatusMsg('Compiling & injecting...');
    const res = await onCompileAndInject(mod, targetProc);
    setStatusMsg(res.message);
    setBusy(false);
  };

  const handleEject = async () => {
    setBusy(true);
    const res = await onEject(mod, targetProc);
    setStatusMsg(res.message);
    setBusy(false);
  };

  const renderFieldControl = (field) => {
    const val = formValues[field.key];

    if (field.type === 'boolean') {
      return (
        <button
          className={`switch ${val ? 'on' : ''}`}
          onClick={() => setFormValues((prev) => ({ ...prev, [field.key]: !prev[field.key] }))}
        >
          <div className="switch-thumb" />
        </button>
      );
    }

    if (field.type === 'select' && field.options) {
      return (
        <select
          className="input"
          style={{ width: 240 }}
          value={String(val ?? '')}
          onChange={(e) => setFormValues((prev) => ({ ...prev, [field.key]: e.target.value }))}
        >
          {field.options.map((opt) => (
            <option key={opt.value} value={opt.value} style={{ background: '#111622' }}>
              {opt.label}
            </option>
          ))}
        </select>
      );
    }

    if (field.type === 'number') {
      const isOpacity = field.key.toLowerCase().includes('opacity') || field.name.toLowerCase().includes('255');
      if (isOpacity) {
        return (
          <div style={{ display: 'flex', alignItems: 'center', gap: 12, width: 240 }}>
            <input
              type="range"
              min={50}
              max={255}
              value={Number(val ?? 225)}
              onChange={(e) => setFormValues((prev) => ({ ...prev, [field.key]: Number(e.target.value) }))}
              style={{ flex: 1, accentColor: '#6366f1' }}
            />
            <span className="font-mono" style={{ fontSize: 12.5, width: 34, textAlign: 'right' }}>
              {val ?? 225}
            </span>
          </div>
        );
      }

      return (
        <input
          type="number"
          className="input"
          style={{ width: 140 }}
          value={val ?? ''}
          onChange={(e) => setFormValues((prev) => ({ ...prev, [field.key]: Number(e.target.value) }))}
        />
      );
    }

    if (field.type === 'array') {
      return (
        <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>
          Advanced list ({Array.isArray(val) ? val.length : 0} items) — edit in Source Code if needed
        </div>
      );
    }

    return (
      <input
        type="text"
        className="input"
        style={{ width: 260 }}
        value={typeof val === 'object' ? JSON.stringify(val) : val ?? ''}
        onChange={(e) => setFormValues((prev) => ({ ...prev, [field.key]: e.target.value }))}
      />
    );
  };

  return (
    <div
      style={{
        position: 'fixed',
        inset: 0,
        background: 'rgba(5, 7, 11, 0.78)',
        backdropFilter: 'blur(8px)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 50,
        padding: 24
      }}
    >
      <div
        className="glass-card"
        style={{
          width: '100%',
          maxWidth: 840,
          maxHeight: '88vh',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          background: '#101521'
        }}
      >
        <div
          style={{
            padding: '18px 22px',
            borderBottom: '1px solid var(--border-subtle)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between'
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
            <img
              src={omnilynxLogo}
              alt="Omnilynx"
              style={{ width: 32, height: 32, objectFit: 'contain', flexShrink: 0 }}
            />
            <div>
              <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                <h2 style={{ fontSize: 18, fontWeight: 600 }}>{mod.parsed.meta.name}</h2>
                <span className="badge badge-indigo">v{mod.parsed.meta.version}</span>
                <span className="badge badge-slate">{mod.origin === 'windhawk' ? 'Omnilynx Engine' : mod.origin}</span>
              </div>
              <p style={{ fontSize: 12.5, color: 'var(--text-secondary)', marginTop: 4 }}>
                by {mod.parsed.meta.author} · ID: <code>{mod.id}</code>
              </p>
            </div>
          </div>
          <button className="btn" onClick={onClose} style={{ padding: 8 }}>
            <X size={16} />
          </button>
        </div>

        <div
          style={{
            display: 'flex',
            gap: 8,
            padding: '10px 22px',
            borderBottom: '1px solid var(--border-subtle)',
            background: 'rgba(9, 12, 18, 0.45)',
            flexWrap: 'wrap'
          }}
        >
          <button
            className={`btn ${tab === 'settings' ? 'btn-primary' : ''}`}
            onClick={() => setTab('settings')}
          >
            <Sliders size={14} /> Easy Settings ({mod.parsed.settingsSchema.length})
          </button>
          <button
            className={`btn ${tab === 'details' ? 'btn-primary' : ''}`}
            onClick={() => setTab('details')}
          >
            <FileText size={14} /> Details
          </button>
          <button
            className={`btn ${tab === 'engine' ? 'btn-primary' : ''}`}
            onClick={() => setTab('engine')}
          >
            <Cpu size={14} /> Run & Manage
          </button>
          <div style={{ flex: 1 }} />
          {onShare && (
            <button className="btn" onClick={() => onShare(mod.id)}>
              <Share2 size={14} /> {lang === 'ar' ? 'مشاركة رابط' : 'Share Link'}
            </button>
          )}
          <button className="btn" onClick={() => onSwap(mod.id)}>
            <ArrowLeftRight size={14} /> {lang === 'ar' ? 'تبديل الملف' : 'Swap File'}
          </button>
          <button className="btn" onClick={() => onEditCode(mod)}>
            <Code2 size={14} /> {lang === 'ar' ? 'تعديل الكود' : 'Edit Source'}
          </button>
        </div>

        <div style={{ flex: 1, overflowY: 'auto', padding: 22 }}>
          {tab === 'settings' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
              {mod.parsed.settingsSchema.length === 0 ? (
                <p style={{ color: 'var(--text-secondary)', fontSize: 13.5 }}>
                  This mod has no extra settings — just toggle it ON or OFF from the main dashboard.
                </p>
              ) : (
                mod.parsed.settingsSchema.map((field) => (
                  <div
                    key={field.key}
                    style={{
                      padding: 14,
                      borderRadius: 10,
                      background: 'rgba(255,255,255,0.03)',
                      border: '1px solid var(--border-subtle)',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      gap: 16
                    }}
                  >
                    <div style={{ minWidth: 0 }}>
                      <div style={{ fontSize: 13.5, fontWeight: 500 }}>{field.name}</div>
                      {field.description && (
                        <div style={{ fontSize: 12, color: 'var(--text-secondary)', marginTop: 3 }}>
                          {field.description}
                        </div>
                      )}
                    </div>
                    {renderFieldControl(field)}
                  </div>
                ))
              )}
              {mod.parsed.settingsSchema.length > 0 && (
                <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: 4 }}>
                  <button className="btn btn-primary" onClick={handleSave} disabled={busy}>
                    <Save size={14} /> Save Settings
                  </button>
                </div>
              )}
            </div>
          )}

          {tab === 'details' && (
            <div>
              <p style={{ color: 'var(--text-primary)', marginBottom: 14, fontSize: 14 }}>
                {sanitizeLinks((lang === 'ar' && translatedDesc) ? translatedDesc : mod.parsed.meta.description)}
              </p>
              <div
                style={{
                  marginBottom: 14,
                  padding: '10px 12px',
                  borderRadius: 10,
                  background: 'rgba(99, 102, 241, 0.08)',
                  border: '1px solid rgba(99, 102, 241, 0.25)',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  gap: 10,
                  flexWrap: 'wrap'
                }}
              >
                <div style={{ minWidth: 0, flex: 1 }}>
                  <div style={{ fontSize: 11.5, color: '#a5b4fc', fontWeight: 700, marginBottom: 3 }}>
                    {lang === 'ar' ? '🔗 رابط مشاركة المود:' : '🔗 Mod Share Link:'}
                  </div>
                  <code className="font-mono" style={{ fontSize: 11.5, wordBreak: 'break-all', userSelect: 'text' }}>
                    {`omnilynx://install?mod=${encodeURIComponent(mod.id)}&repo=${encodeURIComponent('PyDevMedo/Omnilynx')}`}
                  </code>
                </div>
                {onShare && (
                  <button className="btn btn-primary" style={{ padding: '6px 12px' }} onClick={() => onShare(mod.id)}>
                    <Share2 size={13} /> {lang === 'ar' ? 'نسخ الرابط' : 'Copy Link'}
                  </button>
                )}
              </div>
              {mod.parsed.meta.include?.length > 0 && (
                <div style={{ marginBottom: 16, display: 'flex', gap: 6, flexWrap: 'wrap' }}>
                  <span style={{ fontSize: 12, color: 'var(--text-muted)' }}>Target processes:</span>
                  {mod.parsed.meta.include.map((p, idx) => (
                    <span key={idx} className="badge badge-emerald">
                      {p}
                    </span>
                  ))}
                </div>
              )}
              <div className="markdown-body" dangerouslySetInnerHTML={{ __html: readmeHtml }} />
            </div>
          )}

          {tab === 'engine' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
              <div
                style={{
                  padding: 16,
                  borderRadius: 12,
                  background: 'rgba(99, 102, 241, 0.08)',
                  border: '1px solid rgba(99, 102, 241, 0.25)'
                }}
              >
                <div style={{ fontSize: 13.5, fontWeight: 600, marginBottom: 8 }}>
                  Standalone Native x64 DLL Injection
                </div>
                <p style={{ fontSize: 12.5, color: 'var(--text-secondary)', marginBottom: 14 }}>
                  Compile this mod to a native 64-bit DLL and inject or eject it from any running
                  Windows process (e.g. <code>notepad.exe</code>, <code>explorer.exe</code>).
                </p>
                <div style={{ display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap' }}>
                  <input
                    className="input"
                    value={targetProc}
                    onChange={(e) => setTargetProc(e.target.value)}
                    placeholder="Target process (e.g. notepad.exe or PID)"
                    style={{ maxWidth: 260 }}
                  />
                  <button className="btn btn-primary" onClick={handleInject} disabled={busy}>
                    <Play size={14} /> Compile & Inject
                  </button>
                  <button className="btn btn-danger" onClick={handleEject} disabled={busy}>
                    <Square size={14} /> Eject DLL
                  </button>
                </div>
              </div>

              <div style={{ display: 'flex', justifyContent: 'space-between', gap: 10 }}>
                <button className="btn" onClick={() => onDuplicate(mod.id)}>
                  <Copy size={14} /> Duplicate to Editable Mod
                </button>
                {mod.origin === 'wendo' && (
                  <button className="btn btn-danger" onClick={() => onDelete(mod.id)}>
                    <Trash2 size={14} /> Delete Mod
                  </button>
                )}
              </div>
            </div>
          )}

          {statusMsg && (
            <div
              style={{
                marginTop: 14,
                padding: '10px 14px',
                borderRadius: 9,
                background: 'rgba(16, 185, 129, 0.12)',
                border: '1px solid rgba(16, 185, 129, 0.3)',
                color: '#6ee7b7',
                fontSize: 13
              }}
            >
              {statusMsg}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
