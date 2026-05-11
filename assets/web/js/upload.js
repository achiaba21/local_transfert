// ============================================================
// upload.js — flow d'upload web → host
//   1. announce (POST /api/upload-announce) → attend décision host
//   2. upload (POST /api/upload) par fichier avec uploadId
// ============================================================
(function () {
  'use strict';
  const {
    clientLog, goToLogin, escapeHtml, iconFor, formatBytes,
    supportsFolderPick
  } = window.LTR;
  const $ = (s) => document.querySelector(s);

  // V1.4.3 — staging : chaque élément ajouté (fichier ou dossier)
  // attend un choix de destination (Host ou peer P2P) avant envoi.
  // Map<stagingId, { kind, file?, files?, bundle?, li, sent }>
  const staging = new Map();
  function profile() { return window.LTR.webProfile; }
  function stagingId() {
    return 'st-' + Date.now().toString(36)
         + '-' + Math.random().toString(36).slice(2, 6);
  }

  function init() {
    restoreDrafts();
    setupDropZone();
    setupFileInputChange('#file-input');
    setupFileInputChange('#folder-input');
    setupFolderButtonVisibility();
    setupPasteButton();
    setupPasteEvent();
  }

  function restoreDrafts() {
    if (!profile()) return;
    const drafts = profile().get('drafts', []) || [];
    if (drafts.length === 0) return;
    const list = document.getElementById('upload-list');
    if (!list) return;
    drafts.forEach((d) => {
      const li = renderDraftRow(d);
      list.appendChild(li);
    });
  }

  // V1.4.2 — Stratégie en 2 couches pour le presse-papier web :
  //   1. document `paste` event natif : capture Cmd+V utilisateur,
  //      marche sur HTTP non-localhost, pas de permission requise.
  //   2. bouton « Coller » : si navigator.clipboard.read est dispo
  //      (HTTPS / localhost) → paste direct ; sinon → toast invitant
  //      à utiliser Cmd+V (qui passera par la couche 1).
  function setupPasteButton() {
    const btn = document.getElementById('paste-btn');
    console.log('[paste] setupPasteButton btn=', btn);
    if (!btn) return;
    const hasRead = !!(navigator.clipboard
      && (navigator.clipboard.read || navigator.clipboard.readText));
    console.log('[paste] capabilities',
      ' isSecureContext=', window.isSecureContext,
      ' read=', !!(navigator.clipboard && navigator.clipboard.read),
      ' readText=', !!(navigator.clipboard && navigator.clipboard.readText));
    // Bouton TOUJOURS visible — on a la couche `paste` event en
    // fallback même si navigator.clipboard.read est absent.
    btn.hidden = false;
    btn.addEventListener('click', (ev) => {
      console.log('[paste] click');
      ev.preventDefault();
      if (hasRead) {
        handlePaste(btn);
      } else {
        const k = (navigator.platform || '').toLowerCase().includes('mac')
          ? '⌘V' : 'Ctrl+V';
        pasteToast('Appuie sur ' + k + ' pour coller', 'info');
      }
    });
  }

  // V1.4.2 — Listener global `paste`. Le browser émet cet événement
  // quand l'utilisateur fait Cmd+V/Ctrl+V dans la page (hors d'un
  // champ texte qui consomme l'événement). Ne nécessite ni permission
  // ni secure context. Marche sur HTTP LAN.
  function setupPasteEvent() {
    document.addEventListener('paste', (ev) => {
      const t = ev.target;
      const tag = t && t.tagName;
      // Si focus dans un INPUT/TEXTAREA, on laisse le paste local au
      // champ (ne pas voler le contenu pour upload).
      if (tag === 'INPUT' || tag === 'TEXTAREA' ||
          (t && t.isContentEditable)) {
        console.log('[paste] event skipped — input focus');
        return;
      }
      const data = ev.clipboardData;
      console.log('[paste] event types=',
        data ? Array.from(data.types) : '(no clipboardData)',
        ' files=', data ? data.files.length : 0);
      if (!data) return;

      const fakeFiles = extractFromClipboardData(data);
      console.log('[paste] event extracted n=', fakeFiles.length);
      if (fakeFiles.length === 0) {
        pasteToast('Presse-papier vide ou format non supporté', 'warning');
        return;
      }
      ev.preventDefault();
      stageFiles(fakeFiles);
      pasteToast(fakeFiles.length === 1
        ? `${fakeFiles[0].name} ajouté · choisis la destination`
        : `${fakeFiles.length} éléments ajoutés · choisis la destination`,
        'info');
    });
  }

  // Extrait des File objects depuis ClipboardEvent.clipboardData.
  // Priorité : Files (drag/Cmd+C Finder Chrome desktop), Items (image
  // inline), text/plain.
  function extractFromClipboardData(data) {
    const out = [];
    // 1. Files (Chrome desktop expose les fichiers copiés dans le
    // Finder ici).
    if (data.files && data.files.length > 0) {
      for (const f of data.files) out.push(f);
      return out;
    }
    // 2. Items binaires (image inline).
    if (data.items) {
      for (const it of data.items) {
        if (it.kind === 'file') {
          const f = it.getAsFile();
          if (f) {
            // Si le fichier n'a pas de nom (image inline), on en
            // génère un avec timestamp.
            if (!f.name || f.name === 'image.png' || f.name === '') {
              const ext = (it.type === 'image/png') ? 'png'
                : (it.type === 'image/jpeg') ? 'jpg' : 'bin';
              const renamed = new File([f],
                'clipboard-' + timestampSuffix() + '.' + ext,
                { type: it.type });
              out.push(renamed);
            } else {
              out.push(f);
            }
          }
        }
      }
      if (out.length > 0) return out;
    }
    // 3. Text fallback.
    const text = data.getData ? data.getData('text/plain') : '';
    if (text) {
      out.push(new File([text],
        'clipboard-' + timestampSuffix() + '.txt',
        { type: 'text/plain' }));
    }
    return out;
  }

  function pasteToast(text, kind) {
    clientLog('info', '[paste] ' + text);
    if (window.LTR.p2p && window.LTR.p2p.toast) {
      window.LTR.p2p.toast(text, kind || 'info');
      return;
    }
    // Fallback ultime si #p2p-toast indisponible.
    try { alert(text); } catch (e) {}
  }

  function timestampSuffix() {
    const d = new Date();
    const pad = (n) => String(n).padStart(2, '0');
    return d.getFullYear() + pad(d.getMonth() + 1) + pad(d.getDate())
         + '-' + pad(d.getHours()) + pad(d.getMinutes()) + pad(d.getSeconds());
  }

  async function handlePaste(btn) {
    if (btn) btn.disabled = true;
    console.log('[paste] handlePaste start',
      ' secureCtx=', window.isSecureContext,
      ' read=', !!(navigator.clipboard && navigator.clipboard.read),
      ' readText=', !!(navigator.clipboard && navigator.clipboard.readText));
    clientLog('info', '[paste] click — secureCtx='
      + (window.isSecureContext ? '1' : '0')
      + ' read=' + !!(navigator.clipboard && navigator.clipboard.read)
      + ' readText=' + !!(navigator.clipboard && navigator.clipboard.readText));
    try {
      const fakeFiles = await readClipboardItems();
      console.log('[paste] got fakeFiles n=', fakeFiles.length);
      if (fakeFiles.length === 0) {
        pasteToast('Presse-papier vide ou format non supporté', 'warning');
        return;
      }
      stageFiles(fakeFiles);
      pasteToast(fakeFiles.length === 1
        ? `${fakeFiles[0].name} ajouté · choisis la destination`
        : `${fakeFiles.length} éléments ajoutés · choisis la destination`,
        'info');
    } catch (e) {
      const msg = (e && e.message) || String(e);
      console.error('[paste] failed:', e);
      clientLog('warn', '[paste] failed: ' + msg);
      if (!window.isSecureContext) {
        pasteToast('Le presse-papier nécessite HTTPS ou localhost (page actuelle = HTTP)', 'warning');
      } else if (/denied|not allowed|notallowed/i.test(msg)) {
        pasteToast('Autorisation presse-papier refusée — voir paramètres navigateur', 'warning');
      } else {
        pasteToast('Erreur presse-papier : ' + msg, 'warning');
      }
    } finally {
      if (btn) btn.disabled = false;
    }
  }

  async function readClipboardItems() {
    const fakeFiles = [];
    if (navigator.clipboard.read) {
      console.log('[paste] tente clipboard.read()');
      const items = await navigator.clipboard.read();
      console.log('[paste] got items n=', items.length);
      for (const it of items) {
        console.log('[paste] item types=', it.types);
        if (it.types && it.types.includes('image/png')) {
          const blob = await it.getType('image/png');
          fakeFiles.push(new File([blob],
            'clipboard-' + timestampSuffix() + '.png',
            { type: 'image/png' }));
        } else if (it.types && it.types.includes('text/plain')) {
          const blob = await it.getType('text/plain');
          const text = await blob.text();
          if (text) {
            fakeFiles.push(new File([text],
              'clipboard-' + timestampSuffix() + '.txt',
              { type: 'text/plain' }));
          }
        }
      }
      return fakeFiles;
    }
    console.log('[paste] fallback readText()');
    const text = await navigator.clipboard.readText();
    console.log('[paste] readText len=', text ? text.length : 0);
    if (text) {
      fakeFiles.push(new File([text],
        'clipboard-' + timestampSuffix() + '.txt',
        { type: 'text/plain' }));
    }
    return fakeFiles;
  }

  function setupDropZone() {
    const dz = document.getElementById('drop-zone');
    if (!dz) return;
    ['dragenter', 'dragover'].forEach((ev) =>
      dz.addEventListener(ev, (e) => {
        e.preventDefault(); dz.classList.add('drag-over');
      }));
    ['dragleave', 'drop'].forEach((ev) =>
      dz.addEventListener(ev, (e) => {
        e.preventDefault(); dz.classList.remove('drag-over');
      }));
    dz.addEventListener('drop', (e) => {
      if (e.dataTransfer && e.dataTransfer.files) {
        stageFiles(e.dataTransfer.files);
      }
    });
  }

  function setupFileInputChange(selector) {
    const fi = document.querySelector(selector);
    if (!fi) return;
    fi.addEventListener('change', () => {
      stageFiles(fi.files, { source: selector === '#folder-input' ? 'folder' : 'files' });
      fi.value = '';
    });
  }

  // V1.1.7 : masquer le bouton "Choisir un dossier" sur iOS Safari
  // (webkitdirectory non supporté).
  function setupFolderButtonVisibility() {
    const folderBtn = document.getElementById('folder-btn');
    const folderHint = document.getElementById('folder-hint');
    if (!supportsFolderPick()) {
      if (folderBtn) folderBtn.hidden = true;
      if (folderHint) folderHint.hidden = false;
    }
  }

  // V1.4.3 — staging : on AJOUTE chaque fichier dans #upload-list avec
  // un menu « Envoyer à ▾ ». Pas d'envoi avant choix utilisateur.
  function stageFiles(fileList, opts) {
    opts = opts || {};
    const files = Array.from(fileList || []);
    if (files.length === 0) return;
    if (opts.source === 'folder' || files.some((f) => f.webkitRelativePath)) {
      const bundles = groupFolderFiles(files);
      for (const bundle of bundles) {
        if (bundle.files.length === 1 && !bundle.files[0].webkitRelativePath) {
          stageSingleFile(bundle.files[0]);
        } else {
          stageFolderBundle(bundle);
        }
      }
      return;
    }
    for (const f of files) {
      stageSingleFile(f);
    }
  }

  function groupFolderFiles(files) {
    const map = new Map();
    for (const f of files) {
      const rel = f.webkitRelativePath || f.name;
      const parts = rel.split('/').filter(Boolean);
      const root = parts.length > 1 ? parts[0] : '';
      const key = root || ('__file__:' + f.name + ':' + f.size + ':' + f.lastModified);
      if (!map.has(key)) map.set(key, { rootName: root || f.name, files: [] });
      map.get(key).files.push(f);
    }
    return Array.from(map.values()).map((b) => ({
      kind: b.files.some((f) => f.webkitRelativePath) ? 'folder' : 'file',
      rootName: b.rootName,
      files: b.files.sort((a, b2) => {
        const ar = a.webkitRelativePath || a.name;
        const br = b2.webkitRelativePath || b2.name;
        return ar.localeCompare(br);
      }),
      totalSize: b.files.reduce((sum, f) => sum + f.size, 0),
    }));
  }

  function stageSingleFile(file) {
    const id = stagingId();
    const li = renderStagingRow(id, file);
    document.getElementById('upload-list').appendChild(li);
    const draftId = profile() ? profile().addDraft(file) : null;
    staging.set(id, { kind: 'file', file, li, sent: false, draftId });
  }

  function stageFolderBundle(bundle) {
    const id = stagingId();
    const li = renderFolderRow(id, bundle);
    document.getElementById('upload-list').appendChild(li);
    const draftId = profile() ? profile().addDraft({
      name: bundle.rootName,
      size: bundle.totalSize,
      webkitRelativePath: bundle.rootName + '/',
      type: 'application/x-directory',
    }) : null;
    staging.set(id, {
      kind: 'folder',
      files: bundle.files,
      bundle,
      li,
      sent: false,
      draftId,
    });
  }

  function renderDraftRow(draft) {
    const li = document.createElement('li');
    li.className = 'upload-row staging-row upload-row-draft';
    li.dataset.draftId = draft.id || '';
    li.innerHTML = `
      <span class="f-icon">${iconFor(draft.name || 'fichier')}</span>
      <span class="f-name">${escapeHtml(draft.name || 'fichier')}</span>
      <span class="f-size">À re-sélectionner · ${formatBytes(draft.size || 0)}</span>
      <button type="button" class="btn-ghost remove-btn" aria-label="Retirer" title="Retirer">\u2715</button>`;
    li.querySelector('.remove-btn').addEventListener('click', () => {
      if (profile()) profile().removeDraft(draft.id);
      li.remove();
    });
    return li;
  }

  function renderStagingRow(id, file) {
    const rel = file.webkitRelativePath || file.name;
    const li = document.createElement('li');
    li.className = 'upload-row staging-row';
    li.dataset.stagingId = id;
    // V1.5 — aperçu image 40×40 si MIME image/* dispo. ObjectURL
    // révoqué au removeStaging pour libérer la RAM.
    let thumbHtml = `<span class="f-icon">${iconFor(file.name)}</span>`;
    let thumbUrl = null;
    if (file.type && file.type.startsWith('image/')) {
      try {
        thumbUrl = URL.createObjectURL(file);
        thumbHtml = `<img class="staging-thumb" src="${thumbUrl}"
                          alt="" loading="lazy">`;
      } catch (e) { /* fallback icon */ }
    }
    li.innerHTML = `
      ${thumbHtml}
      <span class="f-name">${escapeHtml(rel)}</span>
      <span class="f-size" data-role="progress">${formatBytes(file.size)}</span>
      <button type="button" class="btn btn-secondary send-to-btn">Envoyer \xC3\xA0 \u25BE</button>
      <button type="button" class="btn-ghost remove-btn" aria-label="Retirer" title="Retirer">\u2715</button>
      <div class="send-menu" hidden role="menu"></div>`;
    if (thumbUrl) li.dataset.thumbUrl = thumbUrl;
    li.querySelector('.send-to-btn').addEventListener('click', () =>
      toggleSendMenu(id));
    li.querySelector('.remove-btn').addEventListener('click', () =>
      removeStaging(id));
    return li;
  }

  function renderFolderRow(id, bundle) {
    const li = document.createElement('li');
    li.className = 'upload-row staging-row upload-row-folder';
    li.dataset.stagingId = id;
    li.innerHTML = `
      <span class="f-icon">📁</span>
      <span class="f-name">${escapeHtml(bundle.rootName)}</span>
      <span class="f-size" data-role="progress">${bundle.files.length} fichier${bundle.files.length > 1 ? 's' : ''} · ${formatBytes(bundle.totalSize)}</span>
      <button type="button" class="btn btn-secondary send-to-btn">Envoyer \xC3\xA0 \u25BE</button>
      <button type="button" class="btn-ghost remove-btn" aria-label="Retirer" title="Retirer">\u2715</button>
      <div class="send-menu" hidden role="menu"></div>`;
    li.querySelector('.send-to-btn').addEventListener('click', () =>
      toggleSendMenu(id));
    li.querySelector('.remove-btn').addEventListener('click', () =>
      removeStaging(id));
    return li;
  }

  function removeStaging(id) {
    const e = staging.get(id);
    if (!e) return;
    // V1.5 — libère la mémoire de la thumbnail si elle existait.
    if (e.li && e.li.dataset.thumbUrl) {
      try { URL.revokeObjectURL(e.li.dataset.thumbUrl); } catch (err) {}
    }
    if (e.li && e.li.parentNode) e.li.parentNode.removeChild(e.li);
    if (e.draftId && profile()) profile().removeDraft(e.draftId);
    staging.delete(id);
  }

  function toggleSendMenu(id) {
    const e = staging.get(id);
    if (!e) return;
    const menu = e.li.querySelector('.send-menu');
    if (!menu.hidden) { menu.hidden = true; return; }
    // Ferme tous les autres menus ouverts.
    document.querySelectorAll('.send-menu').forEach((m) => {
      if (m !== menu) m.hidden = true;
    });
    // Construit la liste : Host + chaque peer P2P connecté.
    const peers = (window.LTR.peers && window.LTR.peers.getAll
                   && window.LTR.peers.getAll()) || [];
    let html = '<button type="button" class="dest-btn" data-dest="host">'
             + '\xF0\x9F\x96\xA5\xEF\xB8\x8F  Host</button>';
    for (const p of peers) {
      const safe = escapeHtml(p.displayName);
      html += `<button type="button" class="dest-btn" data-dest="${escapeHtml(p.deviceId)}">`
            + `${escapeHtml(p.emoji)}  ${safe}</button>`;
    }
    if (peers.length === 0) {
      html += '<div class="dest-empty">Aucun appareil P2P connecté</div>';
    }
    menu.innerHTML = html;
    menu.hidden = false;
    menu.querySelectorAll('.dest-btn').forEach((b) => {
      b.addEventListener('click', () => {
        menu.hidden = true;
        dispatchToDestination(id, b.dataset.dest);
      });
    });
  }

  function dispatchToDestination(id, dest) {
    const e = staging.get(id);
    if (!e || e.sent) return;
    if (dest === 'host') {
      e.sent = true;
      if (profile()) profile().set('ui.lastDestination', 'host');
      if (e.draftId && profile()) profile().removeDraft(e.draftId);
      sendToHost(e);
    } else {
      e.sent = true;
      if (profile()) profile().set('ui.lastDestination', dest);
      if (e.draftId && profile()) profile().removeDraft(e.draftId);
      sendToPeer(e, dest);
    }
  }

  // Envoi vers Host : announce + upload (logique conservée de V1.1.7).
  async function sendToHost(entry) {
    const li   = entry.li;
    const progressEl = li.querySelector('[data-role="progress"]');
    li.querySelector('.send-to-btn').remove();
    li.querySelector('.remove-btn').remove();
    progressEl.textContent = 'en attente…';

    const files = entry.kind === 'folder' ? entry.files : [entry.file];
    clientLog('info', '[upload] announce host: ' + files.length + ' file(s)');
    const announcePayload = {
      bundleKind: entry.kind || 'file',
      bundleName: entry.bundle ? entry.bundle.rootName : '',
      files: files.map((file) => ({
        name:         file.name,
        size:         file.size,
        relativePath: file.webkitRelativePath || '',
      })),
    };
    let resp;
    try {
      resp = await fetch('/api/upload-announce', {
        method: 'POST',
        credentials: 'same-origin',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(announcePayload),
      });
    } catch (e) {
      progressEl.textContent = '✗ erreur réseau';
      return;
    }
    if (resp.status === 401) { goToLogin(); return; }
    let body = null;
    try { body = await resp.json(); } catch (e) {}
    if (!body || !body.accepted) {
      const reason = (body && body.reason) || 'refusé';
      progressEl.textContent = reason === 'timeout' ? '✗ pas de réponse' : '✗ refusé';
      setTimeout(() => removeStaging(entry.li.dataset.stagingId), 6000);
      return;
    }
    if (entry.kind === 'folder') {
      await uploadFolder(files, li, body.uploadId, entry.bundle.rootName);
    } else {
      await uploadOne(files[0], li, body.uploadId);
    }
  }

  // Envoi vers un peer P2P via WebRTC DataChannel.
  function sendToPeer(entry, peerDeviceId) {
    const peer = window.LTR.peers && window.LTR.peers.getPeer(peerDeviceId);
    if (!peer) {
      const progressEl = entry.li.querySelector('[data-role="progress"]');
      progressEl.textContent = '✗ pair indisponible';
      return;
    }
    if (!(window.LTR.p2p && window.LTR.p2p.startSendTo)) {
      const progressEl = entry.li.querySelector('[data-role="progress"]');
      progressEl.textContent = '✗ P2P indisponible';
      return;
    }
    // Marque la ligne comme « envoyée vers P2P » et la retire après un
    // court délai. Le suivi détaillé est géré par TransferRegistry V1.3
    // (zone TRANSFERTS · P2P).
    entry.li.querySelector('.send-to-btn').remove();
    entry.li.querySelector('.remove-btn').remove();
    const progressEl = entry.li.querySelector('[data-role="progress"]');
    progressEl.textContent = '→ ' + (peer.emoji || '') + ' ' + peer.displayName;
    if (entry.kind === 'folder') {
      window.LTR.p2p.startSendTo(peer, entry.files, {
        bundle: {
          kind: 'folder',
          name: entry.bundle.rootName,
          fileCount: entry.files.length,
          totalSize: entry.bundle.totalSize,
        },
      });
    } else {
      window.LTR.p2p.startSendTo(peer, [entry.file]);
    }
    setTimeout(() => removeStaging(entry.li.dataset.stagingId), 4000);
  }

  async function uploadFolder(files, li, uploadId, folderName) {
    const progressEl = li.querySelector('[data-role="progress"]');
    const total = files.reduce((sum, f) => sum + f.size, 0);
    let done = 0;
    for (let i = 0; i < files.length; i++) {
      const file = files[i];
      progressEl.textContent = `${i + 1}/${files.length} · 0 %`;
      const result = await uploadOne(file, li, uploadId, {
        labelPrefix: `${i + 1}/${files.length}`,
        onFileProgress: (sent) => {
          const pct = total > 0 ? Math.round(((done + sent) / total) * 100) : 0;
          progressEl.textContent = `${i + 1}/${files.length} · ${pct} %`;
        },
      });
      if (!result || !result.ok) return;
      done += file.size;
    }
    progressEl.textContent = `✓ ${folderName || 'dossier'} envoyé`;
    setTimeout(() => li.remove(), 4000);
  }

  // V1.6.5 — Sprint Stabilité (Wave 1, item C).
  // Tente 1 upload via XMLHttpRequest. Promise<{status, ok}>.
  // - status 200 : ok=true
  // - status 401 : ok=false, redirect /login (par appelant)
  // - autre 4xx/5xx : ok=false, retry possible (par appelant)
  // - erreur réseau (xhr.onerror) : ok=false, retry possible
  function tryUploadOnce(file, li, uploadId, opts) {
    opts = opts || {};
    return new Promise((resolve) => {
      const progressEl = li.querySelector('[data-role="progress"]');

      const fd = new FormData();
      fd.append('file', file);

      const xhr = new XMLHttpRequest();
      const rel = file.webkitRelativePath || '';
      const qs = 'upload_id=' + encodeURIComponent(uploadId)
               + (rel ? '&relative_path=' + encodeURIComponent(rel) : '');
      xhr.open('POST', '/api/upload?' + qs);
      xhr.setRequestHeader('X-Upload-Id', uploadId);
      if (rel) xhr.setRequestHeader('X-Relative-Path', rel);
      xhr.withCredentials = true;
      xhr.upload.onprogress = (e) => {
        if (!e.lengthComputable) return;
        if (opts.onFileProgress) {
          opts.onFileProgress(e.loaded, e.total);
        } else {
          progressEl.textContent =
            Math.round((e.loaded / e.total) * 100) + ' %';
        }
      };
      xhr.onload = () => resolve({ status: xhr.status, ok: xhr.status === 200 });
      xhr.onerror = () => resolve({ status: 0, ok: false });
      xhr.onabort = () => resolve({ status: 0, ok: false });
      xhr.send(fd);
    });
  }

  // V1.6.5 — orchestrateur retry. 3 tentatives avec backoff exponentiel
  // 1s/2s/4s. Sur échec final : affiche bouton « Reprendre » qui relance.
  // Sur 401 : redirige immédiatement vers /login (pas de retry).
  // Note : le serveur ne supporte PAS encore le resume avec offset
  // (V1.6.6). Chaque retry repart from scratch — utile uniquement pour
  // les coupures réseau brèves (< ~7s cumulés) ou les 5xx transitoires.
  function uploadOne(file, li, uploadId, opts) {
    opts = opts || {};
    return new Promise(async (resolve) => {
      const progressEl = li.querySelector('[data-role="progress"]');
      progressEl.textContent = opts.labelPrefix ? opts.labelPrefix + ' · 0 %' : '0 %';
      const delays = [1000, 2000, 4000];
      let lastStatus = 0;

      for (let attempt = 0; attempt <= delays.length; attempt++) {
        if (attempt > 0) {
          progressEl.textContent = '⏳ retry ' + attempt + '/3…';
          await new Promise((r) => setTimeout(r, delays[attempt - 1]));
          progressEl.textContent = opts.labelPrefix ? opts.labelPrefix + ' · 0 %' : '0 %';
        }
        const r = await tryUploadOnce(file, li, uploadId, opts);
        lastStatus = r.status;
        if (r.ok) {
          if (!opts.labelPrefix) {
            progressEl.textContent = '✓ envoyé';
            setTimeout(() => li.remove(), 4000);
          }
          resolve({ ok: true });
          return;
        }
        if (r.status === 401) {
          goToLogin();
          resolve({ ok: false });
          return;
        }
        // 4xx (sauf 401) : pas de retry (erreur cliente définitive)
        if (r.status >= 400 && r.status < 500) break;
      }

      // Échec final : bouton « Reprendre » manuel.
      progressEl.innerHTML = '';
      const span = document.createElement('span');
      span.textContent = lastStatus === 0
        ? '✗ erreur réseau · '
        : ('✗ échec (' + lastStatus + ') · ');
      const btn = document.createElement('button');
      btn.textContent = '↻ Reprendre';
      btn.className = 'btn-resume-upload';
      btn.onclick = () => {
        progressEl.textContent = '0 %';
        uploadOne(file, li, uploadId, opts);  // récursif, nouveau cycle 3 retries
      };
      progressEl.appendChild(span);
      progressEl.appendChild(btn);
      resolve({ ok: false });
    });
  }

  window.LTR = window.LTR || {};
  window.LTR.initUpload = init;
})();
