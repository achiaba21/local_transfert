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

  // V1.2.1 : 16 KB (au lieu de 64 KB) pour compat Safari. Safari ne
  // négocie pas toujours `a=max-message-size` dans le SDP, donc le SCTP
  // retombe au défaut RFC 8831 = 16 384 octets. Au-delà, Safari drop
  // silencieusement → le receveur ne voit rien arriver.
  const CHUNK_SIZE   = 16 * 1024;            // 16 KB
  const BUFFER_HIGH  = 1 * 1024 * 1024;      // 1 MB seuil backpressure
  const BUFFER_LOW   = 256 * 1024;           // attendre que ça redescende
  const ACCEPT_TTL_MS = 60_000;              // 60s pour accepter
  const SENDER_TTL_MS = 90_000;              // 90s : abandon si pas connecté

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
      phase: 'waiting',
    };
    conns.set(deviceId, state);
    state.uiCard = markCardSending(deviceId, 'waiting');
    showSticky();
    // TTL côté émetteur : si pas connecté en 90s, on abandonne
    // proprement (le receveur n'a peut-être jamais cliqué Accepter
    // ou a fermé son onglet).
    state.senderTtl = setTimeout(() => {
      if (state.phase !== 'sending') {
        clientLog('warn', '[p2p] sender TTL — pas de réponse');
        toast(`${peer.displayName} n'a pas répondu`, 'warning');
        postSignal(deviceId, 'cancel', { reason: 'sender-ttl' });
        cleanup(deviceId, 'Pas de réponse');
      }
    }, SENDER_TTL_MS);

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
      const ics = pc.iceConnectionState;
      clientLog('info', '[p2p] ice=' + ics
                       + ' for ' + state.peer.deviceId.substring(0, 8));
      // Transition phase côté émetteur : waiting (offer envoyée) →
      // connecting (ICE en cours) → sending (DataChannel ouvert).
      if (state.role === 'sender' && state.phase === 'waiting'
          && (ics === 'checking' || ics === 'connected')) {
        state.phase = 'connecting';
        if (state.uiCard) setCardPhase(state.uiCard, 'connecting');
      }
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
    dc.onopen  = () => {
      state.phase = 'sending';
      if (state.uiCard) setCardPhase(state.uiCard, 'sending');
      sendNextFile(state);
    };
    dc.onerror = (e) => {
      // Si tout a été envoyé avant l'erreur, c'est probablement le
      // receveur qui a fermé sa pc → on traite comme succès, pas erreur.
      if (state.allFilesSent) {
        cleanup(state.peer.deviceId, '✓ Envoyé');
        return;
      }
      clientLog('error', '[p2p] dc error: ' + (e && e.message));
      cleanup(state.peer.deviceId, '✗ Erreur DataChannel');
    };
    dc.onclose = () => {
      cleanup(state.peer.deviceId,
              state.allFilesSent ? '✓ Envoyé' : '✗ Connexion fermée');
    };
  }

  // Attend que le buffer SCTP soit sous BUFFER_HIGH avant le prochain
  // send. Utilisé entre chunks ET avant chaque message JSON (file-meta,
  // file-end, all-done) pour éviter qu'un dc.send déborde le buffer
  // côté Safari → OperationError silencieux → multi-fichier cassé.
  async function awaitDrain(dc) {
    while (dc.readyState === 'open' && dc.bufferedAmount > BUFFER_HIGH) {
      await new Promise((r) => {
        const onLow = () => {
          dc.removeEventListener('bufferedamountlow', onLow);
          r();
        };
        dc.addEventListener('bufferedamountlow', onLow);
        setTimeout(onLow, 200);
      });
    }
  }

  // Send unifié : check readyState, try/catch explicite avec log précis.
  // Retourne true en succès, false en échec (le caller déclenche cleanup).
  function safeSend(state, data, label) {
    if (!state.dc || state.dc.readyState !== 'open') {
      clientLog('warn', '[p2p] send skipped (' + label
                       + ') — dc state=' + (state.dc && state.dc.readyState));
      return false;
    }
    try {
      state.dc.send(data);
      return true;
    } catch (e) {
      clientLog('error', '[p2p] send failed (' + label + '): '
                       + (e && e.message));
      return false;
    }
  }

  async function sendNextFile(state) {
    if (state.currentFileIdx >= state.files.length) {
      // Tout est envoyé — annoncer "all-done", attendre que le buffer
      // soit vidé, puis fermer. Le flag allFilesSent permet à
      // dc.onerror/onclose de traiter une fermeture distante comme
      // succès au lieu d'erreur.
      await awaitDrain(state.dc);
      safeSend(state, JSON.stringify({ kind: 'all-done' }), 'all-done');
      state.allFilesSent = true;
      while (state.dc && state.dc.readyState === 'open'
             && state.dc.bufferedAmount > 0) {
        await new Promise((r) => setTimeout(r, 50));
      }
      try { state.dc.close(); } catch {}
      return;
    }
    const file = state.files[state.currentFileIdx];
    state.startedAt = state.startedAt || Date.now();

    // Backpressure AVANT chaque JSON (le buffer peut être plein des
    // chunks du fichier précédent). Sans ça, multi-fichier casse.
    await awaitDrain(state.dc);

    if (state.currentFileIdx === 0) {
      const summary = {
        kind: 'session-meta',
        count: state.files.length,
        totalBytes: state.totalBytes,
      };
      if (!safeSend(state, JSON.stringify(summary), 'session-meta')) {
        cleanup(state.peer.deviceId, '✗ Erreur réseau');
        return;
      }
    }
    const meta = {
      kind: 'file-meta',
      name: file.name,
      size: file.size,
      type: file.type || 'application/octet-stream',
      idx:  state.currentFileIdx,
    };
    if (!safeSend(state, JSON.stringify(meta), 'file-meta')) {
      cleanup(state.peer.deviceId, '✗ Erreur réseau');
      return;
    }

    const reader = file.stream().getReader();
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        let pos = 0;
        while (pos < value.byteLength) {
          await awaitDrain(state.dc);
          const end = Math.min(pos + CHUNK_SIZE, value.byteLength);
          // Copie défensive (subarray = vue partagée) pour éviter
          // que Safari interprète mal le buffer si la prochaine
          // itération overwrite la zone source.
          const slice = new Uint8Array(value.buffer,
                                        value.byteOffset + pos,
                                        end - pos).slice();
          if (!safeSend(state, slice, 'chunk')) {
            cleanup(state.peer.deviceId, '✗ Erreur réseau');
            try { reader.cancel(); } catch {}
            return;
          }
          state.bytesSent += slice.byteLength;
          pos = end;
          updateProgress(state);
        }
      }
    } catch (e) {
      clientLog('error', '[p2p] read failed: ' + (e && e.message));
      cleanup(state.peer.deviceId, '✗ Lecture fichier');
      return;
    }

    await awaitDrain(state.dc);
    if (!safeSend(state, JSON.stringify({
        kind: 'file-end', idx: state.currentFileIdx }), 'file-end')) {
      cleanup(state.peer.deviceId, '✗ Erreur réseau');
      return;
    }
    state.currentFileIdx += 1;
    sendNextFile(state).catch((e) =>
      clientLog('error', '[p2p] sendNextFile rec failed: '
                       + (e && e.message)));
  }

  // ====================================================================
  // DataChannel RECEIVER : recompose meta + chunks → Blob → download
  // ====================================================================
  function wireReceiverDc(dc, state) {
    dc.onopen  = () => clientLog('info', '[p2p] receiver dc open');
    dc.onerror = (e) => {
      if (state.allDoneSeen) {
        cleanup(state.peer.deviceId, '✓ Reçu');
        return;
      }
      clientLog('error', '[p2p] receiver dc error: ' + (e && e.message));
      cleanup(state.peer.deviceId, '✗ Erreur DataChannel');
    };
    dc.onclose = () => {
      // Le sender ferme proprement après avoir flushé all-done.
      cleanup(state.peer.deviceId,
              state.allDoneSeen ? '✓ Reçu' : '✗ Connexion fermée');
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
      // Marque l'UI ✓ Reçu mais NE PAS fermer ici. C'est l'émetteur
      // qui ferme proprement après flush ; notre dc.onclose finalisera
      // le cleanup. Fermer ici = race qui fait apparaitre "Erreur
      // DataChannel" côté émetteur.
      state.allDoneSeen = true;
      if (state.uiCard) {
        const sub = state.uiCard.querySelector('.peer-sub');
        if (sub) sub.textContent = '✓ Reçu';
      }
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
        // Le destinataire n'est plus connecté (logout, onglet fermé,
        // session expirée). On notifie l'utilisateur côté émetteur
        // au lieu de rester bloqué silencieusement.
        clientLog('warn', '[p2p] target offline ' + toDeviceId.substring(0, 8));
        const state = conns.get(toDeviceId);
        if (state && state.role === 'sender'
            && state.phase !== 'sending') {
          const name = state.peer ? state.peer.displayName : 'Le destinataire';
          toast(`${name} n'est plus connecté`, 'warning');
          cleanup(toDeviceId, 'Hors-ligne');
        }
      }
    } catch (e) {
      clientLog('error', '[p2p] signal POST failed: ' + (e && e.message));
    }
  }

  // Annulation côté émetteur déclenchée par le bouton ✕ de la card.
  // Émet un signal cancel vers le receveur (qui ferme sa modale ou
  // sa connexion) et nettoie l'état local.
  function cancelOutgoing(deviceId) {
    const state = conns.get(deviceId);
    if (!state || state.role !== 'sender') return;
    clientLog('info', '[p2p] sender cancel ' + deviceId.substring(0, 8));
    postSignal(deviceId, 'cancel', { reason: 'user' });
    cleanup(deviceId, 'Annulé');
  }

  // ====================================================================
  // UI : modale réception (bottom-sheet mobile / modal desktop)
  //
  // File d'attente : si une offre arrive pendant qu'une modale est
  // ouverte, on l'empile au lieu d'écraser. La 1ère décision traitée,
  // on dépile la suivante. Sans ça, les listeners s'accumulent et un
  // clic Accepter fait accepter plusieurs offres simultanément.
  // ====================================================================
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

  // ====================================================================
  // UI : marquage card peer + sticky bar bas
  // ====================================================================
  function markCardSending(deviceId, phase = 'waiting') {
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
    // Bouton ✕ pour annuler. Une seule fois — le handler est posé ici.
    if (!card.querySelector('.peer-cancel-btn')) {
      const x = document.createElement('button');
      x.type = 'button';
      x.className = 'peer-cancel-btn';
      x.textContent = '✕';
      x.setAttribute('aria-label', 'Annuler le transfert');
      x.addEventListener('click', (ev) => {
        ev.stopPropagation();
        cancelOutgoing(deviceId);
      });
      card.appendChild(x);
    }
    return card;
  }

  // Met à jour le sous-label de la card selon la phase courante.
  function setCardPhase(card, phase) {
    const sub = card.querySelector('.peer-sub');
    if (!sub) return;
    if (phase === 'waiting')         sub.textContent = 'En attente de réponse…';
    else if (phase === 'connecting') sub.textContent = 'Connexion…';
    else if (phase === 'sending')    sub.textContent = '0 % · …';
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
    if (state.senderTtl) clearTimeout(state.senderTtl);
    if (state.uiCard) {
      const x = state.uiCard.querySelector('.peer-cancel-btn');
      if (x) x.remove();
    }
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
