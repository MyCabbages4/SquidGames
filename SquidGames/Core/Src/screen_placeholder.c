#include "screen_placeholder.h"
#include <stdio.h>
#include "ui_theme.h"

static lv_obj_t *label;
static int id;

void placeholder_build(lv_obj_t *parent, void* ctx) {
    int id = (int)(intptr_t)ctx;

	// Voltage value label (top right)
	label = lv_label_create(parent);
	lv_label_set_text_fmt(label, "SCREEN #%d", id);
//	lv_obj_set_style_text_color(label, lv_color_hex(0x1B1B1B), LV_PART_MAIN);
//	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
	ui_theme_apply_label(label);

	lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
}
void placeholder_enter(void) {
	printf("entering screen_placeholder %d\n\r", id);
	return;
}

void placeholder_exit(void)  {
	return;
}
