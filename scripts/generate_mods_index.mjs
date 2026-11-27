import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const MODS_DIR = path.resolve(__dirname, '..', 'mods');
const REPO = 'PyDevMedo/Omnilynx';

function extractMeta(code) {
  const metaMatch = code.match(/\/\/\s*==WindhawkMod==([\s\S]*?)\/\/\s*==\/WindhawkMod==/);
  const block = metaMatch ? metaMatch[1] : code.slice(0, 4000);
  const getOne = (key, fallback = '') => {
    const m = block.match(new RegExp(`//\\s*@${key}\\s+(.+)`));
    return m ? m[1].trim() : fallback;
  };
  const getMany = (key) => {
    const out = [];
    const re = new RegExp(`//\\s*@${key}\\s+(.+)`, 'g');
    let m;
    while ((m = re.exec(block)) !== null) {
      out.push(m[1].trim());
    }
    return out;
  };
  return {
    id: getOne('id'),
    name: getOne('name'),
    description: getOne('description'),
    version: getOne('version', '1.0.0'),
    author: getOne('author', 'Community'),
    github: getOne('github', ''),
    include: getMany('include'),
    exclude: getMany('exclude'),
    compilerOptions: getOne('compilerOptions', '')
  };
}

function extractRealPreviewUrl(code) {
  const readmeMatch = code.match(/\/\/\s*==WindhawkModReadme==([\s\S]*?)\/\/\s*==\/WindhawkModReadme==/);
  const readme = readmeMatch ? readmeMatch[1] : '';
  if (!readme) return null;

  // 1. Markdown image ![alt](https://...)
  const mdImg = readme.match(/!\[[^\]]*\]\((https?:\/\/[^)\s]+)\)/i);
  if (mdImg && mdImg[1]) return mdImg[1];

  // 2. HTML <img src="https://...">
  const htmlImg = readme.match(/<img[^>]+src=["'](https?:\/\/[^"']+)["']/i);
  if (htmlImg && htmlImg[1]) return htmlImg[1];

  // 3. Direct image URL in readme
  const rawImg = readme.match(/(https?:\/\/[^\s)"'<>]+\.(?:png|jpe?g|gif|webp))/i);
  if (rawImg && rawImg[1]) return rawImg[1];

  return null;
}

function inferCategory(id, name, desc, includes) {
  const text = `${id} ${name} ${desc} ${includes.join(' ')}`.toLowerCase();
  if (text.includes('taskbar') || text.includes('tray')) return 'Taskbar';
  if (text.includes('start') || text.includes('menu') || text.includes('clock')) return 'Start & Shell';
  if (text.includes('explorer') || text.includes('folder') || text.includes('file')) return 'Explorer';
  if (text.includes('mica') || text.includes('acrylic') || text.includes('dark') || text.includes('opacity') || text.includes('aero')) return 'Visuals & FX';
  if (text.includes('alt-tab') || text.includes('window') || text.includes('snap') || text.includes('top')) return 'Productivity';
  return 'System';
}

const files = fs.readdirSync(MODS_DIR).filter((f) => f.endsWith('.wh.cpp')).sort();
const entries = [];
let withPreview = 0;

for (const file of files) {
  const fullPath = path.join(MODS_DIR, file);
  const stat = fs.statSync(fullPath);
  const code = fs.readFileSync(fullPath, 'utf8');
  const meta = extractMeta(code);
  const id = (meta.id || file.replace(/\.wh\.cpp$/i, '')).trim();
  const preview = extractRealPreviewUrl(code);
  if (preview) withPreview++;

  entries.push({
    id,
    fileName: file,
    name: meta.name || id,
    description: meta.description || '',
    version: meta.version || '1.0.0',
    author: meta.author || 'Community',
    github: meta.github || '',
    include: meta.include,
    exclude: meta.exclude,
    compilerOptions: meta.compilerOptions,
    category: inferCategory(id, meta.name, meta.description, meta.include),
    previewImg: preview,
    preview_url: preview,
    size: stat.size,
    downloadUrl: `https://raw.githubusercontent.com/${REPO}/main/mods/${file}`,
    htmlUrl: `https://github.com/${REPO}/blob/main/mods/${file}`
  });
}

const json = JSON.stringify(entries, null, 2);
fs.writeFileSync(path.join(MODS_DIR, 'omnilynx-store.json'), json, 'utf8');
fs.writeFileSync(path.join(MODS_DIR, 'mods_index.json'), json, 'utf8');
console.log(`Indexed ${entries.length} mods (${withPreview} with embedded preview URLs).`);
