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

  // V1.6.5 — Sprint Stabilité (Wave 3 item H).
  // Tente de recréer la session via le cookie persistent ltr_remember.
  // Retourne true si succès (= caller peut retry sa requête initiale).
  // Anti-boucle infinie : un seul refresh par hit 401, géré via flag.
  let refreshInFlight = null;
  async function tryRefreshSession() {
    if (refreshInFlight) return refreshInFlight;
    refreshInFlight = (async () => {
      try {
        const r = await fetch('/api/auth/refresh', {
          method: 'POST',
          credentials: 'same-origin',
        });
        return r.ok;
      } catch {
        return false;
      } finally {
        // Reset après un court délai pour permettre la prochaine tentative
        // si la session re-expire plus tard.
        setTimeout(() => { refreshInFlight = null; }, 5000);
      }
    })();
    return refreshInFlight;
  }

  // V1.6.5 — handle401 amélioré : essaye d'abord ltr_remember puis seulement
  // redirect /login en dernier recours. Retourne :
  //   - true si redirection /login déclenchée (caller doit abort)
  //   - false si pas un 401 (caller continue normalement)
  //   - 'retry' si refresh OK (caller doit relancer la même requête)
  async function handle401Async(resp) {
    if (!resp || resp.status !== 401) return false;
    const ok = await tryRefreshSession();
    if (ok) {
      clientLog('info', '[refresh] session recréée via ltr_remember');
      return 'retry';
    }
    goToLogin();
    return true;
  }

  function handle401(resp) {
    if (resp && resp.status === 401) { goToLogin(); return true; }
    return false;
  }

  // Détecte la plateforme via User-Agent.
  // V1.6.5+ : depuis iPadOS 13, Safari iPad envoie un UA "Macintosh"
  // qui matche /Mac OS X/ et trompe la détection. On le distingue d'un
  // vrai Mac via `navigator.maxTouchPoints > 1` (un Mac Desktop n'a pas
  // d'écran tactile multi-points). Idem si l'utilisateur coche manuellement
  // « Demander la version pour ordinateur » sur Safari iOS.
  function detectPlatform() {
    const ua = navigator.userAgent || '';
    if (/iPhone|iPad|iPod|iOS/i.test(ua)) return 'iOS';
    if (/Android/i.test(ua)) return 'Android';
    // Détection iPadOS Desktop Mode : UA dit Macintosh MAIS écran tactile.
    if (/Mac OS X|Macintosh/i.test(ua)) {
      const tp = (navigator.maxTouchPoints || 0);
      if (tp > 1 && 'ontouchend' in document) return 'iOS';
      return 'macOS';
    }
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
    clientLog, goToLogin, handle401, handle401Async, tryRefreshSession,
    detectPlatform, supportsFolderPick,
    iconFor, formatBytes, escapeHtml,
  });
})();
