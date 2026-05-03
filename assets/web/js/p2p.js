// ============================================================
// p2p.js — Orchestrator P2P (V1.6.1, refactor split)
//
// Coordonne les 3 modules :
//   - p2p_transport.js  (RTCPeerConnection, ICE, signaling)
//   - p2p_session.js    (sendNextFile, watchdogs, cleanup)
//   - p2p_ui.js         (modale, sticky, toast)
//
// API publique exposée sur window.LTR.p2p :
//   { startSendTo, handleSignal, cleanupAll, toast }
// ============================================================
(function () {
  'use strict';
  const { clientLog } = window.LTR;

  // V1.4 — TTL côté émetteur (pas connecté en N s → abandon).
  const SENDER_TTL_MS = 90_000;
  // V1.2 — TTL côté receveur (pas accepté en N s → auto-refuse).
  const ACCEPT_TTL_MS = 60_000;

  function transport() { return window.LTR.p2pTransport; }
  function session()   { return window.LTR.p2pSession; }
  function ui()        { return window.LTR.p2pUi; }

  // ====================================================================
  // OUTGOING — démarrage d'un envoi A → B
  // ====================================================================
  async function startSendTo(peer, files) {
    const T = transport();
    const S = session();
    const U = ui();
    const { deviceId } = peer;
    if (!files || files.length === 0) return;
    if (T.conns.has(T.connKey(deviceId, 'sender'))) {
      clientLog('warn', '[p2p] envoi déjà en cours vers ' + deviceId);
      U.toast(`Envoi déjà en cours vers ${peer.displayName}`, 'warning');
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
      bytesAckedByReceiver: 0,
      lastAckAt: Date.now(),
      totalBytes: files.reduce((a, f) => a + f.size, 0),
      startedAt: 0,
      uiCard: null,
      phase: 'waiting',
      fileStatuses: files.map((f, i) => ({
        idx: i, name: f.name, size: f.size,
        status: 'pending', bytes: 0, error: null,
        entryId: null,
      })),
    };
    if (window.LTR.transferRegistry) {
      files.forEach((f, i) => {
        const id = window.LTR.transferRegistry.addEntry({
          direction: 'out', peer,
          name: f.name, size: f.size, file: f,
        });
        state.fileStatuses[i].entryId = id;
      });
    }
    T.conns.set(T.connKey(deviceId, 'sender'), state);
    state.uiCard = U.markCardSending(deviceId, 'waiting');
    U.showSticky();
    state.senderTtl = setTimeout(() => {
      if (state.phase !== 'sending') {
        clientLog('warn', '[p2p] sender TTL — pas de réponse');
        U.toast(`${peer.displayName} n'a pas répondu`, 'warning');
        T.postSignal(deviceId, 'cancel', { reason: 'sender-ttl' });
        S.cleanup(state, 'Pas de réponse');
      }
    }, SENDER_TTL_MS);

    try {
      const pc = new RTCPeerConnection(T.RTC_CONFIG);
      state.pc = pc;
      T.wirePcCommon(pc, state);

      const dc = pc.createDataChannel('ltr', { ordered: true });
      state.dc = dc;
      dc.binaryType = 'arraybuffer';
      S.wireSenderDc(dc, state);

      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      await T.postSignal(deviceId, 'offer', { sdp: offer });
      clientLog('info', '[p2p] offer sent to ' + deviceId.substring(0, 8));
    } catch (e) {
      clientLog('error', '[p2p] startSendTo failed: ' + (e && e.message));
      S.cleanup(state, '✗ Erreur P2P');
    }
  }

  // ====================================================================
  // INCOMING — réception d'un signal du host (SSE p2p-signal)
  // ====================================================================
  async function handleSignal(msg) {
    const T = transport();
    const S = session();
    const U = ui();
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

    let state = null;
    if (type === 'answer' || type === 'ack' || type === 'refuse') {
      state = T.getConn(from, 'sender');
    } else if (type === 'cancel' || type === 'bye') {
      state = T.getConn(from, 'sender') || T.getConn(from, 'receiver');
    } else if (type === 'ice') {
      state = T.getConn(from, 'receiver') || T.getConn(from, 'sender');
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
        if (state.pendingIceQueue) state.pendingIceQueue.push(payload.candidate);
        return;
      }
      try {
        await state.pc.addIceCandidate(payload.candidate);
      } catch (e) { /* benign */ }
    } else if (type === 'ack') {
      if (typeof payload.bytes === 'number') {
        state.bytesAckedByReceiver = payload.bytes;
        state.lastAckAt = Date.now();
        U.updateProgress(state);
      }
    } else if (type === 'refuse') {
      clientLog('info', '[p2p] receiver refused');
      U.toast(`${state.peer.displayName} a refusé`, 'warning');
      S.cleanup(state, 'Refusé');
    } else if (type === 'cancel' || type === 'bye') {
      S.cleanup(state, 'Annulé');
    }
  }

  async function handleIncomingOffer(peer, payload) {
    const T = transport();
    const S = session();
    const U = ui();
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
      fileStatuses: [],
    };
    T.conns.set(T.connKey(peer.deviceId, 'receiver'), state);

    state.ttlTimer = setTimeout(() => {
      if (state.pc === null) {
        clientLog('warn', '[p2p] offer TTL expired');
        T.postSignal(peer.deviceId, 'refuse', { reason: 'ttl' });
        S.cleanup(state, 'Expiré');
      }
    }, ACCEPT_TTL_MS);

    U.showIncomingModal(peer, async (accepted) => {
      clearTimeout(state.ttlTimer);
      if (!accepted) {
        await T.postSignal(peer.deviceId, 'refuse', {});
        S.cleanup(state, 'Refusé');
        return;
      }
      try {
        const pc = new RTCPeerConnection(T.RTC_CONFIG);
        state.pc = pc;
        T.wirePcCommon(pc, state);
        pc.ondatachannel = (ev) => {
          state.dc = ev.channel;
          state.dc.binaryType = 'arraybuffer';
          S.wireReceiverDc(state.dc, state);
        };
        await pc.setRemoteDescription(state.pendingOfferSdp);
        for (const c of state.pendingIceQueue) {
          try { await pc.addIceCandidate(c); } catch {}
        }
        state.pendingIceQueue = [];
        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        await T.postSignal(peer.deviceId, 'answer', { sdp: answer });
        state.startedAt = Date.now();
        state.uiCard = U.markCardSending(peer.deviceId, 'connecting');
        U.showSticky();
      } catch (e) {
        clientLog('error', '[p2p] accept failed: ' + (e && e.message));
        S.cleanup(state, '✗ Connexion P2P échouée');
      }
    });
  }

  // ====================================================================
  // API publique — préservée à 100% par rapport à V1.5
  // ====================================================================
  window.LTR = window.LTR || {};
  window.LTR.p2p = {
    startSendTo,
    handleSignal,
    cleanupAll: () => session().cleanupAll(),
    toast:      (text, kind) => ui().toast(text, kind),
  };
})();
