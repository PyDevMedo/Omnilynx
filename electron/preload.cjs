const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('wendoAPI', {
  // Mods management
  listInstalledMods: () => ipcRenderer.invoke('mods:listInstalled'),
  saveModSource: (payload) => ipcRenderer.invoke('mods:saveSource', payload),
  deleteMod: (modId) => ipcRenderer.invoke('mods:delete', modId),
  toggleMod: (payload) => ipcRenderer.invoke('mods:toggle', payload),
  saveModSettings: (payload) => ipcRenderer.invoke('mods:saveSettings', payload),
  importModFile: () => ipcRenderer.invoke('mods:importFile'),
  importModContent: (payload) => ipcRenderer.invoke('mods:importContent', payload),
  swapModFile: (modId) => ipcRenderer.invoke('mods:swapFile', modId),
  duplicateMod: (modId) => ipcRenderer.invoke('mods:duplicate', modId),
  openModsFolder: () => ipcRenderer.invoke('mods:openFolder'),

  // Compiler & Native Engine
  compileMod: (payload) => ipcRenderer.invoke('engine:compile', payload),
  injectMod: (payload) => ipcRenderer.invoke('engine:inject', payload),
  ejectMod: (payload) => ipcRenderer.invoke('engine:eject', payload),
  listProcesses: () => ipcRenderer.invoke('engine:listProcesses'),
  getEngineInfo: () => ipcRenderer.invoke('engine:info'),

  // Explore (GitHub Store)
  fetchStoreIndex: (repoUrl) => ipcRenderer.invoke('store:fetchIndex', repoUrl),
  fetchStoreModSource: (payload) => ipcRenderer.invoke('store:fetchModSource', payload),
  installFromLink: (link) => ipcRenderer.invoke('mods:installFromLink', link),
  translateText: (payload) => ipcRenderer.invoke('app:translateText', payload),

  // Windows Quick Tweaks (instant system-wide changes)
  getTweaks: () => ipcRenderer.invoke('tweaks:getAll'),
  applyTweak: (payload) => ipcRenderer.invoke('tweaks:apply', payload),

  // App Settings & Utilities
  getAppConfig: () => ipcRenderer.invoke('app:getConfig'),
  saveAppConfig: (config) => ipcRenderer.invoke('app:saveConfig', config),
  openExternal: (url) => ipcRenderer.invoke('app:openExternal', url),
  restartExplorer: () => ipcRenderer.invoke('app:restartExplorer')
});
