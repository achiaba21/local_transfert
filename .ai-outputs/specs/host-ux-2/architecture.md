# Architecture — Sprint UX-2 Zone Transferts robuste

**Date :** 2026-04-24
**NEW_PROJECT :** false
**UI_REQUIRED :** true (mockups scroll + annuler + ouvrir dossier + progress hiérarchisée)

---

## 1. Vue d'ensemble

Le changement s'articule en 4 axes :

1. **UI** — drawTransferBar complètement revu : scroll horizontal + flèches,
   progress plus épaisse, hiérarchie %, bouton annuler InProgress, bouton
   « Ouvrir le dossier » pour Incoming+Done.
2. **State** — `UiTransfer` gagne un champ `terminalAt` + direction déjà enum.
3. **Backend cancellation** — natif TCP déjà prêt (`cancelSession`), web
   streaming-zip gagne un flag atomique shared par session côté `WebService`.
4. **Auto-clean** — boucle `AppController::tick(dt)` qui purge les cards
   expirées.

### Files à toucher

| Fichier | Action |
|---------|--------|
| `include/ltr/app/app_state.hpp` | Ajout `UiTransfer::terminalAt` |
| `include/ltr/app/app_controller.hpp` | Ajout `tick(sf::Time)`, `openDownloadDir()` |
| `src/app/app_controller.cpp` | Tick + open dir + pose terminalAt sur Done/Failed/Cancelled |
| `src/ui/ui_app.cpp` | Appel `controller_.tick(dt)` dans la boucle |
| `include/ltr/ui/screens/main_screen.hpp` | Scroll state + rects boutons |
| `src/ui/screens/main_screen.cpp` | Refonte `drawTransferBar` |
| `include/ltr/web/web_service.hpp` | Ajout `cancelSession(sid)` + cancel flags map |
| `src/web/web_service.cpp` | Impl + intégration avec ticket |
| `include/ltr/web/streaming_zip_source.hpp` | Champ atomique externe |
| `src/web/streaming_zip_source.cpp` | Vérification flag à chaque chunk |
| `src/web/routes/download_routes.cpp` | Réception du flag, vérif à chaque chunk |
| `include/ltr/core/shell_open.hpp` (NEW) | Helper cross-platform `openInFileManager` |
| `src/core/shell_open.cpp` (NEW) | Impl macOS/Windows/Linux |
| `CMakeLists.txt` | Ajout shell_open.cpp |

Aucun nouveau widget autonome — tout reste inline dans `drawTransferBar`.

---

## 2. Structures modifiées

### UiTransfer (app_state.hpp)

```cpp
struct UiTransfer {
    std::string sessionId;
    std::string peerName;
    TransferDirection direction{TransferDirection::Outgoing};
    std::uint64_t totalBytes{0};
    std::uint64_t bytesTransferred{0};
    double speedBps{0.0};
    std::chrono::seconds eta{0};
    domain::TransferStatus status{domain::TransferStatus::Pending};
    std::string error;

    // V1.1.8-UX2 : posé par AppController quand le status passe à un état
    // terminal (Done/Failed/Cancelled). Utilisé par tick() pour auto-clean.
    std::chrono::steady_clock::time_point terminalAt{};
};
```

### AppController (app_controller.hpp)

```cpp
class AppController {
public:
    // ... existant ...

    // V1.1.8-UX2 : drain timer-based (auto-clean + éventuels TTL futurs).
    // Appelé depuis UiApp::run chaque frame.
    void tick(sf::Time dt);

    // V1.1.8-UX2 : ouvre le dossier de téléchargement dans le file manager
    // OS (Finder/Explorer/xdg-open). Appelé depuis le bouton post-transfert.
    void openDownloadDir();

    // V1.1.8-UX2 : cancelSession existe déjà — on l'étend pour router vers
    // WebService si la session est Web.
    void cancelSession(const std::string& sessionId);
};
```

### Auto-clean logic

