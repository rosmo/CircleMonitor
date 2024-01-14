#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define TAG "CircleMonitor"

// static int s_retry_num = 0;

bool wifi_connected = false;
static int wifi_disconnects = 0;
EventGroupHandle_t wifi_event_group;
static const int CONNECTED_BIT = BIT0;

void sync_time(void)
{
    // Set timezone
    ESP_LOGI(TAG, "Timezone is set to: %s", CONFIG_ESP_NTP_TZ);
    setenv("TZ", CONFIG_ESP_NTP_TZ, 1);
    tzset();
    
    // Synchronize time with NTP
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    ESP_LOGI(TAG, "Getting time via NTP from: %s", CONFIG_ESP_NTP_SERVER);
    esp_sntp_setservername(0, CONFIG_ESP_NTP_SERVER);
    
    const int total_max_retries = 3;
    int total_retries = 0;
    time_t now = 0;
    struct tm timeinfo = { 0 };
    do {
        esp_sntp_init();

        int retry = 0;

        // NTP request is sent every 15 seconds, so wait 25 seconds for sync.
        const int retry_count = 10;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
            ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        if (retry == retry_count) {
            total_retries += 1;
            esp_sntp_stop();
        } else {
            break;
        }
    } while (total_retries < total_max_retries);
    assert(total_retries < total_max_retries);

    time(&now);
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
}

bool wifi_is_connected()
{
    EventBits_t bits;
    bits = xEventGroupGetBits(wifi_event_group);
    if (bits != 0)
    {
        return true;
    } 
    return false;
}

void wifi_disconnection_cb(void *pvParameter) 
{
    wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)pvParameter;
    ESP_LOGE(TAG, "Disconnected from WiFi, reason=%u, rssi=%d", d->reason, d->rssi);
    wifi_disconnects++;
    if (wifi_disconnects > 3) 
    {
        ESP_LOGE(TAG, "Resetting because too many WiFi disconnects.");
        esp_restart();
    }
}

void wifi_connection_cb(void *pvParameter) 
{
    ip_event_got_ip_t* param = (ip_event_got_ip_t*)pvParameter;
    char str_ip[16];

	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);
	ESP_LOGI(TAG, "Connected to WiFi, IP address: %s", str_ip);

    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
}

void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

	wifi_manager_start();
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &wifi_connection_cb);
    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &wifi_disconnection_cb);
}
