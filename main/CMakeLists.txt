
file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/*.c")
list(TRANSFORM SRC_FILES REPLACE "${CMAKE_CURRENT_LIST_DIR}/" "")
file(GLOB WEB_FILES "../web/*")

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES
    driver
    json
    esp_wifi
    esp_netif
    esp_event
    esp_http_server
    esp_http_client
    esp_https_ota
    app_update
    nvs_flash
    mqtt
    bt
    esp_timer
    esp_system
    freertos
    log
    # embed the web files into the binary
    EMBED_FILES ${WEB_FILES}
)
