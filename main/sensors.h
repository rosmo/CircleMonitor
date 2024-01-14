#ifndef _SENSORS_H
#define _SENSORS_H
#include "lvgl.h"
#include <stdbool.h>

typedef struct _threshold
{
    double start;
    double end;
    char *fmt;
} threshold;

typedef struct _sensor
{
    char *path;
    char *text;
    char value[32];
    lv_obj_t *scr;
    lv_obj_t *label;
    threshold fmts[5];
} sensor;

typedef sensor sensor_array[];

esp_err_t update_sensors(void);
sensor **get_sensors(int *num_sensors, bool *update_failed_flag);
void keep_updating_sensors(void);

#endif