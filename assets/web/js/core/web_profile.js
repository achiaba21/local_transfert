// ============================================================
// web_profile.js — profil local du navigateur web
//
// Centralise les préférences UX non sensibles. Les cookies HttpOnly restent
// la source de vérité pour l'authentification.
// ============================================================
(function () {
  'use strict';

  const KEY = 'ltr-web-profile-v1';

  const defaults = {
    ui: {
      activeTxTab: 'host',
      installClosed: false,
      shareOpen: false,
      lastDestination: 'host',
    },
    drafts: [],
    updatedAt: 0,
  };

  let profile = load();

  function clone(v) {
    return JSON.parse(JSON.stringify(v));
  }

  function load() {
    try {
      const raw = localStorage.getItem(KEY);
      if (!raw) return clone(defaults);
      const parsed = JSON.parse(raw);
      return {
        ...clone(defaults),
        ...parsed,
        ui: { ...defaults.ui, ...(parsed.ui || {}) },
        drafts: Array.isArray(parsed.drafts) ? parsed.drafts : [],
      };
    } catch {
      return clone(defaults);
    }
  }

  function save() {
    profile.updatedAt = Date.now();
    try { localStorage.setItem(KEY, JSON.stringify(profile)); } catch {}
    if (window.LTR.idb) {
      window.LTR.idb.set('ltr-web-profile', 'default', clone(profile))
        .catch(() => {});
    }
  }

  async function hydrateFromIdb() {
    if (!window.LTR.idb) return profile;
    try {
      const stored = await window.LTR.idb.get('ltr-web-profile', 'default');
      if (stored && (!profile.updatedAt || stored.updatedAt > profile.updatedAt)) {
        profile = {
          ...clone(defaults),
          ...stored,
          ui: { ...defaults.ui, ...(stored.ui || {}) },
          drafts: Array.isArray(stored.drafts) ? stored.drafts : [],
        };
        save();
      }
    } catch {}
    return profile;
  }

  function get(path, fallback) {
    const parts = String(path || '').split('.').filter(Boolean);
    let cur = profile;
    for (const p of parts) {
      if (!cur || typeof cur !== 'object' || !(p in cur)) return fallback;
      cur = cur[p];
    }
    return cur;
  }

  function set(path, value) {
    const parts = String(path || '').split('.').filter(Boolean);
    if (parts.length === 0) return;
    let cur = profile;
    for (let i = 0; i < parts.length - 1; i++) {
      const p = parts[i];
      if (!cur[p] || typeof cur[p] !== 'object') cur[p] = {};
      cur = cur[p];
    }
    cur[parts[parts.length - 1]] = value;
    save();
  }

  function addDraft(file, destination) {
    if (!file) return null;
    const id = 'draft-' + Date.now().toString(36)
      + '-' + Math.random().toString(36).slice(2, 7);
    profile.drafts.push({
      id,
      name: file.webkitRelativePath || file.name || 'fichier',
      size: file.size || 0,
      type: file.type || '',
      destination: destination || profile.ui.lastDestination || 'host',
      createdAt: Date.now(),
      needsReselect: true,
    });
    profile.drafts = profile.drafts.slice(-30);
    save();
    return id;
  }

  function removeDraft(id) {
    if (!id) return;
    profile.drafts = profile.drafts.filter((d) => d.id !== id);
    save();
  }

  function clearDrafts() {
    profile.drafts = [];
    save();
  }

  function clearLocalIdentity() {
    try { localStorage.removeItem('ltr_device_id'); } catch {}
    try { localStorage.removeItem(KEY); } catch {}
    profile = clone(defaults);
    if (window.LTR.idb) {
      window.LTR.idb.delete('ltr-web-profile', 'default').catch(() => {});
    }
  }

  window.LTR = window.LTR || {};
  window.LTR.webProfile = {
    hydrateFromIdb,
    get,
    set,
    addDraft,
    removeDraft,
    clearDrafts,
    clearLocalIdentity,
    snapshot: () => clone(profile),
  };
})();
