#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ltr::web {

// V1.1.8 : un ticket a maintenant 2 modes.
//   File          = stream direct d'un fichier unique sur disque
//   StreamingZip  = assemble un zip à la volée depuis une liste de fichiers,
//                   sans jamais écrire de zip sur disque
enum class TicketKind { File, StreamingZip };

// Entrée d'un zip streamé : fichier source absolu + chemin relatif dans
// l'archive + taille (snapshot au moment de l'issue du ticket).
struct ZipEntry {
    std::filesystem::path abs;
    std::string relInZip;
    std::uint64_t size{0};
};

// Ticket de download. Rejouable pendant TTL (plus de consume destructif
// en V1.1.8 — le visiteur peut retélécharger si rupture réseau ou mauvaise
// manip).
struct DownloadTicket {
    std::string   id;              // 32 hex chars — URL-safe
    std::string   sessionToken;    // session destinataire
    std::string   sessionId;       // transferSession id (pour events)
    TicketKind    kind{TicketKind::File};

    // kind == File
    std::filesystem::path path;

    // kind == StreamingZip
    std::vector<ZipEntry> zipEntries;

    std::string   displayName;     // nom exposé au client
    std::uint64_t size{0};         // taille totale du body HTTP (zip size)
    std::chrono::steady_clock::time_point expiresAt{};
};

} // namespace ltr::web
