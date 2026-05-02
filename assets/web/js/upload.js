// ============================================================
// upload.js — flow d'upload web → host
//   1. announce (POST /api/upload-announce) → attend décision host
//   2. upload (POST /api/upload) par fichier avec uploadId
// ============================================================
(function () {
  'use strict';
  const {
    clientLog, goToLogin, escapeHtml, iconFor,
    supportsFolderPick
  } = window.LTR;
  const $ = (s) => document.querySelector(s);

  function init() {
    setupDropZone();
    setupFileInputChange('#file-input');
    setupFileInputChange('#folder-input');
    setupFolderButtonVisibility();
    setupPasteButton();
    setupPasteEvent();
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
      uploadFiles(fakeFiles).then(() => {
        pasteToast(fakeFiles.length === 1
          ? `${fakeFiles[0].name} envoyé`
          : `${fakeFiles.length} éléments envoyés`,
          'success');
      });
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
      await uploadFiles(fakeFiles);
      pasteToast(fakeFiles.length === 1
        ? `${fakeFiles[0].name} envoyé`
        : `${fakeFiles.length} éléments envoyés`,
        'success');
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
        uploadFiles(e.dataTransfer.files);
      }
    });
  }

  function setupFileInputChange(selector) {
    const fi = document.querySelector(selector);
    if (!fi) return;
    fi.addEventListener('change', () => {
      uploadFiles(fi.files);
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

  async function uploadFiles(fileList) {
    const files = Array.from(fileList || []);
    if (files.length === 0) return;

    // Ligne "en attente" immédiate.
    const rows = files.map((f) => {
      const rel = f.webkitRelativePath || f.name;
      const li = document.createElement('li');
      li.className = 'upload-row';
      li.innerHTML = `
        <span class="f-icon">${iconFor(f.name)}</span>
        <span class="f-name">${escapeHtml(rel)}</span>
        <span class="f-size" data-role="progress">en attente…</span>`;
      document.getElementById('upload-list').appendChild(li);
      return li;
    });

    clientLog('info',
      '[upload] announce ' + files.length + ' fichier(s)');

    // V1.1.7 : inclut relativePath pour chaque fichier (dossier préservé).
    const announcePayload = {
      files: files.map((f) => ({
        name:         f.name,
        size:         f.size,
        relativePath: f.webkitRelativePath || '',
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
      rows.forEach((li) =>
        li.querySelector('[data-role="progress"]').textContent = '✗ erreur réseau');
      return;
    }

    if (resp.status === 401) { goToLogin(); return; }

    let body = null;
    try { body = await resp.json(); } catch (e) {}

    if (!body || !body.accepted) {
      const reason = (body && body.reason) || 'refusé';
      rows.forEach((li) =>
        li.querySelector('[data-role="progress"]').textContent =
          reason === 'timeout' ? '✗ pas de réponse' : '✗ refusé');
      setTimeout(() => rows.forEach((li) => li.remove()), 6000);
      return;
    }

    const uploadId = body.uploadId;
    for (let i = 0; i < files.length; i++) {
      await uploadOne(files[i], rows[i], uploadId);
    }
  }

  function uploadOne(file, li, uploadId) {
    return new Promise((resolve) => {
      const progressEl = li.querySelector('[data-role="progress"]');
      progressEl.textContent = '0 %';

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
        progressEl.textContent =
          Math.round((e.loaded / e.total) * 100) + ' %';
      };
      xhr.onload = () => {
        if (xhr.status === 200) {
          progressEl.textContent = '✓ envoyé';
          setTimeout(() => li.remove(), 4000);
        } else if (xhr.status === 401) {
          goToLogin();
        } else {
          progressEl.textContent = '✗ échec (' + xhr.status + ')';
        }
        resolve();
      };
      xhr.onerror = () => {
        progressEl.textContent = '✗ erreur réseau';
        resolve();
      };
      xhr.send(fd);
    });
  }

  window.LTR = window.LTR || {};
  window.LTR.initUpload = init;
})();
