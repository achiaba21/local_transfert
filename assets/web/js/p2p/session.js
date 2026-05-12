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
  const CHUNK_MIN    = 16 * 1024;
  const CHUNK_MAX    = 64 * 1024;
  const BUFFER_HIGH  = 1 * 1024 * 1024;
  const BUFFER_LOW   = 256 * 1024;
  const BUFFER_HIGH_MAX = 4 * 1024 * 1024;
  const BUFFER_LOW_MIN  = 128 * 1024;
  // V1.3 — Robustesse
  const WATCHDOG_NO_DATA_MS = 10_000;
  const NO_ACK_TIMEOUT_MS   = 10_000;
  const ACK_INTERVAL_MS     = 500;
  const DRAIN_TIMEOUT_MS    = 30_000;
  // V1.6.3 — OPFS receveur. Détection au boot + cap fallback.
  const OPFS_AVAILABLE = !!(typeof navigator !== 'undefined'
                            && navigator.storage
                            && navigator.storage.getDirectory);
  const FALLBACK_CAP_BYTES = 1024 * 1024 * 1024;  // 1 Go cap si Blob
  const STORAGE_SAFETY_BYTES = 64 * 1024 * 1024;  // marge metadata/OPFS
  const PENDING_TTL_MS = 24 * 60 * 60 * 1000;
  console.log('[p2p] V1.6.3 OPFS_AVAILABLE=', OPFS_AVAILABLE,
              ' navigator.storage=', !!navigator.storage,
              ' getDirectory=', !!(navigator.storage
                                    && navigator.storage.getDirectory));

  function transport() { return window.LTR.p2pTransport; }
  function ui()        { return window.LTR.p2pUi; }

  function makeSessionId() {
    if (globalThis.crypto && globalThis.crypto.randomUUID) {
      return globalThis.crypto.randomUUID();
    }
    return 'p2p-' + Date.now().toString(36)
      + '-' + Math.random().toString(36).slice(2, 10);
  }

  function stableFileId(file) {
    const name = file && file.name ? file.name : 'file';
    const size = file && typeof file.size === 'number' ? file.size : 0;
    const modified = file && typeof file.lastModified === 'number'
      ? file.lastModified : 0;
    return `${size}:${modified}:${name}`;
  }

  function tuneFlow(state, dc) {
    state.chunkSize = state.chunkSize || CHUNK_SIZE;
    state.bufferHigh = state.bufferHigh || BUFFER_HIGH;
    state.bufferLow = state.bufferLow || BUFFER_LOW;
    if (!dc) return;
    if (dc.bufferedAmount > state.bufferHigh * 0.8) {
      state.chunkSize = Math.max(CHUNK_MIN, Math.floor(state.chunkSize / 2));
      state.bufferHigh = Math.max(BUFFER_HIGH, Math.floor(state.bufferHigh * 0.85));
    } else if (dc.bufferedAmount < state.bufferLow * 0.5) {
      state.chunkSize = Math.min(CHUNK_MAX, state.chunkSize * 2);
      state.bufferHigh = Math.min(BUFFER_HIGH_MAX, Math.floor(state.bufferHigh * 1.15));
    }
    state.bufferLow = Math.max(BUFFER_LOW_MIN, Math.floor(state.bufferHigh / 4));
    dc.bufferedAmountLowThreshold = state.bufferLow;
  }

  async function hasStorageRoom(requiredBytes) {
    if (!navigator.storage || !navigator.storage.estimate) {
      return { ok: true, available: null };
    }
    try {
      const est = await navigator.storage.estimate();
      const quota = est.quota || 0;
      const usage = est.usage || 0;
      if (!quota) return { ok: true, available: null };
      const available = Math.max(0, quota - usage);
      return {
        ok: available >= requiredBytes + STORAGE_SAFETY_BYTES,
        available,
      };
    } catch (e) {
      clientLog('warn', '[p2p] storage estimate failed: '
                        + (e && e.message));
      return { ok: true, available: null };
    }
  }

  async function receiverAbort(state, error, label) {
    clientLog('warn', '[p2p] receiver abort ' + error);
    const fs = state.fileStatuses && state.fileStatuses[state.fileStatuses.length - 1];
    if (fs && fs.status === 'sending') {
      fs.status = 'failed';
      fs.error = error;
      syncFileStatus(state, fs);
    }
    if (state.dc && state.dc.readyState === 'open') {
      try {
        state.dc.send(JSON.stringify({ kind: 'receiver-error', error }));
      } catch {}
      await new Promise((r) => setTimeout(r, 80));
    }
    if (ui()) ui().toast(label || 'Réception impossible', 'warning');
    cleanup(state, label || 'Échec réception');
  }

  function setSessionUiState(state, text) {
    state.uiStatusLabel = text || '';
    if (state.uiCard && ui()) {
      const sub = state.uiCard.querySelector('.peer-sub');
      if (sub && text) sub.textContent = text;
    }
    if (ui()) ui().refreshSticky();
  }

  function updateFilePhase(state, fs, phase) {
    if (!fs) return;
    fs.phase = phase;
    syncFileStatus(state, fs);
  }

  async function awaitDrain(dc, state) {
    const high = state && state.bufferHigh ? state.bufferHigh : BUFFER_HIGH;
    while (dc.readyState === 'open' && dc.bufferedAmount > high) {
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
    state.bundleFailed = state.bundleFailed || !!state.bundleEntryId;
    syncFileStatus(state, fs);
    updateBundleEntry(state, 'failed', {
      error: errorTag,
      phase: 'failed',
    });
    state.currentFileIdx += 1;
    sendNextFile(state).catch((e) =>
      clientLog('error', '[p2p] sendNextFile rec: ' + (e && e.message)));
  }

  function syncFileStatus(state, fs) {
    if (!fs || !fs.entryId || !window.LTR.transferRegistry) return;
    window.LTR.transferRegistry.updateEntry(fs.entryId, {
      status: fs.status,
      bytes: fs.bytes,
      error: fs.error,
      phase: fs.phase,
      resumePct: fs.resumePct || 0,
    });
    if ((fs.status === 'sent' || fs.status === 'received')
        && state.peer) {
      window.LTR.transferRegistry.notifyComplete(
        state.role === 'sender' ? 'out' : 'in',
        fs.name, state.peer.displayName);
    }
  }

  function updateBundleEntry(state, status, patch) {
    if (!state || !state.bundleEntryId || !window.LTR.transferRegistry) return;
    const bytes = state.role === 'sender'
      ? state.bytesSent
      : state.bytesReceived;
    window.LTR.transferRegistry.updateEntry(state.bundleEntryId, Object.assign({
      status,
      bytes,
      phase: status === 'sending' ? 'sending' : 'done',
      error: null,
    }, patch || {}));
  }

  function buildSessionMeta(state) {
    return {
      kind: 'session-meta',
      sessionId: state.sessionId,
      count: state.files.length,
      totalBytes: state.totalBytes,
      bundleKind: state.bundle && state.bundle.kind,
      bundleName: state.bundle && state.bundle.name,
      bundleFileCount: state.bundle && state.bundle.fileCount,
    };
  }

  function buildFileMeta(state, file) {
    return {
      kind: 'file-meta',
      sessionId: state.sessionId,
      name: file.name,
      size: file.size,
      type: file.type || 'application/octet-stream',
      relativePath: file.webkitRelativePath || file.name,
      idx:  state.currentFileIdx,
      fileId: state.sessionId + ':' + state.currentFileIdx,
      stableFileId: stableFileId(file),
      lastModified: file.lastModified || 0,
    };
  }

  function wireSenderDc(dc, state) {
    state.chunkSize = state.chunkSize || CHUNK_SIZE;
    state.bufferHigh = state.bufferHigh || BUFFER_HIGH;
    state.bufferLow = state.bufferLow || BUFFER_LOW;
    dc.bufferedAmountLowThreshold = state.bufferLow;
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
    dc.onmessage = (ev) => {
      if (typeof ev.data !== 'string') return;
      let msg;
      try { msg = JSON.parse(ev.data); } catch { return; }
      if (msg.kind === 'ack' && typeof msg.bytes === 'number') {
        state.bytesAckedByReceiver = msg.bytes;
        state.lastAckAt = Date.now();
        if (msg.fileId && typeof msg.fileBytes === 'number') {
          const fs = state.fileStatuses.find((x) => x.fileId === msg.fileId);
          if (fs) fs.bytesAcked = msg.fileBytes;
        }
        if (ui()) ui().updateProgress(state);
      } else if (msg.kind === 'receiver-error') {
        const reason = msg.error || 'receiver-error';
        clientLog('warn', '[p2p] receiver-error: ' + reason);
        const fs = state.fileStatuses && state.fileStatuses[state.currentFileIdx];
        if (fs) {
          fs.status = 'failed';
          fs.error = reason;
          syncFileStatus(state, fs);
        }
        cleanup(state, humanReceiverError(reason));
      } else if (msg.kind === 'file-ready') {
        const key = msg.fileId || (msg.sessionId + ':' + msg.idx);
        const waiter = state.fileReadyWaiters && state.fileReadyWaiters.get(key);
        if (waiter) {
          state.fileReadyWaiters.delete(key);
          waiter.resolve({
            offset: Math.max(0, Number(msg.offset) || 0),
          });
        }
      }
    };
  }

  function waitForFileReady(state, meta) {
    const key = meta.fileId;
    state.fileReadyWaiters = state.fileReadyWaiters || new Map();
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        state.fileReadyWaiters.delete(key);
        reject(new Error('file-ready timeout'));
      }, WATCHDOG_NO_DATA_MS);
      state.fileReadyWaiters.set(key, {
        resolve: (value) => {
          clearTimeout(timer);
          resolve(value);
        },
      });
    });
  }

  async function sendNextFile(state) {
    if (state.currentFileIdx >= state.files.length) {
      await awaitDrain(state.dc, state);
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
    fs.phase = state.currentFileIdx === 0 ? 'preparing' : 'sending';
    syncFileStatus(state, fs);
    setSessionUiState(state, 'Préparation…');

    await awaitDrain(state.dc, state);

    if (state.currentFileIdx === 0) {
      state.sessionId = state.sessionId || makeSessionId();
      const summary = buildSessionMeta(state);
      if (!await safeSend(state, JSON.stringify(summary), 'session-meta')) {
        fs.status = 'failed'; fs.error = 'session-meta';
        syncFileStatus(state, fs);
        cleanup(state, '✗ Erreur réseau');
        return;
      }
    }
    const meta = buildFileMeta(state, file);
    fs.fileId = meta.fileId;
    fs.stableFileId = meta.stableFileId;
    if (!await safeSend(state, JSON.stringify(meta), 'file-meta')) {
      skipFailedFile(state, fs, 'meta');
      return;
    }

    let resumeOffset = 0;
    try {
      const ready = await waitForFileReady(state, meta);
      resumeOffset = Math.min(file.size, ready.offset || 0);
    } catch (e) {
      clientLog('warn', '[p2p] receiver not ready: ' + (e && e.message));
      skipFailedFile(state, fs, 'receiver_ready');
      return;
    }

    if (resumeOffset > 0) {
      state.bytesSent += resumeOffset;
      state.bytesAckedByReceiver = Math.max(
        state.bytesAckedByReceiver, state.bytesSent);
      fs.bytes = resumeOffset;
      fs.resumePct = Math.floor((resumeOffset / Math.max(1, file.size)) * 100);
      fs.phase = 'resuming';
      syncFileStatus(state, fs);
      setSessionUiState(state, 'Reprise à ' + fs.resumePct + ' %');
    } else {
      updateFilePhase(state, fs, 'sending');
      setSessionUiState(state, 'Envoi…');
    }

    const reader = file.slice(resumeOffset).stream().getReader();
    let aborted = false;
    let fileBytesSent = resumeOffset;
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        let pos = 0;
        while (pos < value.byteLength) {
          tuneFlow(state, state.dc);
          await awaitDrain(state.dc, state);
          const end = Math.min(pos + state.chunkSize, value.byteLength);
          const slice = new Uint8Array(value.buffer,
                                        value.byteOffset + pos,
                                        end - pos).slice();
          if (!await safeSend(state, slice, 'chunk')) {
            aborted = true;
            try { reader.cancel(); } catch {}
            break;
          }
          state.bytesSent += slice.byteLength;
          fileBytesSent += slice.byteLength;
          fs.bytes = fileBytesSent;
          const now = Date.now();
          if (!fs.lastRegistryAt || now - fs.lastRegistryAt > 500) {
            fs.lastRegistryAt = now;
            syncFileStatus(state, fs);
            updateBundleEntry(state, 'sending');
          }
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

    await awaitDrain(state.dc, state);
    updateFilePhase(state, fs, 'finalizing');
    setSessionUiState(state, 'Finalisation…');
    if (!await safeSend(state, JSON.stringify({
        kind: 'file-end',
        sessionId: state.sessionId,
        idx: state.currentFileIdx,
        fileId: state.sessionId + ':' + state.currentFileIdx,
      }), 'file-end')) {
      skipFailedFile(state, fs, 'end');
      return;
    }
    fs.status = 'sent';
    fs.bytes = fs.size;
    syncFileStatus(state, fs);
    if (state.bundleEntryId && state.currentFileIdx >= state.files.length - 1
        && !state.bundleFailed) {
      updateBundleEntry(state, 'sent', {
        bytes: state.totalBytes,
        phase: 'done',
      });
      if (window.LTR.transferRegistry && state.peer) {
        window.LTR.transferRegistry.notifyComplete(
          'out',
          state.bundle ? state.bundle.name : fs.name,
          state.peer.displayName);
      }
    } else {
      updateBundleEntry(state, 'sending');
    }
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
            kind: 'ack',
            bytes: state.bytesReceived,
            fileId: state.activeFile && state.activeFile.fileId,
            fileBytes: state.activeFile && state.activeFile.received,
          }));
        } catch {}
        // V1.6.5 — Sprint Stabilité (Wave 2 item E) : persiste l'état
        // de réception en cours dans IndexedDB pour qu'un reload onglet
        // détecte le pending et propose « Reprendre » à l'utilisateur.
        const cur = state.activeFile;
        if (cur && cur.opfsName && cur.received < cur.size) {
          updatePendingSidecar(state, cur).catch(() => {});
        }
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
        state.controlQueue = (state.controlQueue || Promise.resolve())
          .then(() => handleReceiverControl(msg, state))
          .catch((e) =>
            clientLog('error', '[p2p] handleReceiverControl: '
                      + (e && e.message)));
      } else {
        // V1.6.3 — chunk binaire : sérialise via writeQueue pour ne
        // pas perdre l'ordre quand OPFS write est async.
        const cur = state.activeFile;
        if (!cur) return;
        if (state.noDataWatchdog) {
          clearTimeout(state.noDataWatchdog);
          state.noDataWatchdog = null;
        }
        cur.writeQueue = (cur.writeQueue || Promise.resolve()).then(
          async () => {
            if (cur.opfsReady) await cur.opfsReady;
            if (cur.opfsWritable) {
              await cur.opfsWritable.write({
                type: 'write', position: cur.received, data
              });
            } else if (cur.chunks) {
              cur.chunks.push(data);
            }
            cur.received += data.byteLength;
            state.bytesReceived += data.byteLength;
            const fs = state.fileStatuses[cur.idx];
            if (fs) {
              fs.bytes = cur.received;
              fs.phase = 'writing';
              const now = Date.now();
              if (!cur.lastRegistryAt || now - cur.lastRegistryAt > 500) {
                cur.lastRegistryAt = now;
                syncFileStatus(state, fs);
                updateBundleEntry(state, 'sending');
              }
            }
            // Log toutes les ~50 Mo pour visualiser la progression OPFS.
            const MILESTONE = 50 * 1024 * 1024;
            const prevMS = Math.floor((cur.received - data.byteLength) / MILESTONE);
            const newMS  = Math.floor(cur.received / MILESTONE);
            if (newMS > prevMS) {
              console.log('[p2p] receive progress', cur.name,
                          Math.round(cur.received / 1024 / 1024) + 'MB /',
                          Math.round(cur.size / 1024 / 1024) + 'MB',
                          cur.opfsWritable ? '(OPFS)' : '(Blob)');
            }
            if (ui()) ui().updateProgress(state);
          }).catch((e) =>
            clientLog('error', '[p2p] write chunk: '
                      + (e && e.message)));
      }
    };
  }

  async function handleReceiverControl(msg, state) {
    if (msg.kind === 'session-meta') {
      if (!msg.sessionId || typeof msg.count !== 'number') {
        await receiverAbort(state, 'protocole_session',
          'Session P2P invalide');
        return;
      }
      const room = await hasStorageRoom(msg.totalBytes || 0);
      if (!room.ok) {
        await receiverAbort(state, 'quota_insuffisant',
          'Espace insuffisant pour recevoir');
        return;
      }
      state.sessionId = msg.sessionId;
      state.expectedFileCount = msg.count;
      state.totalBytes = msg.totalBytes || 0;
      state.bundle = msg.bundleKind === 'folder' ? {
        kind: 'folder',
        name: msg.bundleName || 'dossier',
        fileCount: msg.bundleFileCount || msg.count,
      } : null;
      state.startedAt = Date.now();
      if (state.bundle && window.LTR.transferRegistry) {
        state.bundleEntryId = window.LTR.transferRegistry.addEntry({
          direction: 'in',
          peer: state.peer,
          name: state.bundle.name,
          size: state.totalBytes,
          kind: 'folder',
          fileCount: state.bundle.fileCount,
        });
      }
      setSessionUiState(state, 'Vérification espace…');
    } else if (msg.kind === 'file-meta') {
      const idx = state.fileStatuses.length;
      const expectedFileId = state.sessionId + ':' + idx;
      if (!state.sessionId || msg.sessionId !== state.sessionId
          || msg.idx !== idx || msg.fileId !== expectedFileId
          || state.activeFile) {
        await receiverAbort(state, 'protocole_fichier',
          'Protocole fichier invalide');
        return;
      }
      const resume = await findResumeCandidate(state, msg);
      const cur = {
        idx, fileId: msg.fileId, stableFileId: msg.stableFileId || '',
        name: msg.name, relativePath: msg.relativePath || msg.name,
        size: msg.size, type: msg.type,
        received: resume ? resume.bytesWritten : 0,
        // OPFS-or-Blob — décidé selon capability et taille.
        chunks: null, opfsHandle: null, opfsWritable: null,
        opfsName: null, opfsReady: null,
        writeQueue: Promise.resolve(),
      };
      // V1.6.3 — Setup storage : OPFS si dispo, sinon Blob avec cap.
      if (OPFS_AVAILABLE) {
        cur.opfsName = resume
          ? resume.opfsName
          : `ltr-${state.peer.deviceId}-${idx}-${Date.now()}`;
        console.log('[p2p] file-meta', idx, msg.name,
                    '(' + msg.size + 'B) → OPFS path,',
                    ' opfsName=', cur.opfsName);
        cur.opfsReady = (async () => {
          const root = await navigator.storage.getDirectory();
          cur.opfsHandle = await root.getFileHandle(
            cur.opfsName, { create: true });
          cur.opfsWritable = await cur.opfsHandle.createWritable({
            keepExistingData: !!resume,
          });
          console.log('[p2p] OPFS handle ready for', cur.opfsName);
        })().catch((e) => {
          console.error('[p2p] OPFS init failed:', e);
          clientLog('error', '[p2p] OPFS init failed: ' + (e && e.message));
          if (cur.received > 0) {
            cur.initFailed = true;
            receiverAbort(state, 'opfs_resume',
              'Reprise navigateur indisponible').catch(() => {});
            return;
          }
          if (cur.size > FALLBACK_CAP_BYTES) {
            receiverAbort(state, 'opfs_indisponible',
              'Stockage navigateur indisponible').catch(() => {});
            return;
          }
          cur.chunks = [];  // fallback Blob pour ce fichier
        });
      } else if (msg.size > FALLBACK_CAP_BYTES) {
        console.warn('[p2p] file >1 Go REFUSED (Blob fallback):',
                     msg.name, msg.size, 'B');
        clientLog('warn', '[p2p] file too big for Blob fallback: '
                          + msg.size);
        await receiverAbort(state, 'taille_navigateur',
          'Navigateur trop limité pour ce fichier');
        return;
      } else {
        console.log('[p2p] file-meta', idx, msg.name,
                    '(' + msg.size + 'B) → Blob fallback path');
        cur.chunks = [];
      }
      state.receivingFiles.push(cur);
      state.activeFile = cur;
      if (cur.received > 0) state.bytesReceived += cur.received;
      const fs = {
        idx, fileId: msg.fileId, name: msg.name, size: msg.size,
        status: 'sending', bytes: cur.received, error: null, entryId: null,
        phase: cur.received > 0 ? 'resuming' : 'writing',
        resumePct: cur.received > 0
          ? Math.floor((cur.received / Math.max(1, cur.size)) * 100)
          : 0,
      };
      if (window.LTR.transferRegistry && !state.bundleEntryId) {
        fs.entryId = window.LTR.transferRegistry.addEntry({
          direction: 'in',
          peer: state.peer,
          name: msg.name,
          size: msg.size,
        });
      }
      state.fileStatuses.push(fs);
      syncFileStatus(state, fs);
      updateBundleEntry(state, 'sending');
      setSessionUiState(state, cur.received > 0
        ? 'Reprise réception à ' + fs.resumePct + ' %'
        : 'Écriture disque…');
      await sendFileReady(state, cur);
    } else if (msg.kind === 'file-end') {
      await finalizeReceivedFile(state, msg);
    } else if (msg.kind === 'all-done') {
      if (state.activeFile) {
        await receiverAbort(state, 'protocole_fin',
          'Fin de session invalide');
        return;
      }
      state.allDoneSeen = true;
      if (state.bundle) {
        try {
          await downloadReceivedFolderZip(state);
          updateBundleEntry(state, 'received', {
            bytes: state.totalBytes,
            phase: 'done',
          });
          if (window.LTR.transferRegistry && state.peer) {
            window.LTR.transferRegistry.notifyComplete(
              'in', state.bundle.name, state.peer.displayName);
          }
        } catch (e) {
          clientLog('error', '[p2p] folder zip failed: ' + (e && e.message));
          updateBundleEntry(state, 'failed', {
            error: 'zip_failed',
            phase: 'failed',
          });
        }
      }
      if (state.uiCard) {
        const sub = state.uiCard.querySelector('.peer-sub');
        if (sub) sub.textContent = '✓ Reçu';
      }
    }
  }

  const ZIP_ENCODER = new TextEncoder();
  let ZIP_CRC_TABLE = null;

  function zipU16(n) {
    const b = new Uint8Array(2);
    b[0] = n & 0xff; b[1] = (n >>> 8) & 0xff;
    return b;
  }

  function zipU32(n) {
    const b = new Uint8Array(4);
    b[0] = n & 0xff; b[1] = (n >>> 8) & 0xff;
    b[2] = (n >>> 16) & 0xff; b[3] = (n >>> 24) & 0xff;
    return b;
  }

  function crcTable() {
    if (ZIP_CRC_TABLE) return ZIP_CRC_TABLE;
    ZIP_CRC_TABLE = new Uint32Array(256);
    for (let i = 0; i < 256; i++) {
      let c = i;
      for (let k = 0; k < 8; k++) {
        c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
      }
      ZIP_CRC_TABLE[i] = c >>> 0;
    }
    return ZIP_CRC_TABLE;
  }

  function crc32Update(crc, bytes) {
    const table = crcTable();
    for (let i = 0; i < bytes.length; i++) {
      crc = table[(crc ^ bytes[i]) & 0xff] ^ (crc >>> 8);
    }
    return crc >>> 0;
  }

  async function crc32Blob(blob) {
    const table = crcTable();
    let crc = 0xffffffff;
    const reader = blob.stream().getReader();
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      for (let i = 0; i < value.length; i++) {
        crc = table[(crc ^ value[i]) & 0xff] ^ (crc >>> 8);
      }
    }
    return (crc ^ 0xffffffff) >>> 0;
  }

  function zipPath(name) {
    return String(name || 'fichier')
      .replace(/\\/g, '/')
      .replace(/^\/+/, '')
      .split('/')
      .filter((part) => part && part !== '.' && part !== '..')
      .join('/') || 'fichier';
  }

  async function makeZipBlob(files) {
    const parts = [];
    const central = [];
    let offset = 0;
    for (const item of files) {
      if (item.blob.size > 0xffffffff) {
        throw new Error('zip32_file_too_large');
      }
      const nameBytes = ZIP_ENCODER.encode(zipPath(item.name));
      const crc = await crc32Blob(item.blob);
      const local = [
        zipU32(0x04034b50), zipU16(20), zipU16(0x0800), zipU16(0),
        zipU16(0), zipU16(0), zipU32(crc),
        zipU32(item.blob.size), zipU32(item.blob.size),
        zipU16(nameBytes.length), zipU16(0), nameBytes,
      ];
      parts.push(...local, item.blob);
      const localSize = local.reduce((sum, p) => sum + p.length, 0)
        + item.blob.size;
      central.push(...[
        zipU32(0x02014b50), zipU16(20), zipU16(20), zipU16(0x0800),
        zipU16(0), zipU16(0), zipU16(0), zipU32(crc),
        zipU32(item.blob.size), zipU32(item.blob.size),
        zipU16(nameBytes.length), zipU16(0), zipU16(0), zipU16(0),
        zipU16(0), zipU32(0), zipU32(offset), nameBytes,
      ]);
      offset += localSize;
    }
    const centralSize = central.reduce((sum, p) => sum + p.length, 0);
    if (offset > 0xffffffff || centralSize > 0xffffffff) {
      throw new Error('zip32_too_large');
    }
    parts.push(...central);
    parts.push(
      zipU32(0x06054b50), zipU16(0), zipU16(0),
      zipU16(files.length), zipU16(files.length),
      zipU32(centralSize), zipU32(offset), zipU16(0));
    return new Blob(parts, { type: 'application/zip' });
  }

  async function makeZipBlobOpfs(files, zipName) {
    if (!OPFS_AVAILABLE || !navigator.storage || !navigator.storage.getDirectory) {
      return null;
    }
    if (files.length > 0xffff) throw new Error('zip32_too_many_files');
    const root = await navigator.storage.getDirectory();
    const opfsName = 'ltr-zip-' + Date.now().toString(36)
      + '-' + Math.random().toString(36).slice(2, 8);
    const handle = await root.getFileHandle(opfsName, { create: true });
    const writable = await handle.createWritable({ keepExistingData: false });
    const central = [];
    let offset = 0;

    async function writePart(part) {
      await writable.write(part);
      offset += part.size || part.byteLength || part.length || 0;
    }

    try {
      for (const item of files) {
        if (item.blob.size > 0xffffffff) {
          throw new Error('zip32_file_too_large');
        }
        const localOffset = offset;
        const nameBytes = ZIP_ENCODER.encode(zipPath(item.name));
        const header = [
          zipU32(0x04034b50), zipU16(20), zipU16(0x0808), zipU16(0),
          zipU16(0), zipU16(0), zipU32(0), zipU32(0), zipU32(0),
          zipU16(nameBytes.length), zipU16(0), nameBytes,
        ];
        for (const p of header) await writePart(p);

        let crc = 0xffffffff;
        const reader = item.blob.stream().getReader();
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          crc = crc32Update(crc, value);
          await writePart(value);
        }
        crc = (crc ^ 0xffffffff) >>> 0;

        const desc = [
          zipU32(0x08074b50), zipU32(crc),
          zipU32(item.blob.size), zipU32(item.blob.size),
        ];
        for (const p of desc) await writePart(p);
        central.push(...[
          zipU32(0x02014b50), zipU16(20), zipU16(20), zipU16(0x0808),
          zipU16(0), zipU16(0), zipU16(0), zipU32(crc),
          zipU32(item.blob.size), zipU32(item.blob.size),
          zipU16(nameBytes.length), zipU16(0), zipU16(0), zipU16(0),
          zipU16(0), zipU32(0), zipU32(localOffset), nameBytes,
        ]);
      }

      const centralStart = offset;
      for (const p of central) await writePart(p);
      const centralSize = offset - centralStart;
      if (offset > 0xffffffff || centralSize > 0xffffffff) {
        throw new Error('zip32_too_large');
      }
      const eocd = [
        zipU32(0x06054b50), zipU16(0), zipU16(0),
        zipU16(files.length), zipU16(files.length),
        zipU32(centralSize), zipU32(centralStart), zipU16(0),
      ];
      for (const p of eocd) await writePart(p);
      await writable.close();
      return {
        blob: await handle.getFile(),
        opfsName,
        name: zipName || 'dossier.zip',
      };
    } catch (e) {
      try { await writable.close(); } catch {}
      try { await root.removeEntry(opfsName); } catch {}
      throw e;
    }
  }

  async function downloadReceivedFolderZip(state) {
    const files = state.bundleFiles || [];
    if (files.length === 0) return;
    setSessionUiState(state, 'Création ZIP…');
    const zipName = (state.bundle.name || 'dossier') + '.zip';
    let zipOpfsName = null;
    let zip;
    const opfsZip = await makeZipBlobOpfs(files, zipName);
    if (opfsZip) {
      zip = opfsZip.blob;
      zipOpfsName = opfsZip.opfsName;
    } else {
      zip = await makeZipBlob(files);
    }
    const url = URL.createObjectURL(zip);
    const a = document.createElement('a');
    a.href = url;
    a.download = zipName;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(url), 2000);
    if (window.LTR.webInbox && window.LTR.webInbox.addBlob) {
      window.LTR.webInbox.addBlob({
        name: zipName,
        size: zip.size,
        kind: 'folder',
        from: state.peer ? state.peer.displayName : 'P2P',
        fileCount: files.length,
      }, zip).catch(() => {});
    }
    if (OPFS_AVAILABLE) {
      setTimeout(async () => {
        try {
          const root = await navigator.storage.getDirectory();
          for (const item of files) {
            if (item.opfsName) {
              try { await root.removeEntry(item.opfsName); } catch {}
            }
          }
          if (zipOpfsName) {
            try { await root.removeEntry(zipOpfsName); } catch {}
          }
        } catch {}
      }, 5000);
    }
  }

  async function finalizeReceivedFile(state, msg) {
    const cur = state.activeFile;
    if (!cur) return;
    const fs = state.fileStatuses[state.fileStatuses.length - 1];
    if (!msg || msg.sessionId !== state.sessionId
        || msg.idx !== cur.idx || msg.fileId !== cur.fileId) {
      await receiverAbort(state, 'protocole_file_end',
        'Fin de fichier invalide');
      return;
    }

    // V1.6.3 — Attendre que la write queue OPFS soit drainée AVANT
    // de comparer received vs size.
    if (cur.writeQueue) {
      try { await cur.writeQueue; } catch {}
    }
    updateFilePhase(state, fs, 'verifying');
    setSessionUiState(state, 'Vérification fichier…');
    if (cur.size && cur.received !== cur.size) {
      clientLog('warn', '[p2p] file truncated: ' + cur.name
                       + ' received=' + cur.received + ' expected=' + cur.size);
      if (fs) {
        fs.status = 'failed'; fs.error = 'taille_invalide';
        syncFileStatus(state, fs);
      }
      // Cleanup partial : supprime opfsHandle s'il existe.
      if (cur.opfsName) {
        try {
          const root = await navigator.storage.getDirectory();
          await root.removeEntry(cur.opfsName);
        } catch {}
      }
      cur.chunks = null;
      return;
    }

    let blob;
    if (cur.opfsWritable) {
      // Path OPFS : ferme le writable, récupère le File handle.
      console.log('[p2p] file-end OPFS path:', cur.name,
                  cur.received, 'B written');
      try { await cur.opfsWritable.close(); } catch {}
      try {
        blob = await cur.opfsHandle.getFile();
        console.log('[p2p] OPFS getFile OK,', blob.size, 'B');
      } catch (e) {
        console.error('[p2p] OPFS getFile failed:', e);
        clientLog('error', '[p2p] OPFS getFile: ' + (e && e.message));
        if (fs) {
          fs.status = 'failed'; fs.error = 'opfs_read';
          syncFileStatus(state, fs);
        }
        return;
      }
    } else if (cur.chunks) {
      // Path Blob legacy : Blob construit depuis les chunks RAM.
      console.log('[p2p] file-end Blob path:', cur.name,
                  cur.chunks.length, 'chunks,', cur.received, 'B');
      blob = new Blob(cur.chunks, { type: cur.type });
      cur.chunks = null;
    } else {
      // Fichier qui a été marqué failed (>1 Go fallback) — skip.
      console.warn('[p2p] file-end skipped (no chunks/handle):', cur.name);
      return;
    }

    if (state.bundle) {
      state.bundleFiles = state.bundleFiles || [];
      state.bundleFiles.push({
        name: cur.relativePath || cur.name || 'download',
        blob,
        opfsName: cur.opfsName,
      });
    } else {
      const url = URL.createObjectURL(blob);
      const a   = document.createElement('a');
      a.href = url;
      a.download = cur.name || 'download';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      setTimeout(() => URL.revokeObjectURL(url), 2000);
      if (window.LTR.webInbox && window.LTR.webInbox.addBlob) {
        window.LTR.webInbox.addBlob({
          name: cur.name || 'download',
          size: blob.size,
          kind: 'file',
          from: state.peer ? state.peer.displayName : 'P2P',
        }, blob).catch(() => {});
      }
    }

    // Cleanup OPFS handle après download.
    if (cur.opfsName && !state.bundle) {
      setTimeout(async () => {
        try {
          const root = await navigator.storage.getDirectory();
          await root.removeEntry(cur.opfsName);
        } catch {}
      }, 5000);
    }

    if (fs) {
      fs.status = 'received';
      fs.bytes = cur.received;
      fs.phase = 'done';
      syncFileStatus(state, fs);
    }
    state.activeFile = null;
    // V1.6.5 — Sprint Stabilité (Wave 2 item E) : sidecar cleanup OK.
    clearPendingSidecar(cur).catch(() => {});
  }

  async function sendFileReady(state, cur) {
    if (cur.opfsReady) {
      try { await cur.opfsReady; } catch {}
    }
    if (cur.initFailed) return;
    if (!state.dc || state.dc.readyState !== 'open') return;
    try {
      state.dc.send(JSON.stringify({
        kind: 'file-ready',
        sessionId: state.sessionId,
        idx: cur.idx,
        fileId: cur.fileId,
        offset: cur.received || 0,
      }));
    } catch (e) {
      clientLog('warn', '[p2p] file-ready send failed: '
                        + (e && e.message));
    }
  }

  async function findResumeCandidate(state, msg) {
    if (!OPFS_AVAILABLE || !window.LTR.idb) return null;
    let pending;
    try {
      pending = await window.LTR.idb.all('ltr-p2p-pending');
    } catch {
      return null;
    }
    const now = Date.now();
    for (const item of pending || []) {
      const v = item.value;
      if (!v || !v.lastAckAt || now - v.lastAckAt <= PENDING_TTL_MS) continue;
      await removePendingPartial(v).catch(() => {});
    }
    const stableId = msg.stableFileId || '';
    const match = (pending || [])
      .map((item) => item.value)
      .filter((v) => {
        const meta = v && v.fileMeta;
        const peer = v && v.peer;
        return v && meta && peer
          && (!v.lastAckAt || now - v.lastAckAt <= PENDING_TTL_MS)
          && peer.deviceId === state.peer.deviceId
          && ((stableId && meta.stableFileId === stableId)
              || (!stableId && meta.name === msg.name && meta.size === msg.size))
          && v.bytesWritten > 0
          && v.bytesWritten < msg.size;
      })
      .sort((a, b) => (b.lastAckAt || 0) - (a.lastAckAt || 0))[0];
    if (!match || !match.opfsName) return null;
    try {
      const root = await navigator.storage.getDirectory();
      const handle = await root.getFileHandle(match.opfsName);
      const file = await handle.getFile();
      const bytesWritten = Math.min(file.size, match.bytesWritten || 0);
      if (bytesWritten <= 0 || bytesWritten >= msg.size) return null;
      return { opfsName: match.opfsName, bytesWritten };
    } catch (e) {
      clientLog('warn', '[p2p] resume candidate invalid: '
                        + (e && e.message));
      return null;
    }
  }

  async function removePendingPartial(entry) {
    if (!entry || !entry.opfsName) return;
    if (OPFS_AVAILABLE) {
      try {
        const root = await navigator.storage.getDirectory();
        await root.removeEntry(entry.opfsName);
      } catch {}
    }
    if (window.LTR.idb) {
      await window.LTR.idb.delete('ltr-p2p-pending', entry.opfsName);
    }
  }

  function humanReceiverError(error) {
    const map = {
      quota_insuffisant: 'Espace insuffisant',
      protocole_session: 'Session invalide',
      protocole_fichier: 'Protocole fichier invalide',
      protocole_fin: 'Fin de session invalide',
      protocole_file_end: 'Fin de fichier invalide',
      taille_navigateur: 'Navigateur trop limité',
      opfs_indisponible: 'Stockage navigateur indisponible',
      opfs_resume: 'Reprise navigateur indisponible',
    };
    return map[error] || 'Récepteur indisponible';
  }

  function cleanupError(label) {
    const text = label || '';
    if (text.includes('Wi-Fi') || text.includes('route')
        || text.includes('réseau') || text.includes('Réseau')) {
      return 'network_lost';
    }
    if (text.includes('Annulé')) return 'cancelled';
    if (text.includes('fermée') || text.includes('muet')
        || text.includes('Hors-ligne')) {
      return 'peer_closed';
    }
    if (text.includes('DataChannel')) return 'network_lost';
    return 'send';
  }

  function failOpenFileStatuses(state, label) {
    if (!state || !state.fileStatuses) return;
    const error = cleanupError(label);
    state.bundleFailed = state.bundleFailed || !!state.bundleEntryId;
    for (const fs of state.fileStatuses) {
      if (fs.status !== 'pending' && fs.status !== 'sending') continue;
      fs.status = 'failed';
      fs.error = error;
      syncFileStatus(state, fs);
    }
    updateBundleEntry(state, 'failed', {
      error,
      phase: 'failed',
    });
  }

  // V1.6.5 — Wave 2 item E : helpers de persistance du sidecar IndexedDB.
  // Utilise opfsName comme clé unique (collision impossible : Date.now() +
  // deviceId + idx). Si IndexedDB indispo (Safari iOS sandboxed) : silent
  // skip — le receveur perd la possibilité de reprendre, mais le transfert
  // se déroule normalement (fallback comportement V1.6.4).
  async function updatePendingSidecar(state, cur) {
    if (!window.LTR.idb || !cur.opfsName) return;
    const entry = {
      opfsName: cur.opfsName,
      peer: {
        deviceId:    state.peer.deviceId,
        displayName: state.peer.displayName,
        emoji:       state.peer.emoji,
        platformLabel: state.peer.platformLabel,
      },
      fileMeta: {
        name: cur.name,
        relativePath: cur.relativePath || cur.name,
        size: cur.size,
        type: cur.type,
        stableFileId: cur.stableFileId || '',
      },
      bytesWritten: cur.received,
      lastAckAt:    Date.now(),
      startedAt:    state.startedAt || Date.now(),
    };
    return window.LTR.idb.set('ltr-p2p-pending', cur.opfsName, entry);
  }

  async function clearPendingSidecar(cur) {
    if (!window.LTR.idb || !cur || !cur.opfsName) return;
    return window.LTR.idb.delete('ltr-p2p-pending', cur.opfsName);
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
    const isSuccess = label && label.startsWith('✓');
    if (!isSuccess) failOpenFileStatuses(state, label);
    if (!isSuccess && label && ui() && ui().showDiagnostic) {
      ui().showDiagnostic(label);
    }
    if (state.ttlTimer)        clearTimeout(state.ttlTimer);
    if (state.connectTimer)    clearTimeout(state.connectTimer);
    if (state.senderTtl)       clearTimeout(state.senderTtl);
    if (state.disconnectTimer) clearTimeout(state.disconnectTimer);
    if (state.iceRestartTimer) clearTimeout(state.iceRestartTimer);
    if (state.noDataWatchdog)  clearTimeout(state.noDataWatchdog);
    if (state.ackTimer)        clearInterval(state.ackTimer);
    if (state.ackWatchdog)     clearInterval(state.ackWatchdog);
    if (state.uiCard) {
      const x = state.uiCard.querySelector('.peer-cancel-btn');
      if (x) x.remove();
    }
    // V1.6.5 — Wave 2 item E : nettoie aussi les sidecars IndexedDB des
    // fichiers en cours sur cette session si on cleanup avec succès.
    // En cas d'échec (label ≠ ✓ Reçu), on les LAISSE pour permettre la
    // reprise au prochain boot d'onglet.
    if (isSuccess && state.role === 'receiver' && state.receivingFiles) {
      for (const cur of state.receivingFiles) {
        if (cur.opfsName) clearPendingSidecar(cur).catch(() => {});
      }
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
    CHUNK_SIZE, CHUNK_MIN, CHUNK_MAX, BUFFER_HIGH, BUFFER_LOW,
    ACK_INTERVAL_MS, NO_ACK_TIMEOUT_MS,
    WATCHDOG_NO_DATA_MS, DRAIN_TIMEOUT_MS,
    hasStorageRoom,
  };
  window.LTR.p2pSessionTest = {
    buildSessionMeta,
    buildFileMeta,
    zipPath,
  };
})();
