include(FetchContent)

# ---------- OpenSSL (V1.6.4 : HTTPS LAN + TOFU Ed25519) ----------------------
# Cherche openssl système. Sur macOS Homebrew : /opt/homebrew/opt/openssl@3.
# Sur Windows : OPENSSL_ROOT_DIR à passer manuellement ou via vcpkg.
if(APPLE AND NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3" CACHE PATH "")
endif()
find_package(OpenSSL REQUIRED)

# ---------- SFML 2.6 ----------------------------------------------------------
set(SFML_BUILD_NETWORK  ON  CACHE BOOL "" FORCE)
set(SFML_BUILD_GRAPHICS ON  CACHE BOOL "" FORCE)
set(SFML_BUILD_WINDOW   ON  CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO    OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        2.6.1
    GIT_SHALLOW    TRUE
)

# ---------- nlohmann_json -----------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(SFML nlohmann_json)

# ---------- picosha2 (single header, public domain) ---------------------------
FetchContent_Declare(
    picosha2_src
    GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(picosha2_src)
if(NOT picosha2_src_POPULATED)
    FetchContent_Populate(picosha2_src)
endif()
add_library(picosha2 INTERFACE)
target_include_directories(picosha2 INTERFACE ${picosha2_src_SOURCE_DIR})

# ---------- tinyfiledialogs ---------------------------------------------------
FetchContent_Declare(
    tinyfd_src
    GIT_REPOSITORY https://git.code.sf.net/p/tinyfiledialogs/code
    GIT_TAG        master
)
FetchContent_GetProperties(tinyfd_src)
if(NOT tinyfd_src_POPULATED)
    FetchContent_Populate(tinyfd_src)
endif()
add_library(tinyfiledialogs STATIC
    ${tinyfd_src_SOURCE_DIR}/tinyfiledialogs.c
)
target_include_directories(tinyfiledialogs PUBLIC ${tinyfd_src_SOURCE_DIR})
if(MSVC)
    target_compile_definitions(tinyfiledialogs PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(APPLE)
    target_link_libraries(tinyfiledialogs PUBLIC "-framework AppKit")
endif()

# ---------- cpp-httplib (single-header HTTP server/client) --------------------
FetchContent_Declare(
    cpphttplib_src
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.1
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(cpphttplib_src)
if(NOT cpphttplib_src_POPULATED)
    FetchContent_Populate(cpphttplib_src)
endif()
add_library(cpphttplib INTERFACE)
target_include_directories(cpphttplib INTERFACE ${cpphttplib_src_SOURCE_DIR})
# NOTE : NE PAS définir CPPHTTPLIB_OPENSSL_SUPPORT (même à 0) — le header
# teste `#ifdef`, pas la valeur. Simplement l'omettre → build HTTP plain.
if(WIN32)
    target_link_libraries(cpphttplib INTERFACE ws2_32)
endif()

# ---------- qrcodegen (1 cpp, public domain) ----------------------------------
FetchContent_Declare(
    qrcodegen_src
    GIT_REPOSITORY https://github.com/nayuki/QR-Code-generator.git
    GIT_TAG        v1.8.0
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(qrcodegen_src)
if(NOT qrcodegen_src_POPULATED)
    FetchContent_Populate(qrcodegen_src)
endif()
add_library(qrcodegen STATIC
    ${qrcodegen_src_SOURCE_DIR}/cpp/qrcodegen.cpp)
target_include_directories(qrcodegen PUBLIC
    ${qrcodegen_src_SOURCE_DIR}/cpp)

# ---------- miniz (single-file zip, public domain) ---------------------------
FetchContent_Declare(
    miniz_src
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.0.2
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(miniz_src)
if(NOT miniz_src_POPULATED)
    FetchContent_Populate(miniz_src)
endif()
# miniz.h inclut miniz_export.h (généré par le CMake upstream que l'on
# n'utilise pas ici). On fournit un stub minimal pour un build static.
set(MINIZ_EXPORT_STUB "${CMAKE_BINARY_DIR}/miniz_stub/miniz_export.h")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/miniz_stub")
file(WRITE "${MINIZ_EXPORT_STUB}"
"#ifndef MINIZ_EXPORT_H\n"
"#define MINIZ_EXPORT_H\n"
"#define MINIZ_EXPORT\n"
"#endif\n")

file(GLOB MINIZ_SOURCES ${miniz_src_SOURCE_DIR}/miniz*.c)
add_library(miniz STATIC ${MINIZ_SOURCES})
target_include_directories(miniz PUBLIC
    ${miniz_src_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/miniz_stub)
if(MSVC)
    target_compile_definitions(miniz PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
