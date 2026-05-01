// ============================================================
// p2p.js — transferts WebRTC DataChannel direct entre navigateurs
//
// V1.2 — Sprint Web P2P
//
// Le host n'est qu'un signaling : POST /api/p2p/signal pour relayer
// les SDP offer/answer + ICE candidates. Une fois la connexion établie,
// les fichiers transitent en P2P sur RTCDataChannel sans plus solliciter
// le host.
//
// Le module gère N connexions simultanées (1 par destinataire). Chaque
// connexion a un état distinct : idle / negotiating / waiting-accept /
// connecting / sending / receiving / done / failed.
//
// ICE config : LAN-only (V1, pas de STUN public). Suffisant si les 2
// devices sont sur le même Wi-Fi ou même réseau filaire.
// Chunking : 64 KB par send, backpressure via bufferedAmount < 1 MB.
// ============================================================
(function () {
  'use strict';
  const { clientLog, escapeHtml, formatBytes } = window.LTR;
  const $ = (s) => document.querySelector(s);

  // Configuration WebRTC — LAN-only V1.
  const RTC_CONFIG = { iceServers: [] };

  const CHUNK_SIZE   = 64 * 1024;            // 64 KB
  const BUFFER_HIGH  = 1 * 1024 * 1024;      // 1 MB seuil backpressure
  const BUFFER_LOW   = 256 * 1024;           // attendre que ça redescende
  const ACCEPT_TTL_MS = 60_000;              // 60s pour accepter

  // Map<deviceId, ConnectionState>
  const conns = new Map();

  // ====================================================================
  // OUTGOING — démarrage d'un envoi A → B
  // ====================================================================
  async function startSendTo(peer, files) {
    const { deviceId } = peer;
    if (!files || files.length === 0) return;
    if (conns.has(deviceId)) {
      clientLog('warn', '[p2p] connexion déjà en cours vers ' + deviceId);
      return;
    }

    const state = {
      role: 'sender',
      peer,
      files,
      pc: null,
      dc: null,
      currentFileIdx: 0,
      bytesSent: 0,
      totalBytes: files.reduce((a, f) => a + f.size, 0),
      startedAt: 0,
      uiCard: null,
    };
    conns.set(deviceId, state);
    state.uiCard = markCardSending(deviceId, 0);
    showSticky();

    try {
      const pc = new RTCPeerConnection(RTC_CONFIG);
      state.pc = pc;
      wirePcCommon(pc, state);

      const dc = pc.createDataChannel('ltr', { ordered: true });
      state.dc = dc;
      dc.binaryType = 'arraybuffer';
      wireSenderDc(dc, state);

      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      await postSignal(deviceId, 'offer', { sdp: offer });
      clientLog('info', '[p2p] offer sent to ' + deviceId.substring(0, 8));
    } catch (e) {
      clientLog('error', '[p2p] startSendTo failed: ' + (e && e.message));
      cleanup(deviceId, '✗ Erreur P2P');
    }
  }

  // ====================================================================
  // INCOMING — réception d'un signal du host
  // ====================================================================
  async function handleSignal(msg) {
    const { from, type, payload } = msg;
    if (!from) return;

    if (type === 'offer') {
      const peer = window.LTR.peers && window.LTR.peers.getPeer(from);
      if (!peer) {
        clientLog('warn', '[p2p] offer from unknown peer ' + from);
        return;
      }
      await handleIncomingOffer(peer, payload);
      return;
    }

    const state = conns.get(from);
    if (!state) {
      clientLog('warn', '[p2p] signal type=' + type
                       + ' but no connection for ' + from.substring(0, 8));
      return;
    }

    if (type === 'answer') {
      try {
        await state.pc.setRemoteDescription(payload.sdp);
      } catch (e) { clientLog('error', '[p2p] setRemote(answer): ' + e); }
    } else if (type === 'ice') {
      // Si la pc n'est pas encore créée (receveur qui n'a pas encore
      // cliqué Accepter), file les candidates pour les drainer plus tard.
      // Sans ça, les ICE candidates initiales du sender sont perdues
      // → la connexion ne s'établit jamais.
      if (!state.pc) {
        if (state.pendingIceQueue) {
          state.pendingIceQueue.push(payload.candidate);
        }
        return;
      }
      try {
        await state.pc.addIceCandidate(payload.candidate);
      } catch (e) { /* ignore — candidates can fail benignly */ }
    } else if (type === 'refuse') {
      clientLog('info', '[p2p] receiver refused');
      toast(`${state.peer.displayName} a refusé`, 'warning');
      cleanup(from, 'Refusé');
    } else if (type === 'cancel' || type === 'bye') {
      cleanup(from, 'Annulé');
    }
  }

  async function handleIncomingOffer(peer, payload) {
    // Préparer un état receiver mais ne pas créer la pc tant que
    // l'utilisateur n'a pas accepté.
    const state = {
      role: 'receiver',
      peer,
      pc: null,
      dc: null,
      pendingOfferSdp: payload.sdp,
      pendingIceQueue: [],
      receivingFiles: [],   // [{ name, size, received: 0, chunks: [] }]
      currentFileIdx: 0,
      bytesReceived: 0,
      totalBytes: 0,
      filesMeta: null,
      startedAt: 0,
      uiCard: null,
      ttlTimer: null,
    };
    conns.set(peer.deviceId, state);

    state.ttlTimer = setTimeout(() => {
      if (state.pc === null) {
        clientLog('warn', '[p2p] offer TTL expired');
        postSignal(peer.deviceId, 'refuse', { reason: 'ttl' });
        cleanup(peer.deviceId, 'Expiré');
      }
    }, ACCEPT_TTL_MS);

    showIncomingModal(peer, async (accepted) => {
      clearTimeout(state.ttlTimer);
      if (!accepted) {
        await postSignal(peer.deviceId, 'refuse', {});
        cleanup(peer.deviceId, 'Refusé');
        return;
      }
      try {
        const pc = new RTCPeerConnection(RTC_CONFIG);
        state.pc = pc;
        wirePcCommon(pc, state);
        pc.ondatachannel = (ev) => {
          state.dc = ev.channel;
          state.dc.binaryType = 'arraybuffer';
          wireReceiverDc(state.dc, state);
        };
        await pc.setRemoteDescription(state.pendingOfferSdp);
        for (const c of state.pendingIceQueue) {
          try { await pc.addIceCandidate(c); } catch {}
        }
        state.pendingIceQueue = [];
        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        await postSignal(peer.deviceId, 'answer', { sdp: answer });
        state.startedAt = Date.now();
        state.uiCard = markCardSending(peer.deviceId, 0);
        showSticky();
      } catch (e) {
        clientLog('error', '[p2p] accept failed: ' + (e && e.message));
        cleanup(peer.deviceId, '✗ Connexion P2P échouée');
      }
    });
  }

  // ====================================================================
  // RTCPeerConnection — wiring commun (ICE candidates, state changes)
  // ====================================================================
  function wirePcCommon(pc, state) {
    pc.onicecandidate = (ev) => {
      if (ev.candidate) {
        postSignal(state.peer.deviceId, 'ice',
          { candidate: ev.candidate.toJSON ? ev.candidate.toJSON() : ev.candidate });
      }
    };
    pc.oniceconnectionstatechange = () => {
      clientLog('info', '[p2p] ice=' + pc.iceConnectionState
                       + ' for ' + state.peer.deviceId.substring(0, 8));
    };
    pc.onconnectionstatechange = () => {
      const cs = pc.connectionState;
      clientLog('info', '[p2p] pc state=' + cs
                       + ' for ' + state.peer.deviceId.substring(0, 8));
      if (cs === 'failed') {
        cleanup(state.peer.deviceId, '✗ Connexion P2P échouée');
      } else if (cs === 'closed' || cs === 'disconnected') {
        // disconnected peut être transitoire — on attend le timeout
        // de l'API native, qui passera failed/closed.
      }
    };
    // Fallback : si on n'atteint pas 'connected' en 20 s, on coupe avec
    // un message clair plutôt que de rester bloqué à 0 %.
    state.connectTimer = setTimeout(() => {
      if (pc.connectionState !== 'connected') {
        clientLog('warn', '[p2p] connect timeout (state='
                         + pc.connectionState + ')');
        cleanup(state.peer.deviceId, '✗ Pas de route LAN');
      }
    }, 20000);
  }

  // ====================================================================
  // DataChannel SENDER : envoie meta + chunks de chaque fichier en série
  // ====================================================================
  function wireSenderDc(dc, state) {
    dc.bufferedAmountLowThreshold = BUFFER_LOW;
    dc.onopen  = () => sendNextFile(state);
    dc.onerror = (e) => {
      clientLog('error', '[p2p] dc error: ' + (e && e.message));
      cleanup(state.peer.deviceId, '✗ Erreur DataChannel');
    };
    dc.onclose = () => {
      // Si tous les fichiers ont été envoyés, on cleanup avec succès.
      if (state.currentFileIdx >= state.files.length) {
        cleanup(state.peer.deviceId, '✓ Envoyé');
      }
    };
  }

  async function sendNextFile(state) {
    if (state.currentFileIdx >= state.files.length) {
      // Tout est envoyé — annoncer "all-done" puis fermer.
      try { state.dc.send(JSON.stringify({ kind: 'all-done' })); } catch {}
      setTimeout(() => {
        try { state.dc.close(); } catch {}
      }, 200);
      return;
    }
    const file = state.files[state.currentFileIdx];
    state.startedAt = state.startedAt || Date.now();

    // Annonce meta (JSON) AVANT de pousser le binaire.
    if (state.currentFileIdx === 0) {
      const summary = {
        kind: 'session-meta',
        count: state.files.length,
        totalBytes: state.totalBytes,
      };
      state.dc.send(JSON.stringify(summary));
    }
    const meta = {
      kind: 'file-meta',
      name: file.name,
      size: file.size,
      type: file.type || 'application/octet-stream',
      idx:  state.currentFileIdx,
    };
    state.dc.send(JSON.stringify(meta));

    let offset = 0;
    const reader = file.stream().getReader();
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        // value = Uint8Array. On chunk en CHUNK_SIZE.
        let pos = 0;
        while (pos < value.byteLength) {
          // backpressure
          while (state.dc.bufferedAmount > BUFFER_HIGH) {
            await new Promise((r) => {
              const onLow = () => { state.dc.removeEventListener('bufferedamountlow', onLow); r(); };
              state.dc.addEventListener('bufferedamountlow', onLow);
              setTimeout(onLow, 200);  // safety fallback
            });
          }
          const end = Math.min(pos + CHUNK_SIZE, value.byteLength);
          const slice = value.subarray(pos, end);
          state.dc.send(slice);
          offset       += slice.byteLength;
          state.bytesSent += slice.byteLength;
          pos = end;
          updateProgress(state);
        }
      }
    } catch (e) {
      clientLog('error', '[p2p] read/send failed: ' + (e && e.message));
      cleanup(state.peer.deviceId, '✗ Lecture fichier');
      return;
    }

    state.dc.send(JSON.stringify({ kind: 'file-end', idx: state.currentFileIdx }));
    state.currentFileIdx += 1;
    sendNextFile(state);
  }

  // ====================================================================
  // DataChannel RECEIVER : recompose meta + chunks → Blob → download
  // ====================================================================
  function wireReceiverDc(dc, state) {
    dc.onopen  = () => clientLog('info', '[p2p] receiver dc open');
    dc.onerror = (e) => {
      clientLog('error', '[p2p] receiver dc error: ' + (e && e.message));
      cleanup(state.peer.deviceId, '✗ Erreur DataChannel');
    };
    dc.onclose = () => {
      // À la fin réussie le sender émet all-done puis close —
      // si on a tout reçu, considérer fini.
    };
    dc.onmessage = (ev) => {
      const data = ev.data;
      if (typeof data === 'string') {
        let msg;
        try { msg = JSON.parse(data); }
        catch { return; }
        handleReceiverControl(msg, state);
      } else {
        // Binary chunk pour le fichier courant.
        const cur = state.receivingFiles[state.receivingFiles.length - 1];
        if (!cur) return;
        cur.chunks.push(data);
        cur.received   += data.byteLength;
        state.bytesReceived += data.byteLength;
        updateProgress(state);
      }
    };
  }

  function handleReceiverControl(msg, state) {
    if (msg.kind === 'session-meta') {
      state.totalBytes = msg.totalBytes || 0;
      state.startedAt  = Date.now();
    } else if (msg.kind === 'file-meta') {
      state.receivingFiles.push({
        name: msg.name, size: msg.size, type: msg.type,
        received: 0, chunks: [],
      });
    } else if (msg.kind === 'file-end') {
      finalizeReceivedFile(state);
    } else if (msg.kind === 'all-done') {
      cleanup(state.peer.deviceId, '✓ Reçu');
    }
  }

  function finalizeReceivedFile(state) {
    const cur = state.receivingFiles[state.receivingFiles.length - 1];
    if (!cur) return;
    const blob = new Blob(cur.chunks, { type: cur.type });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href = url;
    a.download = cur.name || 'download';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(url), 2000);
    cur.chunks = [];  // libère la RAM
  }

  // ====================================================================
  // Helpers signaling
  // ====================================================================
  async function postSignal(toDeviceId, type, payload) {
    try {
      const res = await fetch('/api/p2p/signal', {
        method: 'POST',
        credentials: 'same-origin',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ to: toDeviceId, type, payload }),
        keepalive: true,
      });
      if (res.status === 401) {
        if (window.LTR.goToLogin) window.LTR.goToLogin();
      } else if (res.status === 404) {
        clientLog('warn', '[p2p] target offline ' + toDeviceId.substring(0, 8));
      }
    } catch (e) {
      clientLog('error', '[p2p] signal POST failed: ' + (e && e.message));
    }
  }

  // ====================================================================
  // UI : modale réception (bottom-sheet mobile / modal desktop)
  // ====================================================================
  function showIncomingModal(peer, decideCb) {
    const modal = $('#p2p-incoming-modal');
    if (!modal) {
      // Pas d'UI dispo — auto-refuse silencieusement.
      decideCb(false);
      return;
    }
    $('#p2p-incoming-emoji').textContent = peer.emoji;
    $('#p2p-incoming-title').textContent =
      peer.displayName + ' veut t\'envoyer des fichiers';
    $('#p2p-incoming-sub').textContent  = peer.platformLabel;
    modal.hidden = false;

    // Reset TTL bar animation
    const fill = $('#p2p-incoming-ttl-fill');
    if (fill) {
      fill.style.transition = 'none';
      fill.style.width = '100%';
      // force reflow
      void fill.offsetWidth;
      fill.style.transition = `width ${ACCEPT_TTL_MS}ms linear`;
      fill.style.width = '0%';
    }

    const onAccept = () => { close(); decideCb(true); };
    const onRefuse = () => { close(); decideCb(false); };
    function close() {
      modal.hidden = true;
      $('#p2p-incoming-accept').removeEventListener('click', onAccept);
      $('#p2p-incoming-refuse').removeEventListener('click', onRefuse);
    }
    $('#p2p-incoming-accept').addEventListener('click', onAccept);
    $('#p2p-incoming-refuse').addEventListener('click', onRefuse);
  }

  // ====================================================================
  // UI : marquage card peer + sticky bar bas
  // ====================================================================
  function markCardSending(deviceId) {
    const card = document.querySelector(
      `.peer-card[data-device-id="${cssEscape(deviceId)}"]`);
    if (!card) return null;
    card.classList.add('peer-card--sending');
    const sub = card.querySelector('.peer-sub');
    if (sub) sub.textContent = '0 % · …';
    let bar = card.querySelector('.peer-progress-bar');
    if (!bar) {
      bar = document.createElement('div');
      bar.className = 'peer-progress-bar';
      bar.innerHTML = '<span></span>';
      card.appendChild(bar);
    }
    bar.firstElementChild.style.width = '0%';
    return card;
  }

  function updateProgress(state) {
    const total = state.role === 'sender' ? state.totalBytes
                                           : state.totalBytes;
    const done  = state.role === 'sender' ? state.bytesSent
                                           : state.bytesReceived;
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

  function showSticky() { refreshSticky(); }
  function refreshSticky() {
    const bar = $('#p2p-sticky-bar');
    if (!bar) return;
    const active = Array.from(conns.values()).filter(
      (s) => s.startedAt && s.role !== 'idle');
    if (active.length === 0) {
      bar.hidden = true;
      return;
    }
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

  function cleanup(deviceId, label) {
    const state = conns.get(deviceId);
    if (!state) return;
    try { if (state.dc) state.dc.close(); } catch {}
    try { if (state.pc) state.pc.close(); } catch {}
    if (state.uiCard) {
      const sub = state.uiCard.querySelector('.peer-sub');
      if (sub && label) sub.textContent = label;
      state.uiCard.classList.remove('peer-card--sending');
      // Restaurer le platformLabel original après 3s.
      const peer = state.peer;
      setTimeout(() => {
        if (sub) sub.textContent = peer.platformLabel || '';
        const bar = state.uiCard && state.uiCard.querySelector('.peer-progress-bar');
        if (bar) bar.remove();
      }, 3000);
    }
    if (state.ttlTimer) clearTimeout(state.ttlTimer);
    if (state.connectTimer) clearTimeout(state.connectTimer);
    conns.delete(deviceId);
    refreshSticky();
  }

  function cleanupAll() {
    Array.from(conns.keys()).forEach((id) => cleanup(id, ''));
  }

  // Toast simple — on réutilise un container global s'il existe.
  function toast(text, kind) {
    clientLog('info', '[p2p:toast] ' + text);
    const c = $('#p2p-toast');
    if (!c) return;
    c.textContent = text;
    c.dataset.kind = kind || 'info';
    c.hidden = false;
    setTimeout(() => { c.hidden = true; }, 3000);
  }

  // CSS.escape n'est pas dispo partout — fallback simple.
  function cssEscape(s) {
    return String(s).replace(/[^a-zA-Z0-9_-]/g, (c) => '\\' + c);
  }

  window.LTR = window.LTR || {};
  window.LTR.p2p = { startSendTo, handleSignal, cleanupAll };
})();
