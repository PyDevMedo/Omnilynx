import React, { useState, useEffect, useMemo, Suspense, lazy } from 'react';
import {
  LayoutGrid,
  Compass,
  Activity,
  Settings,
  Plus,
  Upload,
  Search,
  Code2,
  Sliders,
  Download,
  RefreshCw,
  CheckCircle2,
  Cpu,
  RotateCcw,
  FolderOpen,
  ArrowLeftRight,
  Zap,
  Wand2,
  Eye,
  Power,
  Share2,
  Link2,
  Trash2
} from 'lucide-react';
import omnilynxLogo from './assets/omnilynx-logo.png';
import { parseModSource, createNewModTemplate } from './utils/modParser';
import { getLocalizedModCopy } from './utils/modTranslations';
import ModDetailsModal from './components/ModDetailsModal';

// Lazy load heavy CodeMirror modals to keep idle RAM low
const ModEditorModal = lazy(() => import('./components/ModEditorModal'));
const StorePreviewModal = lazy(() => import('./components/StorePreviewModal'));

const CATEGORIES = ['All', 'Taskbar', 'Start & Shell', 'Explorer', 'Visuals & FX', 'Productivity', 'System'];

const I18N = {
  ar: {
    brandName: 'Omnilynx',
    brandSub: 'منصة تخصيص الويندوز الاحترافية',
    langBtn: '🌐 English',
    activePill: (n) => `⚡ ${n} قيد التشغيل`,
    compilerReady: 'Clang x64 جاهز',
    noCompiler: 'بدون مترجم',
    navMods: '🧩 المودات المثبتة',
    navTweaks: '⚡ تعديلات الويندوز',
    navStore: '🛒 متجر المودات',
    navSettings: '⚙️ الإعدادات',
    newMod: '✨ + مود جديد',
    importMod: '📥 استيراد .wh.cpp',
    openFolder: '📂 مجلد المودات',
    tipTitle: '💡 نصيحة سريعة',
    tipBody: 'اضغط على زر الباور الصغير ⏻ في أي كارت لتشغيل أو إيقاف المود فوراً.',
    quoteBtn: '🔄 اقتباس جديد',
    modsTitle: '🚀 لوحة التحكم بالمودات',
    modsSub: 'تحكم كامل بزر الباور المضيء — تشغيل، تبديل، وتخصيص بضغطة واحدة.',
    searchMods: '🔍 ابحث عن مود بالاسم...',
    glassPreset: '💎 باقة الزجاج',
    disableAll: 'إيقاف الكل',
    onTag: '⚡ يعمل الآن',
    offTag: '⏸️ متوقف',
    detailsBtn: '🎛️ الإعدادات',
    tweaksTitle: '⚡ تعديلات الويندوز الفورية',
    tweaksSub: 'تعديلات مباشرة على نظام الويندوز — تعمل فوراً بدون تثبيت إضافي.',
    storeTitle: '🛒 متجر المودات',
    storeSearch: '🔍 ابحث في مودات المجتمع...',
    refresh: 'تحديث',
    previewBtn: '👁️ معاينة',
    installBtn: '⬇️ تثبيت',
    reinstallBtn: '✅ تحديث',
    settingsTitle: '⚙️ إعدادات Omnilynx',
    quotes: [
      { text: '«البساطة هي قمة الاحترافية؛ اجعل أدواتك تعمل في صمت وتمنحك تجربة بصرية مريحة.» ✨', author: '— فلسفة Omnilynx' },
      { text: '«التفاصيل الصغيرة هي التي تصنع الفارق بين برنامج عادي وتجربة يومية استثنائية.» 💎', author: '— هندسة واجهات المستخدم' },
      { text: '«الجودة ليست فعلاً عابراً، بل عادة تتجلى في كل زر، وكل حركة، وكل تفصيلة.» 🚀', author: '— أرسطو (بتصرف)' }
    ],
    tweaksList: [
      { key: 'darkMode', title: '🌙 الوضع الليلي للويندوز', desc: 'تبديل النظام والبرامج بين الوضع الليلي والنهاري فوراً.' },
      { key: 'transparency', title: '💎 تأثيرات الشفافية والزجاج', desc: 'تفعيل خامات Mica وAcrylic عبر واجهات الويندوز.' },
      { key: 'taskbarCenter', title: '🎯 توسيط أيقونات شريط المهام', desc: 'محاذاة أيقونات شريط المهام في المنتصف أو اليسار.' },
      { key: 'clockSeconds', title: '🕒 إظهار الثواني في الساعة', desc: 'عرض الثواني في ساعة شريط المهام لنظام ويندوز 11.' },
      { key: 'fileExtensions', title: '📂 إظهار امتدادات الملفات', desc: 'إظهار امتدادات الملفات (.exe, .cpp, .png) دائماً.' },
      { key: 'compactMode', title: '📐 الوضع المضغوط للمستكشف', desc: 'تقليل المسافات بين العناصر في File Explorer.' }
    ]
  },
  en: {
    brandName: 'Omnilynx',
    brandSub: 'Professional Windows Customization Suite',
    langBtn: '🌐 العربية',
    activePill: (n) => `⚡ ${n} Active`,
    compilerReady: 'Clang x64 Ready',
    noCompiler: 'No Compiler',
    navMods: '🧩 Installed Mods',
    navTweaks: '⚡ Windows Tweaks',
    navStore: '🛒 Explore Store',
    navSettings: '⚙️ Settings',
    newMod: '✨ + New Mod',
    importMod: '📥 Import .wh.cpp',
    openFolder: '📂 Open Mods Folder',
    tipTitle: '💡 Quick Tip',
    tipBody: 'Click the compact power button ⏻ on any card to toggle a mod on or off instantly.',
    quoteBtn: '🔄 New Quote',
    modsTitle: '🚀 Mod Control Center',
    modsSub: 'Precision control with illuminated power switches — toggle, swap, and customize.',
    searchMods: '🔍 Search mods by name...',
    glassPreset: '💎 Glass Preset',
    disableAll: 'Disable All',
    onTag: '⚡ Active',
    offTag: '⏸️ Standby',
    detailsBtn: '🎛️ Options',
    tweaksTitle: '⚡ Windows Quick Tweaks',
    tweaksSub: 'Instant system-wide Windows 11/10 customization — no extra service required.',
    storeTitle: '🛒 Explore Mod Store',
    storeSearch: '🔍 Search community mods...',
    refresh: 'Refresh',
    previewBtn: '👁️ Preview',
    installBtn: '⬇️ Install',
    reinstallBtn: '✅ Update',
    settingsTitle: '⚙️ Omnilynx Settings',
    quotes: [
      { text: '“Simplicity is the ultimate sophistication; great tools work silently and feel effortless.” ✨', author: '— Omnilynx Philosophy' },
      { text: '“Small details are what separate an ordinary utility from an extraordinary daily experience.” 💎', author: '— Craft & UI Design' },
      { text: '“Quality is not an act, it is a habit reflected in every button, motion, and detail.” 🚀', author: '— Aristotle' }
    ],
    tweaksList: [
      { key: 'darkMode', title: '🌙 Windows Dark Mode', desc: 'Switch Windows & Apps between Dark and Light theme immediately.' },
      { key: 'transparency', title: '💎 Transparency & Mica', desc: 'Enable glass, Acrylic, and Mica transparency across Windows.' },
      { key: 'taskbarCenter', title: '🎯 Center Taskbar Icons', desc: 'Align taskbar icons to Center (ON) or Left (OFF).' },
      { key: 'clockSeconds', title: '🕒 Show Clock Seconds', desc: 'Display seconds in the Windows 11 system tray clock.' },
      { key: 'fileExtensions', title: '📂 Show File Extensions', desc: 'Always show file extensions (.exe, .cpp, .png) in Explorer.' },
      { key: 'compactMode', title: '📐 Compact File Explorer', desc: 'Reduce spacing between items in Windows 11 File Explorer.' }
    ]
  }
};

