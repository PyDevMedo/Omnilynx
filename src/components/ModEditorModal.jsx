import React, { useState } from 'react';
import CodeMirror from '@uiw/react-codemirror';
import { cpp } from '@codemirror/lang-cpp';
import { oneDark } from '@codemirror/theme-one-dark';
import { X, Save, Hammer, Play, Terminal } from 'lucide-react';

export default function ModEditorModal({ initialId, initialCode, onClose, onSaved }) {
  const [modId, setModId] = useState(initialId || 'my-custom-mod');
  const [code, setCode] = useState(initialCode || '');
  const [logs, setLogs] = useState('Ready to edit & compile.');
  const [compiling, setCompiling] = useState(false);

  const handleSave = async () => {
    const res = await window.wendoAPI.saveModSource({ id: modId, code });
    if (res.ok) {
      setLogs(`Saved to ${res.filePath}`);
      onSaved();
    }
  };

  const handleCompile = async () => {
    setCompiling(true);
    setLogs('Compiling with Clang x86_64...');
    const res = await window.wendoAPI.compileMod({ id: modId, code });
    if (res.ok) {
      setLogs(`SUCCESS: Built ${res.dllPath}\nCompiler: ${res.compiler}`);
      onSaved();
    } else {
      setLogs(`COMPILE ERROR:\n${res.stderr || res.stdout}`);
    }
    setCompiling(false);
  };

  const handleCompileAndInject = async () => {
    setCompiling(true);
    setLogs('Compiling...');
    const comp = await window.wendoAPI.compileMod({ id: modId, code });
    if (!comp.ok) {
      setLogs(`COMPILE ERROR:\n${comp.stderr || comp.stdout}`);
      setCompiling(false);
      return;
    }
    const targetMatch = code.match(/\/\/\s*@include\s+(.*)/);
    const target = targetMatch ? targetMatch[1].trim() : 'notepad.exe';
    setLogs(`Compiled: ${comp.dllPath}\nInjecting into ${target}...`);
    const inj = await window.wendoAPI.injectMod({ target, dllPath: comp.dllPath });
    setLogs((prev) => `${prev}\n${JSON.stringify(inj, null, 2)}`);
    onSaved();
    setCompiling(false);
  };

  return (
    <div
      style={{
        position: 'fixed',
        inset: 0,
        background: 'rgba(5, 7, 11, 0.85)',
        backdropFilter: 'blur(10px)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 60,
        padding: 20
      }}
    >
      <div
        className="glass-card"
        style={{
          width: '100%',
          maxWidth: 1100,
          height: '90vh',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          background: '#0d111a'
        }}
      >
        {/* Top bar */}
        <div
          style={{
            padding: '12px 18px',
            borderBottom: '1px solid var(--border-subtle)',
            display: 'flex',
            alignItems: 'center',
            gap: 12
          }}
        >
          <span style={{ fontSize: 13, fontWeight: 600, color: 'var(--text-secondary)' }}>Mod ID:</span>
          <input
            className="input font-mono"
            style={{ width: 240, padding: '6px 10px', fontSize: 12.5 }}
            value={modId}
            onChange={(e) => setModId(e.target.value)}
          />
          <div style={{ flex: 1 }} />
          <button className="btn" onClick={handleSave}>
            <Save size={14} /> Save
          </button>
          <button className="btn btn-primary" onClick={handleCompile} disabled={compiling}>
            <Hammer size={14} /> Compile DLL
          </button>
          <button className="btn btn-success" onClick={handleCompileAndInject} disabled={compiling}>
            <Play size={14} /> Compile & Inject
          </button>
          <button className="btn" onClick={onClose} style={{ padding: 7 }}>
            <X size={16} />
          </button>
        </div>

        {/* CodeMirror C++ Editor */}
        <div style={{ flex: 1, overflow: 'auto', userSelect: 'text' }}>
          <CodeMirror
            value={code}
            height="100%"
            theme={oneDark}
            extensions={[cpp()]}
            onChange={(val) => setCode(val)}
            style={{ fontSize: 13.5, height: '100%' }}
          />
        </div>

        {/* Compiler Output Console */}
        <div
          style={{
            height: 150,
            borderTop: '1px solid var(--border-subtle)',
            background: '#07090e',
            padding: '10px 16px',
            display: 'flex',
            flexDirection: 'column'
          }}
        >
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: 6,
              fontSize: 11.5,
              color: 'var(--text-muted)',
              marginBottom: 6
            }}
          >
            <Terminal size={13} /> COMPILER & ENGINE OUTPUT
          </div>
          <pre
            className="font-mono"
            style={{
              flex: 1,
              overflowY: 'auto',
              fontSize: 12,
              color: '#cbd5e1',
              whiteSpace: 'pre-wrap',
              userSelect: 'text'
            }}
          >
            {logs}
          </pre>
        </div>
      </div>
    </div>
  );
}
