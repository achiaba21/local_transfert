# Tests minimaux, sans framework (assertions simples).
# Compilés si `-DLTR_BUILD_TESTS=ON` est passé à CMake.
#
# V1.1.1 : on force les tests à compiler avec asserts actifs même en
# Release, sinon assert() est un no-op et le test passe sans rien vérifier.
add_compile_options($<$<CONFIG:Release>:-UNDEBUG>)

add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE ltr_core)
add_test(NAME protocol COMMAND test_protocol)

add_executable(test_hash test_hash.cpp)
target_link_libraries(test_hash PRIVATE ltr_core)
add_test(NAME hash COMMAND test_hash)

# --- tests couche web ---
add_executable(test_web_session_store test_web_session_store.cpp)
target_link_libraries(test_web_session_store PRIVATE ltr_core)
add_test(NAME web_session_store COMMAND test_web_session_store)

add_executable(test_web_session_dedup test_web_session_dedup.cpp)
target_link_libraries(test_web_session_dedup PRIVATE ltr_core)
add_test(NAME web_session_dedup COMMAND test_web_session_dedup)

# V1.2 — Sprint Web P2P : nom auto stable (adjectif + animal + emoji).
add_executable(test_display_name test_display_name.cpp)
target_link_libraries(test_display_name PRIVATE ltr_core)
add_test(NAME display_name COMMAND test_display_name)

