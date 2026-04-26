# Architecture — Sprint Web Batch UX

**Date :** 2026-04-24
**NEW_PROJECT :** false
**UI_REQUIRED :** true (mockups badge + modale liste dans ui-proposal.md)

---

## 1. Vue d'ensemble

| Composant | Axe A (Bundle DL) | Axe B (Inbox host) |
|-----------|-------------------|--------------------|
| Backend | +route `/api/download/bundle/:sid`, +helper `listBySession` | +`AppState::webInbox`, +methods accept/refuse/all |
| Web JS/CSS | bouton « Télécharger tout » + logique hybride | — |
| Desktop UI | — | badge header + modale liste étendue |
| Config | — | `webAnnounceTimeoutSec` (default 300) |

### Fichiers à toucher

| Fichier | Action |
|---------|--------|
| `include/ltr/web/download_ticket_store.hpp` + `.cpp` | +`listBySession(sid)` |
| `src/web/routes/download_routes.cpp` | +route bundle |
| `include/ltr/infra/config.hpp` + `src/infra/config.cpp` | +`webAnnounceTimeoutSec` |
| `src/web/routes/upload_routes.cpp` | utilise `cfg.webAnnounceTimeoutSec` au lieu de 120 s hardcodé |
| `include/ltr/app/app_state.hpp` | +struct `WebInboxEntry` + `std::vector<WebInboxEntry> webInbox` + `float webInboxFadeElapsed` |
| `include/ltr/app/app_controller.hpp` | +`acceptWebUpload(id)`, `refuseWebUpload(id)`, `acceptAllWebUploads()`, `refuseAllWebUploads()` |
| `src/app/app_controller.cpp` | onEvent WebIncomingOfferEvent → push inbox, signatures étendues, accept-all avec un seul folder picker |
| `include/ltr/ui/ui_app.hpp` + `src/ui/ui_app.cpp` | plus de fallback sur `pendingWebOffer` (supprimé de AppState) — toutes les demandes web vont dans l'inbox |
| `include/ltr/ui/screens/main_screen.hpp` + `src/ui/screens/main_screen.cpp` | +badge header + hit test + fade |
| `include/ltr/ui/screens/incoming_offer_screen.hpp` + `src/ui/screens/incoming_offer_screen.cpp` | +mode `InboxList` affichant N entrées avec boutons |
| `assets/web/js/download.js` | +bouton bundle + logique hybride |
| `assets/web/css/style.css` | +styles `.dl-bundle` |
| `tests/test_download_ticket.cpp` | +cas listBySession (store déjà prête) |
| `docs-agents/WEB.md` | +sections bundle + inbox |

---

## 2. Backend — listBySession + bundle route

### 2.1 DownloadTicketStore::listBySession

```cpp
// include/ltr/web/download_ticket_store.hpp
class DownloadTicketStore {
public:
    // ... existant ...

    // V1.1.9-batch : retourne tous les tickets actifs d'une session.
    // Utile pour le bundle download web.
    std::vector<DownloadTicket> listBySession(
        const std::string& sessionToken) const;
};
```

Impl : parcourt `tickets_` sous mutex, filtre par `sessionToken`,
retourne une copie.

### 2.2 Route bundle

```cpp
// src/web/routes/download_routes.cpp
server.Get("/api/download/bundle", [&svc](...) {
    const auto token = readTokenCookie(req);
    const auto sess = svc.sessions().validate(token);
    if (!sess) { res.status = 401; return; }
    svc.sessions().touch(token);

    const auto tickets = svc.tickets().listBySession(token);
    if (tickets.empty()) { res.status = 404; return; }

    // Fusionner tous les tickets en une seule liste ZipEntry.
    std::vector<ZipEntry> entries;
    for (const auto& tkt : tickets) {
        if (tkt.kind == TicketKind::File) {
            ZipEntry e;
            e.abs      = tkt.path;
            e.relInZip = tkt.displayName;
            e.size     = tkt.size;
            entries.push_back(std::move(e));
        } else { // StreamingZip : développer les entries
            for (const auto& ze : tkt.zipEntries) {
                entries.push_back(ze);  // relInZip garde arbo
            }
        }
    }

    const auto zipSize = StreamingZipSource::computeZipSize(entries);
    const std::string filename = "LocalTransfer-" + now_ts() + ".zip";

    res.set_header("Content-Disposition",
        "attachment; filename*=UTF-8''" + rfc5987Encode(filename));
    res.set_header("Content-Length", std::to_string(zipSize));
    res.set_header("Cache-Control", "no-store");

    auto source = std::make_shared<StreamingZipSource>(std::move(entries));
    res.set_content_provider(zipSize, "application/zip",
        [source](std::size_t, std::size_t length,
                 httplib::DataSink& sink) {
            const bool more = source->provide(sink, length);
            if (!more) sink.done();
            return more && !source->errored();
        });
});
```

