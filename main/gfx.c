#include "hal/spi_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

#include "lvgl_helpers.h"
#include "sensors.h"
#include "wifi.h"

#include "poppins_medium_20.h"
#include "poppins_medium_56.h"

#define LV_TICK_PERIOD_MS 1
#define TAG "CircleMonitor"

SemaphoreHandle_t xGuiSemaphore;

static lv_style_t value_style;
static lv_style_t label_style;
static lv_style_t screen_style;
static lv_obj_t *default_screen;
static lv_obj_t *default_label;
static lv_obj_t *init_screen;
static lv_obj_t *init_label;

lv_color_t *buf1 = NULL;

void tickInc(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

void init_gfx(void)
{
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &tickInc,
        .name = "lvgl_tick_inc",
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    // Display buffer has to be under 4KB due to SPI max transfer size
    size_t display_buffer_size = 4000;
    buf1 = (lv_color_t *)heap_caps_malloc(display_buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, display_buffer_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.rotated = LV_DISP_ROT_270;
    lv_disp_drv_register(&disp_drv);

    lvgl_interface_init();
    lvgl_display_gpios_init();
    disp_driver_init(&disp_drv);

    ESP_LOGI(TAG, "Display init done.");
}

void init_ui(void)
{
    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, lv_color_black());

    lv_style_init(&label_style);
    lv_style_set_text_font(&label_style, &poppins_medium_20);
    lv_style_set_text_color(&label_style, lv_color_white());
    lv_style_set_text_align(&label_style, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&value_style);
    lv_style_set_text_font(&value_style, &poppins_medium_56);
    lv_style_set_text_color(&value_style, lv_color_white());

    sensor **sensors = NULL;
    int num_sensors = 0;
    bool update_failed = false;
    sensors = get_sensors(&num_sensors, &update_failed);
    for (int i = 0; i < num_sensors; i++)
    {
        if (i == 0)
        {
            sensors[i]->scr = lv_scr_act();
        }
        else
        {
            sensors[i]->scr = lv_obj_create(NULL);
        }
        lv_obj_add_style(sensors[i]->scr, &screen_style, 0);

        lv_obj_t *text_label = lv_label_create(sensors[i]->scr);
        lv_label_set_text(text_label, sensors[i]->text);
        lv_obj_align(text_label, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_style(text_label, &label_style, 0);
        lv_obj_set_style_opa(text_label, LV_OPA_40, 0);

        lv_obj_set_style_pad_top(text_label, 40, LV_PART_MAIN);

        sensors[i]->label = lv_label_create(sensors[i]->scr);
        lv_label_set_text(sensors[i]->label, sensors[i]->value);
        lv_obj_align(sensors[i]->label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(sensors[i]->label, &value_style, 0);
        lv_label_set_recolor(sensors[i]->label, 1);
    }
    default_screen = lv_obj_create(NULL);
    lv_obj_add_style(default_screen, &screen_style, 0);

    default_label = lv_label_create(default_screen);
    lv_label_set_text(default_label, "Waiting for\nsensor data...");
    lv_obj_align(default_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(default_label, &label_style, 0);
    lv_obj_set_style_opa(default_label, LV_OPA_30, 0);

    init_screen = lv_obj_create(NULL);
    lv_obj_add_style(init_screen, &screen_style, 0);

    char init_label_text[128] = { '\0' };
    snprintf(init_label_text, sizeof(init_label_text), "Connect to:\n%s\n(pw: %s)", CONFIG_DEFAULT_AP_SSID, CONFIG_DEFAULT_AP_PASSWORD);
    init_label = lv_label_create(init_screen);
    lv_label_set_text(init_label, init_label_text);
    lv_obj_align(init_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(init_label, &label_style, 0);
    lv_obj_set_style_opa(init_label, LV_OPA_30, 0);
}

void ui_rotate_task(void *pvParameter)
{
    (void)pvParameter;
    int screen_index = 1;
    TickType_t start_ticks, ticks;
    sensor **sensors = NULL;
    int num_sensors = 0;
    bool update_failed = false;
    bool wifi_connected = wifi_is_connected();
    sensors = get_sensors(&num_sensors, &update_failed);

    if (!wifi_connected)
    {
        vTaskDelay(pdMS_TO_TICKS(250));
        wifi_connected = wifi_is_connected();
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_scr_load(init_screen);
            xSemaphoreGive(xGuiSemaphore);
        }
    }
        
    start_ticks = xTaskGetTickCount();
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(50));

        ticks = xTaskGetTickCount();
        if (pdTICKS_TO_MS(ticks - start_ticks) > 3300 || update_failed)
        {
            start_ticks = ticks;
            if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
            {
                if (update_failed) 
                {
                    if (lv_scr_act() != default_screen)
                    {
                        ESP_LOGI(TAG, "Update failed, showing default screen.");
                        lv_scr_load(default_screen);
                    }
                } else {
                    for (int i = 0; i < num_sensors; i++)
                    {
                        lv_label_set_text(sensors[i]->label, sensors[i]->value);
                        lv_label_set_recolor(sensors[i]->label, 1);
                    }
                    lv_scr_load_anim(sensors[screen_index]->scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 300, false);
                    screen_index++;
                    if (screen_index == num_sensors)
                    {
                        screen_index = 0;
                    }
                }
                xSemaphoreGive(xGuiSemaphore);
            }
        }
    }
}

void ui_task(void *pvParameter)
{
    (void)pvParameter;

    while (1)
    {
        /* Delay 15ms (approximately 60fps) */
        vTaskDelay(pdMS_TO_TICKS(15));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
    /* A task should NEVER return */
    free(buf1);
    vTaskDelete(NULL);
}

void run_ui(void)
{
    xTaskCreatePinnedToCore(ui_task, "gui", 4096 * 2, NULL, 0, NULL, 0);
    xTaskCreatePinnedToCore(ui_rotate_task, "carousel", 4096 * 2, NULL, 0, NULL, 0);
}