# V1.2 — Sprint Web P2P : routes signaling (401/400/404/204).
add_executable(test_p2p_signal_routes test_p2p_signal_routes.cpp)
target_link_libraries(test_p2p_signal_routes PRIVATE ltr_core cpphttplib)
add_test(NAME p2p_signal_routes COMMAND test_p2p_signal_routes)
set_tests_properties(p2p_signal_routes PROPERTIES
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# V1.4 — Sprint Clipboard Paste : invariants de l'API readClipboard.
add_executable(test_clipboard_stub test_clipboard_stub.cpp)
target_link_libraries(test_clipboard_stub PRIVATE ltr_ui)
add_test(NAME clipboard_stub COMMAND test_clipboard_stub)

# V1.5 — Sprint Hardening : encodeur PNG extrait dans core.
add_executable(test_png_encoder test_png_encoder.cpp)
target_link_libraries(test_png_encoder PRIVATE ltr_core)
add_test(NAME png_encoder COMMAND test_png_encoder)

add_executable(test_download_ticket test_download_ticket.cpp)
target_link_libraries(test_download_ticket PRIVATE ltr_core)
add_test(NAME download_ticket COMMAND test_download_ticket)

add_executable(test_qr_code test_qr_code.cpp)
target_link_libraries(test_qr_code PRIVATE ltr_core)
add_test(NAME qr_code COMMAND test_qr_code)

add_executable(test_http_smoke test_http_smoke.cpp)
target_link_libraries(test_http_smoke PRIVATE ltr_core cpphttplib)
add_test(NAME http_smoke COMMAND test_http_smoke)
set_tests_properties(http_smoke PROPERTIES
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

find_program(NODE_EXECUTABLE node)
if (NODE_EXECUTABLE)
  add_test(NAME web_folder_logic
           COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_web_folder_logic.js)
endif()

# V1.1.8 : test du générateur zip streamé (formule Content-Length + zip valide).
add_executable(test_streaming_zip test_streaming_zip.cpp)
target_link_libraries(test_streaming_zip PRIVATE ltr_core cpphttplib miniz)
add_test(NAME streaming_zip COMMAND test_streaming_zip)

# V1.1.9 — Sprint Transfer Resume : tests I/O sidecar + pending-sessions.
add_executable(test_resume_sidecar test_resume_sidecar.cpp)
target_link_libraries(test_resume_sidecar PRIVATE ltr_core)
add_test(NAME resume_sidecar COMMAND test_resume_sidecar)

# V1.6.4 — Sprint Sécurité (Wave 2 TOFU TCP) : registre known_peers.json.
add_executable(test_known_peers test_known_peers.cpp)
target_link_libraries(test_known_peers PRIVATE ltr_core)
add_test(NAME known_peers COMMAND test_known_peers)

# V1.6.5 — Sprint Stabilité (Wave 1, item D) : parsing Range RFC 7233.
add_executable(test_range_parser test_range_parser.cpp)
target_link_libraries(test_range_parser PRIVATE ltr_core)
add_test(NAME range_parser COMMAND test_range_parser)

# V1.6.5 — Sprint Stabilité (Wave 3 item H) : tokens persistents HMAC.
add_executable(test_persistent_token test_persistent_token.cpp)
target_link_libraries(test_persistent_token PRIVATE ltr_core)
add_test(NAME persistent_token COMMAND test_persistent_token)

# V1.6.5 — Sprint Stabilité (Wave 4 item J) : historique des pairs.
add_executable(test_peers_history test_peers_history.cpp)
target_link_libraries(test_peers_history PRIVATE ltr_core)
add_test(NAME peers_history COMMAND test_peers_history)

# V1.6.5 — Sprint Stabilité (Wave 4 item K) : historique des transferts.
add_executable(test_transfer_history test_transfer_history.cpp)
target_link_libraries(test_transfer_history PRIVATE ltr_core)
add_test(NAME transfer_history COMMAND test_transfer_history)

add_executable(test_business_policy test_business_policy.cpp)
target_link_libraries(test_business_policy PRIVATE ltr_core)
add_test(NAME business_policy COMMAND test_business_policy)

add_executable(test_quota_service test_quota_service.cpp)
target_link_libraries(test_quota_service PRIVATE ltr_core)
add_test(NAME quota_service COMMAND test_quota_service)

add_executable(test_audit_export_service test_audit_export_service.cpp)
target_link_libraries(test_audit_export_service PRIVATE ltr_core)
add_test(NAME audit_export_service COMMAND test_audit_export_service)

# Phase 2 — Portail Client Externe.
add_executable(test_deposit_link_service test_deposit_link_service.cpp)
target_link_libraries(test_deposit_link_service PRIVATE ltr_core)
add_test(NAME deposit_link_service COMMAND test_deposit_link_service)

add_executable(test_deposit_receipt_service test_deposit_receipt_service.cpp)
target_link_libraries(test_deposit_receipt_service PRIVATE ltr_core)
add_test(NAME deposit_receipt_service COMMAND test_deposit_receipt_service)

add_executable(test_deposit_history test_deposit_history.cpp)
target_link_libraries(test_deposit_history PRIVATE ltr_core)
add_test(NAME deposit_history COMMAND test_deposit_history)

add_executable(test_deposit_session_service test_deposit_session_service.cpp)
target_link_libraries(test_deposit_session_service PRIVATE ltr_core)
add_test(NAME deposit_session_service COMMAND test_deposit_session_service)

add_executable(test_deposit_routes test_deposit_routes.cpp)
target_link_libraries(test_deposit_routes PRIVATE ltr_core cpphttplib)
add_test(NAME deposit_routes COMMAND test_deposit_routes)
set_tests_properties(deposit_routes PROPERTIES
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Phase 3 — Contrôle IT.
add_executable(test_cidr_matcher test_cidr_matcher.cpp)
target_link_libraries(test_cidr_matcher PRIVATE ltr_core)
add_test(NAME cidr_matcher COMMAND test_cidr_matcher)

add_executable(test_policy_enforcement test_policy_enforcement.cpp)
target_link_libraries(test_policy_enforcement PRIVATE ltr_core)
add_test(NAME policy_enforcement COMMAND test_policy_enforcement)

add_executable(test_retention_service test_retention_service.cpp)
target_link_libraries(test_retention_service PRIVATE ltr_core)
add_test(NAME retention_service COMMAND test_retention_service)

# V1.1.10 — Sprint UI Layout System : DSL HBox/VBox + breakpoints + cache.
add_executable(test_layout_box test_layout_box.cpp)
target_link_libraries(test_layout_box PRIVATE ltr_ui)
add_test(NAME layout_box COMMAND test_layout_box)

add_executable(test_breakpoint test_breakpoint.cpp)
target_link_libraries(test_breakpoint PRIVATE ltr_ui)
add_test(NAME breakpoint COMMAND test_breakpoint)

add_executable(test_label_ellipsis test_label_ellipsis.cpp)
target_link_libraries(test_label_ellipsis PRIVATE ltr_ui)
add_test(NAME label_ellipsis COMMAND test_label_ellipsis)
