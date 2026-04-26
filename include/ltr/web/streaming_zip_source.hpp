#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "ltr/web/download_ticket.hpp"

namespace httplib { class DataSink; }

namespace ltr::web {

// Générateur zip incrémental (mode STORE, avec data descriptor).
//
// Consommé par download_routes pour streamer un zip directement dans la
// réponse HTTP sans jamais écrire de fichier temp. Une instance par GET —
// le ticket est rejouable mais chaque GET a sa propre state machine.
//
// Format produit : ZIP 2.0 STORE avec bit 3 du general purpose bit flag
// (sizes + CRC32 écrits après les data dans un data descriptor). Cela
// permet de streamer sans pré-calculer le CRC32 (donc sans double lecture
// disque).
//
// Thread-ownership : une instance = un worker cpp-httplib. Aucun accès
// concurrent.
class StreamingZipSource {
public:
    explicit StreamingZipSource(std::vector<ZipEntry> entries);

    // Rempli `sink` d'au plus `maxBytes` octets.
    // Return : true  = encore à streamer, false = terminé (ou erreur).
    // Si erreur (fichier source disparu/illisible), errored_ est set et
    // le caller coupe la connexion HTTP.
    bool provide(httplib::DataSink& sink, std::size_t maxBytes);

    std::uint64_t bytesWritten() const { return written_; }
    bool errored() const { return errored_; }
    const std::string& errorMsg() const { return errorMsg_; }

    // Taille exacte du zip produit, utilisable comme Content-Length.
    // Formule STORE + data descriptor (bit 3) :
    //   Σ(30 + nameLen + fileSize + 16 + 46 + nameLen) + 22
    //   = Σ(92 + 2·nameLen + fileSize) + 22
    static std::uint64_t computeZipSize(const std::vector<ZipEntry>& entries);

private:
    enum class Phase {
        StartEntry,       // ouvrir ifstream + écrire local header
        StreamData,       // pousser les bytes du fichier
        WriteDataDesc,    // écrire le data descriptor (CRC + sizes)
        WriteCentralDir,  // écrire le central directory (tout à la fin)
        WriteEocd,        // End Of Central Directory
        Done
    };

    // Helpers de sérialisation binaire (little-endian).
    static void writeLE16(std::string& buf, std::uint16_t v);
    static void writeLE32(std::string& buf, std::uint32_t v);

    // Construit le local file header pour `entries_[curEntry_]` dans buf_.
    void buildLocalHeader();
    // Construit le data descriptor pour l'entrée courante dans buf_.
    void buildDataDescriptor();
    // Construit le central directory complet dans buf_ (appelé une fois
    // quand toutes les entries sont streamées).
    void buildCentralDirectory();
    // Construit l'EOCD dans buf_.
    void buildEocd(std::uint64_t cdOffset, std::uint64_t cdSize);

    // Vide `buf_` dans `sink`, au plus maxBytes octets. Avance bufCursor_.
    // Return : octets réellement écrits (0 = plus rien dans le buffer).
    std::size_t drainBuffer(httplib::DataSink& sink, std::size_t maxBytes);

    std::vector<ZipEntry> entries_;
    std::size_t curEntry_{0};
    Phase       phase_{Phase::StartEntry};

    // Lecture du fichier courant
    std::ifstream curFile_;
    std::uint32_t curCrc_{0};
    std::uint64_t curRead_{0};

    // Offsets pour le central directory (1 par entrée)
    std::vector<std::uint64_t> localOffsets_;
    std::vector<std::uint32_t> crcs_;

    // Buffer tampon (header / data desc / CD / EOCD)
    std::string buf_;
    std::size_t bufCursor_{0};

    // État global
    std::uint64_t written_{0};  // total octets poussés au sink
    std::uint64_t cdOffset_{0}; // offset où commence le central dir

    bool        errored_{false};
    std::string errorMsg_;
};

} // namespace ltr::web
