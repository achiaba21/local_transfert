cmake_minimum_required(VERSION 3.20)
# V1.1.8-UX3 : OBJCXX ajouté pour compiler drag_drop_mac.mm sur APPLE.
# Inclus dans la liste de LANGUAGES directement pour éviter un
# enable_language() conditionnel qui causait "Generate step failed".
if(APPLE)
    project(local_transfer VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else()
    project(local_transfer VERSION 0.1.0 LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(LTR_BUILD_TESTS "Build unit tests" OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(cmake/Dependencies.cmake)

# --- embedding des assets web ------------------------------------------------
# Génère un header C++ par asset web (index.html, app.js, style.css, icônes).
set(LTR_WEB_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
set(LTR_WEB_ASSETS_DIR "${LTR_WEB_GEN_DIR}/ltr/web/assets")

function(ltr_embed_file SRC SYMBOL MIME OUT_BASENAME)
    set(DST "${LTR_WEB_ASSETS_DIR}/${OUT_BASENAME}.hpp")
    add_custom_command(
        OUTPUT ${DST}
        COMMAND ${CMAKE_COMMAND}
                -DSRC=${SRC}
                -DDST=${DST}
                -DSYMBOL=${SYMBOL}
                -DMIME=${MIME}
                -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake
        DEPENDS ${SRC} ${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake
        VERBATIM)
    set(${OUT_BASENAME}_GEN ${DST} PARENT_SCOPE)
endfunction()

ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/html/index.html
               IndexHtml "text/html; charset=utf-8" index_html)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/html/login.html
               LoginHtml "text/html; charset=utf-8" login_html)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/app.js
               AppJs "application/javascript; charset=utf-8" app_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/common.js
               CommonJs "application/javascript; charset=utf-8" common_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/upload.js
               UploadJs "application/javascript; charset=utf-8" upload_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/download.js
               DownloadJs "application/javascript; charset=utf-8" download_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/peers.js
               PeersJs "application/javascript; charset=utf-8" peers_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/p2p.js
               P2pJs "application/javascript; charset=utf-8" p2p_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/p2p_transport.js
               P2pTransportJs "application/javascript; charset=utf-8" p2p_transport_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/p2p_session.js
               P2pSessionJs "application/javascript; charset=utf-8" p2p_session_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/p2p_ui.js
               P2pUiJs "application/javascript; charset=utf-8" p2p_ui_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/transfer_registry.js
               TransferRegistryJs "application/javascript; charset=utf-8" transfer_registry_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/share.js
               ShareJs "application/javascript; charset=utf-8" share_js)
# V1.6.5 — Wave 2 item E : wrapper IndexedDB minimal
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/idb.js
               IdbJs "application/javascript; charset=utf-8" idb_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/web_profile.js
               WebProfileJs "application/javascript; charset=utf-8" web_profile_js)
# V1.6.5 — Wave 3 item I : stockage chiffré du PIN (WebCrypto AES-GCM)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/pin_storage.js
               PinStorageJs "application/javascript; charset=utf-8" pin_storage_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/login.js
               LoginJs "application/javascript; charset=utf-8" login_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/css/style.css
               StyleCss "text/css; charset=utf-8" style_css)
# Phase 2 — Portail Client Externe.
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/html/deposit.html
               DepositHtml "text/html; charset=utf-8" deposit_html)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/html/deposit_expired.html
               DepositExpiredHtml "text/html; charset=utf-8" deposit_expired_html)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/html/deposit_receipt.html
               DepositReceiptHtml "text/html; charset=utf-8" deposit_receipt_html)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/deposit.js
               DepositJs "application/javascript; charset=utf-8" deposit_js)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/js/host_deposit_links.js
               HostDepositLinksJs "application/javascript; charset=utf-8" host_deposit_links_js)

# V1.6.6+ — Polices Geist copiées dans le dossier de build (cohérence
# Mac+Win, sans embed binaire qui faisait exploser le compile time —
# le script EmbedFile.cmake en hex est O(N²) sur les fichiers > 100 KB).
# theme.cpp les charge ensuite via sf::Font::loadFromFile en cherchant
# en priorité dans le cwd "assets/fonts/" puis fallback système.
file(COPY ${CMAKE_SOURCE_DIR}/assets/fonts/Geist-Regular.ttf
          ${CMAKE_SOURCE_DIR}/assets/fonts/Geist-Bold.ttf
     DESTINATION ${CMAKE_BINARY_DIR}/assets/fonts)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/icons/upload.svg
               IconUpload "image/svg+xml" icon_upload)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/icons/download.svg
               IconDownload "image/svg+xml" icon_download)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/web/icons/file.svg
               IconFile "image/svg+xml" icon_file)

