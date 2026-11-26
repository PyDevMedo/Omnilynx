const { app, BrowserWindow, ipcMain, dialog, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const { execFile } = require('child_process');
const https = require('https');

const isDev = !app.isPackaged;
const ROOT_DIR = path.resolve(__dirname, '..');
const ENGINE_BIN = path.join(ROOT_DIR, 'engine', 'bin', 'wendo_injector.exe');
const ENGINE_INCLUDE = path.join(ROOT_DIR, 'engine', 'include');

// Cleanly separated workspace folders
const MODS_SOURCE_DIR = path.join(ROOT_DIR, 'installed_mods');
const MODS_COMPILED_DIR = path.join(ROOT_DIR, 'mods_compiled');
const CONFIG_DIR = path.join(ROOT_DIR, 'config');
const STATE_FILE = path.join(CONFIG_DIR, 'state.json');
const CONFIG_FILE = path.join(CONFIG_DIR, 'config.json');
const SETTINGS_INI = path.join(
  process.env.LOCALAPPDATA || path.join(app.getPath('appData'), '..', 'Local'),
  'Wendo',
  'mod_settings.ini'
);

const WINDHAWK_MODS_DIR = 'C:\\ProgramData\\Windhawk\\ModsSource';
const WINDHAWK_PROFILE_FILE = 'C:\\ProgramData\\Windhawk\\userprofile.json';

function ensureDirs() {
  for (const dir of [MODS_SOURCE_DIR, MODS_COMPILED_DIR, CONFIG_DIR, path.dirname(SETTINGS_INI)]) {
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
  }
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

function findCompiler() {
  const candidates = [
    'C:\\Program Files\\Windhawk\\Compiler\\bin\\clang++.exe',
    'C:\\Program Files\\LLVM\\bin\\clang++.exe',
    'C:\\msys64\\ucrt64\\bin\\g++.exe',
    'C:\\msys64\\mingw64\\bin\\g++.exe'
  ];
  for (const p of candidates) {
    if (fs.existsSync(p)) return p;
  }
  return null;
}

function httpsGetJson(url) {
  return new Promise((resolve, reject) => {
    const req = https.get(
      url,
      {
        headers: {
          'User-Agent': 'Wendo-Studio/1.0',
          Accept: 'application/vnd.github.v3+json'
        }
      },
      (res) => {
        let data = '';
        res.on('data', (chunk) => (data += chunk));
        res.on('end', () => {
          if (res.statusCode >= 200 && res.statusCode < 300) {
            try {
              resolve(JSON.parse(data));
            } catch {
              reject(new Error('Invalid JSON response'));
            }
          } else {
            reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 200)}`));
          }
        });
      }
    );
    req.on('error', reject);
  });
}

function httpsGetText(url) {
  return new Promise((resolve, reject) => {
    const req = https.get(
      url,
      { headers: { 'User-Agent': 'Wendo-Studio/1.0' } },
      (res) => {
        let data = '';
        res.on('data', (chunk) => (data += chunk));
        res.on('end', () => {
          if (res.statusCode >= 200 && res.statusCode < 300) {
            resolve(data);
          } else {
            reject(new Error(`HTTP ${res.statusCode}`));
          }
        });
      }
    );
    req.on('error', reject);
  });
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

function extractTargetsFromCode(code) {
  const targets = [];
  const regex = /\/\/\s*@include\s+(.*)/g;
  let m;
  while ((m = regex.exec(code)) !== null) {
    const val = m[1].trim();
    if (val && val !== '*') targets.push(val);
  }
  return targets.length > 0 ? targets : ['notepad.exe'];
}

function isWindhawkOnlyMod(code = '') {
  return (
    code.includes('windhawk_utils.h') ||
    code.includes('windhawk_api.h') ||
    code.includes('Wh_FindFirstSymbol') ||
    code.includes('WindhawkUtils::') ||
    code.includes('WH_MOD_ID') ||
    code.includes('windhawk-tool-mod')
  );
}

async function compileModInternal(modId, code, compilerOptions = '') {
  if (isWindhawkOnlyMod(code)) {
    return {
      ok: false,
      stderr:
        'This mod uses Windhawk PDB symbol APIs (windhawk_utils.h / Wh_FindFirstSymbol) and runs via Windhawk Engine rather than Wendo standalone DLL compiler.'
    };
  }

  const compiler = findCompiler();
  if (!compiler) {
    return {
      ok: false,
      stderr: 'No C++ compiler found (clang++ / g++). Install LLVM or Windhawk Compiler.'
    };
  }

  const safeId = modId.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
  const srcPath = path.join(MODS_SOURCE_DIR, `${safeId}.wh.cpp`);
  const dllPath = path.join(MODS_COMPILED_DIR, `${safeId}.dll`);

  fs.writeFileSync(srcPath, code, 'utf8');
  let buildSrcPath = srcPath;

  if (!code.includes('wendo_mod.h')) {
    const tempBuildPath = path.join(MODS_COMPILED_DIR, `${safeId}.build.cpp`);
    fs.writeFileSync(tempBuildPath, `#include "wendo_mod.h"\n${code}`, 'utf8');
    buildSrcPath = tempBuildPath;
  }

  const extraFlags = (compilerOptions || '')
    .split(/\s+/)
    .filter((f) => f && f.startsWith('-'));

  const args = [
    '--target=x86_64-w64-windows-gnu',
    '-std=c++20',
    '-DUNICODE',
    '-D_UNICODE',
    `-DWH_MOD_ID=L"${safeId}"`,
    '-shared',
    '-O2',
    `-I${ENGINE_INCLUDE}`,
    buildSrcPath,
    '-o',
    dllPath,
    '-luser32',
    '-lgdi32',
    '-ldwmapi',
    '-lshell32',
    ...extraFlags
  ];

  const res = await runExecFile(compiler, args);
  return {
    ...res,
    dllPath: res.ok ? dllPath : null,
    compiler
  };
}

function flattenSettings(obj, prefix = '', out = {}) {
  for (const [k, v] of Object.entries(obj || {})) {
    if (k.startsWith('$')) continue;
    const fullKey = prefix ? `${prefix}.${k}` : k;
    if (Array.isArray(v)) {
      v.forEach((item, idx) => {
        if (item && typeof item === 'object') {
          flattenSettings(item, `${fullKey}[${idx}]`, out);
        } else {
          out[`${fullKey}[${idx}]`] = item;
        }
      });
    } else if (v && typeof v === 'object') {
      flattenSettings(v, fullKey, out);
    } else {
      out[fullKey] = v;
    }
  }
  return out;
}

async function writeWindhawkModSettings(modId, settingsObj) {
  const settingsPath = `HKLM:\\SOFTWARE\\Windhawk\\Engine\\Mods\\${modId}\\Settings`;
  const flat = flattenSettings(settingsObj);
  const lines = [
    `$ErrorActionPreference = 'Stop'`,
    `if (-not (Test-Path '${settingsPath}')) { New-Item -Path '${settingsPath}' -Force | Out-Null }`
  ];

  for (const [k, v] of Object.entries(flat)) {
    const safeKey = k.replace(/'/g, "''");
    if (typeof v === 'boolean') {
      lines.push(`Set-ItemProperty -Path '${settingsPath}' -Name '${safeKey}' -Value ${v ? 1 : 0} -Type DWord`);
    } else if (typeof v === 'number' && Number.isInteger(v)) {
      lines.push(`Set-ItemProperty -Path '${settingsPath}' -Name '${safeKey}' -Value ${v} -Type DWord`);
    } else {
      const safeVal = String(v ?? '').replace(/'/g, "''");
      lines.push(`Set-ItemProperty -Path '${settingsPath}' -Name '${safeKey}' -Value '${safeVal}' -Type String`);
    }
  }

  const scriptFile = path.join(CONFIG_DIR, 'save_wh_settings.ps1');
  fs.writeFileSync(scriptFile, lines.join('\r\n'), 'utf8');

  let res = await runExecFile('powershell.exe', [
    '-NoProfile',
    '-ExecutionPolicy',
    'Bypass',
    '-File',
    scriptFile
  ]);

  if (!res.ok) {
    res = await runExecFile('powershell.exe', [
      '-NoProfile',
      '-Command',
      `Start-Process powershell.exe -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','${scriptFile}'`
    ]);
  }
  return res;
}

async function setWindhawkModDisabled(modId, disabled) {
  const regPath = `HKLM:\\SOFTWARE\\Windhawk\\Engine\\Mods\\${modId}`;
  const val = disabled ? 1 : 0;

  const psScript = `
$ErrorActionPreference = 'Stop'
if (Test-Path '${regPath}') {
  Set-ItemProperty -Path '${regPath}' -Name 'Disabled' -Value ${val} -Type DWord
} else {
  New-Item -Path '${regPath}' -Force | Out-Null
  Set-ItemProperty -Path '${regPath}' -Name 'Disabled' -Value ${val} -Type DWord
}
$profilePath = '${WINDHAWK_PROFILE_FILE}'
if (Test-Path $profilePath) {
  $json = Get-Content $profilePath -Raw | ConvertFrom-Json
  if ($json.mods -and $json.mods.'${modId}') {
    $json.mods.'${modId}'.disabled = $${disabled ? 'true' : 'false'}
    $json | ConvertTo-Json -Depth 10 | Set-Content $profilePath -Encoding UTF8
  }
}
`;

  const scriptFile = path.join(CONFIG_DIR, 'toggle_wh.ps1');
  fs.writeFileSync(scriptFile, psScript, 'utf8');

  let res = await runExecFile('powershell.exe', [
    '-NoProfile',
    '-ExecutionPolicy',
    'Bypass',
    '-File',
    scriptFile
  ]);

  if (!res.ok) {
    res = await runExecFile('powershell.exe', [
      '-NoProfile',
      '-Command',
      `Start-Process powershell.exe -Verb RunAs -Wait -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','${scriptFile}'`
    ]);
  }

  if (!res.ok) {
    fs.writeFileSync(
      path.join(CONFIG_DIR, 'last_error.log'),
      `[${new Date().toISOString()}] toggle ${modId} failed:\n${res.stderr}\n${res.stdout}\n`,
      'utf8'
    );
  }

  return res;
}

// Low-RAM optimizations for smooth user experience
app.commandLine.appendSwitch('renderer-process-limit', '1');
app.commandLine.appendSwitch('disable-background-networking');
app.commandLine.appendSwitch('disable-features', 'Spellcheck,AutofillServerCommunication');

const translationCache = new Map();

async function translateTextCached(text, targetLang = 'ar') {
  if (!text || text.length < 3) return text;
  const key = `${targetLang}:${text.slice(0, 120)}:${text.length}`;
  if (translationCache.has(key)) return translationCache.get(key);

  try {
    // Preserve markdown image tags while translating surrounding text
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
  const iconPath = path.join(ROOT_DIR, 'public', 'omnilynx-logo.png');
  const win = new BrowserWindow({
    width: 1360,
    height: 860,
    minWidth: 1020,
    minHeight: 680,
    title: 'Omnilynx',
    icon: fs.existsSync(iconPath) ? iconPath : undefined,
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

app.whenReady().then(() => {
  ensureDirs();

  // IPC: List installed mods (Windhawk + Wendo mods/)
  ipcMain.handle('mods:listInstalled', async () => {
    const state = readJsonSafe(STATE_FILE, { mods: {} });
    const whProfile = readJsonSafe(WINDHAWK_PROFILE_FILE, { mods: {} });
    const modsMap = new Map();

    const scanDir = (dir, sourceOrigin) => {
      if (!fs.existsSync(dir)) return;
      const files = fs.readdirSync(dir).filter((f) => f.endsWith('.wh.cpp') || f.endsWith('.cpp'));
      for (const file of files) {
        const fullPath = path.join(dir, file);
        const modId = file.replace(/\.wh\.cpp$/, '').replace(/\.cpp$/, '');
        const code = fs.readFileSync(fullPath, 'utf8');
        const compiledDll = path.join(MODS_COMPILED_DIR, `${modId}.dll`);
        const localState = state.mods[modId] || {};
        const whState = whProfile.mods?.[modId] || {};

        const isEnabled =
          sourceOrigin === 'windhawk'
            ? whState.disabled === false
            : Boolean(localState.enabled);

        modsMap.set(modId, {
          id: modId,
          fileName: file,
          filePath: fullPath,
          origin: sourceOrigin,
          code,
          compiled: fs.existsSync(compiledDll),
          dllPath: compiledDll,
          enabled: isEnabled,
          settings: localState.settings || whState.settings || {}
        });
      }
    };

    scanDir(WINDHAWK_MODS_DIR, 'windhawk');
    scanDir(MODS_SOURCE_DIR, 'wendo');

    return Array.from(modsMap.values());
  });

  ipcMain.handle('mods:saveSource', async (_, { id, code }) => {
    const safeId = id.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
    const filePath = path.join(MODS_SOURCE_DIR, `${safeId}.wh.cpp`);
    fs.writeFileSync(filePath, code, 'utf8');
    return { ok: true, id: safeId, filePath };
  });

  ipcMain.handle('mods:delete', async (_, modId) => {
    const filePath = path.join(MODS_SOURCE_DIR, `${modId}.wh.cpp`);
    const dllPath = path.join(MODS_COMPILED_DIR, `${modId}.dll`);
    if (fs.existsSync(filePath)) fs.unlinkSync(filePath);
    if (fs.existsSync(dllPath)) fs.unlinkSync(dllPath);
    const state = readJsonSafe(STATE_FILE, { mods: {} });
    delete state.mods[modId];
    writeJsonSafe(STATE_FILE, state);
    return { ok: true };
  });

  // Real Toggle: compiles + injects Wendo mods, or updates Windhawk registry for Windhawk mods
  ipcMain.handle('mods:toggle', async (_, { id, enabled, origin, code, compilerOptions }) => {
    const state = readJsonSafe(STATE_FILE, { mods: {} });

    if (origin === 'windhawk' || isWindhawkOnlyMod(code)) {
      const res = await setWindhawkModDisabled(id, !enabled);
      state.mods[id] = { ...(state.mods[id] || {}), enabled };
      writeJsonSafe(STATE_FILE, state);

      // Restart Windhawk tray silently so the change takes effect immediately
      const whExe = 'C:\\Program Files\\Windhawk\\windhawk.exe';
      if (res.ok && fs.existsSync(whExe)) {
        runExecFile(whExe, ['-restart', '-tray-only']);
      }

      return {
        ok: res.ok,
        enabled,
        message: res.ok
          ? `Mod ${id} ${enabled ? 'enabled' : 'disabled'} & Windhawk Engine reloaded.`
          : `Could not update Windhawk registry (Admin permission needed): ${res.stderr}`
      };
    }

    // Standalone Wendo mod
    const dllPath = path.join(MODS_COMPILED_DIR, `${id}.dll`);
    const targets = extractTargetsFromCode(code || '');

    if (enabled) {
      if (!fs.existsSync(dllPath)) {
        const comp = await compileModInternal(id, code, compilerOptions);
        if (!comp.ok) {
          return { ok: false, enabled: false, message: `Compile failed: ${comp.stderr}` };
        }
      }

      let injectedAny = false;
      let lastErr = '';
      for (const target of targets) {
        const res = await runExecFile(ENGINE_BIN, ['inject', target, dllPath]);
        try {
          const parsed = JSON.parse(res.stdout);
          if (parsed.ok) injectedAny = true;
          else lastErr = parsed.error || 'Process not running';
        } catch {
          lastErr = res.stderr || 'Injector error';
        }
      }

      state.mods[id] = { ...(state.mods[id] || {}), enabled: true };
      writeJsonSafe(STATE_FILE, state);

      return {
        ok: true,
        enabled: true,
        injected: injectedAny,
        message: injectedAny
          ? `Enabled & injected into ${targets.join(', ')}`
          : `Enabled & compiled (${targets.join(', ')} not currently running — ${lastErr})`
      };
    } else {
      for (const target of targets) {
        await runExecFile(ENGINE_BIN, ['eject', target, `${id}.dll`]);
      }
      state.mods[id] = { ...(state.mods[id] || {}), enabled: false };
      writeJsonSafe(STATE_FILE, state);
      return {
        ok: true,
        enabled: false,
        message: `Disabled & ejected from ${targets.join(', ')}`
      };
    }
  });

  ipcMain.handle('mods:saveSettings', async (_, { id, settings, origin, code }) => {
    const state = readJsonSafe(STATE_FILE, { mods: {} });
    state.mods[id] = { ...(state.mods[id] || {}), settings };
    writeJsonSafe(STATE_FILE, state);

    // Write flat INI for native wendo_mod.h readers
    const lines = ['[Settings]'];
    for (const [k, v] of Object.entries(settings || {})) {
      const val = typeof v === 'boolean' ? (v ? 1 : 0) : v;
      lines.push(`${k}=${String(val)}`);
    }
    fs.writeFileSync(SETTINGS_INI, lines.join('\r\n'), 'utf8');

    // If Windhawk mod, write to HKLM\SOFTWARE\Windhawk\Engine\Mods\<id>\Settings
    if (origin === 'windhawk' || isWindhawkOnlyMod(code || '')) {
      const res = await writeWindhawkModSettings(id, settings);
      const whExe = 'C:\\Program Files\\Windhawk\\windhawk.exe';
      if (res.ok && fs.existsSync(whExe)) {
        await runExecFile(whExe, ['-restart', '-tray-only']);
      }
      return {
        ok: res.ok,
        message: res.ok
          ? 'Settings saved to Windhawk & reloaded!'
          : `Failed to save Windhawk settings: ${res.stderr}`
      };
    }

    // If Wendo standalone mod is enabled, eject & re-inject so new settings apply immediately
    if (state.mods[id]?.enabled) {
      const dllPath = path.join(MODS_COMPILED_DIR, `${id}.dll`);
      const targets = extractTargetsFromCode(code || '');
      if (fs.existsSync(dllPath)) {
        for (const target of targets) {
          await runExecFile(ENGINE_BIN, ['eject', target, `${id}.dll`]);
          await runExecFile(ENGINE_BIN, ['inject', target, dllPath]);
        }
      }
    }

    return { ok: true, message: 'Settings saved & applied!' };
  });

  ipcMain.handle('mods:importFile', async () => {
    const result = await dialog.showOpenDialog({
      title: 'Import Mod File (.wh.cpp / .cpp)',
      filters: [{ name: 'C++ Mod Files', extensions: ['cpp'] }],
      properties: ['openFile']
    });
    if (result.canceled || !result.filePaths.length) return { canceled: true };
    const src = result.filePaths[0];
    const baseName = path.basename(src);
    const modId = baseName.replace(/\.wh\.cpp$/, '').replace(/\.cpp$/, '');
    const dest = path.join(MODS_SOURCE_DIR, `${modId}.wh.cpp`);
    fs.copyFileSync(src, dest);
    return { canceled: false, id: modId, filePath: dest };
  });

  ipcMain.handle('mods:importContent', async (_, { fileName, content }) => {
    const modId = fileName.replace(/\.wh\.cpp$/, '').replace(/\.cpp$/, '').replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
    const dest = path.join(MODS_SOURCE_DIR, `${modId}.wh.cpp`);
    fs.writeFileSync(dest, content, 'utf8');
    return { ok: true, id: modId, filePath: dest };
  });

  ipcMain.handle('mods:swapFile', async (_, modId) => {
    const result = await dialog.showOpenDialog({
      title: `Replace ${modId} with another .wh.cpp file`,
      filters: [{ name: 'C++ Mod Files', extensions: ['cpp'] }],
      properties: ['openFile']
    });
    if (result.canceled || !result.filePaths.length) return { canceled: true };
    const dest = path.join(MODS_SOURCE_DIR, `${modId}.wh.cpp`);
    fs.copyFileSync(result.filePaths[0], dest);
    return { canceled: false, id: modId, filePath: dest };
  });

  ipcMain.handle('mods:duplicate', async (_, modId) => {
    const whFile = path.join(WINDHAWK_MODS_DIR, `${modId}.wh.cpp`);
    const wendoFile = path.join(MODS_SOURCE_DIR, `${modId}.wh.cpp`);
    const src = fs.existsSync(wendoFile) ? wendoFile : whFile;
    if (!fs.existsSync(src)) return { ok: false, error: 'Source file not found' };
    const newId = `${modId}-custom`;
    const dest = path.join(MODS_SOURCE_DIR, `${newId}.wh.cpp`);
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
    modsDir: MODS_SOURCE_DIR,
    compiledDir: MODS_COMPILED_DIR,
    configDir: CONFIG_DIR,
    windhawkDetected: fs.existsSync(WINDHAWK_MODS_DIR)
  }));

  // IPC: Explore Store (GitHub)
  ipcMain.handle('store:fetchIndex', async (_, customRepo) => {
    const repo = customRepo || 'PyDevMedo/Omnilynx';
    const apiUrl = `https://api.github.com/repos/${repo}/contents/mods`;
    const items = await httpsGetJson(apiUrl);
    return items
      .filter((item) => item.name.endsWith('.wh.cpp'))
      .map((item) => ({
        id: item.name.replace(/\.wh\.cpp$/, ''),
        fileName: item.name,
        downloadUrl: item.download_url,
        htmlUrl: item.html_url,
        size: item.size
      }));
  });

  ipcMain.handle('store:fetchModSource', async (_, { downloadUrl }) => {
    return await httpsGetText(downloadUrl);
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
        .replace('/blob/', '/');
      const parts = downloadUrl.split('/');
      const last = parts[parts.length - 1] || 'shared-mod.wh.cpp';
      modId = last.replace(/\.wh\.cpp$/, '').replace(/\.cpp$/, '');
    } else {
      // Allow plain mod ID too
      modId = link.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
      downloadUrl = `https://raw.githubusercontent.com/PyDevMedo/Omnilynx/main/mods/${modId}.wh.cpp`;
    }

    if (!modId || !downloadUrl) return { ok: false, error: 'Could not parse mod link' };

    try {
      const code = await httpsGetText(downloadUrl);
      const safeId = modId.replace(/[^a-zA-Z0-9_-]/g, '-').toLowerCase();
      const dest = path.join(MODS_SOURCE_DIR, `${safeId}.wh.cpp`);
      fs.writeFileSync(dest, code, 'utf8');
      return { ok: true, id: safeId, filePath: dest };
    } catch (e) {
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
      'Stop-Process -Name explorer -Force; Start-Process explorer.exe'
    ]);
  });

  createWindow();
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
