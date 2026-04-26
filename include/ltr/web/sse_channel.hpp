#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

namespace ltr::web {

// Canal SSE unique (1 navigateur abonné). File de messages bloquante :
// push() depuis n'importe quel thread, pop(wait) depuis le handler HTTP
// long-lived. Close() réveille le handler pour débrancher proprement.
class SseChannel {
public:
    SseChannel() = default;

    SseChannel(const SseChannel&)            = delete;
    SseChannel& operator=(const SseChannel&) = delete;

    // Pousse un message SSE (déjà formaté "event:.../data:.../\n\n" ou juste
    // data:...). Thread-safe.
    void push(std::string msg);

    // Attend un message jusqu'à timeout ou close(). true si un message a été
    // récupéré dans `out`, false sinon (timeout ou closed).
    bool waitAndPop(std::string& out, std::chrono::milliseconds timeout);

    // Débloque tous les waiters et passe en état "fermé" (push ignorés).
    void close();

    bool isClosed() const;

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    bool closed_{false};
};

} // namespace ltr::web
