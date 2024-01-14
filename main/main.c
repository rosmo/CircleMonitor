#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "wifi.h"
#include <math.h>
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_mac.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "esp_http_client.h"

#include "gfx.h"
#include "sensors.h"

#define TAG "CircleMonitor"
#define TRUE (1==1)
#define FALSE (!TRUE)
#define uS_TO_S_FACTOR (uint64_t)1000000

void chip_info(void)
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("Silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }
    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

void app_main(void)
{
    printf("CircleMonitor\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_gfx();

    ESP_LOGD(TAG, "Starting UI...");
    init_ui();
    run_ui(NULL);

    ESP_LOGD(TAG, "Initialize WiFi...");
    wifi_init();

    ESP_LOGD(TAG, "Synchronizing time...");
    // sync_time();
 
    ESP_LOGD(TAG, "Updating sensors...");
    update_sensors();
    keep_updating_sensors();
 

    fflush(stdout);
}
 