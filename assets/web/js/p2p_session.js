// ============================================================
// p2p_session.js — Couche session de transfert (V1.6.1)
//
// Responsabilités :
//  - Constantes de chunking / backpressure / watchdog / drain
//  - safeSend, awaitDrain : abstractions de send avec backpressure
//  - sendNextFile, wireSenderDc : flux émetteur
//  - wireReceiverDc, handleReceiverControl, finalizeReceivedFile : flux receveur
//  - skipFailedFile, syncFileStatus : helpers
//  - cleanup, cleanupAll : lifecycle d'une connexion
// ============================================================
(function () {
  'use strict';
  const { clientLog } = window.LTR;

  // V1.2.1 : 16 KB pour compat Safari (SCTP RFC 8831 default).
  const CHUNK_SIZE   = 16 * 1024;
  const BUFFER_HIGH  = 1 * 1024 * 1024;
  const BUFFER_LOW   = 256 * 1024;
  // V1.3 — Robustesse
  const WATCHDOG_NO_DATA_MS = 10_000;
  const NO_ACK_TIMEOUT_MS   = 10_000;
  const ACK_INTERVAL_MS     = 500;
  const DRAIN_TIMEOUT_MS    = 30_000;

  function transport() { return window.LTR.p2pTransport; }
  function ui()        { return window.LTR.p2pUi; }

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

  async function safeSend(state, data, label) {
    const T = transport();
    while (state.disconnectedSince
           && Date.now() - state.disconnectedSince < T.DISCONNECT_TTL_MS) {
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

  function skipFailedFile(state, fs, errorTag) {
    fs.status = 'failed';
    fs.error = errorTag;
    syncFileStatus(state, fs);
    state.currentFileIdx += 1;
    sendNextFile(state).catch((e) =>
      clientLog('error', '[p2p] sendNextFile rec: ' + (e && e.message)));
  }

  function syncFileStatus(state, fs) {
    if (!fs || !fs.entryId || !window.LTR.transferRegistry) return;
    window.LTR.transferRegistry.updateEntry(fs.entryId, {
      status: fs.status, bytes: fs.bytes, error: fs.error,
    });
    if ((fs.status === 'sent' || fs.status === 'received')
        && state.peer) {
      window.LTR.transferRegistry.notifyComplete(
        state.role === 'sender' ? 'out' : 'in',
        fs.name, state.peer.displayName);
    }
  }

  function wireSenderDc(dc, state) {
    dc.bufferedAmountLowThreshold = BUFFER_LOW;
    dc.onopen = () => {
      state.phase = 'sending';
      if (state.uiCard && ui()) ui().setCardPhase(state.uiCard, 'sending');
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
      if (state.allFilesSent) { cleanup(state, '✓ Envoyé'); return; }
      clientLog('error', '[p2p] dc error: ' + (e && e.message));
      cleanup(state, '✗ Erreur DataChannel');
    };
    dc.onclose = () => {
      cleanup(state, state.allFilesSent ? '✓ Envoyé' : '✗ Connexion fermée');
    };
  }

  async function sendNextFile(state) {
    if (state.currentFileIdx >= state.files.length) {
      await awaitDrain(state.dc);
      await safeSend(state, JSON.stringify({ kind: 'all-done' }), 'all-done');
      state.allFilesSent = true;
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
    syncFileStatus(state, fs);

    await awaitDrain(state.dc);

    if (state.currentFileIdx === 0) {
      const summary = {
        kind: 'session-meta',
        count: state.files.length,
        totalBytes: state.totalBytes,
      };
      if (!await safeSend(state, JSON.stringify(summary), 'session-meta')) {
        fs.status = 'failed'; fs.error = 'session-meta';
        syncFileStatus(state, fs);
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
      skipFailedFile(state, fs, 'meta');
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
          if (ui()) ui().updateProgress(state);
        }
        if (aborted) break;
      }
    } catch (e) {
      clientLog('error', '[p2p] read failed: ' + (e && e.message));
      skipFailedFile(state, fs, 'read');
      return;
    }

    if (aborted) { skipFailedFile(state, fs, 'send'); return; }

    await awaitDrain(state.dc);
    if (!await safeSend(state, JSON.stringify({
        kind: 'file-end', idx: state.currentFileIdx }), 'file-end')) {
      skipFailedFile(state, fs, 'end');
      return;
    }
    fs.status = 'sent';
    fs.bytes = fs.size;
    syncFileStatus(state, fs);
    state.currentFileIdx += 1;
    sendNextFile(state).catch((e) =>
      clientLog('error', '[p2p] sendNextFile rec: ' + (e && e.message)));
  }

  function wireReceiverDc(dc, state) {
    dc.onopen = () => {
      clientLog('info', '[p2p] receiver dc open');
      state.noDataWatchdog = setTimeout(() => {
        if (state.bytesReceived === 0) {
          clientLog('warn', '[p2p] receiver no-data watchdog fired');
          cleanup(state, '✗ Pas de données');
        }
      }, WATCHDOG_NO_DATA_MS);
      state.ackTimer = setInterval(() => {
        if (dc.readyState !== 'open') return;
        try {
          dc.send(JSON.stringify({
            kind: 'ack', bytes: state.bytesReceived }));
        } catch {}
      }, ACK_INTERVAL_MS);
    };
    dc.onerror = (e) => {
      if (state.allDoneSeen) { cleanup(state, '✓ Reçu'); return; }
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
        try { msg = JSON.parse(data); } catch { return; }
        handleReceiverControl(msg, state);
      } else {
        const cur = state.receivingFiles[state.receivingFiles.length - 1];
        if (!cur) return;
        cur.chunks.push(data);
        cur.received += data.byteLength;
        state.bytesReceived += data.byteLength;
        if (ui()) ui().updateProgress(state);
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
      state.startedAt = Date.now();
    } else if (msg.kind === 'file-meta') {
      const idx = state.fileStatuses.length;
      state.receivingFiles.push({
        name: msg.name, size: msg.size, type: msg.type,
        received: 0, chunks: [],
      });
      const fs = {
        idx, name: msg.name, size: msg.size,
        status: 'sending', bytes: 0, error: null, entryId: null,
      };
      if (window.LTR.transferRegistry) {
        fs.entryId = window.LTR.transferRegistry.addEntry({
          direction: 'in',
          peer: state.peer,
          name: msg.name,
          size: msg.size,
        });
      }
      state.fileStatuses.push(fs);
      syncFileStatus(state, fs);
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
    if (cur.size && cur.received !== cur.size) {
      clientLog('warn', '[p2p] file truncated: ' + cur.name
                       + ' received=' + cur.received + ' expected=' + cur.size);
      if (fs) {
        fs.status = 'failed'; fs.error = 'taille_invalide';
        syncFileStatus(state, fs);
      }
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
    if (fs) {
      fs.status = 'received';
      fs.bytes = cur.received;
      syncFileStatus(state, fs);
    }
  }

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
    const T = transport();
    if (state.peer && state.role && T) {
      T.conns.delete(T.connKey(state.peer.deviceId, state.role));
    }
    if (ui()) ui().refreshSticky();
  }

  function cleanupAll() {
    const T = transport();
    if (!T) return;
    Array.from(T.conns.values()).forEach((s) => cleanup(s, ''));
  }

  window.LTR = window.LTR || {};
  window.LTR.p2pSession = {
    safeSend, awaitDrain,
    sendNextFile, wireSenderDc, wireReceiverDc,
    handleReceiverControl, finalizeReceivedFile,
    skipFailedFile, syncFileStatus,
    cleanup, cleanupAll,
    CHUNK_SIZE, BUFFER_HIGH, BUFFER_LOW,
    ACK_INTERVAL_MS, NO_ACK_TIMEOUT_MS,
    WATCHDOG_NO_DATA_MS, DRAIN_TIMEOUT_MS,
  };
})();
