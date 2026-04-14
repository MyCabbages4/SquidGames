#ifndef MOTOR_UI_H
#define MOTOR_UI_H

#include "lvgl.h"

// Max voltage for the bar range
//#define MOTOR_VOLTAGE_MAX  12.0f
//#define MOTOR_VOLTAGE_MIN   0.0f

// Widget handle
typedef struct {
    lv_obj_t *container;
    lv_obj_t *bar;
    lv_obj_t *name_label;
    lv_obj_t *value_label;
    float val_min;
    float val_max;
} motor_bar_t;

void motor_bar_create(motor_bar_t *mb, lv_obj_t *parent,
                      const char *name, lv_coord_t x, lv_coord_t y, float vmin, float vmax);
void motor_bar_set_voltage(motor_bar_t *mb, float value);

#endif // MOTOR_UI_H
