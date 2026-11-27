const { app, BrowserWindow, ipcMain, dialog, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const { execFile } = require('child_process');
const https = require('https');

const isDev = !app.isPackaged;
const ROOT_DIR = path.resolve(__dirname, '..');
const INSTALL_DIR = isDev ? ROOT_DIR : path.dirname(process.execPath);
const RESOURCES_DIR = isDev
  ? ROOT_DIR
  : fs.existsSync(process.resourcesPath)
    ? process.resourcesPath
    : path.join(INSTALL_DIR, 'resources');

function resolveDataDir() {
  try {
    fs.accessSync(INSTALL_DIR, fs.constants.W_OK);
    return INSTALL_DIR;
  } catch {
    return path.join(process.env.LOCALAPPDATA || app.getPath('userData'), 'Omnilynx');
  }
}

const DATA_DIR = resolveDataDir();
const ENGINE_BIN = path.join(RESOURCES_DIR, 'engine', 'bin', 'wendo_injector.exe');
const ENGINE_INCLUDE = path.join(RESOURCES_DIR, 'engine', 'include');
const ENGINE_LIB = path.join(RESOURCES_DIR, 'engine', 'lib');
const BUNDLED_MODS_DIR = path.join(RESOURCES_DIR, 'installed_mods');

// Dedicated Omnilynx folders inside the app's own directory
const MODS_SOURCE_DIR = path.join(DATA_DIR, 'Mods');
const MODS_COMPILED_DIR = path.join(DATA_DIR, 'Compiled');
const CONFIG_DIR = path.join(DATA_DIR, 'Config');
const COMPILER_DIR = path.join(DATA_DIR, 'Compiler');
const STATE_FILE = path.join(CONFIG_DIR, 'state.json');
const CONFIG_FILE = path.join(CONFIG_DIR, 'config.json');
const SETTINGS_INI = path.join(CONFIG_DIR, 'mod_settings.ini');

function ensureDirs() {
  for (const dir of [MODS_SOURCE_DIR, MODS_COMPILED_DIR, CONFIG_DIR, COMPILER_DIR]) {
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
  }
  // Seed starter Omnilynx standalone mods into C:\Omnilynx\Mods if missing
  if (fs.existsSync(BUNDLED_MODS_DIR)) {
    for (const file of fs.readdirSync(BUNDLED_MODS_DIR)) {
      if (file.startsWith('omnilynx-') || file.startsWith('wendo-')) {
        if (file.endsWith('.omni.cpp') || file.endsWith('.wh.cpp')) {
          const modId = stripModExt(file);
          const targetOmni = path.join(MODS_SOURCE_DIR, `${modId}.omni.cpp`);
          const targetWh = path.join(MODS_SOURCE_DIR, `${modId}.wh.cpp`);
          if (!fs.existsSync(targetOmni) && !fs.existsSync(targetWh)) {
            fs.copyFileSync(path.join(BUNDLED_MODS_DIR, file), targetOmni);
          }
        }
      }
    }
  }
}

function stripModExt(fileName) {
  return fileName.replace(/\.omni\.cpp$/i, '').replace(/\.wh\.cpp$/i, '').replace(/\.cpp$/i, '');
}

function findModSourceFile(modId) {
  for (const ext of ['.omni.cpp', '.wh.cpp', '.cpp']) {
    const p = path.join(MODS_SOURCE_DIR, `${modId}${ext}`);
    if (fs.existsSync(p)) return p;
  }
  return null;
}

function readJsonSafe(filePath, fallback) {
  try {
    if (fs.existsSync(filePath)) {
      return JSON.parse(fs.readFileSync(filePath, 'utf8'));
    }
  } catch {
    // ignore parse errors
  }
  return fallback;
}

function writeJsonSafe(filePath, data) {
  fs.writeFileSync(filePath, JSON.stringify(data, null, 2), 'utf8');
}

function getErrorLogPath() {
  const dir = path.join(process.env.LOCALAPPDATA || app.getPath('userData'), 'Omnilynx');
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  return path.join(dir, 'error.log');
}

function appendErrorLog(context, detail) {
  try {
    const logPath = getErrorLogPath();
    const ts = new Date().toISOString();
    fs.appendFileSync(logPath, `[${ts}] ${context}: ${detail}\r\n`, 'utf8');
  } catch {
    // ignore logging failure
  }
}

function findCompiler() {
  const candidates = [
    path.join(COMPILER_DIR, 'bin', 'clang++.exe'),
    path.join(COMPILER_DIR, 'bin', 'g++.exe'),
    path.join(RESOURCES_DIR, 'engine', 'compiler', 'bin', 'clang++.exe'),
    path.join(RESOURCES_DIR, 'engine', 'compiler', 'bin', 'g++.exe'),
    'C:\\Program Files\\LLVM\\bin\\clang++.exe',
    'C:\\msys64\\ucrt64\\bin\\g++.exe',
    'C:\\msys64\\mingw64\\bin\\g++.exe'
  ];

  // Also check one level deep inside Compiler dirs (winget zip extracts a subfolder)
  for (const dir of [COMPILER_DIR, 'C:\\Omnilynx\\Compiler']) {
    if (fs.existsSync(dir)) {
      try {
        for (const entry of fs.readdirSync(dir)) {
          const subClang = path.join(dir, entry, 'bin', 'clang++.exe');
          const subGpp = path.join(dir, entry, 'bin', 'g++.exe');
          if (fs.existsSync(subClang)) candidates.unshift(subClang);
          if (fs.existsSync(subGpp)) candidates.unshift(subGpp);
        }
      } catch {
        // ignore
      }
    }
  }

  for (const p of candidates) {
    if (fs.existsSync(p)) return p;
  }
  return null;
}

function httpsGetRaw(url, headers = {}, redirectsLeft = 5) {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { headers: { 'User-Agent': 'Omnilynx-Studio/1.0', ...headers } }, (res) => {
      if ([301, 302, 303, 307, 308].includes(res.statusCode) && res.headers.location && redirectsLeft > 0) {
        res.resume();
        const nextUrl = new URL(res.headers.location, url).toString();
        return httpsGetRaw(nextUrl, headers, redirectsLeft - 1).then(resolve, reject);
      }
      let data = '';
      res.on('data', (chunk) => (data += chunk));
      res.on('end', () => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(data);
        } else {
          reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 200)}`));
        }
      });
    });
    req.on('error', reject);
  });
}

async function httpsGetJson(url) {
  const data = await httpsGetRaw(url, { Accept: 'application/vnd.github.v3+json' });
  try {
    return JSON.parse(data);
  } catch {
    throw new Error('Invalid JSON response');
  }
}

function httpsGetText(url) {
  return httpsGetRaw(url);
}

function runExecFile(exe, args) {
  return new Promise((resolve) => {
    execFile(exe, args, { windowsHide: true, maxBuffer: 4 * 1024 * 1024 }, (error, stdout, stderr) => {
      resolve({
        ok: !error,
        code: error ? error.code : 0,
        stdout: stdout || '',
        stderr: stderr || (error ? error.message : '')
      });
    });
  });
}

function extractTargetRulesFromCode(code) {
  const includes = [];
  const excludes = new Set();
  let hasWildcard = false;
  const incRegex = /\/\/\s*@include\s+(.*)/g;
  const excRegex = /\/\/\s*@exclude\s+(.*)/g;
  let m;
  while ((m = incRegex.exec(code)) !== null) {
    const val = m[1].trim();
    if (val === '*') hasWildcard = true;
    else if (val) includes.push(path.basename(val).toLowerCase());
  }
  while ((m = excRegex.exec(code)) !== null) {
    const val = m[1].trim();
    if (val) excludes.add(path.basename(val).toLowerCase());
  }
  if (includes.length > 0) {
    return { includes: includes.filter((t) => !excludes.has(t)), excludes, hasWildcard: false };
  }
  if (hasWildcard) {
    const defaults = ['explorer.exe', 'notepad.exe', 'chrome.exe', 'msedge.exe', 'windowsterminal.exe', 'code.exe']
      .filter((t) => !excludes.has(t));
    return { includes: defaults, excludes, hasWildcard: true };
  }
  return { includes: ['explorer.exe'], excludes, hasWildcard: false };
}

function extractTargetsFromCode(code) {
  return extractTargetRulesFromCode(code).includes;
}

function extractMetaField(code, key, fallback = '') {
  const m = code.match(new RegExp(`//\\s*@${key}\\s+(.*)`));
  return m ? m[1].trim() : fallback;
}

function writeSettingsIni(state, latestSettings = null) {
  const merged = {};
  for (const mData of Object.values(state.mods || {})) {
    if (mData?.settings && typeof mData.settings === 'object') {
      for (const [k, v] of Object.entries(mData.settings)) {
        if (v !== null && typeof v !== 'object') {
          merged[k] = typeof v === 'boolean' ? (v ? 1 : 0) : v;
        }
      }
    }
  }
  if (latestSettings && typeof latestSettings === 'object') {
    for (const [k, v] of Object.entries(latestSettings)) {
      if (v !== null && typeof v !== 'object') {
        merged[k] = typeof v === 'boolean' ? (v ? 1 : 0) : v;
      }
    }
  }
  const lines = ['[Settings]'];
  for (const [k, v] of Object.entries(merged)) {
    lines.push(`${k}=${String(v)}`);
  }
  fs.writeFileSync(SETTINGS_INI, '\ufeff' + lines.join('\r\n'), 'utf16le');
}

async function compileModInternal(modId, code, compilerOptions = '') {
  const compiler = findCompiler();
  if (!compiler) {
    return {
      ok: false,
      stderr: 'لم يتم العثور على المترجم — افتح الإعدادات واضغط على «تثبيت المترجم تلقائياً».'
    };
  }

  const safeId = modId.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
  const existingPath = findModSourceFile(safeId);
  const srcPath = existingPath || path.join(MODS_SOURCE_DIR, `${safeId}.omni.cpp`);
  const dllPath = path.join(MODS_COMPILED_DIR, `${safeId}.dll`);

  fs.writeFileSync(srcPath, code, 'utf8');
  const needsSdkAutoInclude =
    !code.includes('wendo_mod.h') &&
    !code.includes('omnilynx_mod_sdk.hpp') &&
    !code.includes('omnilynx_mod_iface.hpp');

  const resolvedOptions = compilerOptions || extractMetaField(code, 'compilerOptions', '');
  const extraFlags = resolvedOptions
    .split(/\s+/)
    .filter((f) => f && f.startsWith('-') && f !== '-lwindowsapp');

  // Auto-detect Windows SDK cppwinrt include path if installed on the system
  const winrtIncludeFlags = [];
  const kitsBase = 'C:\\Program Files (x86)\\Windows Kits\\10\\Include';
  if (fs.existsSync(kitsBase)) {
    try {
      const vers = fs.readdirSync(kitsBase).filter((d) => /^\d+\./.test(d)).sort().reverse();
      for (const v of vers) {
        const cppwinrt = path.join(kitsBase, v, 'cppwinrt');
        if (fs.existsSync(cppwinrt)) {
          winrtIncludeFlags.push(`-I${cppwinrt}`);
          break;
        }
      }
    } catch {
      // ignore
    }
  }

  const hasCustomDllMain = /\bDllMain\s*\(/.test(code);

  const escapedIni = SETTINGS_INI.replace(/\\/g, '\\\\');
  const args = [
    '--target=x86_64-w64-windows-gnu',
    '-std=c++20',
    '-fexceptions',
    '-D_WIN32_WINNT=0x0A00',
    '-DWINVER=0x0A00',
    '-DWINRT_LEAN_AND_MEAN',
    '-DUNICODE',
    '-D_UNICODE',
    `-DWH_MOD_ID=L"${safeId}"`,
    `-DWENDO_SETTINGS_INI=L"${escapedIni}"`,
    ...(hasCustomDllMain ? ['-DWENDO_NO_DLLMAIN'] : []),
    '-shared',
    '-static',
    '-static-libgcc',
    '-static-libstdc++',
    '-O2',
    `-I${ENGINE_INCLUDE}`,
    ...winrtIncludeFlags,
    ...(needsSdkAutoInclude ? ['-include', path.join(ENGINE_INCLUDE, 'omnilynx_mod_sdk.hpp')] : []),
    srcPath,
    '-o',
    dllPath,
    `-L${ENGINE_LIB}`,
    '-lminhook',
    '-lkernel32',
    '-luser32',
    '-lgdi32',
    '-ldwmapi',
    '-lshell32',
    '-lshcore',
    '-lcomctl32',
    '-lshlwapi',
    '-luxtheme',
    '-lole32',
    '-loleaut32',
    '-ladvapi32',
    '-lpsapi',
    '-lversion',
    '-ldbghelp',
    '-lruntimeobject',
    '-ldxguid',
    '-luuid',
    ...extraFlags
  ];

  const res = await runExecFile(compiler, args);
  return {
    ...res,
    dllPath: res.ok ? dllPath : null,
    compiler
  };
}

// Low-RAM optimizations
app.commandLine.appendSwitch('renderer-process-limit', '1');
app.commandLine.appendSwitch('disable-background-networking');
app.commandLine.appendSwitch('disable-features', 'Spellcheck,AutofillServerCommunication');

const translationCache = new Map();

async function translateTextCached(text, targetLang = 'ar') {
  if (!text || text.length < 3) return text;
  const key = `${targetLang}:${text.slice(0, 120)}:${text.length}`;
  if (translationCache.has(key)) return translationCache.get(key);

  try {
    const images = [];
    const masked = text.replace(/!\[[^\]]*\]\([^)]+\)/g, (match) => {
      images.push(match);
      return `__IMG_${images.length - 1}__`;
    });

    const chunk = masked.slice(0, 1800);
    const url = `https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=${targetLang}&dt=t&q=${encodeURIComponent(chunk)}`;
    const json = await httpsGetJson(url);
    let translated = Array.isArray(json?.[0]) ? json[0].map((seg) => seg[0]).join('') : text;

    images.forEach((imgTag, idx) => {
      translated = translated.replace(`__IMG_${idx}__`, imgTag);
    });

    translationCache.set(key, translated);
    return translated;
  } catch {
    return text;
  }
}

