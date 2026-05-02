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

# V1.4 — Sprint Clipboard Paste : invariants de l'API readClipboard.
add_executable(test_clipboard_stub test_clipboard_stub.cpp)
target_link_libraries(test_clipboard_stub PRIVATE ltr_ui)
add_test(NAME clipboard_stub COMMAND test_clipboard_stub)

add_executable(test_download_ticket test_download_ticket.cpp)
target_link_libraries(test_download_ticket PRIVATE ltr_core)
add_test(NAME download_ticket COMMAND test_download_ticket)

add_executable(test_qr_code test_qr_code.cpp)
target_link_libraries(test_qr_code PRIVATE ltr_core)
add_test(NAME qr_code COMMAND test_qr_code)

add_executable(test_http_smoke test_http_smoke.cpp)
target_link_libraries(test_http_smoke PRIVATE ltr_core cpphttplib)
add_test(NAME http_smoke COMMAND test_http_smoke)

# V1.1.8 : test du générateur zip streamé (formule Content-Length + zip valide).
add_executable(test_streaming_zip test_streaming_zip.cpp)
target_link_libraries(test_streaming_zip PRIVATE ltr_core cpphttplib miniz)
add_test(NAME streaming_zip COMMAND test_streaming_zip)

# V1.1.9 — Sprint Transfer Resume : tests I/O sidecar + pending-sessions.
add_executable(test_resume_sidecar test_resume_sidecar.cpp)
target_link_libraries(test_resume_sidecar PRIVATE ltr_core)
add_test(NAME resume_sidecar COMMAND test_resume_sidecar)

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
