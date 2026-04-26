// ============================================================
// common.js — utilitaires partagés (log serveur, auth, helpers)
// Chargé avant app.js / upload.js / download.js par <script defer>.
// Exposé via window.LTR pour éviter les globals éparpillés.
// ============================================================
(function () {
  'use strict';

  // Pont JS → logs serveur (/api/clientlog).
  function clientLog(level, msg) {
    try {
      const body = JSON.stringify({ level, msg });
      if (navigator.sendBeacon) {
        navigator.sendBeacon('/api/clientlog',
          new Blob([body], { type: 'application/json' }));
      } else {
        fetch('/api/clientlog', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body, keepalive: true,
        }).catch(() => {});
      }
    } catch (e) { /* ignore */ }
  }

  function goToLogin() { window.location.href = '/login'; }

  function handle401(resp) {
    if (resp && resp.status === 401) { goToLogin(); return true; }
    return false;
  }

  // Détecte la plateforme via User-Agent.
  function detectPlatform() {
    const ua = navigator.userAgent || '';
    if (/iPhone|iPad|iOS/i.test(ua)) return 'iOS';
    if (/Android/i.test(ua)) return 'Android';
    if (/Mac OS X|Macintosh/i.test(ua)) return 'macOS';
    if (/Windows/i.test(ua)) return 'Windows';
    if (/Linux/i.test(ua)) return 'Linux';
    return 'Other';
  }

  // iOS Safari ne supporte pas webkitdirectory : masquer le bouton "dossier".
  function supportsFolderPick() {
    const platform = detectPlatform();
    if (platform === 'iOS') return false;
    const tmp = document.createElement('input');
    return 'webkitdirectory' in tmp;
  }

  function iconFor(name) {
    const ext = (name.split('.').pop() || '').toLowerCase();
    if (['png','jpg','jpeg','gif','webp','svg'].includes(ext)) return '🖼️';
    if (['mp3','wav','flac','ogg','m4a'].includes(ext)) return '🎵';
    if (['mp4','mov','mkv','webm','avi'].includes(ext)) return '🎞️';
    if (['zip','tar','gz','rar','7z'].includes(ext)) return '📦';
    if (['pdf','doc','docx','txt','md','rtf'].includes(ext)) return '📄';
    return '📄';
  }

  function formatBytes(n) {
    if (!n || n < 1024) return (n || 0) + ' o';
    const units = ['Ko', 'Mo', 'Go', 'To'];
    let v = n / 1024; let i = 0;
    while (v >= 1024 && i < units.length - 1) { v /= 1024; ++i; }
    return v.toFixed(v < 10 ? 1 : 0) + ' ' + units[i];
  }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({
      '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
    }[c]));
  }

  // Expose un namespace unique à toutes les autres modules.
  window.LTR = window.LTR || {};
  Object.assign(window.LTR, {
    clientLog, goToLogin, handle401,
    detectPlatform, supportsFolderPick,
    iconFor, formatBytes, escapeHtml,
  });
})();
