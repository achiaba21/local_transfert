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
  // V1.3 — Robustesse
  const WATCHDOG_NO_DATA_MS = 10_000;        // dc open sans 0 byte → ✗
  const NO_ACK_TIMEOUT_MS   = 10_000;        // sender push sans ack → ✗
  const ACK_INTERVAL_MS     = 500;           // receveur émet ack
  const DISCONNECT_TTL_MS   = 15_000;        // pc disconnected dure trop
  const DRAIN_TIMEOUT_MS    = 30_000;        // drain final côté sender

  // Map<connKey, ConnectionState> où connKey = `${deviceId}:${role}`.
  // V1.3 — Lot 4 : permet A↔B simultané (1 entrée sender + 1 entrée
  // receiver pour le même peer chez chaque navigateur).
  const conns = new Map();
  function connKey(deviceId, role) { return deviceId + ':' + role; }
  function getConn(deviceId, role) { return conns.get(connKey(deviceId, role)); }

  // ====================================================================
  // OUTGOING — démarrage d'un envoi A → B
  // ====================================================================
  async function startSendTo(peer, files) {
    const { deviceId } = peer;
    if (!files || files.length === 0) return;
    if (conns.has(connKey(deviceId, 'sender'))) {
      clientLog('warn', '[p2p] envoi déjà en cours vers ' + deviceId);
      toast(`Envoi déjà en cours vers ${peer.displayName}`, 'warning');
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
      bytesAckedByReceiver: 0,    // V1.3 — Lot 3 : vraie progression
      lastAckAt: Date.now(),
      totalBytes: files.reduce((a, f) => a + f.size, 0),
      startedAt: 0,
      uiCard: null,
      phase: 'waiting',
      // V1.3 — Lot 2 : statut par fichier (pour la liste UI persistante)
      fileStatuses: files.map((f, i) => ({
        idx: i, name: f.name, size: f.size,
        status: 'pending', bytes: 0, error: null,
      })),
    };
    conns.set(connKey(deviceId, 'sender'), state);
    state.uiCard = markCardSending(deviceId, 'waiting');
    showSticky();
    // TTL côté émetteur : si pas connecté en 90s, on abandonne.
    state.senderTtl = setTimeout(() => {
      if (state.phase !== 'sending') {
        clientLog('warn', '[p2p] sender TTL — pas de réponse');
        toast(`${peer.displayName} n'a pas répondu`, 'warning');
        postSignal(deviceId, 'cancel', { reason: 'sender-ttl' });
        cleanup(state, 'Pas de réponse');
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
      cleanup(state, '✗ Erreur P2P');
    }
  }

  // ====================================================================
  // INCOMING — réception d'un signal du host
  // ====================================================================
  async function handleSignal(msg) {
    const { from, type, payload } = msg;
    if (!from) return;

    // L'offer crée toujours un nouveau state receiver chez nous.
    if (type === 'offer') {
      const peer = window.LTR.peers && window.LTR.peers.getPeer(from);
      if (!peer) {
        clientLog('warn', '[p2p] offer from unknown peer ' + from);
        return;
      }
      await handleIncomingOffer(peer, payload);
      return;
    }

    // V1.3 — Lot 4 : routage par rôle.
    //  - answer / ice (réponse à mon offer)  → state sender chez moi
    //  - ack (feedback de mes envois)         → state sender
    //  - refuse (ma demande refusée)          → state sender
    //  - cancel / bye : ambigu, on essaie les 2 rôles
    let state = null;
    if (type === 'answer' || type === 'ack' || type === 'refuse') {
      state = getConn(from, 'sender');
    } else if (type === 'cancel' || type === 'bye') {
      state = getConn(from, 'sender') || getConn(from, 'receiver');
    } else if (type === 'ice') {
      // ICE peut venir des deux sens : on essaie le rôle qui a une pc.
      state = getConn(from, 'receiver') || getConn(from, 'sender');
    }
    if (!state) {
      clientLog('warn', '[p2p] signal type=' + type
                       + ' no state for ' + from.substring(0, 8));
      return;
    }

    if (type === 'answer') {
      try {
        await state.pc.setRemoteDescription(payload.sdp);
      } catch (e) { clientLog('error', '[p2p] setRemote(answer): ' + e); }
    } else if (type === 'ice') {
      if (!state.pc) {
        if (state.pendingIceQueue) {
          state.pendingIceQueue.push(payload.candidate);
        }
        return;
      }
      try {
        await state.pc.addIceCandidate(payload.candidate);
      } catch (e) { /* ignore — candidates can fail benignly */ }
    } else if (type === 'ack') {
      // V1.3 — Lot 3 : ack du receveur. L'émetteur connait la vraie
      // progression côté récepteur (vs son bytesSent local).
      if (typeof payload.bytes === 'number') {
        state.bytesAckedByReceiver = payload.bytes;
        state.lastAckAt = Date.now();
        updateProgress(state);
      }
    } else if (type === 'refuse') {
      clientLog('info', '[p2p] receiver refused');
      toast(`${state.peer.displayName} a refusé`, 'warning');
      cleanup(state, 'Refusé');
    } else if (type === 'cancel' || type === 'bye') {
      cleanup(state, 'Annulé');
    }
  }

  async function handleIncomingOffer(peer, payload) {
    const state = {
      role: 'receiver',
      peer,
      pc: null,
      dc: null,
      pendingOfferSdp: payload.sdp,
      pendingIceQueue: [],
      receivingFiles: [],
      currentFileIdx: 0,
      bytesReceived: 0,
      totalBytes: 0,
      filesMeta: null,
      startedAt: 0,
      uiCard: null,
      ttlTimer: null,
      // V1.3 — Lot 2 : statut par fichier sur le receveur aussi
      // (alimenté à la réception de file-meta).
      fileStatuses: [],
    };
    conns.set(connKey(peer.deviceId, 'receiver'), state);

    state.ttlTimer = setTimeout(() => {
      if (state.pc === null) {
        clientLog('warn', '[p2p] offer TTL expired');
        postSignal(peer.deviceId, 'refuse', { reason: 'ttl' });
        cleanup(state, 'Expiré');
      }
    }, ACCEPT_TTL_MS);

    showIncomingModal(peer, async (accepted) => {
      clearTimeout(state.ttlTimer);
      if (!accepted) {
        await postSignal(peer.deviceId, 'refuse', {});
        cleanup(state, 'Refusé');
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
        state.uiCard = markCardSending(peer.deviceId, 'connecting');
        showSticky();
      } catch (e) {
        clientLog('error', '[p2p] accept failed: ' + (e && e.message));
        cleanup(state, '✗ Connexion P2P échouée');
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
        cleanup(state, '✗ Connexion P2P échouée');
      } else if (cs === 'disconnected') {
        // V1.3 — Lot 1 : flottement Wi-Fi transitoire. On marque la
        // pause (sends gelés via safeSend), avec un timer de grâce
        // de DISCONNECT_TTL_MS. Si on revient à 'connected', on
        // reset. Sinon, on cleanup avec message clair.
        if (!state.disconnectedSince) {
          state.disconnectedSince = Date.now();
          if (state.uiCard) {
            const sub = state.uiCard.querySelector('.peer-sub');
            if (sub) sub.textContent = 'Connexion perdue…';
          }
          state.disconnectTimer = setTimeout(() => {
            if (state.disconnectedSince) {
              cleanup(state, '✗ Wi-Fi perdu');
            }
          }, DISCONNECT_TTL_MS);
        }
      } else if (cs === 'connected') {
        // Reprise : annule la pause si elle était active.
        if (state.disconnectedSince) {
          state.disconnectedSince = 0;
          if (state.disconnectTimer) {
            clearTimeout(state.disconnectTimer);
            state.disconnectTimer = null;
          }
          if (state.uiCard && state.phase === 'sending') {
            setCardPhase(state.uiCard, 'sending');
          }
        }
      }
    };
    state.connectTimer = setTimeout(() => {
      if (pc.connectionState !== 'connected') {
        clientLog('warn', '[p2p] connect timeout (state='
                         + pc.connectionState + ')');
        cleanup(state, '✗ Pas de route LAN');
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
      // V1.3 — Lot 3 : démarre le watchdog d'ack côté émetteur.
      // Si on push des bytes mais le receveur ne renvoie pas
      // d'ack pendant NO_ACK_TIMEOUT_MS, on déclare silent stall.
      state.lastAckAt = Date.now();
      state.ackWatchdog = setInterval(() => {
        const now = Date.now();
        if (state.bytesSent > state.bytesAckedByReceiver
            && now - state.lastAckAt > NO_ACK_TIMEOUT_MS) {
          clientLog('warn', '[p2p] sender silent stall — pas d\'ack');
          cleanup(state, '✗ Récepteur muet');
        }
      }, 1000);
      sendNextFile(state);
    };
    dc.onerror = (e) => {
      if (state.allFilesSent) {
        cleanup(state, '✓ Envoyé');
        return;
      }
      clientLog('error', '[p2p] dc error: ' + (e && e.message));
      cleanup(state, '✗ Erreur DataChannel');
    };
    dc.onclose = () => {
      cleanup(state, state.allFilesSent ? '✓ Envoyé' : '✗ Connexion fermée');
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
  // V1.3 — Lot 1 : pause si state.disconnectedSince est set (flottement
  // Wi-Fi). On attend que le pc revienne à 'connected' avant de réessayer.
  // Retourne true en succès, false en échec (le caller déclenche cleanup).
  async function safeSend(state, data, label) {
    while (state.disconnectedSince
           && Date.now() - state.disconnectedSince < DISCONNECT_TTL_MS) {
      await new Promise((r) => setTimeout(r, 200));
    }
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
      // Tout est envoyé — annoncer all-done, attendre drain, fermer.
      await awaitDrain(state.dc);
      await safeSend(state, JSON.stringify({ kind: 'all-done' }), 'all-done');
      state.allFilesSent = true;
      // V1.3 — Lot 1 : timeout drain final 30 s.
      const drainStart = Date.now();
      while (state.dc && state.dc.readyState === 'open'
             && state.dc.bufferedAmount > 0) {
        if (Date.now() - drainStart > DRAIN_TIMEOUT_MS) {
          clientLog('warn', '[p2p] drain timeout — close anyway');
          break;
        }
        await new Promise((r) => setTimeout(r, 50));
      }
      try { state.dc.close(); } catch {}
      return;
    }
    const file = state.files[state.currentFileIdx];
    const fs = state.fileStatuses[state.currentFileIdx];
    state.startedAt = state.startedAt || Date.now();
    fs.status = 'sending';

    await awaitDrain(state.dc);

    if (state.currentFileIdx === 0) {
      const summary = {
        kind: 'session-meta',
        count: state.files.length,
        totalBytes: state.totalBytes,
      };
      if (!await safeSend(state, JSON.stringify(summary), 'session-meta')) {
        fs.status = 'failed'; fs.error = 'session-meta';
        cleanup(state, '✗ Erreur réseau');
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
    if (!await safeSend(state, JSON.stringify(meta), 'file-meta')) {
      fs.status = 'failed'; fs.error = 'meta';
      // V1.3 — Lot 2 : continuer aux fichiers suivants au lieu de
      // cleanup global. On skip celui-ci.
      state.currentFileIdx += 1;
      sendNextFile(state).catch((e) =>
        clientLog('error', '[p2p] sendNextFile rec: ' + (e && e.message)));
      return;
    }

    const reader = file.stream().getReader();
    let aborted = false;
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        let pos = 0;
        while (pos < value.byteLength) {
          await awaitDrain(state.dc);
          const end = Math.min(pos + CHUNK_SIZE, value.byteLength);
          const slice = new Uint8Array(value.buffer,
                                        value.byteOffset + pos,
                                        end - pos).slice();
          if (!await safeSend(state, slice, 'chunk')) {
            aborted = true;
            try { reader.cancel(); } catch {}
            break;
          }
          state.bytesSent += slice.byteLength;
          fs.bytes = state.bytesSent;
          pos = end;
          updateProgress(state);
        }
        if (aborted) break;
      }
    } catch (e) {
      clientLog('error', '[p2p] read failed: ' + (e && e.message));
      fs.status = 'failed'; fs.error = 'read';
      // Skip ce fichier, continue les suivants.
      state.currentFileIdx += 1;
      sendNextFile(state).catch(() => {});
      return;
    }

    if (aborted) {
      fs.status = 'failed'; fs.error = 'send';
      state.currentFileIdx += 1;
      sendNextFile(state).catch(() => {});
      return;
    }

    await awaitDrain(state.dc);
    if (!await safeSend(state, JSON.stringify({
        kind: 'file-end', idx: state.currentFileIdx }), 'file-end')) {
      fs.status = 'failed'; fs.error = 'end';
      state.currentFileIdx += 1;
      sendNextFile(state).catch(() => {});
      return;
    }
    fs.status = 'sent';
    state.currentFileIdx += 1;
    sendNextFile(state).catch((e) =>
      clientLog('error', '[p2p] sendNextFile rec: ' + (e && e.message)));
  }

  // ====================================================================
  // DataChannel RECEIVER : recompose meta + chunks → Blob → download
  // ====================================================================
  function wireReceiverDc(dc, state) {
    dc.onopen  = () => {
      clientLog('info', '[p2p] receiver dc open');
      // V1.3 — Lot 1 : watchdog. DataChannel ouvert mais 0 byte reçu
      // dans WATCHDOG_NO_DATA_MS → cleanup au lieu d'attendre.
      state.noDataWatchdog = setTimeout(() => {
        if (state.bytesReceived === 0) {
          clientLog('warn', '[p2p] receiver no-data watchdog fired');
          cleanup(state, '✗ Pas de données');
        }
      }, WATCHDOG_NO_DATA_MS);
      // V1.3 — Lot 3 : envoi périodique d'ack vers l'émetteur.
      // Permet à l'émetteur d'avoir une vraie progression et de
      // détecter les silent stalls de son côté.
      state.ackTimer = setInterval(() => {
        if (dc.readyState !== 'open') return;
        try {
          dc.send(JSON.stringify({
            kind: 'ack', bytes: state.bytesReceived }));
        } catch { /* dc ferme : on s'en fout */ }
      }, ACK_INTERVAL_MS);
    };
    dc.onerror = (e) => {
      if (state.allDoneSeen) {
        cleanup(state, '✓ Reçu');
        return;
      }
      clientLog('error', '[p2p] receiver dc error: ' + (e && e.message));
      cleanup(state, '✗ Erreur DataChannel');
    };
    dc.onclose = () => {
      cleanup(state, state.allDoneSeen ? '✓ Reçu' : '✗ Connexion fermée');
    };
    dc.onmessage = (ev) => {
      const data = ev.data;
      if (typeof data === 'string') {
        let msg;
        try { msg = JSON.parse(data); }
        catch { return; }
        handleReceiverControl(msg, state);
      } else {
        const cur = state.receivingFiles[state.receivingFiles.length - 1];
        if (!cur) return;
        cur.chunks.push(data);
        cur.received   += data.byteLength;
        state.bytesReceived += data.byteLength;
        updateProgress(state);
        // 1er chunk reçu : on annule le watchdog no-data.
        if (state.noDataWatchdog) {
          clearTimeout(state.noDataWatchdog);
          state.noDataWatchdog = null;
        }
      }
    };
  }

  function handleReceiverControl(msg, state) {
    if (msg.kind === 'session-meta') {
      state.totalBytes = msg.totalBytes || 0;
      state.startedAt  = Date.now();
    } else if (msg.kind === 'file-meta') {
      const idx = state.fileStatuses.length;
      state.receivingFiles.push({
        name: msg.name, size: msg.size, type: msg.type,
        received: 0, chunks: [],
      });
      // V1.3 — Lot 2 : statut par fichier côté receveur aussi.
      state.fileStatuses.push({
        idx, name: msg.name, size: msg.size,
        status: 'sending', bytes: 0, error: null,
      });
    } else if (msg.kind === 'file-end') {
      finalizeReceivedFile(state);
    } else if (msg.kind === 'all-done') {
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
    const fs = state.fileStatuses[state.fileStatuses.length - 1];
    // V1.3 — Lot 1 : intégrité. Si la taille reçue ne matche pas la
    // taille annoncée, on marque le fichier ✗ et on ne télécharge PAS.
    if (cur.size && cur.received !== cur.size) {
      clientLog('warn', '[p2p] file truncated: ' + cur.name
                       + ' received=' + cur.received + ' expected=' + cur.size);
      if (fs) { fs.status = 'failed'; fs.error = 'taille_invalide'; }
      cur.chunks = [];
      return;
    }
    const blob = new Blob(cur.chunks, { type: cur.type });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href = url;
    a.download = cur.name || 'download';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(url), 2000);
    cur.chunks = [];
    if (fs) { fs.status = 'received'; fs.bytes = cur.received; }
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
        const state = getConn(toDeviceId, 'sender');
        if (state && state.phase !== 'sending') {
          const name = state.peer ? state.peer.displayName : 'Le destinataire';
          toast(`${name} n'est plus connecté`, 'warning');
          cleanup(state, 'Hors-ligne');
        }
      }
    } catch (e) {
      clientLog('error', '[p2p] signal POST failed: ' + (e && e.message));
    }
  }

  // Annulation côté émetteur déclenchée par le bouton ✕ de la card.
  function cancelOutgoing(deviceId) {
    const state = getConn(deviceId, 'sender');
    if (!state) return;
    clientLog('info', '[p2p] sender cancel ' + deviceId.substring(0, 8));
    postSignal(deviceId, 'cancel', { reason: 'user' });
    cleanup(state, 'Annulé');
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
    const total = state.totalBytes;
    // V1.3 — Lot 3 : sender utilise la VRAIE progression (ack du
    // receveur) au lieu de bytesSent local. Si pas encore d'ack,
    // fallback sur bytesSent.
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

  // V1.3 — Lot 4 : cleanup prend désormais un state directement.
  // Permet d'éviter la confusion avec deviceId quand 2 connexions
  // existent pour le même peer (sender + receiver).
  function cleanup(state, label) {
    if (!state || state._cleaned) return;
    state._cleaned = true;
    try { if (state.dc) state.dc.close(); } catch {}
    try { if (state.pc) state.pc.close(); } catch {}
    if (state.uiCard) {
      const sub = state.uiCard.querySelector('.peer-sub');
      if (sub && label) sub.textContent = label;
      state.uiCard.classList.remove('peer-card--sending');
      const peer = state.peer;
      setTimeout(() => {
        if (sub) sub.textContent = peer.platformLabel || '';
        const bar = state.uiCard && state.uiCard.querySelector('.peer-progress-bar');
        if (bar) bar.remove();
      }, 3000);
    }
    if (state.ttlTimer)        clearTimeout(state.ttlTimer);
    if (state.connectTimer)    clearTimeout(state.connectTimer);
    if (state.senderTtl)       clearTimeout(state.senderTtl);
    if (state.disconnectTimer) clearTimeout(state.disconnectTimer);
    if (state.noDataWatchdog)  clearTimeout(state.noDataWatchdog);
    if (state.ackTimer)        clearInterval(state.ackTimer);
    if (state.ackWatchdog)     clearInterval(state.ackWatchdog);
    if (state.uiCard) {
      const x = state.uiCard.querySelector('.peer-cancel-btn');
      if (x) x.remove();
    }
    if (state.peer && state.role) {
      conns.delete(connKey(state.peer.deviceId, state.role));
    }
    refreshSticky();
  }

  function cleanupAll() {
    Array.from(conns.values()).forEach((s) => cleanup(s, ''));
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
