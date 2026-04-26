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
