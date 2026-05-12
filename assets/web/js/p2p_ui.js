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
      done = Math.max(state.bytesSent || 0, state.bytesAckedByReceiver || 0);
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
      const label = state.uiStatusLabel || '';
      if (sub) {
        sub.textContent = (label ? label + ' · ' : '') + pct + ' % · ' + speed;
      }
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
      totalDone += (s.role === 'sender'
        ? Math.max(s.bytesSent || 0, s.bytesAckedByReceiver || 0)
        : s.bytesReceived);
      totalAll  += s.totalBytes;
    });
    const pct = totalAll ? Math.floor((totalDone / totalAll) * 100) : 0;
    const label = active.length === 1 && active[0].uiStatusLabel
      ? active[0].uiStatusLabel + ' · '
      : '';
    bar.querySelector('.p2p-sticky-text').textContent =
      `${label}${active.length} transfert${active.length > 1 ? 's' : ''} P2P · ${pct} %`;
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

  function diagnosticText(reason) {
    const text = String(reason || '');
    if (text.includes('route') || text.includes('Pas de route')) {
      return 'Les deux appareils ne sont probablement pas sur le même réseau local, ou le Wi‑Fi bloque les connexions directes.';
    }
    if (text.includes('Wi-Fi') || text.includes('réseau') || text.includes('DataChannel')) {
      return 'Le réseau a été coupé ou instable pendant le transfert. Rapproche les appareils du Wi‑Fi puis relance.';
    }
    if (text.includes('Hors-ligne') || text.includes('fermée') || text.includes('muet')) {
      return 'L’autre appareil a fermé la page, s’est verrouillé, ou a perdu la session. Ouvre LocalTransfer dessus puis réessaie.';
    }
    return 'Le P2P a échoué. Vérifie que les deux appareils sont sur le même Wi‑Fi, sans VPN ni partage de connexion isolant les clients.';
  }

  function showDiagnostic(reason) {
    const box = $('#p2p-diagnostic');
    const text = $('#p2p-diagnostic-text');
    if (!box || !text) return;
    text.textContent = diagnosticText(reason);
    box.hidden = false;
    const close = $('#p2p-diagnostic-close');
    if (close && !close.dataset.wired) {
      close.dataset.wired = '1';
      close.addEventListener('click', () => { box.hidden = true; });
    }
  }

  // ================ V1.6.5 — Toast bloquant TOFU P2P (Wave 4 item L) ================
  // Affiche un toast warning persistant avec 2 boutons. callback reçoit
  // 'trust' ou 'refuse'. Pas d'auto-dismiss : l'utilisateur DOIT décider.
  function showTofuToast(peerName, decideCb) {
    clientLog('info', '[p2p:tofu-toast] ' + peerName);
    let host = $('#p2p-tofu-host');
    if (!host) {
      host = document.createElement('div');
      host.id = 'p2p-tofu-host';
      document.body.appendChild(host);
    }
    host.innerHTML = '';
    const box = document.createElement('div');
    box.className = 'p2p-tofu-toast';
    box.innerHTML = `
      <div class="p2p-tofu-icon">⚠</div>
      <div class="p2p-tofu-body">
        <div class="p2p-tofu-title">L'identité de « ${escapeHtml(peerName)} » a changé.</div>
        <div class="p2p-tofu-sub">Vérifie qui c'est avant de recevoir des fichiers.</div>
        <div class="p2p-tofu-actions">
          <button type="button" class="p2p-tofu-btn p2p-tofu-trust">Faire confiance</button>
          <button type="button" class="p2p-tofu-btn p2p-tofu-refuse">Refuser</button>
        </div>
      </div>`;
    host.appendChild(box);
    const finish = (decision) => {
      host.innerHTML = '';
      decideCb(decision);
    };
    box.querySelector('.p2p-tofu-trust').addEventListener('click',
      () => finish('trust'));
    box.querySelector('.p2p-tofu-refuse').addEventListener('click',
      () => finish('refuse'));
  }

  // ================ Helper ================
  function cssEscape(s) {
    return String(s).replace(/[^a-zA-Z0-9_-]/g, (c) => '\\' + c);
  }

  function escapeHtml(s) {
    return (window.LTR && window.LTR.escapeHtml)
      ? window.LTR.escapeHtml(s)
      : String(s).replace(/[&<>"']/g,
          (c) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  }

  window.LTR = window.LTR || {};
  window.LTR.p2pUi = {
    showIncomingModal, processModalQueue,
    markCardSending, setCardPhase, updateProgress,
    refreshSticky, showSticky,
    toast, showTofuToast, showDiagnostic, cssEscape,
    ACCEPT_TTL_MS,
  };
})();
