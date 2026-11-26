import React, { useState, useEffect, useMemo } from 'react';
import { marked } from 'marked';
import CodeMirror from '@uiw/react-codemirror';
import { cpp } from '@codemirror/lang-cpp';
import { oneDark } from '@codemirror/theme-one-dark';
import { X, Download, CheckCircle2, FileText, Sliders, Code2, ExternalLink } from 'lucide-react';
import { parseModSource } from '../utils/modParser';

export default function StorePreviewModal({ item, isInstalled, installing, onClose, onInstall }) {
  const [tab, setTab] = useState('readme');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [code, setCode] = useState('');

  useEffect(() => {
    let active = true;
    setLoading(true);
    setError('');
    window.wendoAPI
      .fetchStoreModSource({ downloadUrl: item.downloadUrl })
      .then((src) => {
        if (active) {
          setCode(src);
          setLoading(false);
        }
      })
      .catch((err) => {
        if (active) {
          setError(err?.message || 'Failed to fetch mod preview');
          setLoading(false);
        }
      });
    return () => {
      active = false;
    };
  }, [item.downloadUrl]);

  const parsed = useMemo(() => parseModSource(code, item.id), [code, item.id]);
  const readmeHtml = useMemo(
    () => marked.parse(parsed.readme || '*No README provided for this mod.*'),
    [parsed.readme]
  );

  return (
    <div
      style={{
        position: 'fixed',
        inset: 0,
        background: 'rgba(5, 7, 11, 0.82)',
        backdropFilter: 'blur(8px)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 60,
        padding: 24
      }}
    >
      <div
        className="glass-card"
        style={{
          width: '100%',
          maxWidth: 880,
          maxHeight: '88vh',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          background: '#101521'
        }}
      >
        {/* Header */}
        <div
          style={{
            padding: '18px 22px',
            borderBottom: '1px solid var(--border-subtle)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            gap: 12
          }}
        >
          <div style={{ minWidth: 0 }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 10, flexWrap: 'wrap' }}>
              <h2 style={{ fontSize: 18, fontWeight: 600 }}>
                {loading ? item.id : parsed.meta.name}
              </h2>
              {!loading && <span className="badge badge-indigo">v{parsed.meta.version}</span>}
              {!loading && <span className="badge badge-slate">{parsed.category}</span>}
            </div>
            <p style={{ fontSize: 12.5, color: 'var(--text-secondary)', marginTop: 4 }}>
              {loading
                ? `Loading preview for ${item.fileName}...`
                : `by ${parsed.meta.author} · ${(item.size / 1024).toFixed(1)} KB`}
            </p>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            {item.htmlUrl && (
              <button
                className="btn"
                onClick={() => window.wendoAPI.openExternal(item.htmlUrl)}
                title="Open on GitHub"
              >
                <ExternalLink size={14} /> GitHub
              </button>
            )}
            <button
              className={`btn ${isInstalled ? 'btn-success' : 'btn-primary'}`}
              disabled={loading || installing}
              onClick={() => onInstall(item, code)}
            >
              {isInstalled ? (
                <>
                  <CheckCircle2 size={14} /> Reinstall / Update
                </>
              ) : installing ? (
                'Installing...'
              ) : (
                <>
                  <Download size={14} /> Install Mod
                </>
              )}
            </button>
            <button className="btn" onClick={onClose} style={{ padding: 8 }}>
              <X size={16} />
            </button>
          </div>
        </div>

        {/* Tabs */}
        <div
          style={{
            display: 'flex',
            gap: 8,
            padding: '10px 22px',
            borderBottom: '1px solid var(--border-subtle)',
            background: 'rgba(9, 12, 18, 0.45)'
          }}
        >
          <button
            className={`btn ${tab === 'readme' ? 'btn-primary' : ''}`}
            onClick={() => setTab('readme')}
          >
            <FileText size={14} /> Overview & README
          </button>
          <button
            className={`btn ${tab === 'settings' ? 'btn-primary' : ''}`}
            onClick={() => setTab('settings')}
          >
            <Sliders size={14} /> Options Preview ({parsed.settingsSchema.length})
          </button>
          <button
            className={`btn ${tab === 'code' ? 'btn-primary' : ''}`}
            onClick={() => setTab('code')}
          >
            <Code2 size={14} /> Source Code
          </button>
        </div>

        {/* Body */}
        <div style={{ flex: 1, overflowY: 'auto', padding: 22 }}>
          {loading ? (
            <p style={{ color: 'var(--text-secondary)', fontSize: 13.5 }}>
              Fetching mod details from GitHub...
            </p>
          ) : error ? (
            <div
              style={{
                padding: 14,
                borderRadius: 10,
                background: 'rgba(239, 68, 68, 0.14)',
                border: '1px solid rgba(239, 68, 68, 0.3)',
                color: '#fca5a5'
              }}
            >
              {error}
            </div>
          ) : tab === 'readme' ? (
            <div>
              {parsed.meta.description && (
                <p style={{ color: 'var(--text-primary)', marginBottom: 14, fontSize: 14 }}>
                  {parsed.meta.description}
                </p>
              )}
              {parsed.meta.include?.length > 0 && (
                <div style={{ marginBottom: 16, display: 'flex', gap: 6, flexWrap: 'wrap' }}>
                  <span style={{ fontSize: 12, color: 'var(--text-muted)' }}>Target processes:</span>
                  {parsed.meta.include.map((p, idx) => (
                    <span key={idx} className="badge badge-emerald">
                      {p}
                    </span>
                  ))}
                </div>
              )}
              <div className="markdown-body" dangerouslySetInnerHTML={{ __html: readmeHtml }} />
            </div>
          ) : tab === 'settings' ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
              {parsed.settingsSchema.length === 0 ? (
                <p style={{ color: 'var(--text-secondary)', fontSize: 13.5 }}>
                  This mod has no configurable options.
                </p>
              ) : (
                parsed.settingsSchema.map((field) => (
                  <div
                    key={field.key}
                    style={{
                      padding: 14,
                      borderRadius: 10,
                      background: 'rgba(255,255,255,0.03)',
                      border: '1px solid var(--border-subtle)'
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', gap: 12 }}>
                      <div style={{ fontSize: 13.5, fontWeight: 600 }}>{field.name}</div>
                      <span className="badge badge-slate">{field.type}</span>
                    </div>
                    {field.description && (
                      <div style={{ fontSize: 12.5, color: 'var(--text-secondary)', marginTop: 4 }}>
                        {field.description}
                      </div>
                    )}
                  </div>
                ))
              )}
            </div>
          ) : (
            <div style={{ height: 420, borderRadius: 10, overflow: 'hidden', userSelect: 'text' }}>
              <CodeMirror
                value={code}
                height="420px"
                theme={oneDark}
                extensions={[cpp()]}
                editable={false}
                style={{ fontSize: 12.5 }}
              />
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
