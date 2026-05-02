// ============================================================
// app.js — bootstrap de la page principale (authentifiée)
//
// Orchestre :
//  - Récupération des infos host (/api/host-info)
//  - Check session (/api/me)
//  - Heartbeat (/api/ping toutes les 10s)
//  - Init modules upload + download
//  - Install banner + logout
// ============================================================
(function () {
  'use strict';
  const { clientLog, goToLogin, detectPlatform } = window.LTR;
  const $ = (s) => document.querySelector(s);

  // SCRIPT LOADED probe — utile pour debug iOS Safari.
  clientLog('info',
    '[app] SCRIPT LOADED UA=' +
    (navigator.userAgent || '').substring(0, 80) +
    ' readyState=' + document.readyState);

  const state = { hostInfo: null, visitorPlatform: detectPlatform() };

  async function boot() {
    clientLog('info', '[app] boot start');
    try {
      const info = await fetch('/api/host-info').then((r) => r.json());
      state.hostInfo = info;
      $('#host-name').textContent = info.name || 'Host';
      clientLog('info', '[app] host-info OK: ' + info.name);

      const me = await fetch('/api/me', { credentials: 'same-origin' });
      clientLog('info', '[app] /api/me status=' + me.status);
      if (!me.ok) { goToLogin(); return; }

      setupLogout();
      setupInstallBanner();
      startHeartbeat();

      // Délègue aux modules dédiés (upload.js, download.js, peers.js).
      if (window.LTR.initUpload)   window.LTR.initUpload();
      if (window.LTR.initDownload) window.LTR.initDownload();
      // V1.3 — Sprint Web P2P V1.3 : registry + tabs Host/P2P.
      if (window.LTR.transferRegistry) {
        window.LTR.transferRegistry.init();
      }
      setupTxTabs();
      // V1.2 — Sprint Web P2P : annuaire + handler SSE web-peers.
      if (window.LTR.peers && window.LTR.peers.init) {
        window.LTR.peers.init();
        if (window.LTR.addSseListener) {
          window.LTR.addSseListener('web-peers', (ev) => {
            try {
              const data = JSON.parse(ev.data);
              window.LTR.peers.setPeers(data.peers || []);
            } catch (e) {
              clientLog('error', '[app] bad web-peers payload');
            }
          });
          window.LTR.addSseListener('p2p-signal', (ev) => {
            try {
              const data = JSON.parse(ev.data);
              if (window.LTR.p2p) window.LTR.p2p.handleSignal(data);
            } catch (e) {
              clientLog('error', '[app] bad p2p-signal payload');
            }
          });
        }
      }
    } catch (e) {
      clientLog('error', '[app] boot threw: ' + (e && e.message));
      goToLogin();
    }
  }

  // ==================== TX TABS (V1.3) ====================
  function setupTxTabs() {
    const tabHost = $('#tx-tab-host');
    const tabP2p  = $('#tx-tab-p2p');
    const paneHost = $('#transfers-list-host');
    const paneP2p  = $('#p2p-transfers-list');
    if (!tabHost || !tabP2p || !paneHost || !paneP2p) return;
    function activate(which) {
      const isHost = (which === 'host');
      tabHost.classList.toggle('tx-tab-active', isHost);
      tabP2p .classList.toggle('tx-tab-active', !isHost);
      tabHost.setAttribute('aria-selected', String(isHost));
      tabP2p .setAttribute('aria-selected', String(!isHost));
      paneHost.hidden = !isHost;
      paneP2p .hidden =  isHost;
    }
    tabHost.addEventListener('click', () => activate('host'));
    tabP2p .addEventListener('click', () => activate('p2p'));
  }

  // ==================== LOGOUT ====================
  function setupLogout() {
    $('#logout-btn').addEventListener('click', async () => {
      // V1.2 — Sprint Web P2P : ferme proprement les RTCPeerConnection
      // avant logout pour éviter les half-open connections.
      if (window.LTR.p2p && window.LTR.p2p.cleanupAll) {
        window.LTR.p2p.cleanupAll();
      }
      try {
        await fetch('/api/logout',
          { method: 'POST', credentials: 'same-origin' });
      } catch (e) { /* ignore */ }
      goToLogin();
    });
  }

  // ==================== INSTALL BANNER ====================
  // V1.1.10 : on n'affiche le banner QUE si l'OS du visiteur match l'OS
  // du host (cas où /download/self fournit un binaire utilisable). Le cas
  // cross-OS « Voir sur GitHub » est désactivé tant qu'il n'y a pas de
  // releases publiées (cf. multi-os-installer/ANALYSIS.md).
  function setupInstallBanner() {
    try {
      if (sessionStorage.getItem('install-closed') === '1') return;
    } catch (e) {}

    const hostPlatform = state.hostInfo && state.hostInfo.platform;
    const same = hostPlatform === state.visitorPlatform;
    if (!same) return;  // pas de banner cross-OS V1

    const banner = $('#install-banner');
    const title = $('#install-title');
    const sub = $('#install-sub');
    const btn = $('#install-btn');

    title.textContent = `Installer l'app pour ${hostPlatform}`;
    sub.textContent = 'Transferts plus rapides et découverte auto.';
    btn.textContent = 'Installer';
    btn.href = '/download/self';
    btn.removeAttribute('target');
    btn.removeAttribute('rel');
    banner.hidden = false;

    $('#install-close').addEventListener('click', () => {
      banner.hidden = true;
      try { sessionStorage.setItem('install-closed', '1'); } catch (e) {}
    });
  }

  // ==================== HEARTBEAT ====================
  let heartbeatTimer = null;
  async function sendPing() {
    try {
      const resp = await fetch('/api/ping', {
        method: 'POST',
        credentials: 'same-origin',
        keepalive: true,
      });
      if (resp.status === 401) goToLogin();
    } catch (e) { /* transient */ }
  }

  function startHeartbeat() {
    if (heartbeatTimer) return;
    sendPing();
    heartbeatTimer = setInterval(sendPing, 10_000);
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') sendPing();
    });
  }

  // ==================== GO ====================
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
  } else {
    setTimeout(boot, 0);
  }
})();
