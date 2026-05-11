// ============================================================
// p2p_transport.js — Couche transport WebRTC (V1.6.1)
//
// Responsabilités :
//  - Map<connKey, ConnectionState> partagée (conns)
//  - Configuration RTC (LAN-only V1)
//  - wirePcCommon : ICE candidates, state changes, disconnect timer
//  - postSignal : POST /api/p2p/signal + 401/404 handling
//  - cancelOutgoing : signal cancel + cleanup
//
// Dépendances dynamiques (lookup window.LTR.p2pXxx.fn au moment du call)
// pour éviter les ordering issues entre IIFE.
// ============================================================
(function () {
  'use strict';
  const { clientLog } = window.LTR;

  const RTC_CONFIG = { iceServers: [] };
  const DISCONNECT_TTL_MS = 30_000;
  const CONNECT_TIMEOUT_MS = 20_000;
  // V1.6.5 — Sprint Stabilité (Wave 2 item G) : iceRestart rapide si la
  // connexion passe 'disconnected' avant que DISCONNECT_TTL ne tue.
  const ICE_RESTART_DELAY_MS = 1_200;

  // Map<connKey, ConnectionState> — partagée. connKey = "${deviceId}:${role}".
  const conns = new Map();
  function connKey(deviceId, role) { return deviceId + ':' + role; }
  function getConn(deviceId, role) { return conns.get(connKey(deviceId, role)); }

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
      if (state.role === 'sender' && state.phase === 'waiting'
          && (ics === 'checking' || ics === 'connected')) {
        state.phase = 'connecting';
        if (state.uiCard && window.LTR.p2pUi) {
          window.LTR.p2pUi.setCardPhase(state.uiCard, 'connecting');
        }
      }
    };
    pc.onconnectionstatechange = () => {
      const cs = pc.connectionState;
      clientLog('info', '[p2p] pc state=' + cs
                       + ' for ' + state.peer.deviceId.substring(0, 8));
      // V1.6.5 — Sprint Stabilité (Wave 1, item A bis P2P).
      // Dès pc.connectionState='connecting', forcer phase='connecting' pour
      // que l'UI affiche « Connexion en cours » plutôt que rester figée en
      // « En attente ». Avant ce fix, la phase ne basculait que dans
      // oniceconnectionstatechange (qui peut être en retard sur Safari iOS).
      if (state.role === 'sender' && state.phase === 'waiting'
          && (cs === 'connecting' || cs === 'connected')) {
        state.phase = 'connecting';
        if (state.uiCard && window.LTR.p2pUi) {
          window.LTR.p2pUi.setCardPhase(state.uiCard, 'connecting');
        }
      }
      if (cs === 'failed') {
        window.LTR.p2pSession.cleanup(state, '✗ Connexion P2P échouée');
      } else if (cs === 'disconnected') {
        if (!state.disconnectedSince) {
          state.disconnectedSince = Date.now();
          if (state.uiCard) {
            const sub = state.uiCard.querySelector('.peer-sub');
            if (sub) sub.textContent = 'Reconnexion…';
          }
          state.uiStatusLabel = 'Reconnexion…';
          if (window.LTR.p2pUi) window.LTR.p2pUi.refreshSticky();
          // V1.6.5 — Wave 2 item G : tente iceRestart rapidement,
          // AVANT le cleanup à DISCONNECT_TTL_MS.
          // Le sender (initiator) émet l'offre iceRestart ; le receiver
          // applique setRemoteDescription + createAnswer.
          state.iceRestartTimer = setTimeout(() => {
            if (state.disconnectedSince && state.role === 'sender') {
              tryIceRestart(state, pc).catch((err) => {
                clientLog('warn', '[p2p] iceRestart failed: '
                                  + (err && err.message));
              });
            }
          }, ICE_RESTART_DELAY_MS);
          state.disconnectTimer = setTimeout(() => {
            if (state.disconnectedSince) {
              window.LTR.p2pSession.cleanup(state, '✗ Wi-Fi perdu');
            }
          }, DISCONNECT_TTL_MS);
        }
      } else if (cs === 'connected') {
        if (state.disconnectedSince) {
          state.disconnectedSince = 0;
          if (state.disconnectTimer) {
            clearTimeout(state.disconnectTimer);
            state.disconnectTimer = null;
          }
          // V1.6.5 — Wave 2 item G : annule aussi le timer d'iceRestart.
          if (state.iceRestartTimer) {
            clearTimeout(state.iceRestartTimer);
            state.iceRestartTimer = null;
          }
          if (state.uiCard && state.phase === 'sending'
              && window.LTR.p2pUi) {
            window.LTR.p2pUi.setCardPhase(state.uiCard, 'sending');
          }
          if (state.uiStatusLabel === 'Reconnexion…') {
            state.uiStatusLabel = state.role === 'sender' ? 'Envoi…' : 'Écriture disque…';
          }
          if (window.LTR.p2pUi) window.LTR.p2pUi.refreshSticky();
        }
      }
    };
    state.connectTimer = setTimeout(() => {
      if (pc.connectionState !== 'connected') {
        clientLog('warn', '[p2p] connect timeout (state='
                         + pc.connectionState + ')');
        window.LTR.p2pSession.cleanup(state, '✗ Pas de route LAN');
      }
    }, CONNECT_TIMEOUT_MS);
  }

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
          if (window.LTR.p2pUi) {
            window.LTR.p2pUi.toast(`${name} n'est plus connecté`, 'warning');
          }
          window.LTR.p2pSession.cleanup(state, 'Hors-ligne');
        }
      }
    } catch (e) {
      clientLog('error', '[p2p] signal POST failed: ' + (e && e.message));
    }
  }

  function cancelOutgoing(deviceId) {
    const state = getConn(deviceId, 'sender');
    if (!state) return;
    clientLog('info', '[p2p] sender cancel ' + deviceId.substring(0, 8));
    postSignal(deviceId, 'cancel', { reason: 'user' });
    window.LTR.p2pSession.cleanup(state, 'Annulé');
  }

  // V1.6.5 — Sprint Stabilité (Wave 2 item G).
  // Côté sender : créer une nouvelle offer avec iceRestart et l'envoyer
  // au pair via le signaling. Le pair reçoit type='ice-restart', applique
  // setRemoteDescription + createAnswer, renvoie type='ice-restart-answer'.
  async function tryIceRestart(state, pc) {
    if (!state.peer || !state.peer.deviceId) return;
    clientLog('info', '[p2p] tryIceRestart for '
                     + state.peer.deviceId.substring(0, 8));
    try {
      const offer = await pc.createOffer({ iceRestart: true });
      await pc.setLocalDescription(offer);
      postSignal(state.peer.deviceId, 'ice-restart',
        { sdp: offer.sdp, type: offer.type });
    } catch (e) {
      clientLog('warn', '[p2p] iceRestart createOffer failed: '
                        + (e && e.message));
    }
  }

  // V1.6.5 — Wave 2 item G : appelé par p2p.js handleSignal pour
  // un type='ice-restart' reçu (côté receiver).
  async function handleIceRestartIncoming(fromDeviceId, payload) {
    const state = getConn(fromDeviceId, 'receiver')
                || getConn(fromDeviceId, 'sender');
    if (!state || !state.pc) {
      clientLog('warn', '[p2p] ice-restart from unknown peer '
                        + fromDeviceId.substring(0, 8));
      return;
    }
    try {
      await state.pc.setRemoteDescription({
        type: payload.type || 'offer',
        sdp: payload.sdp,
      });
      const answer = await state.pc.createAnswer();
      await state.pc.setLocalDescription(answer);
      postSignal(fromDeviceId, 'ice-restart-answer',
        { sdp: answer.sdp, type: answer.type });
      clientLog('info', '[p2p] ice-restart answer sent to '
                       + fromDeviceId.substring(0, 8));
    } catch (e) {
      clientLog('warn', '[p2p] ice-restart handle failed: '
                        + (e && e.message));
    }
  }

  // V1.6.5 — Wave 2 item G : côté sender reçoit la réponse.
  async function handleIceRestartAnswer(fromDeviceId, payload) {
    const state = getConn(fromDeviceId, 'sender');
    if (!state || !state.pc) return;
    try {
      await state.pc.setRemoteDescription({
        type: payload.type || 'answer',
        sdp: payload.sdp,
      });
      clientLog('info', '[p2p] ice-restart answer applied for '
                       + fromDeviceId.substring(0, 8));
    } catch (e) {
      clientLog('warn', '[p2p] ice-restart-answer apply failed: '
                        + (e && e.message));
    }
  }

  window.LTR = window.LTR || {};
  window.LTR.p2pTransport = {
    conns, connKey, getConn,
    wirePcCommon, postSignal, cancelOutgoing,
    tryIceRestart, handleIceRestartIncoming, handleIceRestartAnswer,
    RTC_CONFIG, DISCONNECT_TTL_MS,
  };
})();
