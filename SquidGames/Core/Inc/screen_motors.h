#ifndef SCREEN_MOTORS_H
#define SCREEN_MOTORS_H

#include "lvgl.h"
#include "screen_motors.h"
#include "motor_ui.h"
#include <stdio.h>
#include <stdlib.h>

void motors_build(lv_obj_t *parent, void* ctx);
void motors_enter(void);
void motors_exit(void);
void motors_set_values(float current_left, float voltage_left, float current_right, float voltage_right);

#endif // SCREEN_MOTORS_H