export default function App() {
  const [lang, setLang] = useState('ar');
  const [qIdx, setQIdx] = useState(0);
  const [page, setPage] = useState('installed');
  const [mods, setMods] = useState([]);
  const [search, setSearch] = useState('');
  const [category, setCategory] = useState('All');
  const [engineInfo, setEngineInfo] = useState({});
  const [appConfig, setAppConfig] = useState({ modsRepo: 'PyDevMedo/Omnilynx' });
  const [dragOver, setDragOver] = useState(false);

  const [selectedMod, setSelectedMod] = useState(null);
  const [editorState, setEditorState] = useState(null);
  const [previewStoreItem, setPreviewStoreItem] = useState(null);

  const [storeMods, setStoreMods] = useState([]);
  const [storeLoading, setStoreLoading] = useState(false);
  const [storeError, setStoreError] = useState('');
  const [storeSearch, setStoreSearch] = useState('');
  const [installingId, setInstallingId] = useState(null);
  const [shareLinkInput, setShareLinkInput] = useState('');
  const [storeMetaCache, setStoreMetaCache] = useState({});

  const [tweaks, setTweaks] = useState({});

  const [toast, setToast] = useState('');
  const [errorBanner, setErrorBanner] = useState('');

  const t = I18N[lang];

  useEffect(() => {
    document.documentElement.lang = lang;
    document.documentElement.dir = lang === 'ar' ? 'rtl' : 'ltr';
  }, [lang]);

  const showToast = (msg, isError = false) => {
    if (isError) setErrorBanner(msg);
    setToast(msg);
    setTimeout(() => setToast(''), 4000);
  };

  const loadInstalled = async () => {
    if (!window.wendoAPI) return;
    const raw = await window.wendoAPI.listInstalledMods();
    setMods(raw.map((m) => ({ ...m, parsed: parseModSource(m.code, m.id) })));
  };

  const loadInit = async () => {
    if (!window.wendoAPI) return;
    await loadInstalled();
    setEngineInfo(await window.wendoAPI.getEngineInfo());
    setAppConfig(await window.wendoAPI.getAppConfig());
  };

  useEffect(() => {
    loadInit();
  }, []);

  const loadStore = async () => {
    setStoreLoading(true);
    setStoreError('');
    try {
      setStoreMods(await window.wendoAPI.fetchStoreIndex(appConfig.modsRepo));
    } catch (e) {
      setStoreError(e.message || 'Failed to load mods from GitHub');
    }
    setStoreLoading(false);
  };

  const loadTweaks = async () => {
    setTweaks((await window.wendoAPI.getTweaks()) || {});
  };

  useEffect(() => {
    if (page === 'explore' && storeMods.length === 0) loadStore();
    if (page === 'tweaks') loadTweaks();
  }, [page]);

  const filteredMods = useMemo(() => {
    return mods.filter((m) => {
      const matchCat = category === 'All' || m.parsed.category === category;
      const q = search.toLowerCase();
      return (
        matchCat &&
        (!q ||
          m.parsed.meta.name.toLowerCase().includes(q) ||
          m.parsed.meta.description.toLowerCase().includes(q) ||
          m.id.toLowerCase().includes(q))
      );
    });
  }, [mods, category, search]);

  const activeCount = useMemo(() => mods.filter((m) => m.enabled).length, [mods]);
  const installedIds = useMemo(() => new Set(mods.map((m) => m.id)), [mods]);

  const filteredStore = useMemo(() => {
    const q = storeSearch.toLowerCase();
    return storeMods.filter((m) => !q || m.id.toLowerCase().includes(q));
  }, [storeMods, storeSearch]);

  // Lazy-fetch metadata & thumbnail images for the visible Store cards
  useEffect(() => {
    if (page !== 'explore' || filteredStore.length === 0) return;
    let cancelled = false;
    const visible = filteredStore.slice(0, 18).filter((item) => !storeMetaCache[item.id]);
    if (visible.length === 0) return;

    (async () => {
      for (const item of visible) {
        if (cancelled) break;
        try {
          const code = await window.wendoAPI.fetchStoreModSource({ downloadUrl: item.downloadUrl });
          if (cancelled) break;
          const parsed = parseModSource(code, item.id);
          const imgMatch = (parsed.readme || '').match(/!\[[^\]]*\]\((https?:\/\/[^)]+)\)/);
          setStoreMetaCache((prev) => ({
            ...prev,
            [item.id]: {
              name: parsed.meta.name || item.id,
              desc: parsed.meta.description || '',
              version: parsed.meta.version || '1.0.0',
              author: parsed.meta.author || 'Omnilynx',
              category: parsed.category,
              previewImg: imgMatch ? imgMatch[1] : null
            }
          }));
        } catch {
          // ignore individual fetch failures
        }
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [page, filteredStore]);

  const handleToggleMod = async (mod) => {
    setErrorBanner('');
    const next = !mod.enabled;
    // Optimistic UI update for instant feedback
    setMods((prev) => prev.map((m) => (m.id === mod.id ? { ...m, enabled: next } : m)));

    const res = await window.wendoAPI.toggleMod({
      id: mod.id,
      enabled: next,
      origin: mod.origin,
      code: mod.code,
      compilerOptions: mod.parsed.meta.compilerOptions
    });
    if (res.ok === false) {
      // Revert if failed
      setMods((prev) => prev.map((m) => (m.id === mod.id ? { ...m, enabled: !next } : m)));
    }
    showToast(res.message || `${mod.parsed.meta.name} ${next ? 'enabled' : 'disabled'}`, res.ok === false);
  };

  const handleApplyGlassPreset = async () => {
    const targets = ['wendo-dark-titlebars', 'wendo-translucent-mica', 'wendo-window-opacity'];
    for (const m of mods) {
      if (targets.includes(m.id) && !m.enabled) {
        await window.wendoAPI.toggleMod({
          id: m.id,
          enabled: true,
          origin: m.origin,
          code: m.code,
          compilerOptions: m.parsed.meta.compilerOptions
        });
      }
    }
    await loadInstalled();
    showToast('💎 Glass Preset Applied!');
  };

  const handleDisableAll = async () => {
    for (const m of mods) {
      if (m.enabled) {
        await window.wendoAPI.toggleMod({
          id: m.id,
          enabled: false,
          origin: m.origin,
          code: m.code,
          compilerOptions: m.parsed.meta.compilerOptions
        });
      }
    }
    await loadInstalled();
    showToast('All mods disabled.');
  };

  const handleInstallStoreMod = async (item, preloadedCode = null) => {
    setInstallingId(item.id);
    try {
      const code = preloadedCode || (await window.wendoAPI.fetchStoreModSource({ downloadUrl: item.downloadUrl }));
      await window.wendoAPI.saveModSource({ id: item.id, code });
      await loadInstalled();
      setPreviewStoreItem(null);
      showToast(`Installed ${item.id}`);
    } catch (e) {
      showToast(`Failed to install ${item.id}: ${e.message}`, true);
    }
    setInstallingId(null);
  };

  const handleImportFile = async () => {
    const res = await window.wendoAPI.importModFile();
    if (!res.canceled) {
      await loadInstalled();
      showToast(`Imported ${res.id}`);
    }
  };

  const handleSwapMod = async (modId) => {
    const res = await window.wendoAPI.swapModFile(modId);
    if (!res.canceled) {
      await loadInstalled();
      setSelectedMod(null);
      showToast(`Swapped ${modId}`);
    }
  };

  const handleDuplicateMod = async (modId) => {
    const res = await window.wendoAPI.duplicateMod(modId);
    if (res.ok) {
      await loadInstalled();
      setSelectedMod(null);
      showToast(`Created editable copy: ${res.id}`);
    }
  };

  const handleShareMod = async (modId) => {
    const repo = appConfig.modsRepo || 'PyDevMedo/Omnilynx';
    const link = `omnilynx://install?mod=${encodeURIComponent(modId)}&repo=${encodeURIComponent(repo)}`;
    try {
      await navigator.clipboard.writeText(link);
      showToast(
        lang === 'ar'
          ? `🔗 تم نسخ رابط مشاركة المود (${modId})`
          : `🔗 Share link copied for ${modId}`
      );
    } catch {
      showToast(link);
    }
  };

  const handleInstallFromLink = async () => {
    if (!shareLinkInput.trim()) return;
    const res = await window.wendoAPI.installFromLink(shareLinkInput);
    if (res.ok) {
      setShareLinkInput('');
      await loadInstalled();
      showToast(
        lang === 'ar' ? `✅ تم تثبيت المود من الرابط: ${res.id}` : `✅ Installed from link: ${res.id}`
      );
    } else {
      showToast(res.error || 'Failed to install from link', true);
    }
  };

  const handleDeleteMod = async (modId, origin) => {
    const msg =
      lang === 'ar'
        ? `هل أنت متأكد من حذف المود "${modId}" نهائياً؟`
        : `Are you sure you want to delete "${modId}"?`;
    if (!window.confirm(msg)) return;
    await window.wendoAPI.deleteMod({ id: modId, origin });
    setSelectedMod(null);
    await loadInstalled();
    showToast(lang === 'ar' ? `🗑️ تم حذف المود: ${modId}` : `🗑️ Deleted ${modId}`);
  };

  const handleDrop = async (e) => {
    e.preventDefault();
    setDragOver(false);
    for (const file of Array.from(e.dataTransfer.files || [])) {
      if (file.name.endsWith('.cpp')) {
        const res = await window.wendoAPI.importModContent({ fileName: file.name, content: await file.text() });
        if (res.ok) showToast(`Imported ${res.id}`);
      }
    }
    await loadInstalled();
  };

  const handleCompileAndInject = async (mod, target) => {
    if (mod.origin === 'windhawk') {
      const res = await window.wendoAPI.toggleMod({
        id: mod.id,
        enabled: true,
        origin: 'windhawk',
        code: mod.code,
        compilerOptions: mod.parsed.meta.compilerOptions
      });
      await loadInstalled();
      return { ok: res.ok, message: res.message };
    }
    const comp = await window.wendoAPI.compileMod({
      id: mod.id,
      code: mod.code,
      compilerOptions: mod.parsed.meta.compilerOptions
    });
    if (!comp.ok) return { ok: false, message: `Compile failed: ${comp.stderr}` };
    const inj = await window.wendoAPI.injectMod({ target, dllPath: comp.dllPath });
    await loadInstalled();
    return inj.ok
      ? { ok: true, message: `Compiled & injected into ${target} (PID ${inj.pid})` }
      : { ok: false, message: `Compiled, but inject failed: ${inj.error}` };
  };

  const handleEject = async (mod, target) => {
    const res = await window.wendoAPI.ejectMod({ target, dllName: `${mod.id}.dll` });
    return res.ok && res.ejected
      ? { ok: true, message: `Ejected from ${target}` }
      : { ok: false, message: res.error || 'DLL not loaded in target process' };
  };

  const currentQuote = t.quotes[qIdx % t.quotes.length];

  return (
    <div
      className="app-shell"
      onDragOver={(e) => {
        e.preventDefault();
        setDragOver(true);
      }}
      onDragLeave={() => setDragOver(false)}
      onDrop={handleDrop}
    >
      {/* Titlebar */}
      <header className="titlebar titlebar-drag">
        <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
          <img
            src={omnilynxLogo}
            alt="Omnilynx"
            style={{ width: 26, height: 26, objectFit: 'contain', filter: 'drop-shadow(0 0 8px rgba(56,189,248,0.45))' }}
          />
          <span style={{ fontWeight: 800, fontSize: 14.5, letterSpacing: '0.02em' }}>{t.brandName}</span>
          <span className="badge badge-emerald">{t.activePill(activeCount)}</span>
        </div>

        <div className="titlebar-no-drag" style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          <button className="btn" style={{ padding: '4px 10px', fontSize: 12 }} onClick={() => setLang(lang === 'ar' ? 'en' : 'ar')}>
            {t.langBtn}
          </button>
          <span className={`badge ${engineInfo.compilerPath ? 'badge-emerald' : 'badge-slate'}`}>
            <Cpu size={11} /> {engineInfo.compilerPath ? t.compilerReady : t.noCompiler}
          </span>
        </div>
      </header>

      <div className="main-layout">
        {/* Sidebar */}
        <aside className="sidebar">
          <button className={`nav-item ${page === 'installed' ? 'active' : ''}`} onClick={() => setPage('installed')}>
            <span style={{ flex: 1 }}>{t.navMods}</span>
            <span className="badge badge-slate">{mods.length}</span>
          </button>
          <button className={`nav-item ${page === 'tweaks' ? 'active' : ''}`} onClick={() => setPage('tweaks')}>
            <span style={{ flex: 1 }}>{t.navTweaks}</span>
            <span className="badge badge-slate">6</span>
          </button>
          <button className={`nav-item ${page === 'explore' ? 'active' : ''}`} onClick={() => setPage('explore')}>
            <span>{t.navStore}</span>
          </button>
          <button className={`nav-item ${page === 'settings' ? 'active' : ''}`} onClick={() => setPage('settings')}>
            <span>{t.navSettings}</span>
          </button>

          <div
            style={{
              marginTop: 'auto',
              padding: 12,
              borderRadius: 12,
              background: 'rgba(255,255,255,0.03)',
              border: '1px solid var(--border-subtle)',
              marginBottom: 8
            }}
          >
            <div style={{ fontSize: 12, fontWeight: 700, color: '#a5b4fc', marginBottom: 4 }}>{t.tipTitle}</div>
            <div style={{ fontSize: 11.5, color: 'var(--text-secondary)', lineHeight: 1.55 }}>{t.tipBody}</div>
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: 7 }}>
            <button
              className="btn btn-primary"
              onClick={() =>
                setEditorState({
                  id: `custom-mod-${Date.now().toString().slice(-4)}`,
                  code: createNewModTemplate()
                })
              }
            >
              {t.newMod}
            </button>
            <button className="btn" onClick={handleImportFile}>{t.importMod}</button>
            <button className="btn" onClick={() => window.wendoAPI.openModsFolder()}>{t.openFolder}</button>
          </div>
        </aside>

        {/* Main Content */}
        <main className="content-area">
          {errorBanner && (
            <div
              style={{
                marginBottom: 16,
                padding: '12px 16px',
                borderRadius: 10,
                background: 'rgba(239, 68, 68, 0.14)',
                border: '1px solid rgba(239, 68, 68, 0.35)',
                color: '#fecaca',
                fontSize: 13,
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                gap: 12,
                userSelect: 'text'
              }}
            >
              <span>{errorBanner}</span>
              <button className="btn" style={{ padding: '4px 10px' }} onClick={() => setErrorBanner('')}>✕</button>
            </div>
          )}

          {/* Quote Banner */}
          <section className="quote-banner">
            <div>
              <div style={{ fontSize: 14.5, fontWeight: 700, lineHeight: 1.65 }}>{currentQuote.text}</div>
              <div style={{ fontSize: 12, color: '#a5b4fc', marginTop: 3, fontWeight: 600 }}>{currentQuote.author}</div>
            </div>
            <button className="btn" onClick={() => setQIdx((i) => i + 1)}>{t.quoteBtn}</button>
          </section>

          {page === 'installed' && (
            <div>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 16, gap: 14, flexWrap: 'wrap' }}>
                <div>
                  <h1 style={{ fontSize: 21, fontWeight: 800 }}>{t.modsTitle}</h1>
                  <p style={{ fontSize: 12.8, color: 'var(--text-secondary)', marginTop: 3 }}>{t.modsSub}</p>
                </div>
                <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                  <input
                    className="input"
                    style={{ width: 250 }}
                    placeholder={t.searchMods}
                    value={search}
                    onChange={(e) => setSearch(e.target.value)}
                  />
                  <button className="btn" onClick={loadInstalled}><RefreshCw size={14} /></button>
                </div>
              </div>

              <div style={{ display: 'flex', justifyContent: 'space-between', gap: 10, flexWrap: 'wrap', marginBottom: 18 }}>
                <div style={{ display: 'flex', gap: 7, flexWrap: 'wrap' }}>
                  {CATEGORIES.map((cat) => (
                    <button
                      key={cat}
                      className={`btn ${category === cat ? 'btn-primary' : ''}`}
                      style={{ padding: '5px 12px', fontSize: 12, borderRadius: 99 }}
                      onClick={() => setCategory(cat)}
                    >
                      {cat}
                    </button>
                  ))}
                </div>
                <div style={{ display: 'flex', gap: 8 }}>
                  <button className="btn btn-success" onClick={handleApplyGlassPreset}>{t.glassPreset}</button>
                  <button className="btn" onClick={handleDisableAll}>{t.disableAll}</button>
                </div>
              </div>

              <div className="mod-grid">
                {filteredMods.map((mod) => {
                  const copy = getLocalizedModCopy(mod, lang);
                  const imgMatch = (mod.parsed.readme || '').match(/!\[[^\]]*\]\((https?:\/\/[^)]+)\)/);
                  const previewImg = imgMatch ? imgMatch[1] : null;
                  return (
                  <article key={mod.id} className={`glass-card ${mod.enabled ? 'is-on' : ''}`} style={{ padding: 18, display: 'flex', flexDirection: 'column', gap: 12 }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', gap: 12 }}>
                      <div style={{ minWidth: 0 }}>
                        <div style={{ fontSize: 15, fontWeight: 700 }}>{copy.name}</div>
                        <div style={{ fontSize: 11.5, color: 'var(--text-muted)', marginTop: 2 }}>
                          v{mod.parsed.meta.version} · {mod.parsed.meta.author}
                        </div>
                      </div>
                      <button
                        className={`power-btn ${mod.enabled ? 'on' : ''}`}
                        onClick={() => handleToggleMod(mod)}
                        title={mod.enabled ? t.onTag : t.offTag}
                      >
                        <Power size={15} />
                      </button>
                    </div>

                    {previewImg && (
                      <div style={{ height: 110, borderRadius: 10, overflow: 'hidden', border: '1px solid var(--border-subtle)', background: '#0b0e14' }}>
                        <img src={previewImg} alt={copy.name} style={{ width: '100%', height: '100%', objectFit: 'cover' }} />
                      </div>
                    )}

                    <p style={{ fontSize: 12.5, color: 'var(--text-secondary)', lineHeight: 1.55, flex: 1 }}>
                      {copy.desc || '—'}
                    </p>

                    <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
                      <span className={`badge ${mod.enabled ? 'badge-emerald' : 'badge-slate'}`}>
                        {mod.enabled ? t.onTag : t.offTag}
                      </span>
                      <span className="badge badge-indigo">{mod.parsed.category}</span>
                      <span className="badge badge-slate">{mod.origin === 'windhawk' ? 'Omnilynx' : mod.origin}</span>
                    </div>

                    <div style={{ display: 'flex', gap: 8, paddingTop: 10, borderTop: '1px solid var(--border-subtle)' }}>
                      <button className="btn" style={{ flex: 1 }} onClick={() => setSelectedMod(mod)}>
                        {t.detailsBtn}
                      </button>
                      <button className="btn" onClick={() => handleShareMod(mod.id)} title={lang === 'ar' ? 'مشاركة رابط المود' : 'Share Mod Link'}>
                        <Share2 size={14} />
                      </button>
                      <button className="btn" onClick={() => handleSwapMod(mod.id)} title="Swap Mod File">
                        <ArrowLeftRight size={14} />
                      </button>
                      <button className="btn" onClick={() => setEditorState({ id: mod.id, code: mod.code })} title="Edit Source">
                        <Code2 size={14} />
                      </button>
                      <button className="btn btn-danger" onClick={() => handleDeleteMod(mod.id, mod.origin)} title={lang === 'ar' ? 'حذف المود' : 'Delete Mod'}>
                        <Trash2 size={14} />
                      </button>
                    </div>
                  </article>
                  );
                })}
              </div>
            </div>
          )}

          {page === 'tweaks' && (
            <div>
              <div style={{ marginBottom: 18 }}>
                <h1 style={{ fontSize: 21, fontWeight: 800 }}>{t.tweaksTitle}</h1>
                <p style={{ fontSize: 12.8, color: 'var(--text-secondary)', marginTop: 3 }}>{t.tweaksSub}</p>
              </div>
              <div className="mod-grid">
                {t.tweaksList.map((item) => {
                  const isOn = Boolean(tweaks[item.key]);
                  return (
                    <article key={item.key} className={`glass-card ${isOn ? 'is-on' : ''}`} style={{ padding: 18, display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: 14 }}>
                      <div>
                        <div style={{ fontSize: 14.5, fontWeight: 700 }}>{item.title}</div>
                        <div style={{ fontSize: 12.5, color: 'var(--text-secondary)', marginTop: 4 }}>{item.desc}</div>
                      </div>
                      <button
                        className={`power-btn ${isOn ? 'on' : ''}`}
                        onClick={async () => {
                          const next = !isOn;
                          setTweaks((prev) => ({ ...prev, [item.key]: next }));
                          const res = await window.wendoAPI.applyTweak({ key: item.key, value: next });
                          showToast(res.message || 'Applied!');
                        }}
                      >
                        <Power size={15} />
                      </button>
                    </article>
                  );
                })}
              </div>
            </div>
          )}

          {page === 'explore' && (
            <div>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 18, gap: 14, flexWrap: 'wrap' }}>
                <div>
                  <h1 style={{ fontSize: 21, fontWeight: 800 }}>{t.storeTitle}</h1>
                  <p style={{ fontSize: 12.5, color: 'var(--text-secondary)', marginTop: 3 }}>
                    <code>{appConfig.modsRepo}</code>
                  </p>
                </div>
                <div style={{ display: 'flex', gap: 10 }}>
                  <input className="input" style={{ width: 250 }} placeholder={t.storeSearch} value={storeSearch} onChange={(e) => setStoreSearch(e.target.value)} />
                  <button className="btn" onClick={loadStore} disabled={storeLoading}>{t.refresh}</button>
                </div>
              </div>

              {/* Install from Share Link Bar */}
              <div className="glass-card" style={{ padding: 14, marginBottom: 18, display: 'flex', alignItems: 'center', gap: 10, flexWrap: 'wrap' }}>
                <Link2 size={16} color="#a5b4fc" />
                <input
                  className="input"
                  style={{ flex: 1, minWidth: 240 }}
                  placeholder={lang === 'ar' ? 'الصق رابط مشاركة المود هنا (omnilynx://install?mod=... أو رابط GitHub)' : 'Paste shared mod link here (omnilynx://install?mod=... or GitHub URL)'}
                  value={shareLinkInput}
                  onChange={(e) => setShareLinkInput(e.target.value)}
                />
                <button className="btn btn-primary" onClick={handleInstallFromLink}>
                  {lang === 'ar' ? '⬇️ تثبيت من الرابط' : '⬇️ Install from Link'}
                </button>
              </div>

              {storeError && <div style={{ padding: 12, borderRadius: 10, background: 'rgba(239,68,68,0.14)', color: '#fca5a5', marginBottom: 14 }}>{storeError}</div>}

              {storeLoading ? (
                <p style={{ color: 'var(--text-secondary)' }}>Loading from GitHub...</p>
              ) : (
                <div className="mod-grid">
                  {filteredStore.slice(0, 60).map((item) => {
                    const isInstalled = installedIds.has(item.id);
                    const meta = storeMetaCache[item.id];
                    return (
                      <article key={item.id} className="glass-card" style={{ padding: 16, display: 'flex', flexDirection: 'column', gap: 10 }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', gap: 8 }}>
                          <div>
                            <div style={{ fontSize: 14.5, fontWeight: 700 }}>{meta?.name || item.id}</div>
                            <div style={{ fontSize: 11.5, color: 'var(--text-muted)', marginTop: 2 }}>
                              {meta ? `v${meta.version} · ${meta.author}` : `${item.fileName} · ${(item.size / 1024).toFixed(1)} KB`}
                            </div>
                          </div>
                          {meta?.category && <span className="badge badge-indigo">{meta.category}</span>}
                        </div>

                        {meta?.previewImg && (
                          <div style={{ height: 110, borderRadius: 10, overflow: 'hidden', border: '1px solid var(--border-subtle)', background: '#0b0e14' }}>
                            <img src={meta.previewImg} alt={meta.name} style={{ width: '100%', height: '100%', objectFit: 'cover' }} />
                          </div>
                        )}

                        {meta?.desc && (
                          <p style={{ fontSize: 12.2, color: 'var(--text-secondary)', lineHeight: 1.5, flex: 1 }}>
                            {meta.desc}
                          </p>
                        )}

                        <div style={{ display: 'flex', gap: 8, marginTop: 'auto', paddingTop: 6 }}>
                          <button className="btn" onClick={() => setPreviewStoreItem(item)}>{t.previewBtn}</button>
                          <button className="btn" onClick={() => handleShareMod(item.id)} title={lang === 'ar' ? 'مشاركة' : 'Share'}>
                            <Share2 size={14} />
                          </button>
                          <button
                            className={`btn ${isInstalled ? 'btn-success' : 'btn-primary'}`}
                            style={{ flex: 1 }}
                            disabled={installingId === item.id}
                            onClick={() => handleInstallStoreMod(item)}
                          >
                            {isInstalled ? t.reinstallBtn : installingId === item.id ? '...' : t.installBtn}
                          </button>
                        </div>
                      </article>
                    );
                  })}
                </div>
              )}
            </div>
          )}

          {page === 'settings' && (
            <div style={{ maxWidth: 700, display: 'flex', flexDirection: 'column', gap: 16 }}>
              <h1 style={{ fontSize: 21, fontWeight: 800 }}>{t.settingsTitle}</h1>
              <div className="glass-card" style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 12 }}>
                <div style={{ fontSize: 14, fontWeight: 700 }}>GitHub Mods Repository</div>
                <div style={{ display: 'flex', gap: 10 }}>
                  <input className="input" value={appConfig.modsRepo} onChange={(e) => setAppConfig({ ...appConfig, modsRepo: e.target.value })} />
                  <button className="btn btn-primary" onClick={async () => { await window.wendoAPI.saveAppConfig(appConfig); showToast('Saved!'); }}>Save</button>
                </div>
              </div>
              <div className="glass-card" style={{ padding: 20, display: 'flex', flexDirection: 'column', gap: 10 }}>
                <div style={{ fontSize: 14, fontWeight: 700 }}>Omnilynx Native Engine</div>
                <div style={{ fontSize: 12.5, color: 'var(--text-secondary)', lineHeight: 1.7 }}>
                  <div>Mods: <code>{engineInfo.modsDir}</code></div>
                  <div>DLLs: <code>{engineInfo.compiledDir}</code></div>
                  <div>Compiler: <code>{engineInfo.compilerPath || 'Not found'}</code></div>
                </div>
                <div style={{ display: 'flex', gap: 10, marginTop: 6 }}>
                  <button className="btn" onClick={() => window.wendoAPI.openModsFolder()}><FolderOpen size={14} /> {t.openFolder}</button>
                  <button className="btn" onClick={async () => { await window.wendoAPI.restartExplorer(); showToast('Explorer restarted.'); }}><RotateCcw size={14} /> Restart Explorer</button>
                </div>
              </div>
            </div>
          )}
        </main>
      </div>

      {dragOver && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(99,102,241,0.2)', backdropFilter: 'blur(6px)', border: '3px dashed #818cf8', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 90, pointerEvents: 'none' }}>
          <div style={{ fontSize: 20, fontWeight: 700 }}>Drop .wh.cpp mod file to install into Omnilynx</div>
        </div>
      )}

      {selectedMod && (
        <ModDetailsModal
          mod={selectedMod}
          lang={lang}
          onClose={() => setSelectedMod(null)}
          onSaveSettings={async (id, settings, modObj) => {
            const res = await window.wendoAPI.saveModSettings({ id, settings, origin: modObj?.origin, code: modObj?.code });
            await loadInstalled();
            return res;
          }}
          onEditCode={(m) => { setSelectedMod(null); setEditorState({ id: m.id, code: m.code }); }}
          onDelete={(id, origin) => handleDeleteMod(id, origin)}
          onSwap={handleSwapMod}
          onDuplicate={handleDuplicateMod}
          onShare={handleShareMod}
          onCompileAndInject={handleCompileAndInject}
          onEject={handleEject}
        />
      )}

      <Suspense fallback={null}>
        {editorState && (
          <ModEditorModal
            initialId={editorState.id}
            initialCode={editorState.code}
            onClose={() => setEditorState(null)}
            onSaved={loadInstalled}
          />
        )}
        {previewStoreItem && (
          <StorePreviewModal
            item={previewStoreItem}
            lang={lang}
            isInstalled={installedIds.has(previewStoreItem.id)}
            installing={installingId === previewStoreItem.id}
            onClose={() => setPreviewStoreItem(null)}
            onInstall={handleInstallStoreMod}
            onShare={handleShareMod}
          />
        )}
      </Suspense>

      {toast && (
        <div style={{ position: 'fixed', bottom: 20, insetInlineEnd: 24, background: '#1e293b', border: '1px solid rgba(99,102,241,0.45)', color: '#f8fafc', padding: '10px 16px', borderRadius: 10, fontSize: 13, boxShadow: '0 10px 30px rgba(0,0,0,0.45)', zIndex: 100 }}>
          {toast}
        </div>
      )}
    </div>
  );
}