```cpp
void AppController::tick(sf::Time /*dt*/) {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    constexpr auto kDoneTtl     = seconds(10);
    constexpr auto kFailedTtl   = seconds(30);

    auto it = state_.transfers.begin();
    while (it != state_.transfers.end()) {
        const auto& t = *it;
        if (t.terminalAt.time_since_epoch().count() == 0) { ++it; continue; }
        const auto age = now - t.terminalAt;
        const bool expire =
            (t.status == domain::TransferStatus::Done      && age > kDoneTtl) ||
            (t.status == domain::TransferStatus::Failed    && age > kFailedTtl) ||
            (t.status == domain::TransferStatus::Cancelled && age > kFailedTtl);
        if (expire) it = state_.transfers.erase(it);
        else        ++it;
    }
}
```

### Quand poser terminalAt

Dans `onEvent` :
- `TransferDoneEvent` → trouve UiTransfer, set status=Done + `terminalAt = now`
- `TransferFailedEvent` → idem avec Failed
- Sur `cancelSession` → set status=Cancelled + `terminalAt = now`

---

## 3. Cancellation web streaming

### WebService (web_service.hpp)

```cpp
class WebService {
    // ...
public:
    // V1.1.8-UX2 : signale à un download en cours de s'arrêter.
    // Thread-safe. Le flag est capturé par les lambdas providers des
    // download_routes ; le prochain chunk vérifie et return false.
    void cancelSession(const std::string& sessionId);

    // Accès interne pour les routes (createCancelFlag au démarrage du GET).
    std::shared_ptr<std::atomic<bool>> acquireCancelFlag(const std::string& sessionId);

private:
    std::mutex cancelMu_;
    std::unordered_map<std::string,
        std::shared_ptr<std::atomic<bool>>> cancelFlags_;
};
```

### download_routes.cpp

Chaque provider (streamFile et streamZip) :
- Au début du GET, appelle `acquireCancelFlag(sessionId)` → shared_ptr<atomic<bool>>
- Capture ce shared_ptr dans la lambda
- Vérifie `if (cancelFlag->load()) { return false; }` à chaque chunk avant `sink.write`
- Si `errored_ == false` mais flag vu → émettre `TransferFailedEvent{cancelled}`

### StreamingZipSource

Pas de changement intrinsèque : le cancel est porté par le lambda de la
route qui embrasse la source, pas par la source elle-même. La source
continue d'exposer `errored_`/`cancel()` interne, mais le chemin de
cancel principal passe par la route (plus simple, centralisé).

### AppController::cancelSession

```cpp
void AppController::cancelSession(const std::string& sessionId) {
    // 1) Natif TCP
    if (client_) client_->cancel(sessionId);
    if (server_) server_->cancelSession(sessionId);
    // 2) Web
    if (web_)    web_->cancelSession(sessionId);
    // 3) Marqueur local (l'event TransferFailed viendra aussi mais mise à
    //    jour immédiate pour feedback UI).
    for (auto& t : state_.transfers) {
        if (t.sessionId == sessionId &&
            (t.status == domain::TransferStatus::InProgress ||
             t.status == domain::TransferStatus::Accepted)) {
            t.status = domain::TransferStatus::Cancelled;
            t.terminalAt = std::chrono::steady_clock::now();
        }
    }
}
```

---

## 4. Helper `shellOpen`

```cpp
// include/ltr/core/shell_open.hpp
#pragma once
#include <filesystem>
namespace ltr::core {
// Ouvre `path` avec l'application système par défaut (Finder/Explorer/xdg-open).
// Return true si la commande a été lancée (pas forcément succès de l'ouverture).
bool openInFileManager(const std::filesystem::path& path);
} // namespace ltr::core
```

```cpp
// src/core/shell_open.cpp
#include "ltr/core/shell_open.hpp"
#include <cstdlib>
#include <string>

namespace ltr::core {
namespace {
// Shell-escape stricte POSIX : entoure de ' et double tout '.
std::string shellEscape(const std::string& s) {
#if defined(_WIN32)
    // Sur Windows on utilise ShellExecuteW, pas de shell escape nécessaire
    return s;
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
#endif
}
} // namespace

bool openInFileManager(const std::filesystem::path& path) {
    const auto s = path.string();
#if defined(__APPLE__)
    const auto cmd = "open " + shellEscape(s);
    return std::system(cmd.c_str()) == 0;
#elif defined(_WIN32)
    // ShellExecuteW direct — pas de shell intermédiaire
    std::wstring w(s.begin(), s.end());
    const auto res = ShellExecuteW(nullptr, L"open", w.c_str(),
                                    nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(res) > 32;
#else
    const auto cmd = "xdg-open " + shellEscape(s) + " >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
#endif
}
} // namespace ltr::core
```

