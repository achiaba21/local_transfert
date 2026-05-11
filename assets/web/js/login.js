// ============================================================
// LocalTransfer — page /login
// Gère la saisie du PIN 6 chiffres.
// Auto-submit uniquement pour les QR signés par l'URL autologin=1.
// ============================================================
(function () {
  'use strict';

  const $  = (s) => document.querySelector(s);
  const $$ = (s) => Array.from(document.querySelectorAll(s));
  const params = new URLSearchParams(window.location.search);
  const signedOut = params.get('signed_out') === '1';

  // ---------- device_id stable (localStorage) ----------
  function uuidv4() {
    // Fallback simple sans dépendance crypto.subtle (OK mobile safari).
    const hex = '0123456789abcdef';
    const bytes = new Uint8Array(16);
    if (window.crypto && window.crypto.getRandomValues) {
      window.crypto.getRandomValues(bytes);
    } else {
      for (let i = 0; i < 16; i++) bytes[i] = Math.floor(Math.random() * 256);
    }
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    let s = '';
    for (let i = 0; i < 16; i++) {
      if (i === 4 || i === 6 || i === 8 || i === 10) s += '-';
      s += hex[(bytes[i] >> 4) & 0xf] + hex[bytes[i] & 0xf];
    }
    return s;
  }

  function getDeviceId() {
    let id = null;
    try { id = localStorage.getItem('ltr_device_id'); } catch (e) { /* private mode */ }
    if (!id) {
      id = uuidv4();
      try { localStorage.setItem('ltr_device_id', id); } catch (e) { /* ignore */ }
    }
    return id;
  }

  // ---------- host name ----------
  fetch('/api/host-info')
    .then((r) => r.json())
    .then((info) => { $('#host-name').textContent = info.name || 'cet appareil'; })
    .catch(() => { /* ignore */ });

  if (!signedOut && window.LTR.tryRefreshSession) {
    window.LTR.tryRefreshSession().then((ok) => {
      if (ok) window.location.href = '/';
    }).catch(() => {});
  }

  // ---------- V1.6.4 — bandeau certificat HTTPS ----------
  // Désactive le bouton « Se connecter » tant que l'empreinte n'a pas été
  // confirmée par l'utilisateur (checkbox cochée). Si la même empreinte
  // a déjà été acceptée sur ce host, on auto-coche pour éviter le frottement
  // au retour de l'utilisateur.
  const submitBtn = $('.btn-login');
  const TRUST_KEY = 'ltr-cert-trusted-fingerprint';
  let certVerified = true;     // par défaut OK (HTTP plain ou empreinte mémorisée)
  let certCheckDone = window.location.protocol !== 'https:';
  let urlPrefilledPin = false;
  let qrAutoLogin = false;
  let autoLoginAttempted = false;

  function updateSubmitState() {
    if (submitBtn) submitBtn.disabled = !certVerified;
  }

  function maybeAutoSubmitFromQr() {
    if (!qrAutoLogin || !urlPrefilledPin || autoLoginAttempted) return;
    if (!certCheckDone || !certVerified) return;
    autoLoginAttempted = true;
    setTimeout(() => { submit(); }, 0);
  }

  if (window.location.protocol === 'https:') {
    fetch('/api/cert-info', { credentials: 'same-origin' })
      .then((r) => r.json())
      .then((info) => {
        const fp = (info && info.fingerprint) || '';
        if (!fp) return;
        let trustedFp = null;
        try { trustedFp = localStorage.getItem(TRUST_KEY); } catch (e) {}
        if (trustedFp === fp) {
          // Déjà accepté pour ce cert : pas de bandeau.
          return;
        }
        // Sinon : afficher le bandeau, désactiver le submit jusqu'à coche.
        certVerified = false;
        updateSubmitState();
        const banner = $('#cert-banner');
        $('#cert-fingerprint').textContent = fp;
        banner.hidden = false;
        $('#cert-trusted').addEventListener('change', (e) => {
          certVerified = !!e.target.checked;
          updateSubmitState();
          if (certVerified) {
            try { localStorage.setItem(TRUST_KEY, fp); } catch (e) {}
            maybeAutoSubmitFromQr();
          }
        });
      })
      .catch(() => { /* HTTP fallback ou cert-info indisponible : on laisse passer */ })
      .finally(() => {
        certCheckDone = true;
        maybeAutoSubmitFromQr();
      });
  }
  updateSubmitState();

  // ---------- saisie PIN ----------
  const inputs = $$('.pin-input');

  function clearError() {
    $('#pin-error').hidden = true;
    inputs.forEach((i) => i.classList.remove('error'));
  }

  function showError(msg) {
    const el = $('#pin-error');
    el.textContent = msg || 'Code incorrect, réessayez.';
    el.hidden = false;
    inputs.forEach((i) => { i.classList.add('error'); i.value = ''; });
    inputs[0].focus();
  }

  inputs.forEach((input, idx) => {
    input.addEventListener('input', () => {
      input.value = input.value.replace(/[^0-9]/g, '').slice(0, 1);
      clearError();
      // Auto-focus case suivante pour l'ergonomie mobile.
      // MAIS : aucun auto-submit, même quand toutes les cases sont remplies.
      if (input.value && idx < inputs.length - 1) inputs[idx + 1].focus();
    });
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Backspace' && !input.value && idx > 0) {
        inputs[idx - 1].focus();
      }
    });
    input.addEventListener('paste', (e) => {
      e.preventDefault();
      const txt = (e.clipboardData.getData('text') || '').replace(/\D/g, '');
      [...txt].forEach((c, i) => {
        if (inputs[i]) inputs[i].value = c;
      });
      clearError();
      const lastFilled = Math.min(txt.length - 1, inputs.length - 1);
      if (lastFilled >= 0) inputs[lastFilled].focus();
    });
  });

  inputs[0].focus();

  // V1.5 — Sprint Hardening : si l'URL contient ?pin=XXXXXX (QR avec
  // PIN scanné depuis la SharePanel desktop), pré-remplir les 6 cases.
  // Avec &autologin=1 (QR host), on soumet automatiquement après validation
  // du garde-fou certificat HTTPS.
  try {
    const prefilled = (params.get('pin') || '').replace(/\D/g, '');
    qrAutoLogin = params.get('autologin') === '1';
    if (prefilled.length === 6) {
      [...prefilled].forEach((c, i) => { if (inputs[i]) inputs[i].value = c; });
      inputs[5].focus();
      urlPrefilledPin = true;
      clientLog('info', 'pin pré-rempli depuis URL');
      maybeAutoSubmitFromQr();
    }
  } catch (e) { /* ignore */ }

  // V1.6.5 — Sprint Stabilité (Wave 3 item I).
  // « Mémoriser le PIN » : disabled si pas en HTTPS (WebCrypto requiert
  // un secure context). Pré-coche depuis localStorage si le PIN a déjà
  // été stocké pour ce cert et qu'on l'a chargé pour pré-remplir.
  let cachedCertFingerprint = '';
  const optPin = $('#opt-remember-pin');
  if (optPin) {
    if (window.LTR.pinStorage && !window.LTR.pinStorage.isAvailable()) {
      optPin.disabled = true;
      const row = $('#opt-pin-row');
      if (row) row.title = 'Disponible uniquement en HTTPS';
      const span = row ? row.querySelector('span') : null;
      if (span) span.textContent = 'Mémoriser le PIN (HTTPS uniquement)';
    }
  }

  // Si fingerprint cert dispo + PIN sauvegardé → pré-remplit silencieusement.
  if (window.location.protocol === 'https:'
      && window.LTR.pinStorage && window.LTR.pinStorage.isAvailable()) {
    fetch('/api/cert-info', { credentials: 'same-origin' })
      .then((r) => r.json())
      .then(async (info) => {
        const fp = (info && info.fingerprint) || '';
        if (!fp) return;
        cachedCertFingerprint = fp;
        const stored = await window.LTR.pinStorage.loadPin(fp);
        if (stored && stored.length === 6) {
          [...stored].forEach((c, i) => { if (inputs[i]) inputs[i].value = c; });
          if (optPin) optPin.checked = true;
          clientLog('info', 'PIN restauré depuis localStorage chiffré');
        }
      })
      .catch(() => { /* ignore */ });
  }

  // ---------- soumission ----------
  async function submit() {
    const pin = inputs.map((i) => i.value).join('');
    if (pin.length !== 6) {
      showError('Veuillez saisir les 6 chiffres.');
      return;
    }
    const device_id = getDeviceId();
    const remember = !!($('#opt-remember') && $('#opt-remember').checked);
    const rememberPin = !!(optPin && optPin.checked && !optPin.disabled);
    clientLog('info', 'submit POST /api/auth device_id='
                + device_id.substring(0, 8)
                + ' remember=' + remember + ' rememberPin=' + rememberPin);

    let resp;
    try {
      resp = await fetch('/api/auth', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ pin, device_id, remember }),
        credentials: 'same-origin',
      });
    } catch (e) {
      clientLog('error', 'fetch /api/auth threw: ' + (e && e.message));
      showError('Erreur réseau, vérifiez votre connexion.');
      return;
    }

    clientLog('info', '/api/auth status=' + resp.status);

    if (resp.status === 401) {
      let body = null;
      try { body = await resp.json(); } catch (e) {}
      if (body && body.device_id) {
        try { localStorage.setItem('ltr_device_id', body.device_id); } catch (e) {}
      }
      showError('Code incorrect, réessayez.');
      return;
    }

    if (resp.ok) {
      // V1.1.1 : le serveur a retourné 200 + Set-Cookie. Navigation
      // top-level explicite — plus fiable sur iOS Safari que fetch redirect.
      let body = null;
      try { body = await resp.json(); } catch (e) {}

      // V1.6.5 — Wave 3 item I : sauvegarde le PIN chiffré si demandé,
      // ou efface si l'utilisateur a explicitement décoché.
      if (window.LTR.pinStorage) {
        if (rememberPin && cachedCertFingerprint) {
          try {
            await window.LTR.pinStorage.savePin(pin, cachedCertFingerprint);
            clientLog('info', 'PIN chiffré sauvegardé');
          } catch (e) {
            clientLog('warn', 'savePin failed: ' + (e && e.message));
          }
        } else if (!rememberPin) {
          window.LTR.pinStorage.clearPin();
        }
      }

      const next = (body && body.next) || '/';
      clientLog('info', 'auth ok → navigation vers ' + next);
      window.location.href = next;
      return;
    }

    clientLog('error', 'auth unexpected status=' + resp.status);
    showError('Erreur serveur (' + resp.status + ')');
  }

  // Pont JS → logs serveur (visible par moi pendant le debug iOS).
  function clientLog(level, msg) {
    try {
      navigator.sendBeacon && navigator.sendBeacon(
        '/api/clientlog',
        new Blob([JSON.stringify({ level, msg: '[login] ' + msg })],
                 { type: 'application/json' }));
    } catch (e) { /* ignore */ }
  }

  $('#pin-form').addEventListener('submit', (e) => {
    e.preventDefault();
    submit();
  });
})();
