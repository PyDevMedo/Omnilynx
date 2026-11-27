import yaml from 'js-yaml';

export function parseModSource(code = '', fallbackId = 'unknown-mod') {
  const meta = {
    id: fallbackId,
    name: fallbackId,
    description: '',
    version: '1.0.0',
    author: 'Unknown',
    github: '',
    homepage: '',
    include: [],
    exclude: [],
    compilerOptions: ''
  };

  const metaMatch = code.match(/\/\/\s*==WindhawkMod==([\s\S]*?)\/\/\s*==\/WindhawkMod==/);
  if (metaMatch) {
    const lines = metaMatch[1].split(/\r?\n/);
    for (const line of lines) {
      const m = line.match(/\/\/\s*@(\w+)\s+(.*)/);
      if (!m) continue;
      const key = m[1].trim();
      const val = m[2].trim();
      if (key === 'include' || key === 'exclude') {
        meta[key].push(val);
      } else if (key in meta) {
        meta[key] = val;
      }
    }
  }

  let readme = '';
  const readmeMatch = code.match(
    /\/\/\s*==WindhawkModReadme==\s*\/\*([\s\S]*?)\*\/\s*\/\/\s*==\/WindhawkModReadme==/
  );
  if (readmeMatch) {
    readme = readmeMatch[1].trim();
  }

  let settingsSchema = [];
  const settingsMatch = code.match(
    /\/\/\s*==WindhawkModSettings==\s*\/\*([\s\S]*?)\*\/\s*\/\/\s*==\/WindhawkModSettings==/
  );
  if (settingsMatch) {
    try {
      const parsed = yaml.load(settingsMatch[1]);
      if (Array.isArray(parsed)) {
        settingsSchema = parsed.map((item) => {
          const keys = Object.keys(item || {}).filter((k) => !k.startsWith('$'));
          const key = keys[0] || 'setting';
          const defaultValue = item[key];

          // Normalize $options (Windhawk uses array of {value: label} objects)
          let options = null;
          if (Array.isArray(item.$options)) {
            options = item.$options.map((opt) => {
              if (typeof opt === 'object' && opt !== null) {
                const [val, label] = Object.entries(opt)[0] || ['', ''];
                return { value: val, label: String(label) };
              }
              return { value: String(opt), label: String(opt) };
            });
          }

          let type = 'string';
          if (options) type = 'select';
          else if (Array.isArray(defaultValue)) type = 'array';
          else if (typeof defaultValue === 'boolean') type = 'boolean';
          else if (typeof defaultValue === 'number') type = 'number';

          return {
            key,
            defaultValue,
            type,
            name: item.$name || key,
            description: item.$description || '',
            options
          };
        });
      }
    } catch {
      // ignore malformed YAML
    }
  }

  const haystack = `${meta.id} ${meta.name} ${meta.description} ${meta.include.join(' ')}`.toLowerCase();
  let category = 'System';
  if (haystack.includes('taskbar')) category = 'Taskbar';
  else if (haystack.includes('start') || haystack.includes('notification')) category = 'Start & Shell';
  else if (haystack.includes('explorer') || haystack.includes('file')) category = 'Explorer';
  else if (haystack.includes('translucent') || haystack.includes('animation') || haystack.includes('cursor'))
    category = 'Visuals & FX';
  else if (haystack.includes('chrome') || haystack.includes('alt-tab') || haystack.includes('clipboard'))
    category = 'Productivity';

  return { meta, readme, settingsSchema, category };
}

export function createNewModTemplate(modId = 'my-custom-mod', modName = 'My Custom Mod') {
  return `// ==WindhawkMod==
// @id              ${modId}
// @name            ${modName}
// @description     Custom Windows modification built with Omnilynx Studio
// @version         1.0.0
// @author          Omnilynx User
// @include         notepad.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# ${modName}
Describe what your mod does here. Supports **Markdown** formatting.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- enabled: true
  $name: Enable Feature
  $description: Turn this feature on or off
- opacity: 240
  $name: Window Opacity (50-255)
*/
// ==/WindhawkModSettings==

#include "wendo_mod.h"

BOOL Wh_ModInit() {
    Wh_Log(L"${modName} loaded successfully!");
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"${modName} unloaded!");
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"${modName} settings changed!");
}
`;
}
