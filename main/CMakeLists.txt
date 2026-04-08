file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/*.c")
list(TRANSFORM SRC_FILES REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "")

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS ""
)
