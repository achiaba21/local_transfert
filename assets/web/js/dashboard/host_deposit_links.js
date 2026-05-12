// Phase 2 — Onglet dashboard host pour gérer les liens de dépôt.
// Charge la liste, crée/révoque, affiche l'URL + un placeholder QR.

(() => {
  'use strict';

  const $ = (id) => document.getElementById(id);

  async function fetchLinks() {
    try {
      const r = await fetch('/api/host/deposit-links', { credentials: 'same-origin' });
      if (!r.ok) return [];
      const j = await r.json();
      return j.links || [];
    } catch (e) {
      return [];
    }
  }

  function fmtBytes(n) {
    if (!n) return 'Sans limite';
    if (n < 1024 * 1024 * 1024) return (n / (1024 * 1024)).toFixed(0) + ' Mo';
    return (n / (1024 * 1024 * 1024)).toFixed(1) + ' Go';
  }
  function fmtExpiry(l) {
    if (l.revoked) return 'Révoqué';
    if (l.expiresAt === 0) return 'Sans expiration';
    const left = l.expiresAt - Math.floor(Date.now() / 1000);
    if (left <= 0) return 'Expiré';
    if (left < 3600) return Math.round(left / 60) + ' min restantes';
    if (left < 86400) return Math.round(left / 3600) + ' h restantes';
    return Math.round(left / 86400) + ' j restants';
  }

  function buildUrl(token) {
    return location.origin + '/deposit/' + token;
  }

  function render(links) {
    const root = $('host-deposit-list');
    if (!root) return;
    root.innerHTML = '';
    if (links.length === 0) {
      root.innerHTML = '<p class="host-deposit-empty">Aucun lien de dépôt pour le moment.</p>';
      return;
    }
    for (const l of links) {
      const card = document.createElement('article');
      card.className = 'host-deposit-card' + (l.active ? '' : ' host-deposit-card-inactive');
      const url = buildUrl(l.token);
      card.innerHTML = `
        <header>
          <h3>${escapeHtml(l.label)}</h3>
          <span class="host-deposit-status">${fmtExpiry(l)}</span>
        </header>
        <p class="host-deposit-url"><code>${escapeHtml(url)}</code></p>
        <p class="host-deposit-meta">Limites : ${fmtBytes(l.maxBytesPerDeposit)} ·
           ${l.maxFilesPerDeposit > 0 ? l.maxFilesPerDeposit + ' fichiers max' : 'Fichiers illimités'}</p>
        <div class="host-deposit-actions">
          <button type="button" class="btn btn-secondary" data-action="copy">Copier l'URL</button>
          <button type="button" class="btn btn-danger" data-action="revoke">Révoquer</button>
        </div>
      `;
      card.querySelector('[data-action="copy"]').addEventListener('click', async () => {
        try { await navigator.clipboard.writeText(url); } catch (e) {}
      });
      card.querySelector('[data-action="revoke"]').addEventListener('click', async () => {
        if (!confirm('Révoquer ce lien ?')) return;
        await fetch('/api/host/deposit-links/' + encodeURIComponent(l.id),
                    { method: 'DELETE', credentials: 'same-origin' });
        load();
      });
      root.appendChild(card);
    }
  }

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;')
      .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  async function load() {
    render(await fetchLinks());
  }

  function expirySecondsFromChoice(choice) {
    const map = {
      '1h':    3600,
      '24h':   86400,
      '7d':    7 * 86400,
      '30d':   30 * 86400,
      'never': 0,
    };
    return map[choice] !== undefined ? map[choice] : 7 * 86400;
  }

  async function onCreate(e) {
    e.preventDefault();
    const form = e.target;
    const label   = form.querySelector('[name=label]').value.trim();
    const expChoice = form.querySelector('[name=expiry]').value;
    const maxMb   = parseInt(form.querySelector('[name=maxMb]').value, 10) || 0;
    const maxF    = parseInt(form.querySelector('[name=maxFiles]').value, 10) || 0;
    const consent = form.querySelector('[name=consentText]').value.trim();
    if (!label) return;

    const expSec = expirySecondsFromChoice(expChoice);
    const expiresAt = expSec === 0 ? 0 : Math.floor(Date.now() / 1000) + expSec;

    const r = await fetch('/api/host/deposit-links', {
      method: 'POST',
      credentials: 'same-origin',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        label,
        consentText: consent,
        maxBytesPerDeposit: maxMb * 1024 * 1024,
        maxFilesPerDeposit: maxF,
        expiresAt,
      }),
    });
    if (r.status === 402) {
      alert('Cette fonctionnalité nécessite un plan Business.');
      return;
    }
    if (!r.ok) {
      alert('Création du lien échouée.');
      return;
    }
    form.reset();
    load();
  }

  function init() {
    const form = $('host-deposit-form');
    if (form) form.addEventListener('submit', onCreate);
    load();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  // Expose pour debug + refresh manuel.
  window.HostDepositLinks = { reload: load };
})();
