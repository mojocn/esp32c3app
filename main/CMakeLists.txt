
file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/*.c")
list(TRANSFORM SRC_FILES REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "")
file(GLOB WEB_FILES "../web/*")

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    # embed the web files into the binary
    EMBED_FILES ${WEB_FILES}
)
