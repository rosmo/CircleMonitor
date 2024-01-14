#ifndef _GFX_H
#define _GFX_H

void init_gfx(void);
void init_ui(void);
void run_ui(void *pvParameter);

extern SemaphoreHandle_t xGuiSemaphore;

#endif