// Phase 2 — Portail Client Externe : logique de la page deposit.html.
// Aucune dépendance sur les modules du dashboard (pin_storage, peers, p2p).
//
// Flux :
//   1. fetch /api/deposit/info?token=...    → métadonnées du lien
//   2. déposant remplit nom + consent       → unlock zone fichiers
//   3. POST /api/deposit/begin              → ouvre session
//   4. POST /api/deposit/upload (×N)        → multipart par fichier
//   5. POST /api/deposit/finalize           → reçu signé
//
// Erreurs : toujours un message non technique côté UI.

(() => {
  'use strict';

  const path = location.pathname.split('/');
  const token = path[path.length - 1] || '';

  const $ = (id) => document.getElementById(id);
  const labelEl   = $('deposit-label');
  const hostEl    = $('deposit-host');
  const consentEl = $('deposit-consent');
  const consentTx = $('deposit-consent-text');
  const limitsEl  = $('deposit-limits');
  const nameEl    = $('deposit-name');
  const dropEl    = $('deposit-drop');
  const lockEl    = $('deposit-lock');
  const browseBtn = $('deposit-browse-btn');
  const fileInput = $('deposit-file-input');
  const listEl    = $('deposit-file-list');
  const totalEl   = $('deposit-total');
  const submitBtn = $('deposit-submit');
  const progBox   = $('deposit-progress');
  const progFill  = $('deposit-progress-fill');
  const progText  = $('deposit-progress-text');
  const errEl     = $('deposit-error');
  const filesSect = $('deposit-files-section');

  let info = null;
  const pickedFiles = [];

  function fmtBytes(n) {
    if (n < 1024) return n + ' o';
    if (n < 1024 * 1024) return (n / 1024).toFixed(0) + ' Ko';
    if (n < 1024 * 1024 * 1024) return (n / (1024 * 1024)).toFixed(1) + ' Mo';
    return (n / (1024 * 1024 * 1024)).toFixed(2) + ' Go';
  }

  function showError(msg) {
    errEl.textContent = msg;
    errEl.hidden = false;
  }
  function clearError() {
    errEl.textContent = '';
    errEl.hidden = true;
  }

  function refreshLockState() {
    const hasName = nameEl.value.trim().length > 0;
    const consent = consentEl.checked;
    const ready = hasName && consent;
    filesSect.dataset.locked = ready ? 'false' : 'true';
    lockEl.hidden = ready;
    dropEl.hidden = !ready;
    refreshSubmitState();
  }

  function refreshSubmitState() {
    const hasName = nameEl.value.trim().length > 0;
    const consent = consentEl.checked;
    submitBtn.disabled = !(hasName && consent && pickedFiles.length > 0);
  }

  function renderFiles() {
    listEl.innerHTML = '';
    let total = 0;
    for (const f of pickedFiles) {
      const li = document.createElement('li');
      li.className = 'deposit-file-row';
      const name = document.createElement('span');
      name.className = 'deposit-file-name';
      name.textContent = f.name;
      const size = document.createElement('span');
      size.className = 'deposit-file-size';
      size.textContent = fmtBytes(f.size);
      const rm = document.createElement('button');
      rm.type = 'button';
      rm.className = 'deposit-file-remove';
      rm.setAttribute('aria-label', 'Retirer ce fichier');
      rm.textContent = '×';
      rm.addEventListener('click', () => {
        const idx = pickedFiles.indexOf(f);
        if (idx >= 0) pickedFiles.splice(idx, 1);
        renderFiles();
        refreshSubmitState();
      });
      li.append(name, size, rm);
      listEl.appendChild(li);
      total += f.size;
    }
    if (pickedFiles.length > 0 && info) {
      totalEl.hidden = false;
      if (info.maxBytes > 0) {
        totalEl.textContent = 'Total : ' + fmtBytes(total)
          + ' sur ' + fmtBytes(info.maxBytes);
      } else {
        totalEl.textContent = 'Total : ' + fmtBytes(total);
      }
    } else {
      totalEl.hidden = true;
    }
  }

  function addFiles(files) {
    clearError();
    const maxFiles = info && info.maxFiles ? info.maxFiles : 0;
    const maxBytes = info && info.maxBytes ? info.maxBytes : 0;
    let totalAfter = pickedFiles.reduce((acc, f) => acc + f.size, 0);
    for (const f of files) {
      if (maxFiles > 0 && pickedFiles.length >= maxFiles) {
        showError('Trop de fichiers (' + (pickedFiles.length + 1)
          + ' sur ' + maxFiles + ').');
        break;
      }
      totalAfter += f.size;
      if (maxBytes > 0 && totalAfter > maxBytes) {
        showError('Le dépôt dépasse la taille autorisée ('
          + fmtBytes(totalAfter) + ' sur ' + fmtBytes(maxBytes) + ').');
        break;
      }
      pickedFiles.push(f);
    }
    renderFiles();
    refreshSubmitState();
  }

  async function loadInfo() {
    try {
      const r = await fetch('/api/deposit/info?token=' + encodeURIComponent(token));
      if (!r.ok) {
        location.href = '/deposit_expired_redirect';
        return;
      }
      info = await r.json();
      labelEl.textContent     = info.label || 'Dépôt';
      hostEl.textContent      = info.hostName || '';
      consentTx.textContent   = info.consentText
        || ('J\'accepte que mes fichiers soient transmis à '
            + (info.hostName || ''));
      document.title          = (info.label || 'Dépôt') + ' — Envoi';
      const limParts = [];
      if (info.maxFiles > 0) limParts.push(info.maxFiles + ' fichiers max');
      if (info.maxBytes > 0) limParts.push(fmtBytes(info.maxBytes) + ' max');
      limitsEl.textContent = limParts.join(' · ') || 'Aucune limite';
    } catch (e) {
      showError('Impossible de charger les informations. Réessayez.');
    }
  }

  async function submit() {
    if (submitBtn.disabled) return;
    submitBtn.disabled = true;
    progBox.hidden = false;
    progText.textContent = 'Préparation…';
    progFill.style.width = '0%';
    clearError();

    let sessionId = null;
    try {
      const r = await fetch('/api/deposit/begin?token='
          + encodeURIComponent(token), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          name: nameEl.value.trim(),
          consent: consentEl.checked,
        }),
      });
      const j = await r.json();
      if (!r.ok) {
        showError(j.message || 'Une erreur est survenue.');
        progBox.hidden = true;
        submitBtn.disabled = false;
        return;
      }
      sessionId = j.sessionId;
    } catch (e) {
      showError('Connexion perdue. Réessayez.');
      progBox.hidden = true;
      submitBtn.disabled = false;
      return;
    }

    let uploaded = 0;
    for (const f of pickedFiles) {
      progText.textContent = 'Envoi de ' + f.name + '…';
      const fd = new FormData();
      fd.append('file', f, f.name);
      try {
        const r = await fetch('/api/deposit/upload?session='
            + encodeURIComponent(sessionId), {
          method: 'POST',
          body: fd,
        });
        if (!r.ok) {
          const j = await r.json().catch(() => ({}));
          showError(j.message || 'Échec d\'envoi de ' + f.name + '.');
          progBox.hidden = true;
          submitBtn.disabled = false;
          return;
        }
      } catch (e) {
        showError('Connexion perdue pendant l\'envoi. Réessayez.');
        progBox.hidden = true;
        submitBtn.disabled = false;
        return;
      }
      uploaded += 1;
      progFill.style.width = Math.round(uploaded / pickedFiles.length * 100) + '%';
    }

    progText.textContent = 'Finalisation…';
    try {
      const r = await fetch('/api/deposit/finalize?session='
          + encodeURIComponent(sessionId), { method: 'POST' });
      const j = await r.json();
      if (!r.ok) {
        showError(j.message || 'Une erreur est survenue à la finalisation.');
        progBox.hidden = true;
        submitBtn.disabled = false;
        return;
      }
      renderConfirmation(j);
    } catch (e) {
      showError('Connexion perdue. Réessayez.');
      progBox.hidden = true;
      submitBtn.disabled = false;
    }
  }

  function renderConfirmation(j) {
    document.body.innerHTML = `
      <main class="deposit-confirm">
        <span class="deposit-confirm-check" aria-hidden="true">✓</span>
        <h1>Dépôt enregistré</h1>
        <p class="deposit-confirm-id">Identifiant : <code>${j.receiptId}</code></p>
        <p class="deposit-confirm-meta">${j.fileCount} fichier(s) · ${fmtBytes(j.totalBytes || 0)}</p>
        <a class="deposit-confirm-btn"
           href="${j.downloadUrl}"
           download="deposit-receipt-${j.receiptId}.json">
          ⤓ Télécharger le reçu
        </a>
        <p class="deposit-confirm-thanks">Merci. <strong>${info.hostName || 'Votre interlocuteur'}</strong> a été prévenu de votre envoi.</p>
      </main>
      <footer class="deposit-footer">Sécurisé. Pas de cloud, pas de compte.</footer>
    `;
  }

  // --- wire UI ---
  nameEl.addEventListener('input', refreshLockState);
  consentEl.addEventListener('change', refreshLockState);
  browseBtn.addEventListener('click', () => fileInput.click());
  fileInput.addEventListener('change', (e) => {
    addFiles(e.target.files);
    fileInput.value = '';
  });
  dropEl.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropEl.classList.add('deposit-drop-over');
  });
  dropEl.addEventListener('dragleave', () => {
    dropEl.classList.remove('deposit-drop-over');
  });
  dropEl.addEventListener('drop', (e) => {
    e.preventDefault();
    dropEl.classList.remove('deposit-drop-over');
    if (e.dataTransfer && e.dataTransfer.files) {
      addFiles(e.dataTransfer.files);
    }
  });
  submitBtn.addEventListener('click', submit);

  loadInfo();
})();
