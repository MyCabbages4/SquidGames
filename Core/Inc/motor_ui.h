// ============================================================
// Motor Voltage Bar Widget — Proof of Concept
// Drop this into your main.c or a separate motor_ui.c/h
// ============================================================
//
// Usage:
//   motor_bar_t motor1;
//   motor_bar_create(&motor1, lv_scr_act(), "Motor 1", 10, 30);
//   motor_bar_set_voltage(&motor1, 7.2f);  // update with real ADC value
//

#ifndef MOTOR_UI_H
#define MOTOR_UI_H

#include "lvgl.h"

// Max voltage for the bar range (adjust to your motor's operating range)
#define MOTOR_VOLTAGE_MAX  12.0f
#define MOTOR_VOLTAGE_MIN   0.0f

// Widget handle — holds references to the bar and label
typedef struct {
    lv_obj_t *container;
    lv_obj_t *bar;
    lv_obj_t *name_label;
    lv_obj_t *value_label;
} motor_bar_t;

/**
 * Create a motor voltage bar at position (x, y) on the given parent.
 *
 * @param mb       Pointer to a motor_bar_t struct (you allocate, we fill in)
 * @param parent   LVGL parent object (e.g. lv_scr_act())
 * @param name     Display name, e.g. "Motor 1"
 * @param x, y     Position on screen
 */
static inline void motor_bar_create(motor_bar_t *mb, lv_obj_t *parent,
                                     const char *name, lv_coord_t x, lv_coord_t y) {

    // Container — groups everything together for easy positioning
    mb->container = lv_obj_create(parent);
    lv_obj_set_size(mb->container, 280, 60);
    lv_obj_set_pos(mb->container, x, y);
    lv_obj_set_style_bg_opa(mb->container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mb->container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mb->container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(mb->container, LV_OBJ_FLAG_SCROLLABLE);

    // Motor name label (top left)
    mb->name_label = lv_label_create(mb->container);
    lv_label_set_text(mb->name_label, name);
    lv_obj_set_style_text_color(mb->name_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(mb->name_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(mb->name_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // Voltage value label (top right)
    mb->value_label = lv_label_create(mb->container);
    lv_label_set_text(mb->value_label, "0.0 V");
    lv_obj_set_style_text_color(mb->value_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(mb->value_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(mb->value_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    // The bar itself
    mb->bar = lv_bar_create(mb->container);
    lv_obj_set_size(mb->bar, 270, 20);
    lv_obj_align(mb->bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(mb->bar,
                     (int32_t)(MOTOR_VOLTAGE_MIN * 10),
                     (int32_t)(MOTOR_VOLTAGE_MAX * 10));  // use 0.1V resolution
    lv_bar_set_value(mb->bar, 0, LV_ANIM_OFF);

    // Style the bar background (unfilled portion) — dark grey track
    lv_obj_set_style_bg_color(mb->bar, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mb->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(mb->bar, 5, LV_PART_MAIN);

    // Style the bar indicator (filled portion) — gradient from green to red
    // Green at 0V (safe) → Yellow at mid → Red at max (high voltage)
    lv_obj_set_style_bg_color(mb->bar, lv_color_hex(0x00CC66), LV_PART_INDICATOR);     // start: green
    lv_obj_set_style_bg_grad_color(mb->bar, lv_color_hex(0xFF3333), LV_PART_INDICATOR); // end: red
    lv_obj_set_style_bg_grad_dir(mb->bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(mb->bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(mb->bar, 5, LV_PART_INDICATOR);
}

/**
 * Update the voltage display for a motor bar.
 *
 * @param mb       Pointer to the motor_bar_t
 * @param voltage  Voltage in volts (e.g. 7.2)
 */
static inline void motor_bar_set_voltage(motor_bar_t *mb, float voltage) {
    // Clamp to range
    if (voltage < MOTOR_VOLTAGE_MIN) voltage = MOTOR_VOLTAGE_MIN;
    if (voltage > MOTOR_VOLTAGE_MAX) voltage = MOTOR_VOLTAGE_MAX;

    // Update bar (using 0.1V resolution — multiply by 10)
    lv_bar_set_value(mb->bar, (int32_t)(voltage * 10), LV_ANIM_OFF);

    // Update the text label
    lv_label_set_text_fmt(mb->value_label, "%.1f V", voltage);
}

#endif // MOTOR_UI_H
