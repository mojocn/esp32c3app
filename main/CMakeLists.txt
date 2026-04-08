set(HTML_SRC "${CMAKE_CURRENT_LIST_DIR}/index.html")
set(HTML_GZ  "${CMAKE_CURRENT_LIST_DIR}/index.html.gz")

find_program(GZIP_EXECUTABLE gzip REQUIRED)

execute_process(
    COMMAND ${GZIP_EXECUTABLE} -9 -f -n -c "${HTML_SRC}"
    OUTPUT_FILE "${HTML_GZ}"
    RESULT_VARIABLE _gz_result
    ERROR_VARIABLE  _gz_error
)
if(NOT _gz_result EQUAL 0)
    message(FATAL_ERROR "Failed to gzip index.html: ${_gz_error}")
endif()

file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/*.c")
list(TRANSFORM SRC_FILES REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "")

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS ""
                       EMBED_FILES "index.html.gz"
)
