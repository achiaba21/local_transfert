#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <variant>
#include <vector>

#include "ltr/domain/device.hpp"
#include "ltr/domain/transfer_request.hpp"

namespace ltr::core {

struct PeerSeenEvent      { domain::Device device; };
struct PeerLostEvent      { std::string deviceId; };
struct IncomingOfferEvent { domain::TransferRequest request; };
struct OfferAnsweredEvent { std::string sessionId; bool accepted;
                            std::string reason; };
struct TransferStartedEvent { std::string sessionId; };
struct TransferProgressEvent {
    std::string sessionId;
    std::uint64_t bytes;
    double speedBps;
    std::chrono::seconds eta;
};
struct TransferDoneEvent   { std::string sessionId; };

// V1.1.9 — Sprint Transfer Resume : classification des erreurs pour
// décider du comportement UI (resumable, auto-retry, etc.).
enum class ErrorCategory : std::uint8_t {
    Unknown   = 0,   // safe default — traité comme resumable
    Network   = 1,   // TCP disconnect, timeout, heartbeat miss → resumable + auto-retry
    Protocol  = 2,   // frame invalide, crc mismatch → resumable + retry 1×
    Permanent = 3,   // disk full, perm denied → pas de resume
    Cancelled = 4,   // user cancel → pas de resume
};

struct TransferFailedEvent {
    std::string    sessionId;
    std::string    reason;
    ErrorCategory  category{ErrorCategory::Unknown};
};

struct LogEvent            { std::string level; std::string message; };

// V1.1.2 : upload venant d'un visiteur web (auto-accepté car PIN déjà validé
// à l'auth). Crée directement une UiTransfer côté desktop, SANS modale.
struct WebUploadStartedEvent {
    std::string   sessionId;
    std::string   senderName;    // "Android (Chrome) · ce98"
    std::string   fileName;      // "Devis.pdf"
    std::uint64_t totalBytes{0};
};

// V1.1.2 : le visiteur annonce un upload → le host doit accepter/refuser
// via une modale (sans PIN car déjà authentifié). Après Accept, l'host
// choisit un dossier de destination via un dialog natif. L'uploadId est
// renseigné pour permettre au controller de résoudre la décision.
struct WebIncomingOfferEvent {
    std::string   uploadId;
    std::string   senderName;
    std::uint64_t totalBytes{0};
    int           filesCount{0};
    std::string   firstFileName; // pour affichage compact
};

// V1.1.9-batch : émis par le backend quand un announce timeout côté
// HTTP (waitForDecision expire) sans qu'aucune décision n'ait été prise.
// Permet au AppController de retirer l'entrée correspondante du webInbox
// (sinon elle reste fantôme côté UI alors que le visiteur a déjà reçu
// "✗ pas de réponse").
struct WebOfferTimedOutEvent {
    std::string uploadId;
};

using Event = std::variant<
    PeerSeenEvent, PeerLostEvent,
    IncomingOfferEvent, OfferAnsweredEvent,
    TransferStartedEvent, TransferProgressEvent,
    TransferDoneEvent, TransferFailedEvent,
    LogEvent,
    WebUploadStartedEvent,
    WebIncomingOfferEvent,
    WebOfferTimedOutEvent>;

// File d'événements thread-safe consommée par le thread UI à chaque frame.
class EventBus {
public:
    void post(Event e);
    std::vector<Event> drain();

private:
    std::mutex mu_;
    std::queue<Event> q_;
};

} // namespace ltr::core
