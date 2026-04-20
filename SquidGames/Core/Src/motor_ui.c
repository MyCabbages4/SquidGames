#include "motor_ui.h"

static const float RESOLUTION_MULT = 10000;

void motor_bar_create(motor_bar_t *mb, lv_obj_t *parent,
                      const char *name, lv_coord_t x, lv_coord_t y, lv_coord_t width,  lv_coord_t height,  float vmin, float vmax) {
	printf("creating motor bar\n\r");

	mb->val_max = vmax;
	mb->val_min = vmin;

//	printf("parent=%p\n\r", (void*)parent);
//	lv_mem_monitor_t m; lv_mem_monitor(&m);
//	printf("free=%u biggest=%u\n\r", (unsigned)m.free_size, (unsigned)m.free_biggest_size);
    // Container
    mb->container = lv_obj_create(parent);
    lv_obj_set_size(mb->container, width, height);
    lv_obj_set_pos(mb->container, x, y);
    lv_obj_set_style_bg_opa(mb->container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mb->container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mb->container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(mb->container, LV_OBJ_FLAG_SCROLLABLE);

    // Motor name label (top left)
    mb->name_label = lv_label_create(mb->container);
    lv_label_set_text(mb->name_label, name);
    lv_obj_set_style_text_color(mb->name_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
//    lv_obj_set_style_text_font(mb->name_label, &lv_font_montserrat_14, LV_PART_MAIN);
//    lv_obj_align(mb->name_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // Voltage value label (top right)
    mb->value_label = lv_label_create(mb->container);
    lv_label_set_text(mb->value_label, "0.00");
//	lv_label_set_text_fmt(mb->value_label, "VALUE %d.%d", 0, frac2);
    lv_obj_set_style_text_color(mb->value_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
//    lv_obj_set_style_text_font(mb->value_label, &lv_font_montserrat_14, LV_PART_MAIN);
//    lv_obj_align(mb->value_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    // The bar itself
    mb->bar = lv_bar_create(mb->container);
    lv_obj_set_size(mb->bar, width, height - 20);
    lv_obj_align(mb->bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(mb->bar,
                     (int32_t)(mb->val_min * RESOLUTION_MULT),
                     (int32_t)(mb->val_max * RESOLUTION_MULT));
    lv_bar_set_value(mb->bar, 0, LV_ANIM_OFF);

    // Bar background (unfilled) — dark grey track
    lv_obj_set_style_bg_color(mb->bar, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mb->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(mb->bar, 5, LV_PART_MAIN);

    // Bar indicator (filled) — gradient green to red
    lv_obj_set_style_bg_color(mb->bar, lv_color_hex(0x00CC66), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(mb->bar, lv_color_hex(0xFF3333), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(mb->bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(mb->bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(mb->bar, 5, LV_PART_INDICATOR);
}

//void motor_bar_create_leftalign(motor_bar_t *mb, lv_obj_t *parent,
//        const char *name, lv_coord_t x, lv_coord_t y, lv_coord_t width,  lv_coord_t height,  float vmin, float vmax) {
//	motor_bar_create(mb, parent, name, x, y, width,height, vmin, vmax);
//    lv_obj_align(mb->name_label, LV_ALIGN_TOP_LEFT, 0, 0);
//    lv_obj_align(mb->value_label, LV_ALIGN_TOP_RIGHT, 0, 0);
//    lv_obj_align(mb->container,  LV_ALIGN_LEFT_MID, -18, 0);
////    lv_obj_align(bar_right.container, LV_ALIGN_RIGHT_MID, 18, 0);
//}

//void motor_bar_create_rightalign(motor_bar_t *mb, lv_obj_t *parent,
//        const char *name, lv_coord_t x, lv_coord_t y, lv_coord_t width,  lv_coord_t height,  float vmin, float vmax) {
//	motor_bar_create(mb, parent, name, x, y, width,height, vmin, vmax);
//    lv_obj_align(mb->name_label, LV_ALIGN_TOP_RIGHT, 0, 0);
//    lv_obj_align(mb->value_label, LV_ALIGN_LEFT_MID, 0, 0);
//    lv_obj_align(mb->container,  LV_ALIGN_LEFT_MID, -18, 0);
////    lv_obj_align(bar_right.container, LV_ALIGN_RIGHT_MID, 18, 0);
//}
void motor_bar_set_value(motor_bar_t *mb, float value) {
//    printf("%f value!!", value);

    if (value < mb->val_min) value = mb->val_min;
    if (value > mb->val_max) value = mb->val_max;

    lv_bar_set_value(mb->bar, (int32_t)(value * RESOLUTION_MULT), LV_ANIM_OFF);
    int whole = (int)value;
//	int frac1  = abs(((int)(value * 10)) % 10);
	int frac2  = abs(((int)(value * 100)) % 100);
	lv_label_set_text_fmt(mb->value_label, "%d.%d", whole, frac2);
}
