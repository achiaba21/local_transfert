# Architecture — Sprint V1.6.3 Performance P2P

**Date :** 2026-05-03
**Statut :** ✅ Validée

## Vue d'ensemble

4 lots séquentiels avec validation utilisateur entre chaque.

## LOT 1 — OPFS receveur

### Détection capability au boot
```js
const OPFS_AVAILABLE = !!(navigator.storage
                          && navigator.storage.getDirectory);
const FALLBACK_CAP_BYTES = 1024 * 1024 * 1024;  // 1 Go
```

### Modifs `p2p_session.js`

`handleReceiverControl('file-meta')` :
```js
if (OPFS_AVAILABLE) {
  cur.opfsName = `ltr-${state.peer.deviceId}-${idx}-${Date.now()}`;
  const root = await navigator.storage.getDirectory();
  cur.opfsHandle = await root.getFileHandle(cur.opfsName, {create: true});
  cur.opfsWritable = await cur.opfsHandle.createWritable();
} else {
  // Fallback Blob avec cap
  if (msg.size > FALLBACK_CAP_BYTES) {
    fs.status = 'failed'; fs.error = 'taille_navigateur';
    syncFileStatus(state, fs);
    if (window.LTR.p2pUi) window.LTR.p2pUi.toast(
      `${msg.name} >1 Go : utilise Chrome/Edge récent`, 'warning');
    return;
  }
  cur.chunks = [];  // mode legacy
}
```

`onmessage` binaire :
```js
if (cur.opfsWritable) {
  await cur.opfsWritable.write({type: 'write',
    position: cur.received, data: chunk});
} else {
  cur.chunks.push(chunk);
}
```

`finalizeReceivedFile` :
```js
if (cur.opfsWritable) {
  await cur.opfsWritable.close();
  const file = await cur.opfsHandle.getFile();
  // download file (URL.createObjectURL sur File OPFS)
  // après download : root.removeEntry(cur.opfsName)
} else {
  // Path Blob legacy inchangé
}
```

## LOT 2 — Multi-stream K=4

### Config
- `infra::Config` ajout : `int p2pParallelStreams = 4`
- `auth_routes` ou `host-info` expose la valeur côté JS
- Côté JS : `const K = window.LTR.p2pConfig.parallelStreams || 4`

### Modifs `p2p.js` (orchestrator) startSendTo
```js
state.dcs = [];
for (let i = 0; i < K; ++i) {
  const dc = pc.createDataChannel('ltr-' + i, { ordered: true });
  dc.binaryType = 'arraybuffer';
  state.dcs.push(dc);
}
S.wireSenderDcs(state.dcs, state);
```

### Modifs `p2p_session.js`
- `state.dc` → `state.dcs[]`
- `state.bytesSentByDc[K]` (cumul par-channel)
- `state.bytesAckedByDc[K]`
- `bytesSent = Σ bytesSentByDc`, `bytesAckedByReceiver = Σ bytesAckedByDc`
- `sendNextFile(state)` : assigne `dc = state.dcs[fileIdx % K]`
- ack format : `{kind:'ack', dcId, bytes}` (par-channel)
- Drain final : `Promise.all(state.dcs.map(awaitFullDrain))`

### Modifs receveur
- `pc.ondatachannel` reçoit chaque dc, indexe par `parseInt(label.split('-')[1])`
- Ack timer envoie K acks séparés (1 par channel actif)
- Watchdog no-data : si bytesReceived === 0 après 10 s sur dc[0], cleanup

## LOT 3 — Sidecar IndexedDB resume

### Nouveau `assets/web/js/p2p_resume.js`

```js
const DB_NAME = 'ltr-p2p-resume';
const DB_VERSION = 1;
const STORE = 'partials';
const TTL_MS = 24 * 3600 * 1000;

async function openDb() { /* idb open + upgrade */ }
async function recordPartial({sessionId, fileIdx, name, size,
                              bytesReceived, opfsName}) { /* put */ }
async function getPartials(sessionId) { /* getAll filter by sessionId */ }
async function clearPartials(sessionId) { /* delete keys */ }
async function purgeOld() { /* delete records > TTL_MS */ }

window.LTR.p2pResume = { recordPartial, getPartials, clearPartials, purgeOld };
```

