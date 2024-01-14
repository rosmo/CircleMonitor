#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "sensors.h"
#include "gfx.h"

#define MAX_HTTP_OUTPUT_BUFFER 1024
#define TAG "CircleMonitor"

sensor_array sensors = {
    {.path = "/lpc/nct6687d/0/temperature/6", .text = "Coolant temp", .scr = NULL, .fmts = {{
                                                                                      .start = 0.0,
                                                                                      .end = 40.0,
                                                                                      .fmt = "#238823 %.1f# °C",
                                                                                  },
                                                                                  {
                                                                                      .start = 40.0,
                                                                                      .end = 55.0,
                                                                                      .fmt = "#ffbf00 %.1f# °C"
                                                                                  },
                                                                                  {
                                                                                      .start = 55.0,
                                                                                      .end = 100000.0,
                                                                                      .fmt = "#d2222d %.1f# °C"
                                                                                  },
                                                                                  {.fmt = NULL}}},
    {.path = "/amdcpu/0/power/0",
     .text = "CPU power",
     .scr = NULL,
     .fmts = {
         {
             .start = 0.0,
             .end = 1000000.0,
             .fmt = "%.1f W",
         },
         {.fmt = NULL}}},
    {.path = "/amdcpu/0/temperature/2", .text = "CPU temp", .scr = NULL, .fmts = {{
                                                                                      .start = 0.0,
                                                                                      .end = 50.0,
                                                                                      .fmt = "#238823 %.1f# °C",
                                                                                  },
                                                                                  {
                                                                                      .start = 50.0,
                                                                                      .end = 65.0,
                                                                                      .fmt = "#ffbf00 %.1f# °C"
                                                                                  },
                                                                                  {
                                                                                      .start = 65.0,
                                                                                      .end = 100000.0,
                                                                                      .fmt = "#d2222d %.1f# °C"
                                                                                  },
                                                                                  {.fmt = NULL}}},
    {.path = "/gpu-nvidia/0/power/0", .text = "GPU power", .scr = NULL, .fmts = {{
                                                                                     .start = 0.0,
                                                                                     .end = 1000000.0,
                                                                                     .fmt = "%.1f W",
                                                                                 },
                                                                                 {.fmt = NULL}}},
    {.path = "/gpu-nvidia/0/temperature/0", .text = "GPU temp", .scr = NULL, .fmts = {{
                                                                                          .start = 0.0,
                                                                                          .end = 50.0,
                                                                                          .fmt = "#238823 %.1f# °C",
                                                                                      },
                                                                                      {
                                                                                          .start = 50.0,
                                                                                          .end = 65.0,
                                                                                          .fmt = "#ffbf00 %.1f# °C",
                                                                                      },
                                                                                      {
                                                                                          .start = 65.0,
                                                                                          .end = 100000.0,
                                                                                          .fmt = "#d2222d %.1f# °C",
                                                                                      },
                                                                                      {.fmt = NULL}}},
};
bool update_failed = true;

sensor **get_sensors(int *num_sensors, bool *update_failed_flag)
{
    sensor **s = calloc(sizeof(sensors) / sizeof(sensor), sizeof(sensor *));
    *num_sensors = sizeof(sensors) / sizeof(sensor);
    *update_failed_flag = update_failed;
    for (int i = 0; i < *num_sensors; i++)
    {
        s[i] = &sensors[i];
    }
    return s;
}

