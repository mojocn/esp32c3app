set(HTML_SRC "${CMAKE_CURRENT_LIST_DIR}/index.html")
set(HTML_GZ  "${CMAKE_CURRENT_BINARY_DIR}/index.html.gz")

add_custom_command(
    OUTPUT  "${HTML_GZ}"
    COMMAND python3 -c
            "import gzip,sys;open(sys.argv[2],'wb').write(gzip.compress(open(sys.argv[1],'rb').read(),9))"
            "${HTML_SRC}" "${HTML_GZ}"
    DEPENDS "${HTML_SRC}"
    COMMENT "Gzip-compressing index.html → index.html.gz"
    VERBATIM
)

file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/*.c")
list(TRANSFORM SRC_FILES REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "")

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS ""
                       EMBED_FILES "${HTML_GZ}"
)
