// ============================================================
// p2p_ui.js — Couche UI P2P (V1.6.1)
//
// Responsabilités :
//  - Modale réception (queue d'offres + bottom-sheet/centered modal)
//  - Card peer en mode envoi (markCardSending, setCardPhase)
//  - Sticky bar bas global (refreshSticky, showSticky)
//  - Toast notif court
//  - cssEscape (fallback CSS.escape)
//
// Aucun couplage transport/session : utilise uniquement le DOM et
// les états passés en argument.
// ============================================================
(function () {
  'use strict';
  const { clientLog, formatBytes } = window.LTR;
  const $ = (s) => document.querySelector(s);

  const ACCEPT_TTL_MS = 60_000;

  // ================ Modale réception ================
  let modalQueue = [];
  let modalActive = false;

  function showIncomingModal(peer, decideCb) {
    modalQueue.push({ peer, decideCb });
    if (!modalActive) processModalQueue();
  }

  function processModalQueue() {
    if (modalQueue.length === 0) {
      modalActive = false;
      return;
    }
    modalActive = true;
    const { peer, decideCb } = modalQueue.shift();

    const modal = $('#p2p-incoming-modal');
    if (!modal) {
      decideCb(false);
      processModalQueue();
      return;
    }
    $('#p2p-incoming-emoji').textContent = peer.emoji;
    $('#p2p-incoming-title').textContent =
      peer.displayName + ' veut t\'envoyer des fichiers';
    $('#p2p-incoming-sub').textContent  = peer.platformLabel;
    modal.hidden = false;

    const fill = $('#p2p-incoming-ttl-fill');
    if (fill) {
      fill.style.transition = 'none';
      fill.style.width = '100%';
      void fill.offsetWidth;
      fill.style.transition = `width ${ACCEPT_TTL_MS}ms linear`;
      fill.style.width = '0%';
    }

    const acceptBtn = $('#p2p-incoming-accept');
    const refuseBtn = $('#p2p-incoming-refuse');
    function close() {
      modal.hidden = true;
      acceptBtn.removeEventListener('click', onAccept);
      refuseBtn.removeEventListener('click', onRefuse);
    }
    function onAccept() { close(); decideCb(true);  processModalQueue(); }
    function onRefuse() { close(); decideCb(false); processModalQueue(); }
    acceptBtn.addEventListener('click', onAccept);
    refuseBtn.addEventListener('click', onRefuse);
  }

  // ================ Card peer ================
  function markCardSending(deviceId, phase) {
    phase = phase || 'waiting';
    const card = document.querySelector(
      `.peer-card[data-device-id="${cssEscape(deviceId)}"]`);
    if (!card) return null;
    card.classList.add('peer-card--sending');
    setCardPhase(card, phase);
    let bar = card.querySelector('.peer-progress-bar');
    if (!bar) {
      bar = document.createElement('div');
      bar.className = 'peer-progress-bar';
      bar.innerHTML = '<span></span>';
      card.appendChild(bar);
    }
    bar.firstElementChild.style.width = '0%';
    if (!card.querySelector('.peer-cancel-btn')) {
      const x = document.createElement('button');
      x.type = 'button';
      x.className = 'peer-cancel-btn';
      x.textContent = '✕';
      x.setAttribute('aria-label', 'Annuler le transfert');
      x.addEventListener('click', (ev) => {
        ev.stopPropagation();
        if (window.LTR.p2pTransport) {
          window.LTR.p2pTransport.cancelOutgoing(deviceId);
        }
      });
      card.appendChild(x);
    }
    return card;
  }

  function setCardPhase(card, phase) {
    const sub = card.querySelector('.peer-sub');
    if (!sub) return;
    if (phase === 'waiting')         sub.textContent = 'En attente de réponse…';
    else if (phase === 'connecting') sub.textContent = 'Connexion…';
    else if (phase === 'sending')    sub.textContent = '0 % · …';
  }

  function updateProgress(state) {
    const total = state.totalBytes;
    let done;
    if (state.role === 'sender') {
      done = state.bytesAckedByReceiver || state.bytesSent;
    } else {
      done = state.bytesReceived;
    }
    if (!total) return;
    const pct  = Math.floor((done / total) * 100);
    const dt   = Math.max(0.001, (Date.now() - state.startedAt) / 1000);
    const speed = formatBytes(done / dt) + '/s';
    const card = state.uiCard;
    if (card) {
      const sub = card.querySelector('.peer-sub');
      if (sub) sub.textContent = pct + ' % · ' + speed;
      const bar = card.querySelector('.peer-progress-bar > span');
      if (bar) bar.style.width = pct + '%';
    }
    refreshSticky();
  }

  // ================ Sticky bar ================
  function showSticky() { refreshSticky(); }

  function refreshSticky() {
    const bar = $('#p2p-sticky-bar');
    if (!bar) return;
    const T = window.LTR.p2pTransport;
    if (!T) { bar.hidden = true; return; }
    const active = Array.from(T.conns.values()).filter(
      (s) => s.startedAt && s.role !== 'idle');
    if (active.length === 0) { bar.hidden = true; return; }
    let totalDone = 0, totalAll = 0;
    active.forEach((s) => {
      totalDone += (s.role === 'sender' ? s.bytesSent : s.bytesReceived);
      totalAll  += s.totalBytes;
    });
    const pct = totalAll ? Math.floor((totalDone / totalAll) * 100) : 0;
    bar.querySelector('.p2p-sticky-text').textContent =
      `${active.length} transfert${active.length > 1 ? 's' : ''} P2P · ${pct} %`;
    bar.hidden = false;
  }

  // ================ Toast ================
  function toast(text, kind) {
    clientLog('info', '[p2p:toast] ' + text);
    const c = $('#p2p-toast');
    if (!c) return;
    c.textContent = text;
    c.dataset.kind = kind || 'info';
    c.hidden = false;
    setTimeout(() => { c.hidden = true; }, 3000);
  }

  // ================ Helper ================
  function cssEscape(s) {
    return String(s).replace(/[^a-zA-Z0-9_-]/g, (c) => '\\' + c);
  }

  window.LTR = window.LTR || {};
  window.LTR.p2pUi = {
    showIncomingModal, processModalQueue,
    markCardSending, setCardPhase, updateProgress,
    refreshSticky, showSticky,
    toast, cssEscape,
    ACCEPT_TTL_MS,
  };
})();
