#include "screen_settings.h"
#include <stdio.h>
#include "ili9488.h"

static settings_t settings;

static lv_obj_t* settings_label = NULL;
static lv_obj_t* current_slider = NULL;
static lv_obj_t* speed_mult_slider = NULL;
static lv_obj_t* select_bmat = NULL;	// screen select button matrix
//static bool settings_active = false;

lv_obj_t* mode_select_btnmatrix(lv_obj_t *parent);

static void current_slider_cb(lv_event_t *e) {
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    printf("slider: %ld\n\r", v);
    settings.current_limit = v;
	lv_label_set_text_fmt(settings_label,"settings: mode = %d current_limit = %d, speed_mult=%d", settings.mode, (int)settings.current_limit, (int)settings.speed_mult);
}

static void speed_mult_slider_cb(lv_event_t *e) {
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    printf("slider: %ld\n\r", v);
    settings.speed_mult = v;
	lv_label_set_text_fmt(settings_label,"settings: mode = %d current_limit = %d, speed_mult=%d", settings.mode, (int)settings.current_limit, (int)settings.speed_mult);
}

static void event_cb(lv_event_t * e)
{
	printf("button event \n\r");

    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);	// this is how i can access the thing

    settings.mode = id;
	lv_label_set_text_fmt(settings_label,"settings: mode = %d current_limit = %d, speed_mult=%d", settings.mode, (int)settings.current_limit, (int)settings.speed_mult);
}

void settings_build(lv_obj_t *parent, void* ctx) {
	// default settings
	settings = (settings_t){0, 0.5, 0.5};

	// current limit slider
    current_slider = lv_slider_create(parent);
    lv_obj_set_width(current_slider, 200);
    lv_slider_set_range(current_slider, 0, 100);
    lv_slider_set_value(current_slider, 50, LV_ANIM_OFF);
    lv_obj_align(current_slider, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_add_event_cb(current_slider, current_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

	// speed mult slider
    speed_mult_slider = lv_slider_create(parent);
    lv_obj_set_width(speed_mult_slider, 200);
    lv_slider_set_range(speed_mult_slider, 0, 100);
    lv_slider_set_value(speed_mult_slider, 50, LV_ANIM_OFF);
    lv_obj_align(speed_mult_slider, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_add_event_cb(speed_mult_slider, speed_mult_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // mode select button matrix
    select_bmat = mode_select_btnmatrix(parent);

    // test
    settings_label = lv_label_create(parent);
	lv_label_set_text_fmt(settings_label,"settings: mode = %d current_limit = %d, speed_mult=%d", settings.mode, (int)settings.current_limit, (int)settings.speed_mult);
	lv_obj_set_style_text_color(settings_label, lv_color_hex(0x1B1B1B), LV_PART_MAIN);
//	lv_obj_set_style_text_font(screen_label, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_align(settings_label, LV_ALIGN_TOP_MID, 0, 0);
}
void settings_enter(void) {
//	printf("entering screen_placeholder %d\n\r", id);
	return;
}

void settings_exit(void)  {
	return;
}

settings_t settings_get(void) {
	// NOTE: the settings are always accessible with their previous values
	return settings;
}

lv_obj_t* mode_select_btnmatrix(lv_obj_t *parent) {
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_pad_all(&style_bg, 0);
    lv_style_set_pad_gap(&style_bg, 0);
    lv_style_set_clip_corner(&style_bg, true);
//    lv_style_set_radius(&style_bg, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_bg, 0);

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 0);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_border_opa(&style_btn, LV_OPA_50);
    lv_style_set_border_color(&style_btn, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_side(&style_btn, LV_BORDER_SIDE_INTERNAL);
    lv_style_set_radius(&style_btn, 0);

    static const char * map[] = {"mode 1", "mode 2", "mode 3", "mode 4", "mode 5", ""};

    lv_obj_t * btnm = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(btnm, map);
    lv_obj_add_style(btnm, &style_bg, 0);
    lv_obj_add_style(btnm, &style_btn, LV_PART_ITEMS);
    lv_obj_add_event_cb(btnm, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(btnm, 225, 35);
//    lv_obj_align(btnm, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /*Allow selecting on one number at time*/
    lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_clear_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_clear_btn_ctrl(btnm, 6, LV_BTNMATRIX_CTRL_CHECKABLE);

    lv_btnmatrix_set_one_checked(btnm, true);
    lv_btnmatrix_set_btn_ctrl(btnm, 1, LV_BTNMATRIX_CTRL_CHECKED);

//    lv_obj_center(btnm);
//    lv_obj_set_pos(btnm, ILI9488_WIDTH - 225, ILI9488_HEIGHT - 35);
    lv_obj_align(btnm, LV_ALIGN_CENTER, 0, 50);

    return btnm;
}
