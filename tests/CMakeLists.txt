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