---

## 5. UI : drawTransferBar refonte

### Dimensions revues

```
Zone bottomRect_ : largeur = 100% viewSize, hauteur = 104 px (inchangée)

Inside :
  Header ligne (34 px) : "TRANSFERTS · N"   + flèches L/R si débordement
  Row de cards   (64 px) : cards 340 × 64 scrollables horizontalement
  Padding bas    (6 px)
```

### Structure d'une card (340 × 64)

```
┌────────────────────────────────────────────────────────┐
│ ↑  MacBook Pro                          [Annuler]      │  ← ligne 1 (32 px)
│    85 %  ·  15 Mo/s · 12s                              │  ← ligne 2 (22 px)
│ ══════════════════════════════════════════░░░          │  ← progress 6 px
└────────────────────────────────────────────────────────┘
```

Pour Done+Incoming :
```
┌────────────────────────────────────────────────────────┐
│ ↓  iPhone                           [Ouvrir le dossier]│
│    ✓ Terminé                                           │
│ ══════════════════════════════════════════════════     │
└────────────────────────────────────────────────────────┘
```

### Scroll horizontal

- `float transfersScrollX_` maintenu par MainScreen.
- Au draw : `tx = bottomRect_.left + Spacing::xl - transfersScrollX_`.
- Skip les cards hors-zone visible.
- Total width calculé = N × (cardW + gap). Si > zoneVisibleW → débordement.
- Flèches ← et → à droite du titre "TRANSFERTS · N" (clic = scroll par
  une carte).
- Molette horizontale ET `shift+molette verticale` dans la zone → ajuster
  `transfersScrollX_`.
- Fade gradient sur les bords gauche/droit quand débordement (rectangle
  surface avec alpha dégradé) — optionnel, simple fade-out des 12 px.

### Boutons dans la card

- Annuler : 90 × 22, accentLight bg, accent text. Visible si status ∈
  {Accepted, InProgress}. Position : top-right.
- « Ouvrir le dossier » : 140 × 22, accent bg, white text. Visible si
  direction==Incoming && status==Done. Position : top-right.

### Hiérarchie status

Ligne 2 : `"85 % · 15 Mo/s · 12s"` → garder une seule ligne dense mais
augmenter la taille du `%` à `FontSize::h2` bold, garder le reste en
`FontSize::small`. Implémenté via 2 Labels côte à côte.

---

## 6. CONTRAT D'IMPLÉMENTATION

### Fichiers à créer
- [ ] `include/ltr/core/shell_open.hpp`
- [ ] `src/core/shell_open.cpp`

### Fichiers à modifier
- [ ] `include/ltr/app/app_state.hpp` — ajout `terminalAt`
- [ ] `include/ltr/app/app_controller.hpp` — `tick`, `openDownloadDir`
- [ ] `src/app/app_controller.cpp` — tick + openDownloadDir + poser
      terminalAt sur Done/Failed/Cancelled + étendre cancelSession(web_)
- [ ] `src/ui/ui_app.cpp` — appel `controller_.tick(dt)` dans loop
- [ ] `include/ltr/ui/screens/main_screen.hpp` — scroll state + rects boutons
- [ ] `src/ui/screens/main_screen.cpp` — refonte drawTransferBar +
      handleEvent (boutons + molette + flèches)
- [ ] `include/ltr/web/web_service.hpp` — `cancelSession`, `acquireCancelFlag`
- [ ] `src/web/web_service.cpp` — impl
- [ ] `src/web/routes/download_routes.cpp` — acquireCancelFlag + vérif
      à chaque chunk
- [ ] `CMakeLists.txt` — ajout `src/core/shell_open.cpp`

### Tests
- Aucun nouveau test unitaire (visuel + comportement système). Smoke
  tests manuels listés dans business-spec.md §5.
- 8 tests existants doivent continuer de passer.

---

UI_REQUIRED: true (les 2 layouts card InProgress / Done + la zone avec
flèches scroll sont à valider en mockup avant implémentation).
