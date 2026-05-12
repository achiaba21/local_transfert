// ============================================================
// download.js — réception SSE + download host → web (XHR+blob+progress)
// ============================================================
(function () {
  'use strict';
  const { clientLog, goToLogin, escapeHtml, iconFor, formatBytes } = window.LTR;
  const $ = (s) => document.querySelector(s);

  let sse = null;
  const INBOX_STORE = 'ltr-web-inbox';
  // V1.2 — Sprint Web P2P : registre des handlers SSE supplémentaires
  // (peers.js, p2p.js). On les attache à l'ouverture de la connexion.
  const pendingListeners = [];

  function init() {
    setupInbox();
    renderInbox();
    openSse();
  }

  async function addInboxBlob(meta, blob) {
    if (!window.LTR.idb || !blob) return null;
    const id = 'inbox-' + Date.now().toString(36)
      + '-' + Math.random().toString(36).slice(2, 8);
    const entry = Object.assign({
      id,
      name: 'download',
      size: blob.size || 0,
      kind: 'file',
      from: 'Hôte',
      receivedAt: Date.now(),
    }, meta || {}, { blob });
    await window.LTR.idb.set(INBOX_STORE, id, entry);
    await renderInbox();
    return id;
  }

  function setupInbox() {
    const clear = $('#inbox-clear');
    if (clear) {
      clear.addEventListener('click', async () => {
        if (!window.LTR.idb) return;
        if (!confirm('Supprimer les fichiers reçus conservés dans ce navigateur ?')) {
          return;
        }
        const all = await window.LTR.idb.all(INBOX_STORE).catch(() => []);
        for (const item of all) {
          await window.LTR.idb.delete(INBOX_STORE, item.key).catch(() => {});
        }
        await renderInbox();
      });
    }
  }

  async function storageText() {
    if (!navigator.storage || !navigator.storage.estimate) return 'Stockage : —';
    try {
      const est = await navigator.storage.estimate();
      const used = est.usage || 0;
      const quota = est.quota || 0;
      if (!quota) return 'Stockage : ' + formatBytes(used);
      return 'Stockage : ' + formatBytes(used) + ' / ' + formatBytes(quota);
    } catch (e) {
      return 'Stockage : —';
    }
  }

  async function renderInbox() {
    const list = $('#download-list');
    if (!list) return;
    const storage = $('#inbox-storage');
    if (storage) storage.textContent = await storageText();
    const pending = Array.from(list.querySelectorAll('[data-live-offer="1"]'));
    list.innerHTML = '';
    pending.forEach((n) => list.appendChild(n));
    const all = window.LTR.idb
      ? await window.LTR.idb.all(INBOX_STORE).catch(() => [])
      : [];
    const entries = all.map((x) => x.value)
      .filter(Boolean)
      .sort((a, b) => (b.receivedAt || 0) - (a.receivedAt || 0));
    for (const e of entries) list.appendChild(renderInboxRow(e));
    if (list.children.length === 0) {
      const empty = document.createElement('li');
      empty.className = 'empty-state';
      empty.textContent = 'Aucun fichier reçu pour le moment.';
      list.appendChild(empty);
    }
    $('#recv-count').textContent = String(entries.length);
  }

  function renderInboxRow(entry) {
    const li = document.createElement('li');
    li.className = 'file-row file-row-dl inbox-row';
    const date = entry.receivedAt ? new Date(entry.receivedAt) : null;
    const when = date ? date.toLocaleString() : '';
    li.innerHTML = `
      <div class="dl-head">
        <span class="f-icon">${entry.kind === 'folder' ? '📦' : iconFor(entry.name)}</span>
        <span class="f-name">${escapeHtml(entry.name || 'download')}</span>
        <span class="f-size">${formatBytes(entry.size || 0)}</span>
      </div>
      <div class="inbox-row-meta">
        <span>${escapeHtml(entry.from || 'Hôte')}</span>
        <span>·</span>
        <span>${escapeHtml(when)}</span>
        ${entry.fileCount ? `<span>· ${entry.fileCount} fichier${entry.fileCount > 1 ? 's' : ''}</span>` : ''}
      </div>
      <div class="inbox-actions">
        <button type="button" class="btn btn-primary btn-dl">Télécharger</button>
        <button type="button" class="btn-ghost inbox-delete">Supprimer</button>
      </div>`;
    li.querySelector('.btn-dl').addEventListener('click', () => {
      if (!entry.blob) return;
      triggerBlobDownload(entry.blob, entry.name || 'download');
    });
    li.querySelector('.inbox-delete').addEventListener('click', async () => {
      if (window.LTR.idb) await window.LTR.idb.delete(INBOX_STORE, entry.id);
      await renderInbox();
    });
    return li;
  }

  function triggerBlobDownload(blob, filename) {
    const a = document.createElement('a');
    const objectUrl = URL.createObjectURL(blob);
    a.href = objectUrl;
    a.download = filename || 'download';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(objectUrl), 2000);
  }

  function openSse() {
    if (sse) return;
    clientLog('info', '[dl] openSse');
    const es = new EventSource('/api/events', { withCredentials: true });
    sse = es;

    es.onopen = () => clientLog('info', '[dl] sse onopen');

    es.addEventListener('files-offer', (ev) => {
      clientLog('info', '[dl] sse files-offer');
      try {
        renderOffer(JSON.parse(ev.data));
      } catch (e) {
        clientLog('error', '[dl] bad files-offer');
      }
    });

    // Attache les handlers enregistrés par d'autres modules.
    pendingListeners.forEach(({ name, cb }) => es.addEventListener(name, cb));
    pendingListeners.length = 0;

    es.onerror = () => {
      if (es.readyState === EventSource.CLOSED) {
        clientLog('warn', '[dl] sse CLOSED → goToLogin');
        goToLogin();
      }
    };
  }

  // V1.2 — exposé pour peers.js / p2p.js. Si SSE déjà ouverte, attache
  // immédiatement ; sinon mémorise pour l'attacher à l'open.
  function addSseListener(name, cb) {
    if (sse) sse.addEventListener(name, cb);
    else pendingListeners.push({ name, cb });
  }

  function renderOffer(data) {
    const list = $('#download-list');
    const empty = list.querySelector('.empty-state');
    if (empty) empty.remove();

    const files = data.files || [];

    // V1.1.9-batch : bouton « Télécharger tout » si ≥ 2 fichiers.
    if (files.length >= 2) {
      const totalBytes = files.reduce((s, f) => s + (f.size || 0), 0);
      const wrapper = document.createElement('li');
      wrapper.className = 'dl-bundle';
      wrapper.dataset.liveOffer = '1';
      wrapper.innerHTML = `
        <button type="button" class="btn btn-primary dl-bundle-btn">
          Télécharger tout (${files.length} · ${formatBytes(totalBytes)})
        </button>`;
      list.appendChild(wrapper);
      wrapper.querySelector('.dl-bundle-btn').addEventListener('click',
        () => downloadBundle(files, totalBytes, wrapper));
    }

    files.forEach((f) => {
      const li = document.createElement('li');
      li.className = 'file-row file-row-dl';
      li.dataset.liveOffer = '1';
      const url = '/api/download/' + encodeURIComponent(f.ticketId);
      const btnId = 'dl-' + f.ticketId;
      li.innerHTML = `
        <div class="dl-head">
          <span class="f-icon">${iconFor(f.name)}</span>
          <span class="f-name">${escapeHtml(f.name)}</span>
          <span class="f-size">${formatBytes(f.size)}</span>
          <button type="button" class="btn btn-primary btn-dl" id="${btnId}">Télécharger</button>
        </div>
        <div class="dl-progress" hidden>
          <div class="dl-bar"><span></span></div>
          <div class="dl-meta">
            <span class="dl-pct">0 %</span> · <span class="dl-speed">—</span>
          </div>
        </div>`;
      list.appendChild(li);

      li.querySelector('#' + btnId).addEventListener('click',
        () => downloadWithProgress(url, f.name, f.size, li));
    });

    $('#recv-count').textContent =
      String(list.querySelectorAll('.file-row').length);
  }

  // XHR streaming : onprogress pour % + vitesse en temps réel.
  function downloadWithProgress(url, filename, expectedSize, rowEl) {
    const btn = rowEl.querySelector('.btn-dl');
    const progressEl = rowEl.querySelector('.dl-progress');
    const barSpan = rowEl.querySelector('.dl-bar > span');
    const pctEl = rowEl.querySelector('.dl-pct');
    const speedEl = rowEl.querySelector('.dl-speed');

    btn.disabled = true;
    btn.textContent = 'Téléchargement…';
    progressEl.hidden = false;

    const startTime = Date.now();
    const xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.responseType = 'blob';
    xhr.withCredentials = true;

    xhr.onprogress = (e) => {
      const total = e.lengthComputable ? e.total : expectedSize;
      const loaded = e.loaded;
      if (total > 0) {
        const pct = Math.round((loaded / total) * 100);
        barSpan.style.width = pct + '%';
        pctEl.textContent = pct + ' %';
      } else {
        pctEl.textContent = formatBytes(loaded);
      }
      const elapsed = (Date.now() - startTime) / 1000;
      if (elapsed > 0.2) {
        speedEl.textContent = formatBytes(loaded / elapsed) + '/s';
      }
    };

    xhr.onload = () => {
      if (xhr.status === 401) { goToLogin(); return; }
      if (xhr.status !== 200) {
        btn.textContent = '✗ Échec (' + xhr.status + ')';
        btn.disabled = false;
        progressEl.hidden = true;
        return;
      }
      const blob = xhr.response;
      addInboxBlob({
        name: filename || 'download',
        size: expectedSize || blob.size || 0,
        kind: filename && filename.endsWith('.zip') ? 'folder' : 'file',
        from: 'Hôte',
      }, blob).catch((e) =>
        clientLog('warn', '[inbox] add failed: ' + (e && e.message)));
      triggerBlobDownload(blob, filename || 'download');

      barSpan.style.width = '100%';
      pctEl.textContent = '✓ terminé';
      btn.textContent = '✓ Téléchargé';
      setTimeout(() => { rowEl.remove(); renderInbox(); }, 3500);
    };

    xhr.onerror = () => {
      btn.textContent = '✗ Erreur réseau';
      btn.disabled = false;
      progressEl.hidden = true;
    };

    xhr.send();
  }

  // V1.1.9-batch : « Télécharger tout » — fetch un ZIP unique côté serveur
  // si total < 4 Go (limite ZIP32), sinon fallback séquentiel.
  async function downloadBundle(files, totalBytes, wrapperEl) {
    const btn = wrapperEl.querySelector('.dl-bundle-btn');
    btn.disabled = true;
    btn.textContent = 'Préparation…';

    const LIMIT = 4 * 1024 * 1024 * 1024 - (1 << 20); // 4 Go - 1 Mo marge
    if (totalBytes < LIMIT) {
      // Bundle ZIP serveur.
      try {
        const res = await fetch('/api/download/bundle',
          { credentials: 'same-origin' });
        if (res.status === 401) { goToLogin(); return; }
        if (!res.ok) throw new Error('http ' + res.status);
        const blob = await res.blob();
        const name = 'LocalTransfer-' + Date.now() + '.zip';
        await addInboxBlob({
          name,
          size: blob.size,
          kind: 'folder',
          from: 'Hôte',
          fileCount: files.length,
        }, blob).catch((e) =>
          clientLog('warn', '[inbox] bundle add failed: ' + (e && e.message)));
        triggerBlobDownload(blob, name);
        btn.textContent = '✓ Téléchargé';
        setTimeout(() => { wrapperEl.remove(); renderInbox(); }, 3000);
      } catch (e) {
        clientLog('error', '[dl] bundle: ' + e);
        btn.textContent = '✗ Échec bundle — utilisez les boutons individuels';
        btn.disabled = false;
      }
    } else {
      // Fallback séquentiel : chaque ticket DL un par un.
      btn.textContent = 'Téléchargement séquentiel…';
      for (const f of files) {
        const row = document.querySelector(
          '[id="dl-' + f.ticketId + '"]')?.closest('.file-row');
        if (row) {
          await new Promise((resolve) => {
            const onClickBtn = row.querySelector('.btn-dl');
            if (!onClickBtn) return resolve();
            onClickBtn.click();
            // Attendre la fin (~ marquage ✓ Téléchargé OU error)
            const observer = new MutationObserver(() => {
              if (onClickBtn.textContent.startsWith('✓')
                  || onClickBtn.textContent.startsWith('✗')) {
                observer.disconnect();
                resolve();
              }
            });
            observer.observe(onClickBtn, { childList: true,
                                           characterData: true,
                                           subtree: true });
          });
        }
      }
      btn.textContent = '✓ Tous téléchargés';
      setTimeout(() => wrapperEl.remove(), 3000);
    }
  }

  window.LTR = window.LTR || {};
  window.LTR.initDownload  = init;
  window.LTR.addSseListener = addSseListener;
  window.LTR.webInbox = { addBlob: addInboxBlob, render: renderInbox };
})();
