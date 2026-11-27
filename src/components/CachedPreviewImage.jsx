import React, { useState, useEffect } from 'react';

export default function CachedPreviewImage({ id, url, title, includes = [], category = '', height = 128 }) {
  const [src, setSrc] = useState(url || '');
  const [failed, setFailed] = useState(false);

  useEffect(() => {
    let active = true;
    setFailed(false);
    setSrc(url || '');

    if (window.wendoAPI?.cachePreviewImage && id && url) {
      window.wendoAPI.cachePreviewImage({ id, url }).then((res) => {
        if (active && res?.ok && res.dataUrl) {
          setSrc(res.dataUrl);
        }
      });
    }
    return () => {
      active = false;
    };
  }, [id, url]);

  if (!url || failed) {
    const targets = Array.isArray(includes) && includes.length > 0 ? includes.slice(0, 3).join(', ') : 'Windows Shell';
    return (
      <div
        style={{
          height: 64,
          borderRadius: 10,
          border: '1px solid var(--border-subtle)',
          background: 'rgba(15, 23, 42, 0.55)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '0 12px',
          gap: 8
        }}
      >
        <span style={{ fontSize: 11.5, color: 'var(--text-secondary)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
          🎯 {targets}
        </span>
        {category && (
          <span style={{ fontSize: 10.5, padding: '2px 8px', borderRadius: 999, background: 'rgba(56,189,248,0.12)', color: '#38bdf8', flexShrink: 0 }}>
            {category}
          </span>
        )}
      </div>
    );
  }

  return (
    <div style={{ height, borderRadius: 10, overflow: 'hidden', border: '1px solid var(--border-subtle)', background: '#0b0e14' }}>
      <img
        src={src}
        alt={title || id}
        style={{ width: '100%', height: '100%', objectFit: 'cover', display: 'block' }}
        onError={() => setFailed(true)}
      />
    </div>
  );
}
