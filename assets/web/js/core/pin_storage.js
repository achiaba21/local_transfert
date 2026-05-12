// ============================================================
// pin_storage.js — Stockage chiffré du PIN d'accès (V1.6.5 Wave 3 item I)
//
// API : window.LTR.pinStorage
//   - savePin(pin, certFingerprint)        → Promise<void>
//   - loadPin(certFingerprint)             → Promise<string|null>
//   - clearPin()                            → void
//
// Sécurité :
//   - Chiffrement AES-GCM 256-bit
//   - Clé dérivée via HKDF SHA-256 du fingerprint cert HTTPS
//   - Si le cert change (régénération, IP LAN différente), la clé HKDF
//     change → ancien PIN chiffré devient indéchiffrable → safe.
//   - localStorage entries : 'ltr-pin-encrypted' (cipher b64), 'ltr-pin-iv' (iv b64)
//   - WebCrypto disponible UNIQUEMENT en secure context (HTTPS, localhost).
//     En HTTP plain, savePin() rejette → la checkbox doit être disabled.
//
// ⚠️ PLACEHOLDER V1 — durcissement V2 ciblé.
// Le secret HKDF (`certFingerprint`) est PUBLIC : exposé via /api/cert-info,
// affiché dans le bandeau /login, et lisible dans le cert HTTPS lui-même.
// Un attaquant qui a accès au localStorage (XSS, vol device, malware) peut
// donc déchiffrer le PIN — la protection ne tient QUE contre la lecture
// passive du fichier localStorage.json.
//
// Modèle de menace V1 (assumé par BA Q4) :
//   - Threat protégé : un autre user du même device qui ouvre le browser
//     plus tard et tape `localStorage.getItem('ltr-pin-encrypted')` dans
//     la console → ne peut PAS déchiffrer sans le fingerprint cert.
//   - Threat NON protégé : XSS/malware/vol de device.
//
// Migration V2 ciblée :
//   - WebAuthn (passkey biométrique) au lieu d'AES-GCM pour gate l'accès
//     au PIN, OU
//   - Chiffrement avec une clé non-extractable (CryptoKey extractable=false)
//     persistée dans IndexedDB plutôt qu'un secret dérivable du fingerprint.
// ============================================================
(function () {
  'use strict';

  const STORAGE_PIN = 'ltr-pin-encrypted';
  const STORAGE_IV  = 'ltr-pin-iv';
  const HKDF_INFO   = 'ltr-pin-storage-v1';
  const HKDF_SALT   = 'ltr-localtransfer-salt';

  function isAvailable() {
    return !!(window.crypto && window.crypto.subtle && window.isSecureContext);
  }

  // Buffer/string conversions.
  function strToBuf(s) { return new TextEncoder().encode(s); }
  function bufToB64(buf) {
    const bytes = new Uint8Array(buf);
    let s = '';
    for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
    return btoa(s);
  }
  function b64ToBuf(b64) {
    const s = atob(b64);
    const bytes = new Uint8Array(s.length);
    for (let i = 0; i < s.length; i++) bytes[i] = s.charCodeAt(i);
    return bytes.buffer;
  }

  // Dérive une clé AES-GCM 256-bit depuis le fingerprint cert via HKDF.
  async function deriveKey(certFingerprint) {
    const baseKey = await crypto.subtle.importKey(
      'raw', strToBuf(certFingerprint),
      { name: 'HKDF' }, false, ['deriveKey']);
    return crypto.subtle.deriveKey(
      {
        name: 'HKDF',
        hash: 'SHA-256',
        salt: strToBuf(HKDF_SALT),
        info: strToBuf(HKDF_INFO),
      },
      baseKey,
      { name: 'AES-GCM', length: 256 },
      false,
      ['encrypt', 'decrypt']);
  }

  async function savePin(pin, certFingerprint) {
    if (!isAvailable()) throw new Error('WebCrypto non disponible (besoin HTTPS)');
    if (!certFingerprint) throw new Error('certFingerprint requis');
    const key = await deriveKey(certFingerprint);
    const iv = crypto.getRandomValues(new Uint8Array(12));
    const cipher = await crypto.subtle.encrypt(
      { name: 'AES-GCM', iv }, key, strToBuf(pin));
    try {
      localStorage.setItem(STORAGE_PIN, bufToB64(cipher));
      localStorage.setItem(STORAGE_IV,  bufToB64(iv.buffer));
    } catch (e) {
      throw new Error('localStorage indisponible: ' + e.message);
    }
  }

  async function loadPin(certFingerprint) {
    if (!isAvailable()) return null;
    if (!certFingerprint) return null;
    let cipherB64, ivB64;
    try {
      cipherB64 = localStorage.getItem(STORAGE_PIN);
      ivB64 = localStorage.getItem(STORAGE_IV);
    } catch { return null; }
    if (!cipherB64 || !ivB64) return null;
    try {
      const key = await deriveKey(certFingerprint);
      const iv = new Uint8Array(b64ToBuf(ivB64));
      const cipher = b64ToBuf(cipherB64);
      const plain = await crypto.subtle.decrypt(
        { name: 'AES-GCM', iv }, key, cipher);
      return new TextDecoder().decode(plain);
    } catch (e) {
      // Soit le cert a changé → clé HKDF différente, soit le storage est
      // corrompu. On nettoie pour repartir propre.
      clearPin();
      return null;
    }
  }

  function clearPin() {
    try {
      localStorage.removeItem(STORAGE_PIN);
      localStorage.removeItem(STORAGE_IV);
    } catch { /* ignore */ }
  }

  window.LTR = window.LTR || {};
  window.LTR.pinStorage = { isAvailable, savePin, loadPin, clearPin };
})();