Note : URL sans `:sid` dans la path — on prend la session depuis le
cookie. Plus simple et évite qu'un autre utilisateur ne sniffe un sid.
Route : `GET /api/download/bundle` (pas de param).

---

## 3. Config — timeout

```cpp
struct Config {
    // ... existant ...
    int webAnnounceTimeoutSec{300};  // V1.1.9-batch
};
```

Chargé/sauvé comme les autres champs (j.value avec default).
`WebService` expose `cfg.webAnnounceTimeoutSec` via ctor paramètre.
`upload_routes.cpp` lit `svc.announceTimeoutSec()` au lieu de la
constante 120 s.

---

## 4. AppState — webInbox

```cpp
// include/ltr/app/app_state.hpp

struct WebInboxEntry {
    std::string   uploadId;
    std::string   senderName;      // "iPhone (Safari)"
    std::uint64_t totalBytes{0};
    int           filesCount{0};
    std::string   firstFileName;   // affichage compact
    // V2 : liste détaillée si besoin
};

struct AppState {
    // ... existant ...

    // V1.1.9-batch : file des demandes entrantes web non traitées.
    // Chaque WebIncomingOfferEvent y ajoute une entrée. Vidée par
    // accept/refuse (par id ou all).
    std::vector<WebInboxEntry> webInbox;
    // Secondes depuis le passage à count==0, pour fade du badge.
    float webInboxFadeSec{0.f};
    // Flag UI : la modale inbox est-elle ouverte ?
    bool webInboxModalOpen{false};

    // SUPPRESSION de l'ancienne version mono-demande :
    // std::optional<PendingWebOffer> pendingWebOffer;  (plus utilisé)
};
```

**Note** : le flux natif TCP (offre avec PIN) garde son
`std::optional<IncomingOffer> incomingOffer` existant. Seul le flux web
migre vers la liste.

---

## 5. AppController — logique inbox

```cpp
// include/ltr/app/app_controller.hpp
void acceptWebUpload(const std::string& uploadId);
void refuseWebUpload(const std::string& uploadId);
void acceptAllWebUploads();
void refuseAllWebUploads();
void toggleWebInboxModal();   // open/close
```

### Impl — onEvent WebIncomingOfferEvent

```cpp
// Avant (supprimé) :
// state_.pendingWebOffer = {...};

// Après :
WebInboxEntry entry;
entry.uploadId       = e.uploadId;
entry.senderName     = e.senderName;
entry.totalBytes     = e.totalBytes;
entry.filesCount     = e.filesCount;
entry.firstFileName  = e.firstFileName;
state_.webInbox.push_back(entry);
state_.webInboxFadeSec = 0.f; // reset fade si nouvelle demande
```

### Impl — acceptWebUpload (un seul)

```cpp
void AppController::acceptWebUpload(const std::string& id) {
    // 1) Retirer de l'inbox
    auto it = std::find_if(state_.webInbox.begin(), state_.webInbox.end(),
        [&](const auto& e){ return e.uploadId == id; });
    if (it == state_.webInbox.end()) return;
    state_.webInbox.erase(it);

    // 2) Folder picker
    std::string chosen;
    if (folderPicker_) chosen = folderPicker_(cfg_.downloadDir.string());
    if (chosen.empty()) {
        // Cancel picker → refuse
        if (web_) web_->announces().resolveRefuse(id);
        return;
    }
    if (web_) web_->announces().resolveAccept(id,
        std::filesystem::path(chosen));
}
```

### Impl — acceptAllWebUploads (UN folder picker global)

```cpp
void AppController::acceptAllWebUploads() {
    if (state_.webInbox.empty()) return;

    // 1) UN folder picker pour toute la salve
    std::string chosen;
    if (folderPicker_) chosen = folderPicker_(cfg_.downloadDir.string());
    if (chosen.empty()) return; // cancel picker → rien ne se passe

    const std::filesystem::path dir(chosen);
    // 2) Snapshot ids avant de muter
    std::vector<std::string> ids;
    for (const auto& e : state_.webInbox) ids.push_back(e.uploadId);
    state_.webInbox.clear();

    // 3) Résoudre chaque announce vers le même dir
    for (const auto& id : ids) {
        if (web_) web_->announces().resolveAccept(id, dir);
    }
}
```

### Refuse — symétrique (pas de folder picker)

