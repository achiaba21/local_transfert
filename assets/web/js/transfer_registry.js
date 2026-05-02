// ============================================================
// transfer_registry.js — historique des transferts P2P
//
// V1.3 — Sprint Web P2P V1.3
//
// Source de vérité de l'état des fichiers (envoyés / reçus / en cours
// / échoués). Persiste les MÉTADONNÉES dans localStorage pour survivre
// aux refresh de page. Les Blobs ne sont jamais persistés (trop gros).
//
// Hook par p2p.js : addEntry à la création, updateEntry à chaque
// transition (sending/sent/failed/received), retry pour relancer un
// fichier échoué.
//
// Render dans #p2p-transfers-list (la zone P2P de la footer tabs).
// ============================================================
(function () {
  'use strict';
  const { clientLog, escapeHtml, formatBytes } = window.LTR;
  const $ = (s) => document.querySelector(s);

  const STORAGE_KEY = 'ltr-p2p-history';
  const MAX_ENTRIES = 100;

  // entries[].id = string unique. Chaque entry représente UN fichier.
  // direction: 'out' (j'envoie) | 'in' (je reçois)
  // status: 'pending' | 'sending' | 'sent' | 'received' | 'failed'
  let entries = [];
  // Map en mémoire des File originaux (sender uniquement) — perdus au
  // refresh, donc retry après refresh = impossible (toast invitera à
  // re-sélectionner).
  const originalFiles = new Map();  // entryId → File

  function makeId() {
    return 'tx-' + Date.now().toString(36)
                 + '-' + Math.random().toString(36).slice(2, 8);
  }

  function init() {
    loadFromStorage();
    // V1.3 — boot : entrées laissées en pending/sending par un précédent
    // refresh sont marquées failed reason 'session_perdue'.
    let dirty = false;
    for (const e of entries) {
      if (e.status === 'pending' || e.status === 'sending') {
        e.status = 'failed';
        e.error  = 'session_perdue';
        dirty = true;
      }
    }
    if (dirty) saveToStorage();
    render();
    clientLog('info', '[registry] init — ' + entries.length + ' entries');
  }

  function loadFromStorage() {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) { entries = []; return; }
      const parsed = JSON.parse(raw);
      entries = Array.isArray(parsed) ? parsed : [];
    } catch (e) {
      clientLog('warn', '[registry] load failed: ' + e.message);
      entries = [];
    }
  }

  function saveToStorage() {
    try {
      // Cap à MAX_ENTRIES (drop les plus anciennes).
      if (entries.length > MAX_ENTRIES) {
        entries = entries.slice(-MAX_ENTRIES);
      }
      localStorage.setItem(STORAGE_KEY, JSON.stringify(entries));
    } catch (e) {
      clientLog('warn', '[registry] save failed: ' + e.message);
    }
  }

  // Crée une entry et retourne son id. opts = {
  //   direction: 'out'|'in', peer: {deviceId, displayName, emoji,
  //   platformLabel}, name, size, file?: File }
  function addEntry(opts) {
    const id = makeId();
    const entry = {
      id,
      direction:     opts.direction,
      peerDeviceId:  opts.peer.deviceId,
      peerName:      opts.peer.displayName,
      peerEmoji:     opts.peer.emoji,
      name:          opts.name,
      size:          opts.size,
      status:        'pending',
      bytes:         0,
      error:         null,
      createdAt:     Date.now(),
      finishedAt:    null,
    };
    entries.push(entry);
    if (opts.file) originalFiles.set(id, opts.file);
    saveToStorage();
    render();
    return id;
  }

  // Met à jour une entry. patch = {status?, bytes?, error?}
  function updateEntry(id, patch) {
    const e = entries.find((x) => x.id === id);
    if (!e) return;
    Object.assign(e, patch);
    if (patch.status === 'sent' || patch.status === 'received'
        || patch.status === 'failed') {
      e.finishedAt = Date.now();
    }
    saveToStorage();
    render();
  }

  // Retry — relance un envoi failed (direction='out' uniquement).
  // Nécessite le File original encore en RAM. Si on a refresh la page,
  // originalFiles est vide → toast pour re-sélectionner.
  function retryFile(id) {
    const e = entries.find((x) => x.id === id);
    if (!e || e.direction !== 'out' || e.status !== 'failed') return;
    const file = originalFiles.get(id);
    if (!file) {
      if (window.LTR.p2p && window.LTR.p2p.toast) {
        window.LTR.p2p.toast(
          'Sélectionne à nouveau ce fichier (perdu après refresh)',
          'warning');
      }
      return;
    }
    const peer = window.LTR.peers && window.LTR.peers.getPeer(e.peerDeviceId);
    if (!peer) {
      if (window.LTR.p2p && window.LTR.p2p.toast) {
        window.LTR.p2p.toast(
          `${e.peerName} n'est plus connecté`, 'warning');
      }
      return;
    }
    // Marque pending et relance.
    updateEntry(id, { status: 'pending', error: null, bytes: 0 });
    window.LTR.p2p.startSendTo(peer, [file]);
  }

  function clearAll() {
    entries = [];
    originalFiles.clear();
    saveToStorage();
    render();
  }

  // Compteurs publics pour les tabs.
  function activeCount() {
    return entries.filter(
      (e) => e.status === 'sending' || e.status === 'pending').length;
  }
  function totalCount() { return entries.length; }

  // Stocke la référence File originale (sender) pour permettre le
  // retry. Appelée par p2p.js après addEntry.
  function attachFile(id, file) {
    if (id && file) originalFiles.set(id, file);
  }

  // ====================================================================
  // RENDU
  // ====================================================================
  function render() {
    const list = $('#p2p-transfers-list');
    if (!list) return;
    // tri : plus récent en haut
    const sorted = entries.slice().sort(
      (a, b) => b.createdAt - a.createdAt);

    if (sorted.length === 0) {
      list.innerHTML = '<li class="p2p-empty">Aucun transfert P2P pour le moment.</li>';
      updateTabBadges();
      return;
    }

    const visible = sorted.slice(0, 10);
    const hidden  = sorted.length - visible.length;

    list.innerHTML = visible.map(renderEntry).join('')
      + (hidden > 0
          ? `<li class="p2p-more"><button type="button" class="p2p-more-btn">
              ··· ${hidden} entrée${hidden > 1 ? 's' : ''} plus ancienne${hidden > 1 ? 's' : ''}
              </button></li>`
          : '');

    // Wire boutons retry.
    list.querySelectorAll('.p2p-retry-btn').forEach((b) => {
      b.addEventListener('click', (ev) => {
        ev.stopPropagation();
        retryFile(b.dataset.entryId);
      });
    });
    // Wire bouton voir tout (déploie le reste).
    const moreBtn = list.querySelector('.p2p-more-btn');
    if (moreBtn) {
      moreBtn.addEventListener('click', () => {
        // Re-render avec tous les entries.
        list.innerHTML = sorted.map(renderEntry).join('');
        list.querySelectorAll('.p2p-retry-btn').forEach((b) => {
          b.addEventListener('click', (ev) => {
            ev.stopPropagation();
            retryFile(b.dataset.entryId);
          });
        });
      });
    }
    updateTabBadges();
  }

  function renderEntry(e) {
    const arrow = e.direction === 'out' ? '→' : '←';
    const peerStr = `${e.peerEmoji || ''} ${escapeHtml(e.peerName || '')}`.trim();
    let icon, sub, progress = '';
    if (e.status === 'sending') {
      const pct = e.size > 0
        ? Math.floor((e.bytes / e.size) * 100) : 0;
      icon = '<span class="p2p-icon p2p-icon-sending">↻</span>';
      sub = `${pct} % · ${peerStr}`;
      progress = `<div class="p2p-entry-bar"><span style="width:${pct}%"></span></div>`;
    } else if (e.status === 'sent' || e.status === 'received') {
      icon = '<span class="p2p-icon p2p-icon-done">✓</span>';
      sub = `${formatBytes(e.size)} · ${peerStr} · ${formatTime(e.finishedAt)}`;
    } else if (e.status === 'failed') {
      icon = '<span class="p2p-icon p2p-icon-failed">✗</span>';
      sub = `${humanError(e.error)} · ${peerStr}`;
    } else {
      icon = '<span class="p2p-icon p2p-icon-pending">⏱</span>';
      sub = `En attente · ${peerStr}`;
    }
    const retryBtn = (e.status === 'failed' && e.direction === 'out')
      ? `<button type="button" class="p2p-retry-btn" data-entry-id="${e.id}"
                aria-label="Réessayer">↻ Réessayer</button>`
      : '';
    return `
      <li class="p2p-entry p2p-entry-${e.status}">
        ${icon}
        <div class="p2p-entry-body">
          <div class="p2p-entry-head">
            <span class="p2p-entry-name">${escapeHtml(e.name)}</span>
            <span class="p2p-entry-arrow">${arrow}</span>
          </div>
          <div class="p2p-entry-sub">${sub}</div>
          ${progress}
          ${retryBtn}
        </div>
      </li>`;
  }

  function humanError(err) {
    if (!err) return 'Échec';
    const map = {
      'taille_invalide': 'Fichier tronqué',
      'session_perdue':  'Session perdue (refresh)',
      'meta':            'Échec annonce',
      'chunk':            'Échec réseau',
      'send':             'Échec envoi',
      'read':             'Lecture impossible',
      'end':              'Échec finalisation',
      'session-meta':     'Échec session',
    };
    return map[err] || 'Échec ' + err;
  }

  function formatTime(ts) {
    if (!ts) return '';
    const d = new Date(ts);
    return d.getHours().toString().padStart(2, '0') + ':'
         + d.getMinutes().toString().padStart(2, '0');
  }

  // Met à jour le compteur dans l'onglet P2P (et sur Host).
  function updateTabBadges() {
    const p2pBadge = $('#p2p-tab-count');
    if (p2pBadge) p2pBadge.textContent = String(totalCount());
    const hostBadge = $('#tx-count');
    if (hostBadge) {
      const hostList = $('#transfers-list');
      hostBadge.textContent = String(
        hostList ? hostList.children.length : 0);
    }
  }

  // ====================================================================
  // NOTIFICATION (toast + son + vibration mobile)
  // ====================================================================
  function notifyComplete(direction, fileName, peerName) {
    const text = direction === 'out'
      ? `✓ ${fileName} envoyé à ${peerName}`
      : `✓ ${fileName} reçu de ${peerName}`;
    if (window.LTR.p2p && window.LTR.p2p.toast) {
      window.LTR.p2p.toast(text, 'success');
    }
    // Vibration mobile (silencieux sur desktop).
    try { if (navigator.vibrate) navigator.vibrate(200); } catch {}
    // Son court (base64 OGG ~5Ko, "ding"). Volume bas.
    try { playDing(); } catch {}
  }

  let _audio = null;
  function playDing() {
    // « Ding » sinusoïdal généré via WebAudio (pas de fichier embarqué).
    // Fréquence 880 Hz, durée 120 ms, fade out exponentiel.
    if (!window.AudioContext && !window.webkitAudioContext) return;
    if (!_audio) {
      const Ctx = window.AudioContext || window.webkitAudioContext;
      _audio = new Ctx();
    }
    const ctx = _audio;
    const t = ctx.currentTime;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.frequency.value = 880;
    osc.type = 'sine';
    gain.gain.setValueAtTime(0.001, t);
    gain.gain.exponentialRampToValueAtTime(0.15, t + 0.005);
    gain.gain.exponentialRampToValueAtTime(0.001, t + 0.18);
    osc.connect(gain).connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.2);
  }

  window.LTR = window.LTR || {};
  window.LTR.transferRegistry = {
    init, addEntry, updateEntry, retryFile, attachFile,
    activeCount, totalCount, clearAll, render, notifyComplete,
  };
})();
