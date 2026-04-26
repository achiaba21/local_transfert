#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <map>
#include <vector>

namespace ltr::web {

// V1.1.2 : upload en deux temps (announce → upload).
// Le navigateur annonce d'abord la liste des fichiers à envoyer. Le host
// décide (Accepter + dossier de destination, ou Refuser) via une modale
// desktop. Le JS attend la réponse, puis uploade chaque fichier avec
// l'uploadId correspondant.

struct AnnounceFile {
    std::string   name;         // nom (basename)
    std::uint64_t size{0};
    // V1.1.7 : chemin relatif (depuis la racine du dossier uploadé).
    // Vide pour un fichier isolé. Pour un dossier, ex: "photos/vac/01.jpg".
    // Le serveur reconstruit l'arborescence via create_directories().
    std::string   relativePath;
};

enum class AnnounceDecision { Pending, Accepted, Refused, TimedOut };

// Snapshot copiable — sans mutex/cv. C'est ce que l'extérieur manipule.
struct AnnounceSnapshot {
    std::string                uploadId;
    std::string                sessionToken;
    std::string                senderName;
    std::vector<AnnounceFile>  files;
    std::uint64_t              totalBytes{0};
    AnnounceDecision           decision{AnnounceDecision::Pending};
    std::filesystem::path      targetDir;
};

// Entrée interne du store — non-copyable (mutex + cv). Toujours gardée
// derrière un shared_ptr dans la map pour permettre l'attente depuis un
// thread et la résolution depuis un autre.
struct WebUploadAnnounce {
    std::string                uploadId;
    std::string                sessionToken;
    std::string                senderName;
    std::vector<AnnounceFile>  files;
    std::uint64_t              totalBytes{0};
    AnnounceDecision           decision{AnnounceDecision::Pending};
    std::filesystem::path      targetDir;

    std::mutex                 mu;
    std::condition_variable    cv;

    AnnounceSnapshot snapshot() const {
        AnnounceSnapshot s;
        s.uploadId     = uploadId;
        s.sessionToken = sessionToken;
        s.senderName   = senderName;
        s.files        = files;
        s.totalBytes   = totalBytes;
        s.decision     = decision;
        s.targetDir    = targetDir;
        return s;
    }
};

class WebUploadAnnounceStore {
public:
    WebUploadAnnounceStore() = default;

    WebUploadAnnounceStore(const WebUploadAnnounceStore&)            = delete;
    WebUploadAnnounceStore& operator=(const WebUploadAnnounceStore&) = delete;

    std::string create(const std::string& sessionToken,
                       const std::string& senderName,
                       std::vector<AnnounceFile> files);

    // Bloque jusqu'à décision OU timeout. Retourne un snapshot.
    AnnounceSnapshot waitForDecision(const std::string& uploadId,
                                     std::chrono::milliseconds timeout);

    bool resolveAccept(const std::string& uploadId,
                       const std::filesystem::path& targetDir);
    bool resolveRefuse(const std::string& uploadId);

    // Consomme non-destructif (upload peut avoir plusieurs fichiers).
    std::optional<AnnounceSnapshot> peek(const std::string& uploadId) const;

    void remove(const std::string& uploadId);

private:
    mutable std::mutex mu_;
    std::map<std::string, std::shared_ptr<WebUploadAnnounce>> map_;
};

} // namespace ltr::web