### Modifs `p2p_session.js`

Receveur : à chaque ack émis, throttle 1 s pour `recordPartial`.

À `dc[0].onopen` côté receveur, AVANT de répondre :
```js
const partials = await window.LTR.p2pResume.getPartials(state.sessionId);
if (partials.length > 0) {
  state.dcs[0].send(JSON.stringify({
    kind: 'resume-offer',
    partials: partials.map(p => ({fileIdx: p.fileIdx, bytesReceived: p.bytesReceived}))
  }));
}
```

Sender reçoit `resume-offer` :
```js
state.resumeBytes = {};  // fileIdx → bytesReceived
payload.partials.forEach(p => {
  state.resumeBytes[p.fileIdx] = p.bytesReceived;
});
```

`sendNextFile` pour file with resumeBytes :
```js
const skip = state.resumeBytes[idx] || 0;
const slice = file.slice(skip, file.size);
const reader = slice.stream().getReader();
state.bytesSent += skip;  // déjà reçu, on saute
fs.bytes = skip;
```

`file-meta` annonce : `{name, size, resumeFrom: skip}`.

## LOT 4 — Re-négo auto disconnected

### Modifs `p2p_transport.js::wirePcCommon`

À `pc.connectionState='disconnected'` :
```js
if (!state.disconnectedSince) {
  state.disconnectedSince = Date.now();
  // Démarrer re-négo après 5 s d'inactivité
  state.reNegoTimer = setTimeout(() => triggerReNego(state), 5000);
  // Ancien comportement : cleanup à 15 s seulement si re-négo ÉCHOUE
  state.disconnectTimer = setTimeout(() => {
    if (state.disconnectedSince) {
      window.LTR.p2pSession.cleanup(state, '✗ Wi-Fi perdu');
    }
  }, DISCONNECT_TTL_MS);
}
```

`triggerReNego(state)` (côté sender role uniquement) :
```js
async function triggerReNego(state) {
  if (state.role !== 'sender' || !state.pc) return;
  try {
    const offer = await state.pc.createOffer({ iceRestart: true });
    await state.pc.setLocalDescription(offer);
    await postSignal(state.peer.deviceId, 'offer',
      { sdp: state.pc.localDescription, reNego: true });
  } catch (e) { /* sera cleanup à DISCONNECT_TTL_MS */ }
}
```

### Modifs `p2p.js` orchestrator handleSignal('offer')

```js
if (payload.reNego && type === 'offer') {
  // Re-négo : pas de modale, juste accepter et créer answer
  const state = T.getConn(from, 'receiver');
  if (state && state.pc) {
    await state.pc.setRemoteDescription(payload.sdp);
    const answer = await state.pc.createAnswer();
    await state.pc.setLocalDescription(answer);
    await T.postSignal(from, 'answer', { sdp: answer });
  }
  return;
}
```

À `pc.connectionState='connected'` retour :
- Clear `reNegoTimer` ET `disconnectTimer`
- Reset `state.disconnectedSince = 0`
- Transfert reprend depuis `bytesAckedByReceiver` (logique V1.3)

## CONTRAT D'IMPLÉMENTATION

### Fichiers AJOUTER (1)
- [ ] `assets/web/js/p2p_resume.js`

### Fichiers MODIFIER (~7)
- [ ] `assets/web/js/p2p_session.js` (LOT 1, 2, 3 — refactor majeur)
- [ ] `assets/web/js/p2p_transport.js` (LOT 2 conns dcs[], LOT 4 re-négo)
- [ ] `assets/web/js/p2p.js` orchestrator (LOT 2 K channels, LOT 4 reNego)
- [ ] `assets/web/html/index.html` (script p2p_resume.js)
- [ ] `CMakeLists.txt` (embed p2p_resume.js)
- [ ] `src/web/routes/static_routes.cpp` (route /p2p_resume.js)
- [ ] `include/ltr/infra/config.hpp` + `.cpp` (p2pParallelStreams)
- [ ] `src/web/routes/auth_routes.cpp` host-info → expose K

UI_REQUIRED: false (sauf indicateur resume mineur en LOT 3, à
ajouter inline au sub-label de la card)
