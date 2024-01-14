idf_component_register(SRCS "main.c" "wifi.c" "gfx.c" "sensors.c"
                    INCLUDE_DIRS ""
                    REQUIRES mdns hal driver esp_timer esp_wifi spi_flash nvs_flash esp_http_client lvgl_esp32_drivers json)