# --- icônes PNG app desktop (sprint UX-1) ------------------------------------
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/check.png
               IconCheck "image/png" icon_check)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/folder.png
               IconFolder "image/png" icon_folder)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/close.png
               IconClose "image/png" icon_close_png)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/arrow-up.png
               IconArrowUp "image/png" icon_arrow_up)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/arrow-down.png
               IconArrowDown "image/png" icon_arrow_down)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/radar.png
               IconRadar "image/png" icon_radar)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/no-device.png
               IconNoDevice "image/png" icon_no_device)
ltr_embed_file(${CMAKE_SOURCE_DIR}/assets/icons/qr.png
               IconQrCode "image/png" icon_qr)

set(LTR_WEB_GEN_HEADERS
    ${index_html_GEN}
    ${login_html_GEN}
    ${app_js_GEN}
    ${common_js_GEN}
    ${upload_js_GEN}
    ${download_js_GEN}
    ${peers_js_GEN}
    ${p2p_js_GEN}
    ${p2p_transport_js_GEN}
    ${p2p_session_js_GEN}
    ${p2p_ui_js_GEN}
    ${transfer_registry_js_GEN}
    ${share_js_GEN}
    ${idb_js_GEN}
    ${web_profile_js_GEN}
    ${pin_storage_js_GEN}
    ${login_js_GEN}
    ${style_css_GEN}
    ${deposit_html_GEN}
    ${deposit_expired_html_GEN}
    ${deposit_receipt_html_GEN}
    ${deposit_js_GEN}
    ${host_deposit_links_js_GEN}
    ${icon_upload_GEN}
    ${icon_download_GEN}
    ${icon_file_GEN}
    ${icon_check_GEN}
    ${icon_folder_GEN}
    ${icon_close_png_GEN}
    ${icon_arrow_up_GEN}
    ${icon_arrow_down_GEN}
    ${icon_radar_GEN}
    ${icon_no_device_GEN}
    ${icon_qr_GEN})

add_custom_target(ltr_web_assets DEPENDS ${LTR_WEB_GEN_HEADERS})

# --- self_binary source conditionnelle selon l'OS ---------------------------
if(APPLE)
    set(LTR_SELF_BINARY_SRC src/web/self_binary_mac.cpp)
elseif(WIN32)
    set(LTR_SELF_BINARY_SRC src/web/self_binary_win.cpp)
else()
    set(LTR_SELF_BINARY_SRC src/web/self_binary_posix.cpp)
endif()

