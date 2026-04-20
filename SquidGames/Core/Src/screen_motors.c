#include "screen_motors.h"


static float current_left = 0;
static float voltage_left = 0;
static float current_right = 0;
static float voltage_right = 0;

static motor_bar_t bar_voltage_left;
static motor_bar_t bar_current_left;
static motor_bar_t bar_voltage_right;
static motor_bar_t bar_current_right;

static bool bars_active = false;

void motors_set_values(float cl, float vl, float cr, float vr) {
	current_left = cl;
	voltage_left = vl;
	current_right = cr;
	voltage_right = vr;

	if (!bars_active) return;

    motor_bar_set_value(&bar_voltage_left,  voltage_left);
    motor_bar_set_value(&bar_current_left,  current_left);

    motor_bar_set_value(&bar_voltage_right,  voltage_right);
    motor_bar_set_value(&bar_current_right,  current_right);}

void motor_bar_create_leftalign(motor_bar_t *mb, lv_obj_t *parent,
        const char *name, lv_coord_t x, lv_coord_t y, lv_coord_t width,  lv_coord_t height,  float vmin, float vmax) {
	motor_bar_create(mb, parent, name, x, y, width, height, vmin, vmax);
    lv_obj_align(mb->container,  LV_ALIGN_LEFT_MID, -17 + x, y);
    lv_obj_align(mb->name_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(mb->value_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    // direction of the bar
	lv_obj_set_style_base_dir(mb->bar, LV_BASE_DIR_LTR, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mb->bar, lv_color_hex(0x00CC66), LV_PART_INDICATOR);       // start
    lv_obj_set_style_bg_grad_color(mb->bar, lv_color_hex(0xFF3333), LV_PART_INDICATOR);  // end
}

void motor_bar_create_rightalign(motor_bar_t *mb, lv_obj_t *parent,
        const char *name, lv_coord_t x, lv_coord_t y, lv_coord_t width,  lv_coord_t height,  float vmin, float vmax) {
	motor_bar_create(mb, parent, name, x, y, width,height, vmin, vmax);
    lv_obj_align(mb->container,  LV_ALIGN_RIGHT_MID, 17 + x, y);
    lv_obj_align(mb->name_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_align(mb->value_label, LV_ALIGN_TOP_LEFT, 0, 0);
    // direction of the bar
	lv_obj_set_style_base_dir(mb->bar, LV_BASE_DIR_RTL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mb->bar, lv_color_hex(0xFF3333), LV_PART_INDICATOR);       // start
    lv_obj_set_style_bg_grad_color(mb->bar, lv_color_hex(0x00CC66), LV_PART_INDICATOR);  // end
}

void motors_build(lv_obj_t *parent, void* ctx) {
	//	motor_bar_create(&motorV1, lv_scr_act(), "M1 Voltage", 20, 40, -12, 12);
	printf("entering motor screen\n\r");

	motor_bar_create_leftalign(&bar_current_left, parent, "ML Current", 0, 0, 200, 60, 0, 1);
	motor_bar_set_value(&bar_current_left, current_left);
	motor_bar_create_leftalign(&bar_voltage_left, parent, "ML Voltage", 0, 50, 180, 40, -12, 12);
	motor_bar_set_value(&bar_voltage_left, voltage_left);

	motor_bar_create_rightalign(&bar_current_right, parent, "MR Current", 0, 0, 200, 60, 0, 1);
	motor_bar_set_value(&bar_current_right, current_right);
	motor_bar_create_rightalign(&bar_voltage_right, parent, "MR Voltage", 0, 50, 180, 40, -12, 12);
	motor_bar_set_value(&bar_voltage_right, voltage_right);

}

void motors_enter(void) {
//    update_timer = lv_timer_create(refresh_bars, 50, NULL);  // 20 Hz
	bars_active = true;
}

void motors_exit(void) {
	bars_active = false;
//    if (update_timer) { lv_timer_del(update_timer); update_timer = NULL; }
//    bar_left  = NULL;   // LVGL will free the widgets themselves when it
//    bar_right = NULL;   // deletes the old screen — we just drop our refs.
}