```cpp
void AppController::refuseAllWebUploads() {
    std::vector<std::string> ids;
    for (const auto& e : state_.webInbox) ids.push_back(e.uploadId);
    state_.webInbox.clear();
    for (const auto& id : ids) {
        if (web_) web_->announces().resolveRefuse(id);
    }
}
```

### Fade update (dans tick)

```cpp
void AppController::tick() {
    // ... existant (drain events, auto-clean transfers) ...

    // V1.1.9-batch : fade badge inbox
    if (state_.webInbox.empty()) {
        state_.webInboxFadeSec += /* dt proxy via frame_count */;
    } else {
        state_.webInboxFadeSec = 0.f;
    }
}
```

Note : tick() n'a pas de dt. On peut :
- Passer dt depuis UiApp (plus propre)
- OU utiliser un counter frame pour un proxy approximatif

Simpler : passer dt au tick. Bien qu'un léger refactor, c'est
cohérent avec `update(dt)` déjà passé aux screens. MainScreen passe
l'info au drawHeader via un champ `mutable float webInboxFadeAlpha_`.

---

## 6. UI — badge header

### Position
À droite de la pill `self.name`, entre elle et le bord droit. Quand
visible, décale la pill self.name vers la gauche.

### Dimensions + rendu

```cpp
// main_screen.cpp drawHeader
const int inboxCount = static_cast<int>(st.webInbox.size());
const bool fading = inboxCount == 0 && state.webInboxFadeSec < 3.f;

if (inboxCount > 0 || fading) {
    float alpha = 1.f;
    if (fading) alpha = std::max(0.f, 1.f - state.webInboxFadeSec / 3.f);

    const std::string text = (inboxCount > 0)
        ? ("📥 " + std::to_string(inboxCount))
        : "✓ Tout traité";

    Label lbl;
    lbl.setText(text).setBold(true).setSize(FontSize::overline)
       .setColor(withAlpha(sf::Color::White, alpha));
    const auto m = lbl.measure();
    const float pillW = m.x + 2 * Spacing::md;
    const float pillH = 28.f;
    // Position : à gauche de la pill selfName (ex +pillW + gap)
    // ...
    RoundedRect pill(x, y, pillW, pillH, pillH / 2.f);
    sf::Color bg = inboxCount > 0 ? Colors::accent : Colors::success;
    bg.a = static_cast<sf::Uint8>(alpha * 255);
    pill.setFillColor(bg).draw(target);
    lbl.setPosition(x + Spacing::md, y + 7.f).draw(target);
}
```

### Clic
```cpp
// handleEvent
if (clic dans inboxBadgeRect() && inboxCount > 0) {
    controller_.toggleWebInboxModal();
}
```

---

## 7. UI — modale IncomingOfferScreen étendue

### Mode détection

L'écran `IncomingOfferScreen` fonctionne actuellement en mode unique
(affiche 1 offre native PIN ou 1 offre web). Extension : ajouter un
3e mode `InboxList` qui affiche `state.webInbox` complet.

```cpp
void IncomingOfferScreen::draw(...) const {
    const auto& st = controller_.state();
    if (st.incomingOffer)       drawNativePinOffer(target, *st.incomingOffer);
    else if (st.webInboxModalOpen && !st.webInbox.empty())
                                drawInboxList(target, st.webInbox);
    else                        drawEmpty();
}
```

### drawInboxList mockup

```
┌─────────────────────────────────────────────────────────────┐
│ DEMANDES ENTRANTES · 3   [Refuser tout] [Accepter tout]     │
├─────────────────────────────────────────────────────────────┤
│ ↓ iPhone (Safari) · photo.jpg · 2.4 Mo                      │
│                            [Refuser] [Accepter]             │
├─────────────────────────────────────────────────────────────┤
│ ↓ Android (Chrome) · MonDossier/ · 5 fichiers · 42 Mo       │
│                            [Refuser] [Accepter]             │
├─────────────────────────────────────────────────────────────┤
│ ↓ iPhone (Safari) · notes.txt · 12 Ko                       │
│                            [Refuser] [Accepter]             │
├─────────────────────────────────────────────────────────────┤
│                                        [Fermer sans agir]   │
└─────────────────────────────────────────────────────────────┘
```

Largeur modale : 600 px centrée. Scroll vertical si >5 rows.

Hit tests par row :
- `rowRect(i)` : pour la row
- `acceptRowBtn(i)` / `refuseRowBtn(i)` : boutons dans la row
- Header : `acceptAllBtn()` / `refuseAllBtn()`
- Footer : `closeBtn()`

---

## 8. Web — bouton bundle + logique hybride

