// Test du générateur ZIP streamé (V1.1.8).
//
// 1) Crée 2 fichiers temp (texte 1 Ko + binaire random 2 Ko)
// 2) Vérifie que computeZipSize() renvoie la taille effective du stream
// 3) Consomme tout le zip via StreamingZipSource::provide dans un buffer
// 4) Parse le buffer avec miniz pour vérifier qu'on extrait bien les 2
//    fichiers avec contenu et CRC32 identiques à l'original.

#include "ltr/web/download_ticket.hpp"
#include "ltr/web/streaming_zip_source.hpp"

#include <httplib.h>
#include <miniz.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void writeFile(const fs::path& p, const std::string& content) {
    std::ofstream os(p, std::ios::binary);
    os.write(content.data(),
             static_cast<std::streamsize>(content.size()));
}

std::string readFile(const fs::path& p) {
    std::ifstream is(p, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(is)),
                   std::istreambuf_iterator<char>());
    return s;
}

} // namespace

int main() {
    const auto tmp = fs::temp_directory_path() / "ltr_test_stream_zip";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    // 1) Deux fichiers
    std::string txt(1024, 'A');
    for (std::size_t i = 0; i < txt.size(); ++i) {
        txt[i] = static_cast<char>('A' + (i % 26));
    }
    std::string bin(2048, 0);
    for (std::size_t i = 0; i < bin.size(); ++i) {
        bin[i] = static_cast<char>((i * 7 + 13) & 0xFF);
    }
    writeFile(tmp / "a.txt", txt);
    writeFile(tmp / "b.bin", bin);

    std::vector<ltr::web::ZipEntry> entries;
    {
        ltr::web::ZipEntry e;
        e.abs      = tmp / "a.txt";
        e.relInZip = "bundle/a.txt";
        e.size     = txt.size();
        entries.push_back(e);
    }
    {
        ltr::web::ZipEntry e;
        e.abs      = tmp / "b.bin";
        e.relInZip = "bundle/b.bin";
        e.size     = bin.size();
        entries.push_back(e);
    }

    // 2) Taille précalculée
    const auto expectedSize =
        ltr::web::StreamingZipSource::computeZipSize(entries);
    // Vérif manuelle : 22 + Σ(92 + 2·nameLen + fileSize)
    const std::uint64_t manual =
        22 + (92 + 2 * 12 + 1024) + (92 + 2 * 12 + 2048);
    assert(expectedSize == manual);

    // 3) Consommation via un DataSink factice
    std::string buffer;
    httplib::DataSink sink;
    sink.write = [&buffer](const char* data, std::size_t len) -> bool {
        buffer.append(data, len);
        return true;
    };
    sink.is_writable = []() -> bool { return true; };
    sink.done = []() {};

    ltr::web::StreamingZipSource src(entries);
    while (src.provide(sink, 64 * 1024)) {
        // loop until done
    }
    assert(!src.errored());
    assert(buffer.size() == expectedSize);

    // 4) Parser le buffer avec miniz
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    const auto ok = mz_zip_reader_init_mem(
        &zip, buffer.data(), buffer.size(), 0);
    assert(ok);
    const auto nEntries = mz_zip_reader_get_num_files(&zip);
    assert(nEntries == 2);

    // Entry 0 : bundle/a.txt
    {
        mz_zip_archive_file_stat st;
        assert(mz_zip_reader_file_stat(&zip, 0, &st));
        assert(std::string(st.m_filename) == "bundle/a.txt");
        assert(st.m_uncomp_size == txt.size());
        std::vector<char> out(txt.size());
        assert(mz_zip_reader_extract_to_mem(
            &zip, 0, out.data(), out.size(), 0));
        assert(std::memcmp(out.data(), txt.data(), txt.size()) == 0);
    }
    // Entry 1 : bundle/b.bin
    {
        mz_zip_archive_file_stat st;
        assert(mz_zip_reader_file_stat(&zip, 1, &st));
        assert(std::string(st.m_filename) == "bundle/b.bin");
        assert(st.m_uncomp_size == bin.size());
        std::vector<char> out(bin.size());
        assert(mz_zip_reader_extract_to_mem(
            &zip, 1, out.data(), out.size(), 0));
        assert(std::memcmp(out.data(), bin.data(), bin.size()) == 0);
    }
    mz_zip_reader_end(&zip);

    // Nettoyage
    fs::remove_all(tmp);

    std::cout << "test_streaming_zip OK (" << buffer.size() << " o)\n";
    return 0;
}
