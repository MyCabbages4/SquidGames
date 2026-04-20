#include "motor_ui.h"
#include "ui_theme.h"

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

    // Motor name label
    mb->name_label = lv_label_create(mb->container);
    lv_label_set_text(mb->name_label, name);
    ui_theme_apply_label_dim(mb->name_label);

    // Voltage value label
    mb->value_label = lv_label_create(mb->container);
    lv_label_set_text(mb->value_label, "0.00");
    ui_theme_apply_label(mb->value_label);

    // The bar itself
    mb->bar = lv_bar_create(mb->container);
    lv_obj_set_size(mb->bar, width, height - 20);
    lv_obj_align(mb->bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(mb->bar,
                     (int32_t)(mb->val_min * RESOLUTION_MULT),
                     (int32_t)(mb->val_max * RESOLUTION_MULT));
    lv_bar_set_value(mb->bar, 0, LV_ANIM_OFF);

    ui_theme_apply_bar(mb->bar);
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
	char buf[10];
	sprintf(buf, "%.3f", value);
	lv_label_set_text(mb->value_label, buf);
}