```js
// assets/web/js/download.js
function renderOffer(offer) {
    const totalBytes = offer.files.reduce((s, f) => s + (f.size || 0), 0);
    const multi = offer.files.length >= 2;

    if (multi) {
        const btn = document.createElement('button');
        btn.className = 'dl-bundle-btn';
        btn.textContent = `Télécharger tout (${offer.files.length} · ${formatBytes(totalBytes)})`;
        btn.onclick = () => downloadBundle(offer, totalBytes);
        listEl.appendChild(btn);
    }
    // Rows individuelles comme avant
    // ...
}

async function downloadBundle(offer, totalBytes) {
    const LIMIT = 4 * 1024 * 1024 * 1024; // 4 GiB
    if (totalBytes < LIMIT) {
        // Approche A : bundle ZIP serveur
        const res = await fetch('/api/download/bundle',
            { credentials: 'same-origin' });
        if (res.status === 401) { goToLogin(); return; }
        const blob = await res.blob();
        triggerDownload(blob, 'LocalTransfer-' + Date.now() + '.zip');
    } else {
        // Approche B : fallback séquentiel
        for (const f of offer.files) {
            await downloadWithProgress(f.ticketId, f.name);
        }
    }
}
```

CSS `.dl-bundle-btn` : style primary accent, pleine largeur, 40 px.

---

## 9. Threading & edge cases

| Cas | Traitement |
|-----|------------|
| Announce arrive pendant que la modale est ouverte | Push dans inbox, count augmente, row ajoutée dynamiquement (la modale lit `state.webInbox` à chaque frame) |
| Accept sur une row + accept all en race | Impossible user-side (un seul thread UI). Backend `resolveAccept` idempotent si id manquant (silently returns false) |
| Cancel folder picker sur accept all | `chosen.empty()` → return early, l'inbox reste pleine (aucune row retirée) |
| Timeout announce (300 s) | Backend émet event `TransferFailedEvent{uploadId, "timeout"}` → AppController retire la row de l'inbox |
| Badge disparaît pendant qu'un announce arrive | Fade remis à 0 → badge reprend opacité 1 |

---

## 10. Tests

### Unitaires
- `test_download_ticket` : +cas `listBySession` (3 tickets, 1 autre session → doit retourner les 3)
- `test_download_ticket` : nouvelle fonction déjà, pas de nouvel file

### Smoke manuels
- Host envoie 5 fichiers → web : bouton bundle visible → clic → zip
- Host envoie 1 fichier → web : pas de bouton bundle (un seul fichier)
- Visiteur envoie 3 announces successifs → badge `3` apparaît → clic
  → modale liste 3 rows → Accept tout → 1 folder picker → 3 arrivent
- Visiteur envoie 1 announce → Accept row individuelle → 1 folder
  picker → arrive → badge passe à 0 puis fade-out
- Test timeout : démarrer un announce, attendre 5 min sans cliquer,
  vérifier que le visiteur voit « timeout »

---

## 11. CONTRAT D'IMPLÉMENTATION

### Fichiers à modifier
- [ ] `include/ltr/web/download_ticket_store.hpp` + `.cpp` : listBySession
- [ ] `src/web/routes/download_routes.cpp` : route /api/download/bundle
- [ ] `include/ltr/infra/config.hpp` + `.cpp` : webAnnounceTimeoutSec
- [ ] `include/ltr/web/web_service.hpp` + `.cpp` : prop timeoutSec
- [ ] `src/web/routes/upload_routes.cpp` : lire cfg timeout au lieu de 120
- [ ] `include/ltr/app/app_state.hpp` : WebInboxEntry + webInbox +
      webInboxFadeSec + webInboxModalOpen, suppression pendingWebOffer
- [ ] `include/ltr/app/app_controller.hpp` : accept*/refuse* methods +
      toggleWebInboxModal
- [ ] `src/app/app_controller.cpp` : impl + onEvent refactor
- [ ] `src/ui/ui_app.cpp` : remplacement cascade pendingWebOffer →
      webInboxModalOpen
- [ ] `include/ltr/ui/screens/main_screen.hpp` + `.cpp` : badge header
- [ ] `include/ltr/ui/screens/incoming_offer_screen.hpp` + `.cpp` : mode
      inbox list
- [ ] `assets/web/js/download.js` : bouton bundle + logique hybride
- [ ] `assets/web/css/style.css` : styles bundle
- [ ] `tests/test_download_ticket.cpp` : +listBySession
- [ ] `docs-agents/WEB.md` : bundle + inbox

### Fichiers inchangés
- Couche `ltr::network` (TCP LTR1 + resume) : aucun impact
- Web upload routes (autre que timeout) : inchangés
- UX-1..4 : inchangés

---

UI_REQUIRED: true (mockups dans ce document § 6 et § 7, pas besoin d'agent UI/UX séparé)
