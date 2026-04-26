# EmbedFile.cmake
#
# Script utilitaire qui génère un header C++ embarquant le contenu binaire
# d'un fichier sous forme de tableau constexpr + std::string_view.
#
# Variables d'entrée (à définir avant inclusion) :
#   SRC     — chemin absolu du fichier source à embarquer
#   DST     — chemin absolu du header C++ à générer
#   SYMBOL  — nom du symbole C++ (ex: "IndexHtml")
#   MIME    — chaîne MIME (ex: "text/html; charset=utf-8")
#
# Usage typique (via add_custom_command) :
#   ${CMAKE_COMMAND}
#       -DSRC=${CMAKE_SOURCE_DIR}/assets/web/index.html
#       -DDST=${CMAKE_BINARY_DIR}/generated/ltr/web/assets/index_html.hpp
#       -DSYMBOL=IndexHtml
#       -DMIME=text/html
#       -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake
#
# Sortie : namespace ltr::web::assets { inline constexpr ... };

if(NOT DEFINED SRC OR NOT DEFINED DST OR NOT DEFINED SYMBOL OR NOT DEFINED MIME)
    message(FATAL_ERROR "EmbedFile.cmake: SRC, DST, SYMBOL, MIME requis")
endif()

if(NOT EXISTS "${SRC}")
    message(FATAL_ERROR "EmbedFile.cmake: fichier source introuvable: ${SRC}")
endif()

# Lit le fichier en hex.
file(READ "${SRC}" HEX_CONTENT HEX)
string(LENGTH "${HEX_CONTENT}" HEX_LEN)
math(EXPR BYTE_COUNT "${HEX_LEN} / 2")

# Formate en "0x??, 0x??, ..." avec retour à la ligne tous les 16 octets.
set(OUT_BYTES "")
set(COL 0)
set(POS 0)
while(POS LESS HEX_LEN)
    string(SUBSTRING "${HEX_CONTENT}" ${POS} 2 BYTE)
    if(COL EQUAL 0)
        set(OUT_BYTES "${OUT_BYTES}    ")
    endif()
    set(OUT_BYTES "${OUT_BYTES}0x${BYTE}")
    math(EXPR POS_NEXT "${POS} + 2")
    if(POS_NEXT LESS HEX_LEN)
        set(OUT_BYTES "${OUT_BYTES}, ")
    endif()
    math(EXPR COL "${COL} + 1")
    if(COL EQUAL 16)
        set(OUT_BYTES "${OUT_BYTES}\n")
        set(COL 0)
    endif()
    set(POS ${POS_NEXT})
endwhile()

get_filename_component(SRC_NAME "${SRC}" NAME)

set(HEADER_CONTENT
"// ============================================================================
// Fichier généré par EmbedFile.cmake — NE PAS ÉDITER MANUELLEMENT
// Source : ${SRC_NAME}
// Taille : ${BYTE_COUNT} octets
// ============================================================================
#pragma once

#include <cstddef>
#include <string_view>

namespace ltr::web::assets {

inline constexpr unsigned char ${SYMBOL}Bytes[] = {
${OUT_BYTES}
};

inline constexpr std::size_t ${SYMBOL}Size = ${BYTE_COUNT};

inline constexpr std::string_view ${SYMBOL}Mime = \"${MIME}\";

// reinterpret_cast interdit en constexpr → on garde `inline const`.
inline const std::string_view ${SYMBOL}{
    reinterpret_cast<const char*>(${SYMBOL}Bytes), ${SYMBOL}Size };

} // namespace ltr::web::assets
")

# Crée le dossier parent si nécessaire.
get_filename_component(DST_DIR "${DST}" DIRECTORY)
file(MAKE_DIRECTORY "${DST_DIR}")

file(WRITE "${DST}" "${HEADER_CONTENT}")
