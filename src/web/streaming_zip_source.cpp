#include "ltr/web/streaming_zip_source.hpp"

#include <algorithm>
#include <cstring>

#include <httplib.h>
#include <miniz.h>

#include "ltr/core/logger.hpp"

namespace ltr::web {

namespace {

// Signatures ZIP (little-endian) — RFC/APPNOTE 6.3.x.
constexpr std::uint32_t kSigLocal     = 0x04034b50;
constexpr std::uint32_t kSigDataDesc  = 0x08074b50;
constexpr std::uint32_t kSigCentral   = 0x02014b50;
constexpr std::uint32_t kSigEocd      = 0x06054b50;

// Version needed to extract : 2.0 (STORE + data descriptor).
constexpr std::uint16_t kVersionNeeded = 20;

// General purpose bit flag : bit 3 (use data descriptor) + bit 11 (UTF-8
// filenames). 0x0008 | 0x0800 = 0x0808.
constexpr std::uint16_t kGpFlag = 0x0808;

// Method 0 = STORE.
constexpr std::uint16_t kMethodStore = 0;

// Taille du buffer de lecture par appel `provide` (512 Ko). On sature
// rapidement le chunk size de cpp-httplib (~64 Ko).
constexpr std::size_t kReadBufSize = 512 * 1024;

} // namespace

StreamingZipSource::StreamingZipSource(std::vector<ZipEntry> entries)
    : entries_(std::move(entries)) {
    localOffsets_.reserve(entries_.size());
    crcs_.reserve(entries_.size());
    if (entries_.empty()) {
        phase_ = Phase::WriteCentralDir;
    }
}

void StreamingZipSource::writeLE16(std::string& buf, std::uint16_t v) {
    buf.push_back(static_cast<char>(v & 0xFF));
    buf.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void StreamingZipSource::writeLE32(std::string& buf, std::uint32_t v) {
    buf.push_back(static_cast<char>(v & 0xFF));
    buf.push_back(static_cast<char>((v >> 8) & 0xFF));
    buf.push_back(static_cast<char>((v >> 16) & 0xFF));
    buf.push_back(static_cast<char>((v >> 24) & 0xFF));
}

std::uint64_t StreamingZipSource::computeZipSize(
    const std::vector<ZipEntry>& entries) {
    // Σ(local header + data + data desc + central dir) + EOCD
    // = Σ(30 + nameLen + size + 16 + 46 + nameLen) + 22
    // = Σ(92 + 2·nameLen + size) + 22
    std::uint64_t total = 22;
    for (const auto& e : entries) {
        total += 92 + 2 * static_cast<std::uint64_t>(e.relInZip.size())
               + e.size;
    }
    return total;
}

void StreamingZipSource::buildLocalHeader() {
    const auto& e = entries_[curEntry_];
    buf_.clear();
    buf_.reserve(30 + e.relInZip.size());
    writeLE32(buf_, kSigLocal);
    writeLE16(buf_, kVersionNeeded);
    writeLE16(buf_, kGpFlag);
    writeLE16(buf_, kMethodStore);
    writeLE16(buf_, 0); // last mod time
    writeLE16(buf_, 0); // last mod date
    writeLE32(buf_, 0); // CRC32 (placeholder — data descriptor)
    writeLE32(buf_, 0); // compressed size (placeholder)
    writeLE32(buf_, 0); // uncompressed size (placeholder)
    writeLE16(buf_, static_cast<std::uint16_t>(e.relInZip.size()));
    writeLE16(buf_, 0); // extra field length
    buf_.append(e.relInZip);
    bufCursor_ = 0;
}

void StreamingZipSource::buildDataDescriptor() {
    const auto& e = entries_[curEntry_];
    buf_.clear();
    buf_.reserve(16);
    writeLE32(buf_, kSigDataDesc);
    writeLE32(buf_, curCrc_);
    writeLE32(buf_, static_cast<std::uint32_t>(e.size));  // compressed
    writeLE32(buf_, static_cast<std::uint32_t>(e.size));  // uncompressed
    bufCursor_ = 0;
}

void StreamingZipSource::buildCentralDirectory() {
    buf_.clear();
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        buf_.reserve(buf_.size() + 46 + e.relInZip.size());
        writeLE32(buf_, kSigCentral);
        writeLE16(buf_, kVersionNeeded);           // version made by
        writeLE16(buf_, kVersionNeeded);           // version needed
        writeLE16(buf_, kGpFlag);
        writeLE16(buf_, kMethodStore);
        writeLE16(buf_, 0);                        // last mod time
        writeLE16(buf_, 0);                        // last mod date
        writeLE32(buf_, crcs_[i]);
        writeLE32(buf_, static_cast<std::uint32_t>(e.size)); // comp size
        writeLE32(buf_, static_cast<std::uint32_t>(e.size)); // uncomp size
        writeLE16(buf_, static_cast<std::uint16_t>(e.relInZip.size()));
        writeLE16(buf_, 0);                        // extra field length
        writeLE16(buf_, 0);                        // file comment length
        writeLE16(buf_, 0);                        // disk number start
        writeLE16(buf_, 0);                        // internal file attrs
        writeLE32(buf_, 0);                        // external file attrs
        writeLE32(buf_, static_cast<std::uint32_t>(localOffsets_[i]));
        buf_.append(e.relInZip);
    }
    bufCursor_ = 0;
}

void StreamingZipSource::buildEocd(std::uint64_t cdOffset,
                                    std::uint64_t cdSize) {
    buf_.clear();
    buf_.reserve(22);
    writeLE32(buf_, kSigEocd);
    writeLE16(buf_, 0);                                       // disk number
    writeLE16(buf_, 0);                                       // disk w/ CD
    writeLE16(buf_, static_cast<std::uint16_t>(entries_.size())); // CD entries this disk
    writeLE16(buf_, static_cast<std::uint16_t>(entries_.size())); // CD entries total
    writeLE32(buf_, static_cast<std::uint32_t>(cdSize));
    writeLE32(buf_, static_cast<std::uint32_t>(cdOffset));
    writeLE16(buf_, 0);                                       // comment length
    bufCursor_ = 0;
}

std::size_t StreamingZipSource::drainBuffer(httplib::DataSink& sink,
                                             std::size_t maxBytes) {
    if (bufCursor_ >= buf_.size()) return 0;
    const auto remaining = buf_.size() - bufCursor_;
    const auto toWrite = std::min(remaining, maxBytes);
    if (!sink.write(buf_.data() + bufCursor_, toWrite)) {
        errored_ = true;
        errorMsg_ = "sink_write_failed";
        return 0;
    }
    bufCursor_ += toWrite;
    written_ += toWrite;
    return toWrite;
}

bool StreamingZipSource::provide(httplib::DataSink& sink,
                                  std::size_t maxBytes) {
    while (maxBytes > 0 && !errored_) {
        switch (phase_) {
        case Phase::StartEntry: {
            if (curEntry_ >= entries_.size()) {
                buf_.clear();
                bufCursor_ = 0;
                phase_ = Phase::WriteCentralDir;
                continue;
            }
            const auto& e = entries_[curEntry_];
            curFile_.close();
            curFile_.clear();
            curFile_.open(e.abs, std::ios::binary);
            if (!curFile_.is_open()) {
                errored_ = true;
                errorMsg_ = "open_failed: " + e.abs.string();
                core::log_error("streaming_zip: " + errorMsg_);
                return false;
            }
            localOffsets_.push_back(written_);
            curCrc_ = MZ_CRC32_INIT;
            curRead_ = 0;
            buildLocalHeader();
            phase_ = Phase::StreamData;
            continue;
        }
        case Phase::StreamData: {
            // Finir d'abord de purger le header si encore des octets.
            const auto drained = drainBuffer(sink, maxBytes);
            if (drained > 0) {
                maxBytes -= drained;
                continue;
            }
            // Header vidé → lire le fichier.
            const auto& e = entries_[curEntry_];
            if (curRead_ >= e.size) {
                crcs_.push_back(curCrc_);
                curFile_.close();
                buildDataDescriptor();
                phase_ = Phase::WriteDataDesc;
                continue;
            }
            const auto remaining = e.size - curRead_;
            const auto chunk = std::min<std::uint64_t>(
                std::min<std::uint64_t>(remaining, kReadBufSize), maxBytes);
            std::vector<char> tmp(static_cast<std::size_t>(chunk));
            curFile_.read(tmp.data(), static_cast<std::streamsize>(chunk));
            const auto got = static_cast<std::size_t>(curFile_.gcount());
            if (got == 0) {
                errored_ = true;
                errorMsg_ = "read_eof_early: " + e.abs.string();
                core::log_error("streaming_zip: " + errorMsg_);
                return false;
            }
            curCrc_ = static_cast<std::uint32_t>(mz_crc32(
                curCrc_,
                reinterpret_cast<const unsigned char*>(tmp.data()),
                got));
            if (!sink.write(tmp.data(), got)) {
                errored_ = true;
                errorMsg_ = "sink_write_failed (data)";
                return false;
            }
            curRead_ += got;
            written_ += got;
            maxBytes -= got;
            continue;
        }
        case Phase::WriteDataDesc: {
            const auto drained = drainBuffer(sink, maxBytes);
            if (drained > 0) {
                maxBytes -= drained;
                continue;
            }
            // Data descriptor vidé → entrée suivante.
            ++curEntry_;
            phase_ = Phase::StartEntry;
            continue;
        }
        case Phase::WriteCentralDir: {
            if (bufCursor_ == 0 && buf_.empty()) {
                cdOffset_ = written_;
                buildCentralDirectory();
            }
            const auto drained = drainBuffer(sink, maxBytes);
            if (drained > 0) {
                maxBytes -= drained;
                continue;
            }
            const auto cdSize = written_ - cdOffset_;
            buildEocd(cdOffset_, cdSize);
            phase_ = Phase::WriteEocd;
            continue;
        }
        case Phase::WriteEocd: {
            const auto drained = drainBuffer(sink, maxBytes);
            if (drained > 0) {
                maxBytes -= drained;
                continue;
            }
            phase_ = Phase::Done;
            return false; // signal "done"
        }
        case Phase::Done:
            return false;
        }
    }
    return phase_ != Phase::Done && !errored_;
}

} // namespace ltr::web