# --- core + domain + infra + network + web + app ----------------------------
add_library(ltr_core STATIC
    src/core/event_bus.cpp
    src/core/format.cpp
    src/core/logger.cpp
    src/core/png_encoder.cpp
    src/core/shell_open.cpp
    src/domain/transfer_session.cpp
    src/network/protocol.cpp
    src/network/broadcast_socket.cpp
    src/network/discovery_service.cpp
    src/network/transfer_server.cpp
    src/network/transfer_client.cpp
    src/infra/config.cpp
    src/infra/filesystem_service.cpp
    src/infra/hash_service.cpp
    src/infra/resume_sidecar.cpp
    src/infra/business_policy.cpp
    src/infra/quota_service.cpp
    src/infra/audit_export_service.cpp
    # Phase 2 — Portail Client Externe.
    src/infra/deposit_link.cpp
    src/infra/deposit_link_repository.cpp
    src/infra/deposit_link_service.cpp
    src/infra/deposit_session.cpp
    src/infra/deposit_session_repository.cpp
    src/infra/deposit_session_service.cpp
    src/infra/deposit_history.cpp
    src/infra/deposit_receipt.cpp
    src/infra/deposit_token_generator.cpp
    src/infra/known_peers.cpp        # V1.6.4 — Wave 2 TOFU TCP
    src/infra/crypto_identity.cpp    # V1.6.4 — Wave 2 TOFU TCP
    src/infra/peers_history.cpp      # V1.6.5 — Wave 4 item J
    src/infra/transfer_history.cpp   # V1.6.5 — Wave 4 item K
    src/app/app_controller.cpp
    # --- couche web ---
    src/web/http_server.cpp
    src/web/web_service.cpp
    src/web/web_session_store.cpp
    src/web/display_name.cpp
    src/web/cert_manager.cpp
    src/web/range_parser.cpp           # V1.6.5 — Wave 1 item D
    src/web/download_ticket_store.cpp
    src/web/sse_channel.cpp
    src/web/sse_broadcaster.cpp
    src/web/qr_code.cpp
    src/web/web_upload_announce.cpp
    src/web/streaming_zip_source.cpp
    ${LTR_SELF_BINARY_SRC}
    src/web/routes/route_registrar.cpp
    src/web/routes/route_helpers.cpp
    src/web/routes/auth_routes.cpp
    src/web/routes/logout_routes.cpp
    src/web/routes/upload_routes.cpp
    src/web/routes/download_routes.cpp
    src/web/routes/events_routes.cpp
    src/web/routes/history_routes.cpp
    src/web/routes/deposit_routes.cpp        # Phase 2
    src/web/routes/deposit_admin_routes.cpp  # Phase 2
    src/web/routes/p2p_routes.cpp
    src/web/routes/self_routes.cpp
    src/web/routes/share_routes.cpp
    src/web/routes/static_routes.cpp
)
target_include_directories(ltr_core PUBLIC include ${LTR_WEB_GEN_DIR})
add_dependencies(ltr_core ltr_web_assets)
# V1.6.4 : openssl pour HTTPS (cpp-httplib SSLServer) + TOFU Ed25519.
target_compile_definitions(ltr_core PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(ltr_core
    PUBLIC
        sfml-network
        sfml-system
        sfml-graphics
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
    PRIVATE
        picosha2
        cpphttplib
        qrcodegen
        miniz
)
if(WIN32)
    target_link_libraries(ltr_core PUBLIC ws2_32)
endif()

# V1.1.8-UX3 : drag & drop natif OS — compile conditionnelle par plateforme.
if(APPLE)
    set(LTR_DRAG_DROP_SRC src/ui/drag_drop_mac.mm)
    set_source_files_properties(src/ui/drag_drop_mac.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc -Wno-deprecated-declarations")
elseif(WIN32)
    set(LTR_DRAG_DROP_SRC src/ui/drag_drop_win.cpp)
else()
    set(LTR_DRAG_DROP_SRC src/ui/drag_drop_stub.cpp)
endif()

# V1.4 — Sprint Clipboard Paste : presse-papier natif, même pattern.
if(APPLE)
    set(LTR_CLIPBOARD_SRC src/ui/clipboard_paste_mac.mm)
    set_source_files_properties(src/ui/clipboard_paste_mac.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc -Wno-deprecated-declarations")
elseif(WIN32)
    set(LTR_CLIPBOARD_SRC src/ui/clipboard_paste_win.cpp)
else()
    set(LTR_CLIPBOARD_SRC src/ui/clipboard_paste_stub.cpp)
endif()

# --- ui -----------------------------------------------------------------------
add_library(ltr_ui STATIC
    src/ui/theme.cpp
    src/ui/screen.cpp
    src/ui/ui_app.cpp
    src/ui/rounded_rect.cpp
    src/ui/icon_library.cpp
    # V1.1.10 — Sprint UI Layout System
    src/ui/clip_scope.cpp
    src/ui/layout.cpp
    src/ui/animation.cpp
    src/ui/dpi.cpp
    src/ui/widgets/button.cpp
    src/ui/widgets/dropdown_menu.cpp
    src/ui/widgets/label.cpp
    src/ui/widgets/progress_bar.cpp
    src/ui/widgets/device_list_item.cpp
    src/ui/widgets/file_row.cpp
    src/ui/widgets/card.cpp
    src/ui/widgets/text_input.cpp
    src/ui/widgets/qr_code_view.cpp
    src/ui/widgets/scroll_area.cpp
    src/ui/widgets/share_panel.cpp
    src/ui/widgets/deposit_link_card.cpp        # Phase 2
    src/ui/screens/main_screen.cpp
    src/ui/screens/incoming_offer_screen.cpp
    src/ui/screens/deposit_links_screen.cpp     # Phase 2
    ${LTR_DRAG_DROP_SRC}
    ${LTR_CLIPBOARD_SRC}
)
target_include_directories(ltr_ui PUBLIC include)
target_link_libraries(ltr_ui
    PUBLIC
        ltr_core
        sfml-graphics
        sfml-window
    PRIVATE
        tinyfiledialogs
)

# V1.1.8-UX3 : libs natives drag & drop.
if(APPLE)
    target_link_libraries(ltr_ui PRIVATE "-framework AppKit")
elseif(WIN32)
    target_link_libraries(ltr_ui PRIVATE shell32 comctl32)
endif()

# --- app executable -----------------------------------------------------------
add_executable(local_transfer src/main.cpp)
target_link_libraries(local_transfer PRIVATE ltr_ui)

# Copy assets next to the executable after build.
add_custom_command(TARGET local_transfer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:local_transfer>/assets
    VERBATIM)

# --- tests --------------------------------------------------------------------
if(LTR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
