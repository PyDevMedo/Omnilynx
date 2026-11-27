import React, { useState, useEffect } from 'react';

function buildFallbackSvgDataUrl(title = 'Omnilynx Mod') {
  const safe = String(title).replace(/[<>&"']/g, '');
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="640" height="320" viewBox="0 0 640 320">
    <rect width="640" height="320" rx="14" fill="#0b1120"/>
    <rect x="20" y="20" width="600" height="280" rx="10" fill="#111827" stroke="#38bdf8" stroke-opacity="0.4"/>
    <text x="320" y="155" text-anchor="middle" fill="#f8fafc" font-family="Segoe UI, sans-serif" font-size="20" font-weight="700">${safe}</text>
    <text x="320" y="185" text-anchor="middle" fill="#38bdf8" font-family="Segoe UI, sans-serif" font-size="13">Omnilynx Offline Preview</text>
  </svg>`;
  return `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
}

export default function CachedPreviewImage({ id, url, title, height = 128 }) {
  const [src, setSrc] = useState(() => buildFallbackSvgDataUrl(title));

  useEffect(() => {
    let active = true;
    const targetUrl = url || '';
    if (window.wendoAPI?.cachePreviewImage && id && targetUrl) {
      window.wendoAPI.cachePreviewImage({ id, url: targetUrl }).then((res) => {
        if (active && res?.ok && res.dataUrl) {
          setSrc(res.dataUrl);
        } else if (active && targetUrl) {
          setSrc(targetUrl);
        }
      });
    } else if (targetUrl) {
      setSrc(targetUrl);
    }
    return () => {
      active = false;
    };
  }, [id, url, title]);

  return (
    <div style={{ height, borderRadius: 10, overflow: 'hidden', border: '1px solid var(--border-subtle)', background: '#0b0e14' }}>
      <img
        src={src}
        alt={title || id}
        style={{ width: '100%', height: '100%', objectFit: 'cover', display: 'block' }}
        onError={() => setSrc(buildFallbackSvgDataUrl(title))}
      />
    </div>
  );
}