/*
void walk_sensors(const cJSON *node) {
    const cJSON *child = NULL;
    const cJSON *children = NULL;

    children = cJSON_GetObjectItemCaseSensitive(node, "Children");
    if (children == NULL || !cJSON_IsArray(children)) {
        ESP_LOGI(TAG, "No children for node %p", node);
        return;
    }
    ESP_LOGI(TAG, "Looping children for     %p", node);
    cJSON_ArrayForEach(child, children)
    {
        cJSON *sensor_id = cJSON_GetObjectItemCaseSensitive(child, "sensorid");
        cJSON *value = cJSON_GetObjectItemCaseSensitive(child, "Value");
        if (sensor_id != NULL && cJSON_IsString(sensor_id) && value != NULL && cJSON_IsString(value)) {
            for (int i = 0; i < sizeof(sensors)/sizeof(sensor); i++) {
                if (strcmp(sensors[i].path, sensor_id->valuestring) == 0) {
                    strncpy(sensors[i].value, value->valuestring, sizeof(sensors[i].value)-1);
                    ESP_LOGI(TAG, "Sensor %s (%s): %s", sensors[i].text, sensors[i].path, sensors[i].value);
                }
            }
        }
        walk_sensors(child);
    }
}
*/

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
    case HTTP_EVENT_ON_CONNECTED:
    case HTTP_EVENT_HEADER_SENT:
    case HTTP_EVENT_ON_HEADER:
        break;
    case HTTP_EVENT_ON_DATA:
        // Clean the buffer in case of a new request
        if (output_len == 0 && evt->user_data)
        {
            // we are just starting to copy the output data into the use
            memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
        }
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            if (evt->user_data)
            {
                // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len)
                {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            }
            else
            {
                int content_len = esp_http_client_get_content_length(evt->client);
                if (output_buffer == NULL)
                {
                    // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                    output_buffer = (char *)calloc(content_len + 1, sizeof(char));
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                copy_len = MIN(evt->data_len, (content_len - output_len));
                if (copy_len)
                {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                }
            }
            output_len += copy_len;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        if (output_buffer != NULL)
        {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

/*
esp_err_t update_sensors(void)
{
    char *local_response_buffer = NULL;
    local_response_buffer = (char *)malloc(MAX_HTTP_OUTPUT_BUFFER);
    if (local_response_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate response buffer: %d", MAX_HTTP_OUTPUT_BUFFER);
        return ESP_FAIL;
    }
    esp_err_t err;
    ESP_LOGI(TAG, "Fetching sensors from: %s", CONFIG_ESP_LHWM_URL);
    esp_http_client_config_t config = {
        .url = CONFIG_ESP_LHWM_URL,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRIu64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));

        ESP_LOGI(TAG, "Parsing JSON with length: %d", strlen(local_response_buffer));
        cJSON *root = cJSON_Parse(local_response_buffer);
        if (root == NULL) {
            ESP_LOGE(TAG, "JSON parsing failed: %s", cJSON_GetErrorPtr());
        }
        cJSON_Print(root);
        walk_sensors(root);
        cJSON_Delete(root);
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    return err;
}
*/

esp_err_t update_sensor(sensor *s)
{
    char *local_response_buffer = NULL;
    char url[256] = {'\0'};
    local_response_buffer = (char *)malloc(MAX_HTTP_OUTPUT_BUFFER);
    if (local_response_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate response buffer: %d", MAX_HTTP_OUTPUT_BUFFER);
        return ESP_FAIL;
    }
    esp_err_t err;
    sprintf(url, "%s%s", CONFIG_ESP_LHWM_URL, s->path);
    ESP_LOGI(TAG, "Fetching sensor from: %s", url);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,
        .timeout_ms = 2000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %" PRIu64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        cJSON *root = cJSON_Parse(local_response_buffer);
        cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            if (cJSON_IsNumber(value))
            {
                double numval = cJSON_GetNumberValue(value);
                int i = 0;
                do
                {
                    if (s->fmts[i].fmt == NULL)
                    {
                        break;
                    }
                    if (numval >= s->fmts[i].start && numval <= s->fmts[i].end)
                    {
                        snprintf(s->value, sizeof(s->value), s->fmts[i].fmt, numval);
                        ESP_LOGI(TAG, "Sensor %s: value is %.1f, fmt is: %s, result: %s\n", s->path, numval, s->fmts[i].fmt, s->value);
                        break;
                    }
                    i++;
                } while (i < 5);
            }
            if (cJSON_IsString(value))
            {
                strncpy(s->value, value->valuestring, sizeof(s->value));
            }
            xSemaphoreGive(xGuiSemaphore);
        }
        cJSON_Delete(root);
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    free(local_response_buffer);
    esp_http_client_cleanup(client);

    return err;
}

esp_err_t update_sensors(void)
{
    esp_err_t err;
    ESP_LOGI(TAG, "Updating all sensors...");
    for (int i = 0; i < sizeof(sensors) / sizeof(sensor); i++)
    {
        err = update_sensor(&sensors[i]);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Updating sensor %s returned an error.", sensors[i].path);
            update_failed = true;
            return err;
        }
    }
    update_failed = false;
    return ESP_OK;
}

void run_sensors(void *pvParameter)
{
    (void)pvParameter;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        update_sensors();
    }
    vTaskDelete(NULL);
}

void keep_updating_sensors(void)
{
    xTaskCreatePinnedToCore(run_sensors, "sensors", 4096 * 2, NULL, 0, NULL, 0);
}
