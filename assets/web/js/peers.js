// ============================================================
// peers.js — annuaire des autres appareils web auth
//
// V1.2 — Sprint Web P2P
// Maintient localement la liste des autres devices auth, écoute l'event
// SSE `web-peers` pour la mettre à jour, et rend les pucks horizontaux
// dans #peers-list. Click sur une card → open file picker → délègue à
// LTR.p2p.startSendTo (Wave 3).
//
// Volontairement minimal : pas de retry, pas de cache local. La source
// de vérité = le host. La page se reconstruit à chaque event.
// ============================================================
(function () {
  'use strict';
  const { clientLog, escapeHtml, formatBytes } = window.LTR;
  const $ = (s) => document.querySelector(s);

  let peers = [];   // [{ deviceId, displayName, emoji, platformLabel }]
  let listEl = null;
  let pickerInputEl = null;
  let pendingTargetDeviceId = null;
  let onPeerClickedCb = null;

  function render() {
    if (!listEl) return;
    if (peers.length === 0) {
      listEl.innerHTML = '';
      const sec = $('.peers-section');
      if (sec) sec.hidden = true;
      return;
    }
    const sec = $('.peers-section');
    if (sec) sec.hidden = false;

    listEl.innerHTML = peers.map((p) => `
      <li class="peer-card" data-device-id="${escapeHtml(p.deviceId)}"
          role="option" tabindex="0"
          aria-label="${escapeHtml(p.displayName)} ${escapeHtml(p.platformLabel)}">
        <span class="peer-emoji" aria-hidden="true">${escapeHtml(p.emoji)}</span>
        <span class="peer-name">${escapeHtml(p.displayName)}</span>
        <span class="peer-sub">${escapeHtml(p.platformLabel)}</span>
      </li>
    `).join('');

    listEl.querySelectorAll('.peer-card').forEach((el) => {
      el.addEventListener('click', () => onCardClicked(el.dataset.deviceId));
      el.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          onCardClicked(el.dataset.deviceId);
        }
      });
    });
  }

  function onCardClicked(deviceId) {
    if (!deviceId) return;
    const peer = peers.find((p) => p.deviceId === deviceId);
    if (!peer) return;
    clientLog('info', '[peers] click on ' + peer.displayName);
    pendingTargetDeviceId = deviceId;
    if (onPeerClickedCb) {
      onPeerClickedCb(peer);
    } else {
      pickerInputEl.click();
    }
  }

  function onFilesPicked(files) {
    const target = pendingTargetDeviceId;
    pendingTargetDeviceId = null;
    if (!target || !files || files.length === 0) return;
    const peer = peers.find((p) => p.deviceId === target);
    if (!peer) {
      clientLog('warn', '[peers] target gone before send');
      return;
    }
    const total = Array.from(files).reduce((a, f) => a + f.size, 0);
    if (!confirm(`Envoyer ${files.length} fichier(s) (${formatBytes(total)}) à ${peer.displayName} ?`)) {
      return;
    }
    if (window.LTR && window.LTR.p2p && window.LTR.p2p.startSendTo) {
      window.LTR.p2p.startSendTo(peer, Array.from(files));
    } else {
      clientLog('error', '[peers] LTR.p2p.startSendTo manquant');
    }
  }

  function init() {
    listEl = $('#peers-list');
    pickerInputEl = $('#peers-file-input');
    if (!listEl || !pickerInputEl) {
      clientLog('warn', '[peers] DOM manquant — section désactivée');
      return;
    }
    pickerInputEl.addEventListener('change', () => {
      onFilesPicked(pickerInputEl.files);
      pickerInputEl.value = '';
    });
    clientLog('info', '[peers] init OK');
  }

  function setPeers(list) {
    peers = Array.isArray(list) ? list : [];
    render();
  }

  function getPeer(deviceId) {
    return peers.find((p) => p.deviceId === deviceId) || null;
  }

  // V1.2 — Pour la modale réception : permet à p2p.js de déclencher un
  // override du handler (utilisé pour démontrer la présence d'un peer).
  function onPeerClicked(cb) { onPeerClickedCb = cb; }

  window.LTR = window.LTR || {};
  window.LTR.peers = { init, setPeers, getPeer, onPeerClicked };
})();
