// ============================================================
// idb.js — Wrapper IndexedDB minimal (V1.6.5 Wave 2)
//
// API exposée : window.LTR.idb
//   - get(store, key)         → Promise<value|undefined>
//   - set(store, key, value)  → Promise<void>
//   - delete(store, key)      → Promise<void>
//   - all(store)              → Promise<Array<{key, value}>>
//
// Stores créés à l'ouverture :
//   - 'ltr-p2p-pending'        : transferts P2P interrompus (Wave 2)
//   - 'ltr-p2p-known-peers'    : TOFU P2P, fingerprints DTLS connus (Wave 4)
//   - 'ltr-web-profile'        : préférences locales UX web
//   - 'ltr-web-inbox'          : fichiers reçus conservés en local
//
// Pas de dépendance externe — API native (CLAUDE.md).
// ============================================================
(function () {
  'use strict';

  const DB_NAME = 'ltr-app';
  const DB_VERSION = 3;
  const STORES = [
    'ltr-p2p-pending',
    'ltr-p2p-known-peers',
    'ltr-web-profile',
    'ltr-web-inbox',
  ];

  let dbPromise = null;

  function openDb() {
    if (dbPromise) return dbPromise;
    dbPromise = new Promise((resolve, reject) => {
      if (!window.indexedDB) {
        reject(new Error('IndexedDB not supported'));
        return;
      }
      const req = indexedDB.open(DB_NAME, DB_VERSION);
      req.onupgradeneeded = (e) => {
        const db = e.target.result;
        for (const name of STORES) {
          if (!db.objectStoreNames.contains(name)) {
            db.createObjectStore(name);
          }
        }
      };
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
    return dbPromise;
  }

  function tx(storeName, mode) {
    return openDb().then((db) => {
      const t = db.transaction(storeName, mode);
      return t.objectStore(storeName);
    });
  }

  function get(storeName, key) {
    return tx(storeName, 'readonly').then((store) => new Promise(
      (resolve, reject) => {
        const r = store.get(key);
        r.onsuccess = () => resolve(r.result);
        r.onerror = () => reject(r.error);
      }));
  }

  function set(storeName, key, value) {
    return tx(storeName, 'readwrite').then((store) => new Promise(
      (resolve, reject) => {
        const r = store.put(value, key);
        r.onsuccess = () => resolve();
        r.onerror = () => reject(r.error);
      }));
  }

  function del(storeName, key) {
    return tx(storeName, 'readwrite').then((store) => new Promise(
      (resolve, reject) => {
        const r = store.delete(key);
        r.onsuccess = () => resolve();
        r.onerror = () => reject(r.error);
      }));
  }

  function all(storeName) {
    return tx(storeName, 'readonly').then((store) => new Promise(
      (resolve, reject) => {
        const out = [];
        const cursorReq = store.openCursor();
        cursorReq.onsuccess = (e) => {
          const cursor = e.target.result;
          if (cursor) {
            out.push({ key: cursor.key, value: cursor.value });
            cursor.continue();
          } else {
            resolve(out);
          }
        };
        cursorReq.onerror = () => reject(cursorReq.error);
      }));
  }

  window.LTR = window.LTR || {};
  window.LTR.idb = { get, set, delete: del, all };
})();
