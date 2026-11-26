// Curated bilingual descriptions for core Omnilynx & installed mods
export const MOD_TRANSLATIONS = {
  'omnilynx-always-on-top': {
    ar: { name: '📌 تثبيت النافذة في المقدمة', desc: 'يثبّت نوافذ البرامج المحددة فوق جميع النوافذ الأخرى بشكل دائم.' },
    en: { name: '📌 Always On Top Pin', desc: 'Pins target application windows above all other windows.' }
  },
  'omnilynx-rounded-corners': {
    ar: { name: '🟦 حواف ويندوز 11 الدائرية', desc: 'يطبّق الحواف الدائرية الأنيقة لويندوز 11 على النوافذ المستهدفة.' },
    en: { name: '🟦 Windows 11 Rounded Corners', desc: 'Enforces crisp Windows 11 rounded corners on target windows.' }
  },
  'omnilynx-border-accent': {
    ar: { name: '🌈 إطار مضيء حول النوافذ', desc: 'يضيف إطاراً ملوناً متوهجاً حول نوافذ ويندوز 11 لتخصيص المظهر.' },
    en: { name: '🌈 Glowing Border Accent', desc: 'Applies a custom glowing accent border color to Windows 11 windows.' }
  },
  'wendo-dark-titlebars': {
    ar: { name: '🌙 تعتيم شريط العنوان', desc: 'يفعّل الوضع الليلي الاحترافي على أشرطة العنوان وإطارات البرامج.' },
    en: { name: '🌙 Force Dark Titlebars', desc: 'Applies Windows 11 Immersive Dark Mode to top-level windows.' }
  },
  'wendo-translucent-mica': {
    ar: { name: '🪟 زجاج Mica الشفاف', desc: 'يضيف خامة Acrylic وMica الزجاجية الأنيقة لنوافذ ويندوز 11.' },
    en: { name: '🪟 Translucent Mica Backdrop', desc: 'Applies native Windows 11 Mica / Acrylic backdrop to windows.' }
  },
  'wendo-window-opacity': {
    ar: { name: '✨ شفافية النوافذ الزجاجية', desc: 'يتحكم في درجة شفافية النوافذ المفتوحة بمظهر زجاجي ناعم.' },
    en: { name: '✨ Glass Window Opacity', desc: 'Makes target application windows semi-transparent with configurable opacity.' }
  },
  'overhaulded-alt-tab': {
    ar: { name: '🌀 مبدّل النوافذ Alt+Tab المطور', desc: 'يعيد تصميم شاشة التنقل بين النوافذ بتأثير ثلاثي الأبعاد وخلفية ضبابية.' },
    en: { name: '🌀 Overhauled Alt+Tab', desc: 'Modern 3D carousel task switcher with DWM blur and smooth animations.' }
  },
  'translucent-windows': {
    ar: { name: '💎 النوافذ الشفافة', desc: 'يضيف تأثيرات ضبابية وشفافية Acrylic/Mica لنوافذ الويندوز.' },
    en: { name: '💎 Translucent Windows', desc: 'Adds blur and Acrylic/Mica transparency effects to Windows.' }
  },
  'windows-11-taskbar-styler': {
    ar: { name: '🎨 منسّق شريط المهام', desc: 'يتيح تخصيص شكل شريط المهام في ويندوز 11 بالكامل باستخدام ثيمات جاهزة.' },
    en: { name: '🎨 Windows 11 Taskbar Styler', desc: 'Customize the Windows 11 taskbar appearance with themes and styles.' }
  },
  'windows-11-start-menu-styler': {
    ar: { name: '🪟 منسّق قائمة ابدأ', desc: 'يخصّص تصميم قائمة ابدأ في ويندوز 11 بأشكال وثيمات احترافية.' },
    en: { name: '🪟 Windows 11 Start Menu Styler', desc: 'Customize the Windows 11 Start Menu layout and visual themes.' }
  },
  'windows-11-file-explorer-styler': {
    ar: { name: '📁 منسّق مستكشف الملفات', desc: 'يعدّل ألوان وتصميم مستكشف الملفات في ويندوز 11.' },
    en: { name: '📁 Windows 11 File Explorer Styler', desc: 'Customize the Windows 11 File Explorer interface and styling.' }
  },
  'windows-11-notification-center-styler': {
    ar: { name: '🔔 منسّق مركز الإشعارات', desc: 'يخصّص مظهر مركز الإشعارات والتقويم في ويندوز 11.' },
    en: { name: '🔔 Notification Center Styler', desc: 'Customize the Windows 11 Notification Center and Action Center.' }
  },
  'taskbar-volume-control': {
    ar: { name: '🔊 التحكم بالصوت من الشريط', desc: 'ارفع واخفض صوت النظام بتمرير عجلة الماوس فوق شريط المهام.' },
    en: { name: '🔊 Taskbar Volume Control', desc: 'Adjust system volume by scrolling the mouse wheel over the taskbar.' }
  },
  'windows-animations': {
    ar: { name: '🎬 حركات النوافذ السلسة', desc: 'يعدّل سرعة ونعومة حركات فتح وإغلاق النوافذ في الويندوز.' },
    en: { name: '🎬 Windows Animations', desc: 'Customize window open, close, and minimize animation speed and style.' }
  }
};

export function getLocalizedModCopy(mod, lang) {
  const curated = MOD_TRANSLATIONS[mod.id]?.[lang];
  if (curated) return curated;
  return {
    name: mod.parsed?.meta?.name || mod.id,
    desc: mod.parsed?.meta?.description || ''
  };
}
