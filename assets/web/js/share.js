// share.js — Modal de partage de l'accès au host depuis un navigateur auth.
(function () {
  'use strict';

  const { clientLog, handle401Async } = window.LTR;
  const $ = (s) => document.querySelector(s);

  let currentInfo = null;

  function truncateFingerprint(fp) {
    if (!fp) return '';
    return fp.length > 35 ? fp.slice(0, 35) + '...' : fp;
  }

  async function copyText(text, status) {
    if (!text) return;
    try {
      await navigator.clipboard.writeText(text);
      status.textContent = 'Copié.';
    } catch {
      status.textContent = 'Copie indisponible sur ce navigateur.';
    }
  }

  async function fetchShareInfo() {
    const resp = await fetch('/api/share-info', { credentials: 'same-origin' });
    const auth = await handle401Async(resp);
    if (auth === true) return null;
    if (auth === 'retry') return fetchShareInfo();
    if (!resp.ok) throw new Error('share-info ' + resp.status);
    return resp.json();
  }

  function fillModal(info) {
    currentInfo = info;
    $('#share-url').textContent = info.loginUrl || info.httpsUrl || info.httpUrl || '';
    $('#share-pin').textContent = info.pin || '';
    const fpRow = $('#share-fingerprint-row');
    const fp = info.fingerprint || '';
    fpRow.hidden = !fp;
    $('#share-fingerprint').textContent = truncateFingerprint(fp);
    $('#share-qr-img').src = '/api/share-qr.png?t=' + Date.now();
    $('#share-status').textContent = '';
  }

  function openModal() {
    const modal = $('#share-modal');
    modal.hidden = false;
    document.body.classList.add('modal-open');
    if (window.LTR.webProfile) {
      window.LTR.webProfile.set('ui.shareOpen', true);
    }
  }

  function closeModal() {
    const modal = $('#share-modal');
    modal.hidden = true;
    document.body.classList.remove('modal-open');
    if (window.LTR.webProfile) {
      window.LTR.webProfile.set('ui.shareOpen', false);
    }
  }

  async function showShareModal() {
    try {
      const info = await fetchShareInfo();
      if (!info) return;
      fillModal(info);
      openModal();
    } catch (e) {
      clientLog('error', '[share] open failed: ' + (e && e.message));
      const status = $('#share-status');
      if (status) status.textContent = 'Partage indisponible.';
    }
  }

  function initShare() {
    const btn = $('#share-access-btn');
    if (!btn) return;
    btn.addEventListener('click', showShareModal);
    $('#share-close')?.addEventListener('click', closeModal);
    $('#share-backdrop')?.addEventListener('click', closeModal);
    document.addEventListener('keydown', (ev) => {
      if (ev.key === 'Escape' && !$('#share-modal').hidden) closeModal();
    });
    $('#share-copy-link')?.addEventListener('click', () => {
      copyText(currentInfo && currentInfo.loginUrl, $('#share-status'));
    });
    $('#share-copy-pin')?.addEventListener('click', () => {
      copyText(currentInfo && currentInfo.pin, $('#share-status'));
    });
    if (window.LTR.webProfile
        && window.LTR.webProfile.get('ui.shareOpen', false)) {
      showShareModal();
    }
  }

  window.LTR = window.LTR || {};
  window.LTR.initShare = initShare;
})();
