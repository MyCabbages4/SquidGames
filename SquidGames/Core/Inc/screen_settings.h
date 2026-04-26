#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>

typedef enum{
	BASIC,
	ADV,
	PULL,
	BLUETOOTH,
	EXPLORE,
} mode_t;

typedef struct {
	mode_t mode;
	float current_limit;
	float speed_mult;
} settings_t;


void settings_build(lv_obj_t *parent, void* ctx);
void settings_enter(void);
void settings_exit(void);
settings_t settings_get(void);

#endif // SCREEN_SETTINGS_H
