#include "ltr/web/static_asset.hpp"

// Headers générés par EmbedFile.cmake.
#include "ltr/web/assets/app_js.hpp"
#include "ltr/web/assets/common_js.hpp"
#include "ltr/web/assets/deposit_html.hpp"
#include "ltr/web/assets/deposit_expired_html.hpp"
#include "ltr/web/assets/deposit_js.hpp"
#include "ltr/web/assets/deposit_receipt_html.hpp"
#include "ltr/web/assets/download_js.hpp"
#include "ltr/web/assets/host_deposit_links_js.hpp"
#include "ltr/web/assets/icon_download.hpp"
#include "ltr/web/assets/icon_file.hpp"
#include "ltr/web/assets/icon_upload.hpp"
#include "ltr/web/assets/idb_js.hpp"
#include "ltr/web/assets/login_js.hpp"
#include "ltr/web/assets/p2p_js.hpp"
#include "ltr/web/assets/p2p_session_js.hpp"
#include "ltr/web/assets/p2p_transport_js.hpp"
#include "ltr/web/assets/p2p_ui_js.hpp"
#include "ltr/web/assets/peers_js.hpp"
#include "ltr/web/assets/pin_storage_js.hpp"
#include "ltr/web/assets/share_js.hpp"
#include "ltr/web/assets/style_css.hpp"
#include "ltr/web/assets/transfer_registry_js.hpp"
#include "ltr/web/assets/upload_js.hpp"
#include "ltr/web/assets/web_profile_js.hpp"

namespace ltr::web {

std::vector<StaticAsset> buildStaticAssetTable() {
    using namespace ltr::web::assets;
    return {
        // Scripts page de connexion (publics).
        { "/login.js",            LoginJs,            LoginJsMime,            true },

        // Scripts dashboard (publics, l'auth est sur les routes API).
        { "/app.js",              AppJs,              AppJsMime,              true },
        { "/common.js",           CommonJs,           CommonJsMime,           true },
        { "/upload.js",           UploadJs,           UploadJsMime,           true },
        { "/download.js",         DownloadJs,         DownloadJsMime,         true },
        { "/peers.js",            PeersJs,            PeersJsMime,            true },
        { "/share.js",            ShareJs,            ShareJsMime,            true },
        { "/web_profile.js",      WebProfileJs,       WebProfileJsMime,       true },
        { "/transfer_registry.js",TransferRegistryJs, TransferRegistryJsMime, true },
        { "/idb.js",              IdbJs,              IdbJsMime,              true },
        { "/pin_storage.js",      PinStorageJs,       PinStorageJsMime,       true },

        // Module WebRTC (split en 4 fichiers).
        { "/p2p.js",              P2pJs,              P2pJsMime,              true },
        { "/p2p_transport.js",    P2pTransportJs,     P2pTransportJsMime,     true },
        { "/p2p_session.js",      P2pSessionJs,       P2pSessionJsMime,       true },
        { "/p2p_ui.js",           P2pUiJs,            P2pUiJsMime,            true },

        // Phase 2 — Portail Client Externe.
        { "/deposit.js",            DepositJs,            DepositJsMime,            true },
        { "/host_deposit_links.js", HostDepositLinksJs,   HostDepositLinksJsMime,   true },
        { "/deposit-receipt",       DepositReceiptHtml,   DepositReceiptHtmlMime,   true },

        // CSS partagé.
        { "/style.css",           StyleCss,           StyleCssMime,           true },

        // Icônes.
        { "/icons/upload.svg",    IconUpload,         IconUploadMime,         true },
        { "/icons/download.svg",  IconDownload,       IconDownloadMime,       true },
        { "/icons/file.svg",      IconFile,           IconFileMime,           true },
    };
}

} // namespace ltr::web
