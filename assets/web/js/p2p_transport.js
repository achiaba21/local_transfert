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
  const DISCONNECT_TTL_MS = 15_000;
  const CONNECT_TIMEOUT_MS = 20_000;

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
      if (cs === 'failed') {
        window.LTR.p2pSession.cleanup(state, '✗ Connexion P2P échouée');
      } else if (cs === 'disconnected') {
        if (!state.disconnectedSince) {
          state.disconnectedSince = Date.now();
          if (state.uiCard) {
            const sub = state.uiCard.querySelector('.peer-sub');
            if (sub) sub.textContent = 'Connexion perdue…';
          }
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
          if (state.uiCard && state.phase === 'sending'
              && window.LTR.p2pUi) {
            window.LTR.p2pUi.setCardPhase(state.uiCard, 'sending');
          }
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

  window.LTR = window.LTR || {};
  window.LTR.p2pTransport = {
    conns, connKey, getConn,
    wirePcCommon, postSignal, cancelOutgoing,
    RTC_CONFIG, DISCONNECT_TTL_MS,
  };
})();
