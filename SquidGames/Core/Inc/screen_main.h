#ifndef SCREEN_MAIN_H
#define SCREEN_MAIN_H
#include "lvgl.h"
#include "screen_motors.h"
#include "screen_placeholder.h"
#include "screen_settings.h"
#include "ili9488.h"
#include <stdio.h>
#include <stdlib.h>

//typedef struct {
//    lv_obj_t *container;
//    lv_obj_t *bar;
//    int curr_idx;
//} select_bar;

typedef struct {
    void (*build)(lv_obj_t *parent, void* ctx);
    void (*enter)(void);
    void (*exit)(void);
} screen_def_t;

typedef enum {SCR_MOTORS, SCR_PLACEHOLDER, SCR_SETTINGS, SCR_COUNT} screen_id_t;
//const int SCR_COUNT = 2;

static const screen_def_t screens[SCR_COUNT] = {
    [SCR_MOTORS] = {motors_build, motors_enter, motors_exit},
	[SCR_PLACEHOLDER] = {placeholder_build, placeholder_enter, placeholder_exit},
	[SCR_SETTINGS] = {settings_build, settings_enter, settings_exit}
};

static const screen_id_t bmat_map[5] = {SCR_MOTORS, SCR_SETTINGS, SCR_PLACEHOLDER, SCR_PLACEHOLDER, SCR_PLACEHOLDER};
static const char* screen_labels[SCR_COUNT] = {
		[SCR_MOTORS] = "motor telemetry",
		[SCR_PLACEHOLDER] = "placeholder",
		[SCR_SETTINGS] = "settings"
};

static screen_id_t current = SCR_MOTORS;

void swap_screen(screen_id_t id, void* ctx);
lv_obj_t* screen_select_btnmatrix(void);
void screen_main_init(void);

#endif // SCREEN_MAIN_H