function createWindow() {
  const icoPath = path.join(ROOT_DIR, 'public', 'omnilynx-logo.ico');
  const pngPath = path.join(ROOT_DIR, 'public', 'omnilynx-logo.png');
  const iconPath = fs.existsSync(icoPath) ? icoPath : fs.existsSync(pngPath) ? pngPath : undefined;
  const win = new BrowserWindow({
    width: 1360,
    height: 860,
    minWidth: 1020,
    minHeight: 680,
    title: 'Omnilynx',
    icon: iconPath,
    backgroundColor: '#060911',
    titleBarStyle: 'hidden',
    titleBarOverlay: {
      color: '#060911',
      symbolColor: '#94a3b8',
      height: 44
    },
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });

  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    win.loadURL(process.env.VITE_DEV_SERVER_URL);
  } else if (isDev && !fs.existsSync(path.join(ROOT_DIR, 'dist', 'index.html'))) {
    win.loadURL('http://localhost:5173');
  } else {
    win.loadFile(path.join(ROOT_DIR, 'dist', 'index.html'));
  }
}

app.setAppUserModelId('com.omnilynx.studio');

app.whenReady().then(() => {
  ensureDirs();

  // IPC: List installed Omnilynx mods from C:\Omnilynx\Mods
  ipcMain.handle('mods:listInstalled', async () => {
    const state = readJsonSafe(STATE_FILE, { mods: {} });
    const mods = [];
    if (!fs.existsSync(MODS_SOURCE_DIR)) return mods;

    const files = fs
      .readdirSync(MODS_SOURCE_DIR)
      .filter((f) => f.endsWith('.omni.cpp') || f.endsWith('.wh.cpp') || f.endsWith('.cpp'));

    for (const file of files) {
      const fullPath = path.join(MODS_SOURCE_DIR, file);
      const modId = stripModExt(file);
      const code = fs.readFileSync(fullPath, 'utf8');
      const compiledDll = path.join(MODS_COMPILED_DIR, `${modId}.dll`);
      const localState = state.mods[modId] || {};

      mods.push({
        id: modId,
        fileName: file,
        filePath: fullPath,
        origin: 'omnilynx',
        code,
        compiled: fs.existsSync(compiledDll),
        dllPath: compiledDll,
        enabled: Boolean(localState.enabled),
        settings: localState.settings || {}
      });
    }

    return mods;
  });

  ipcMain.handle('mods:saveSource', async (_, { id, code }) => {
    const safeId = id.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
    const filePath = findModSourceFile(safeId) || path.join(MODS_SOURCE_DIR, `${safeId}.omni.cpp`);
    fs.writeFileSync(filePath, code, 'utf8');
    return { ok: true, id: safeId, filePath };
  });

  ipcMain.handle('mods:delete', async (_, payload) => {
    const modId = typeof payload === 'string' ? payload : payload.id;
    const filePath = findModSourceFile(modId);
    const dllPath = path.join(MODS_COMPILED_DIR, `${modId}.dll`);

    if (filePath && fs.existsSync(ENGINE_BIN)) {
      try {
        const code = fs.readFileSync(filePath, 'utf8');
        const targets = extractTargetsFromCode(code);
        for (const target of targets) {
          await runExecFile(ENGINE_BIN, ['eject', target, `${modId}.dll`]);
        }
      } catch {
        // best-effort eject
      }
    }

    try { if (filePath && fs.existsSync(filePath)) fs.unlinkSync(filePath); } catch { /* ignore */ }
    try { if (fs.existsSync(dllPath)) fs.unlinkSync(dllPath); } catch { /* ignore */ }

    const state = readJsonSafe(STATE_FILE, { mods: {} });
    delete state.mods[modId];
    writeJsonSafe(STATE_FILE, state);
    return { ok: true, id: modId };
  });

  // Standalone Toggle: uses external Win32/DWM execution for window/system mods (zero Win32:5 on UWP/Win11), or native DLL injection for custom C++ hooks
  const EXTERNAL_MOD_IDS = new Set([
    'win-glass-window-opacity', 'wendo-window-opacity',
    'win-mica-acrylic-backdrop', 'wendo-translucent-mica',
    'win-universal-dark-mode', 'wendo-dark-titlebars',
    'win-smooth-window-transitions', 'omnilynx-rounded-corners',
    'omnilynx-always-on-top', 'omnilynx-border-accent',
    'win-switcher-backdrop', 'win-active-window-expose',
    'win-taskbar-floating-style', 'win-taskbar-acrylic-tint',
    'win-taskbar-zorder-control', 'win-taskbar-autohide-toggle',
    'win-alttab-focus-guard', 'win-cursor-follow-focus',
    'win-monitor-snap-focus', 'win-taskbar-stability-guard',
    'win-system-ram-trimmer'
  ]);

  function extractFirstNumericSetting(modSettings) {
    if (!modSettings || typeof modSettings !== 'object') return '0';
    for (const v of Object.values(modSettings)) {
      if (typeof v === 'number') return String(v);
      if (typeof v === 'boolean') return v ? '1' : '0';
    }
    return '0';
  }

  ipcMain.handle('mods:toggle', async (_, { id, enabled, code, compilerOptions }) => {
    const state = readJsonSafe(STATE_FILE, { mods: {} });
    const safeId = id.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
    const srcPath = findModSourceFile(safeId) || path.join(MODS_SOURCE_DIR, `${safeId}.omni.cpp`);
    const resolvedCode = (code && code.trim()) ? code : (fs.existsSync(srcPath) ? fs.readFileSync(srcPath, 'utf8') : '');
    const dllPath = path.join(MODS_COMPILED_DIR, `${safeId}.dll`);
    const targets = extractTargetsFromCode(resolvedCode);

    if (!fs.existsSync(ENGINE_BIN)) {
      const msg = `Native engine missing: ${ENGINE_BIN}`;
      appendErrorLog('mods:toggle', msg);
      dialog.showErrorBox('Omnilynx Engine Error', `${msg}\n\nLog: ${getErrorLogPath()}`);
      return { ok: false, enabled: false, message: msg };
    }

    // Fast, zero-Win32:5 external Win32/DWM execution for window-styling & system mods (silent, never opens windows)
    if (EXTERNAL_MOD_IDS.has(safeId)) {
      const paramVal = extractFirstNumericSetting(state.mods[safeId]?.settings);
      let appliedAny = false;
      let waitingForTarget = false;
      let lastErr = '';
      for (const target of targets) {
        const res = await runExecFile(ENGINE_BIN, ['apply', safeId, target, enabled ? 'on' : 'off', paramVal]);
        try {
          const parsed = JSON.parse(res.stdout);
          if (parsed.ok) appliedAny = true;
          else if (parsed.error === 'Target process has no visible windows') waitingForTarget = true;
          else lastErr = parsed.win32Message ? `${parsed.error} [Win32: ${parsed.win32Message}]` : (parsed.error || 'Apply failed');
        } catch {
          lastErr = res.stderr || res.stdout || 'External apply failed';
        }
      }

      if (!appliedAny && !waitingForTarget && enabled) {
        const errMsg = `Failed to apply ${safeId} to ${targets.join(', ')}: ${lastErr}`;
        appendErrorLog('mods:toggle(external)', errMsg);
        dialog.showErrorBox('Omnilynx Execution Error', `${errMsg}\n\nLog: ${getErrorLogPath()}`);
        return { ok: false, enabled: false, message: errMsg };
      }

      state.mods[safeId] = { ...(state.mods[safeId] || {}), enabled: Boolean(enabled) };
      writeJsonSafe(STATE_FILE, state);
      return {
        ok: true,
        enabled: Boolean(enabled),
        injected: appliedAny,
        message: enabled
          ? (appliedAny
              ? `✅ تم التطبيق في الخلفية على ${targets.join(', ')}`
              : `⏳ مفعّل في الخلفية — سيتصل تلقائياً عند فتح ${targets.join(', ')}`)
          : `Disabled & restored ${targets.join(', ')}`
      };
    }

    if (enabled) {
      writeSettingsIni(state);
      const headerPath = path.join(ENGINE_INCLUDE, 'omnilynx_mod_sdk.hpp');
      const dllMtime = fs.existsSync(dllPath) ? fs.statSync(dllPath).mtimeMs : 0;
      const needsCompile =
        !fs.existsSync(dllPath) ||
        (fs.existsSync(srcPath) && fs.statSync(srcPath).mtimeMs > dllMtime) ||
        (fs.existsSync(headerPath) && fs.statSync(headerPath).mtimeMs > dllMtime);
      if (needsCompile) {
        const comp = await compileModInternal(safeId, resolvedCode, compilerOptions);
        if (!comp.ok) {
          const msg = `Compile failed for ${safeId}: ${comp.stderr}`;
          appendErrorLog('engine:compile', msg);
          dialog.showErrorBox('Omnilynx Compile Error', `${msg}\n\nLog: ${getErrorLogPath()}`);
          return { ok: false, enabled: false, message: msg };
        }
      }

      let injectedAny = false;
      let waitingForTarget = false;
      let lastErr = '';
      let lastWin32Err = 0;

      for (const target of targets) {
        const res = await runExecFile(ENGINE_BIN, ['inject', target, dllPath]);
        let parsed = null;
        try {
          parsed = JSON.parse(res.stdout);
        } catch {
          parsed = null;
        }

        if (parsed && parsed.ok) {
          injectedAny = true;
        } else if (parsed && parsed.error === 'Target process not found') {
          waitingForTarget = true;
        } else if (parsed) {
          lastErr = parsed.win32Message ? `${parsed.error} [Win32: ${parsed.win32Message}]` : (parsed.error || 'Injection failed');
          lastWin32Err = parsed.win32Error || 0;
        } else {
          lastErr = res.stderr || res.stdout || 'Injector failed to execute';
        }
      }

      if (!injectedAny && !waitingForTarget) {
        const errMsg = `Failed to inject ${safeId} into ${targets.join(', ')}: ${lastErr} (Win32=${lastWin32Err})`;
        appendErrorLog('mods:toggle', errMsg);
        dialog.showErrorBox('Omnilynx Execution Error', `${errMsg}\n\nLog: ${getErrorLogPath()}`);
        return {
          ok: false,
          enabled: false,
          message: errMsg
        };
      }

      state.mods[safeId] = { ...(state.mods[safeId] || {}), enabled: true };
      writeJsonSafe(STATE_FILE, state);

      return {
        ok: true,
        enabled: true,
        injected: injectedAny,
        message: injectedAny
          ? `✅ تم التشغيل والحقن في الخلفية (${targets.join(', ')})`
          : `⏳ مفعّل في الخلفية — بانتظار فتح ${targets.join(', ')} للحقن التلقائي`
      };
    } else {
      for (const target of targets) {
        await runExecFile(ENGINE_BIN, ['eject', target, `${safeId}.dll`]);
      }
      state.mods[safeId] = { ...(state.mods[safeId] || {}), enabled: false };
      writeJsonSafe(STATE_FILE, state);
      return {
        ok: true,
        enabled: false,
        message: `Disabled & ejected from ${targets.join(', ')}`
      };
    }
  });

  ipcMain.handle('mods:saveSettings', async (_, { id, settings, code }) => {
    const safeId = id.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
    const state = readJsonSafe(STATE_FILE, { mods: {} });
    state.mods[safeId] = { ...(state.mods[safeId] || {}), settings };
    writeJsonSafe(STATE_FILE, state);
    writeSettingsIni(state, settings);

    if (state.mods[safeId]?.enabled) {
      const targets = extractTargetsFromCode(code || '');
      if (EXTERNAL_MOD_IDS.has(safeId)) {
        const paramVal = extractFirstNumericSetting(settings);
        for (const target of targets) {
          await runExecFile(ENGINE_BIN, ['apply', safeId, target, 'on', paramVal]);
        }
      } else {
        const dllPath = path.join(MODS_COMPILED_DIR, `${safeId}.dll`);
        if (!fs.existsSync(dllPath) && code) {
          await compileModInternal(safeId, code, '');
        }
        if (fs.existsSync(dllPath)) {
          for (const target of targets) {
            await runExecFile(ENGINE_BIN, ['eject', target, `${safeId}.dll`]);
          }
          await new Promise((r) => setTimeout(r, 150));
          for (const target of targets) {
            await runExecFile(ENGINE_BIN, ['inject', target, dllPath]);
          }
        }
      }
    }

    return { ok: true, message: 'Settings saved & applied immediately!' };
  });

  ipcMain.handle('mods:importFile', async () => {
    const result = await dialog.showOpenDialog({
      title: 'Import Omnilynx Mod File (.omni.cpp / .wh.cpp / .cpp)',
      filters: [{ name: 'C++ Mod Files', extensions: ['cpp'] }],
      properties: ['openFile']
    });
    if (result.canceled || !result.filePaths.length) return { canceled: true };
    const src = result.filePaths[0];
    const modId = stripModExt(path.basename(src)).replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
    const dest = path.join(MODS_SOURCE_DIR, `${modId}.omni.cpp`);
    fs.copyFileSync(src, dest);
    return { canceled: false, id: modId, filePath: dest };
  });

  ipcMain.handle('mods:importContent', async (_, { fileName, content }) => {
    const modId = stripModExt(fileName).replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
    const dest = path.join(MODS_SOURCE_DIR, `${modId}.omni.cpp`);
    fs.writeFileSync(dest, content, 'utf8');
    return { ok: true, id: modId, filePath: dest };
  });

  ipcMain.handle('mods:swapFile', async (_, modId) => {
    const result = await dialog.showOpenDialog({
      title: `Replace ${modId} with another mod file`,
      filters: [{ name: 'C++ Mod Files', extensions: ['cpp'] }],
      properties: ['openFile']
    });
    if (result.canceled || !result.filePaths.length) return { canceled: true };
    const dest = findModSourceFile(modId) || path.join(MODS_SOURCE_DIR, `${modId}.omni.cpp`);
    fs.copyFileSync(result.filePaths[0], dest);
    return { canceled: false, id: modId, filePath: dest };
  });

  ipcMain.handle('mods:duplicate', async (_, modId) => {
    const src = findModSourceFile(modId);
    if (!src) return { ok: false, error: 'Source file not found' };
    const newId = `${modId}-custom`;
    const dest = path.join(MODS_SOURCE_DIR, `${newId}.omni.cpp`);
    fs.copyFileSync(src, dest);
    return { ok: true, id: newId, filePath: dest };
  });

  ipcMain.handle('mods:openFolder', async () => {
    await shell.openPath(MODS_SOURCE_DIR);
    return { ok: true, path: MODS_SOURCE_DIR };
  });

  // IPC: Compile C++ mod into native x64 DLL
  ipcMain.handle('engine:compile', async (_, { id, code, compilerOptions }) => {
    return await compileModInternal(id, code, compilerOptions);
  });

  // IPC: Install standalone LLVM-MinGW compiler into app's Compiler folder
  ipcMain.handle('engine:installCompiler', async () => {
    if (!fs.existsSync(COMPILER_DIR)) {
      fs.mkdirSync(COMPILER_DIR, { recursive: true });
    }
    await runExecFile('winget', ['uninstall', '--id', 'MartinStorsjo.LLVM-MinGW.UCRT', '--silent']);
    const res = await runExecFile('winget', [
      'install',
      '--id',
      'MartinStorsjo.LLVM-MinGW.UCRT',
      '--exact',
      '--force',
      '--location',
      COMPILER_DIR,
      '--accept-package-agreements',
      '--accept-source-agreements',
      '--silent'
    ]);
    const compilerPath = findCompiler();
    return {
      ok: Boolean(compilerPath) || res.ok,
      compilerPath,
      message: compilerPath ? `Compiler installed at ${compilerPath}` : res.stderr || res.stdout
    };
  });

  // IPC: Inject / Eject via native wendo_injector.exe
  ipcMain.handle('engine:inject', async (_, { target, dllPath }) => {
    if (!fs.existsSync(ENGINE_BIN)) return { ok: false, error: 'wendo_injector.exe not found' };
    const res = await runExecFile(ENGINE_BIN, ['inject', target, dllPath]);
    try {
      return JSON.parse(res.stdout);
    } catch {
      return { ok: false, error: res.stderr || res.stdout || 'Injector failed' };
    }
  });

  ipcMain.handle('engine:eject', async (_, { target, dllName }) => {
    if (!fs.existsSync(ENGINE_BIN)) return { ok: false, error: 'wendo_injector.exe not found' };
    const res = await runExecFile(ENGINE_BIN, ['eject', target, dllName]);
    try {
      return JSON.parse(res.stdout);
    } catch {
      return { ok: false, error: res.stderr || res.stdout || 'Ejector failed' };
    }
  });

  ipcMain.handle('engine:listProcesses', async () => {
    if (!fs.existsSync(ENGINE_BIN)) return { ok: false, processes: [] };
    const res = await runExecFile(ENGINE_BIN, ['list']);
    try {
      return JSON.parse(res.stdout);
    } catch {
      return { ok: false, processes: [] };
    }
  });

  ipcMain.handle('engine:info', async () => ({
    compilerPath: findCompiler(),
    injectorPath: fs.existsSync(ENGINE_BIN) ? ENGINE_BIN : null,
    installDir: INSTALL_DIR,
    resourcesDir: RESOURCES_DIR,
    dataDir: DATA_DIR,
    modsDir: MODS_SOURCE_DIR,
    compiledDir: MODS_COMPILED_DIR,
    configDir: CONFIG_DIR,
    compilerDir: COMPILER_DIR
  }));

  // IPC: Explore Store (GitHub — prefers verified standalone omnilynx-store.json)
  ipcMain.handle('store:fetchIndex', async (_, customRepo) => {
    const repo = customRepo || 'PyDevMedo/Omnilynx';
    try {
      const curated = await httpsGetJson(`https://raw.githubusercontent.com/${repo}/main/mods/omnilynx-store.json`);
      if (Array.isArray(curated) && curated.length > 0) {
        return curated;
      }
    } catch {
      // fall back to GitHub contents API for custom repos
    }
    const apiUrl = `https://api.github.com/repos/${repo}/contents/mods`;
    const items = await httpsGetJson(apiUrl);
    return items
      .filter((item) => item.name.endsWith('.omni.cpp') || item.name.endsWith('.wh.cpp'))
      .map((item) => ({
        id: stripModExt(item.name),
        fileName: item.name,
        downloadUrl: item.download_url,
        htmlUrl: item.html_url,
        size: item.size
      }));
  });

  ipcMain.handle('previews:cache', async (_, { id, url }) => {
    try {
      const safeId = String(id || '').replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
      if (!safeId || !url || !/^https:\/\//i.test(url)) return { ok: false };
      const cacheDir = path.join(process.env.LOCALAPPDATA || app.getPath('userData'), 'Omnilynx', 'cache', 'previews');
      if (!fs.existsSync(cacheDir)) fs.mkdirSync(cacheDir, { recursive: true });

      const cleanPath = url.split('?')[0].toLowerCase();
      const extMatch = cleanPath.match(/\.(png|jpe?g|gif|webp|svg)$/);
      const ext = extMatch ? `.${extMatch[1] === 'jpeg' ? 'jpg' : extMatch[1]}` : '.png';
      const mimeMap = { '.png': 'image/png', '.jpg': 'image/jpeg', '.gif': 'image/gif', '.webp': 'image/webp', '.svg': 'image/svg+xml' };
      const mime = mimeMap[ext] || 'image/png';

      const localFile = path.join(cacheDir, `${safeId}${ext}`);
      if (fs.existsSync(localFile)) {
        const content = fs.readFileSync(localFile);
        return { ok: true, dataUrl: `data:${mime};base64,${content.toString('base64')}` };
      }

      const buf = await new Promise((resolve, reject) => {
        const fetchBin = (targetUrl, left = 4) => {
          const req = https.get(targetUrl, { headers: { 'User-Agent': 'Omnilynx-Studio/1.0' } }, (res) => {
            if ([301, 302, 303, 307, 308].includes(res.statusCode) && res.headers.location && left > 0) {
              res.resume();
              return fetchBin(new URL(res.headers.location, targetUrl).toString(), left - 1);
            }
            if (res.statusCode < 200 || res.statusCode >= 300) {
              res.resume();
              return reject(new Error(`HTTP ${res.statusCode}`));
            }
            const chunks = [];
            res.on('data', (c) => chunks.push(c));
            res.on('end', () => resolve(Buffer.concat(chunks)));
          });
          req.on('error', reject);
        };
        fetchBin(url);
      });

      fs.writeFileSync(localFile, buf);
      return { ok: true, dataUrl: `data:${mime};base64,${buf.toString('base64')}` };
    } catch {
      return { ok: false };
    }
  });

  ipcMain.handle('store:fetchModSource', async (_, { downloadUrl }) => {
    const rawUrl = String(downloadUrl || '')
      .replace('https://github.com/', 'https://raw.githubusercontent.com/')
      .replace('/blob/', '/')
      .replace('/raw/', '/');
    const text = await httpsGetText(rawUrl);
    if (/^\s*<!doctype\s+html|^\s*<html/i.test(text)) {
      appendErrorLog('store:fetchModSource', `Received HTML instead of mod source from ${rawUrl}`);
      throw new Error('Downloaded HTML page instead of raw C++ mod source');
    }
    return text;
  });

  ipcMain.handle('mods:installFromLink', async (_, rawLink) => {
    const link = String(rawLink || '').trim();
    if (!link) return { ok: false, error: 'Empty link' };

    let modId = '';
    let downloadUrl = '';

    if (link.startsWith('omnilynx://')) {
      try {
        const u = new URL(link);
        modId = u.searchParams.get('mod') || '';
        const repo = u.searchParams.get('repo') || 'PyDevMedo/Omnilynx';
        if (modId) {
          downloadUrl = `https://raw.githubusercontent.com/${repo}/main/mods/${modId}.wh.cpp`;
        }
      } catch {
        return { ok: false, error: 'Invalid omnilynx:// link' };
      }
    } else if (link.startsWith('http://') || link.startsWith('https://')) {
      downloadUrl = link
        .replace('https://github.com/', 'https://raw.githubusercontent.com/')
        .replace('/blob/', '/')
        .replace('/raw/', '/');
      const parts = downloadUrl.split('/');
      const last = parts[parts.length - 1] || 'shared-mod.omni.cpp';
      modId = stripModExt(last);
    } else {
      modId = link.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
      downloadUrl = `https://raw.githubusercontent.com/PyDevMedo/Omnilynx/main/mods/${modId}.wh.cpp`;
    }

    if (!modId || !downloadUrl) return { ok: false, error: 'Could not parse mod link' };

    try {
      const code = await httpsGetText(downloadUrl);
      if (/^\s*<!doctype\s+html|^\s*<html/i.test(code)) {
        throw new Error('URL returned HTML instead of raw C++ source');
      }
      const safeId = modId.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
      const dest = path.join(MODS_SOURCE_DIR, `${safeId}.omni.cpp`);
      fs.writeFileSync(dest, code, 'utf8');
      return { ok: true, id: safeId, filePath: dest };
    } catch (e) {
      appendErrorLog('mods:installFromLink', `${downloadUrl}: ${e.message}`);
      return { ok: false, error: `Download failed: ${e.message}` };
    }
  });

  ipcMain.handle('app:translateText', async (_, { text, targetLang }) => {
    return await translateTextCached(text, targetLang || 'ar');
  });

  // IPC: Windows Quick Tweaks (direct HKCU registry + Explorer broadcast)
  ipcMain.handle('tweaks:getAll', async () => {
    const ps = `
      $adv = Get-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced' -ErrorAction SilentlyContinue
      $pers = Get-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize' -ErrorAction SilentlyContinue
      @{
        taskbarCenter = ($adv.TaskbarAl -ne 0)
        darkMode = ($pers.AppsUseLightTheme -eq 0)
        transparency = ($pers.EnableTransparency -eq 1)
        clockSeconds = ($adv.ShowSecondsInSystemClock -eq 1)
        fileExtensions = ($adv.HideFileExt -eq 0)
        compactMode = ($adv.UseCompactMode -eq 1)
      } | ConvertTo-Json
    `;
    const res = await runExecFile('powershell.exe', ['-NoProfile', '-Command', ps]);
    try {
      return JSON.parse(res.stdout);
    } catch {
      return {};
    }
  });

  ipcMain.handle('tweaks:apply', async (_, { key, value }) => {
    const commands = {
      taskbarCenter: `Set-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced' -Name 'TaskbarAl' -Value ${value ? 1 : 0} -Type DWord`,
      darkMode: `Set-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize' -Name 'AppsUseLightTheme' -Value ${value ? 0 : 1} -Type DWord; Set-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize' -Name 'SystemUsesLightTheme' -Value ${value ? 0 : 1} -Type DWord`,
      transparency: `Set-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize' -Name 'EnableTransparency' -Value ${value ? 1 : 0} -Type DWord`,
      clockSeconds: `Set-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced' -Name 'ShowSecondsInSystemClock' -Value ${value ? 1 : 0} -Type DWord`,
      fileExtensions: `Set-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced' -Name 'HideFileExt' -Value ${value ? 0 : 1} -Type DWord`,
      compactMode: `Set-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced' -Name 'UseCompactMode' -Value ${value ? 1 : 0} -Type DWord`
    };
    const cmd = commands[key];
    if (!cmd) return { ok: false, error: 'Unknown tweak' };

    const broadcast = `
      ${cmd}
      Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class Win32 { [DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Auto)] public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out UIntPtr lpdwResult); }'
      $r = [UIntPtr]::Zero
      [Win32]::SendMessageTimeout([IntPtr]0xffff, 0x001A, [UIntPtr]::Zero, 'ImmersiveColorSet', 2, 1000, [ref]$r) | Out-Null
    `;
    const res = await runExecFile('powershell.exe', ['-NoProfile', '-Command', broadcast]);
    return { ok: res.ok, message: res.ok ? 'Applied to Windows immediately!' : res.stderr };
  });

  // IPC: Config & Utilities
  ipcMain.handle('app:getConfig', async () =>
    readJsonSafe(CONFIG_FILE, {
      modsRepo: 'PyDevMedo/Omnilynx',
      themeAccent: 'indigo',
      autoInjectOnStart: false
    })
  );

  ipcMain.handle('app:saveConfig', async (_, config) => {
    writeJsonSafe(CONFIG_FILE, config);
    return { ok: true };
  });

  ipcMain.handle('app:openExternal', async (_, url) => {
    await shell.openExternal(url);
    return { ok: true };
  });

  ipcMain.handle('app:restartExplorer', async () => {
    return await runExecFile('powershell.exe', [
      '-NoProfile',
      '-Command',
      'Stop-Process -Name explorer -Force; Start-Sleep -Milliseconds 700; if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }'
    ]);
  });

  createWindow();

  // Lightweight auto-injector: only injects NEW target PIDs (zero repeated spawns / zero lag)
  const injectedPidsByMod = new Map();
  let watcherBusy = false;
  setInterval(async () => {
    if (watcherBusy || !fs.existsSync(ENGINE_BIN)) return;
    watcherBusy = true;
    try {
      const state = readJsonSafe(STATE_FILE, { mods: {} });
      const enabledIds = Object.entries(state.mods || {})
        .filter(([, v]) => v && v.enabled)
        .map(([k]) => k);
      if (enabledIds.length === 0) {
        injectedPidsByMod.clear();
        return;
      }

      const listRes = await runExecFile(ENGINE_BIN, ['list']);
      let processes = [];
      try {
        const parsed = JSON.parse(listRes.stdout);
        if (parsed.ok && Array.isArray(parsed.processes)) {
          processes = parsed.processes;
        }
      } catch {
        return;
      }

      const alivePids = new Set(processes.map((p) => p.pid));
      for (const modId of enabledIds) {
        const isExt = EXTERNAL_MOD_IDS.has(modId);
        const dllPath = path.join(MODS_COMPILED_DIR, `${modId}.dll`);
        const srcPath = findModSourceFile(modId);
        if (!srcPath || (!isExt && !fs.existsSync(dllPath))) continue;

        let seen = injectedPidsByMod.get(modId);
        if (!seen) {
          seen = new Set();
          injectedPidsByMod.set(modId, seen);
        }
        for (const pid of [...seen]) {
          if (!alivePids.has(pid)) seen.delete(pid);
        }

        const code = fs.readFileSync(srcPath, 'utf8');
        const { includes, excludes } = extractTargetRulesFromCode(code);
        const targetSet = new Set(includes.map((t) => t.toLowerCase()));
        for (const proc of processes) {
          const procName = String(proc.name || '').toLowerCase();
          if (proc.pid > 4 && targetSet.has(procName) && !excludes.has(procName) && !seen.has(proc.pid)) {
            if (isExt) {
              const paramVal = extractFirstNumericSetting(state.mods[modId]?.settings);
              const res = await runExecFile(ENGINE_BIN, ['apply', modId, procName, 'on', paramVal]);
              try {
                const p = JSON.parse(res.stdout);
                if (p.ok) seen.add(proc.pid);
              } catch {
                // ignore
              }
            } else {
              const res = await runExecFile(ENGINE_BIN, ['inject', String(proc.pid), dllPath]);
              try {
                const p = JSON.parse(res.stdout);
                if (p.ok) seen.add(proc.pid);
              } catch {
                // ignore
              }
            }
          }
        }
      }
    } catch {
      // ignore background watcher errors
    } finally {
      watcherBusy = false;
    }
  }, 5000);
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
