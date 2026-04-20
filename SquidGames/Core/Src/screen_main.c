#include "screen_main.h"

static lv_obj_t* active_screen = NULL;	// currently active screen
static lv_obj_t* select_bmat = NULL;	// screen select button matrix
static lv_obj_t* screen_label = NULL;

void screen_main_init(void) {
	lv_obj_t* root = lv_scr_act();
	lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x2b2700), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root,   LV_OPA_COVER,           LV_PART_MAIN);

	printf("screen_main_init()\n\r");

	// status bar
	select_bmat = screen_select_btnmatrix();
	screen_label = lv_label_create(root);
	lv_label_set_text(screen_label, screen_labels[current]);
	lv_obj_set_style_text_color(screen_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
//	lv_obj_set_style_text_font(screen_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(screen_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);

	// child screen area
	active_screen = lv_obj_create(root);
	lv_obj_set_size(active_screen, 480, ILI9488_HEIGHT - 35);
	lv_obj_align(active_screen, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_clear_flag(active_screen, LV_OBJ_FLAG_SCROLLABLE);

	// swap to placeholder 0
	swap_screen(SCR_MOTORS, (void *)(intptr_t)1);
}

void swap_screen(screen_id_t id, void* ctx) {
//	if(current == id) return;	// TODO unsure if this is needed or not
	printf("swapping to %d\n\r", id);

	screens[current].exit();
	lv_obj_clean(active_screen);

	screens[id].build(active_screen, ctx);
	current = id;
	screens[id].enter();
	lv_label_set_text(screen_label, screen_labels[current]);
}

static void event_cb(lv_event_t * e)
{
	printf("button event \n\r");

    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);	// this is how i can access the thing
    bool prev = id == 0 ? true : false;
    bool next = id == 6 ? true : false;
    if(prev || next) {
        /*Find the checked button*/
        uint32_t i;
        for(i = 1; i < 7; i++) {
            if(lv_btnmatrix_has_btn_ctrl(obj, i, LV_BTNMATRIX_CTRL_CHECKED)) break;
        }

        if(prev && i > 1) i--;
        else if(next && i < 5) i++;

        lv_btnmatrix_set_btn_ctrl(obj, i, LV_BTNMATRIX_CTRL_CHECKED);
//        swap_screen(SCR_PLACEHOLDER, (void *)(intptr_t)i);
    }
//    } else {
//    	swap_screen(SCR_PLACEHOLDER, (void *)(intptr_t)id);
//    }

    id = lv_btnmatrix_get_selected_btn(obj);
    swap_screen(bmat_map[id-1], (void *)(intptr_t)id);
}

/**
 * Make a button group (pagination)
 */
lv_obj_t* screen_select_btnmatrix(void) {
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

    static const char * map[] = {LV_SYMBOL_LEFT, "1", "2", "3", "4", "5", LV_SYMBOL_RIGHT, ""};

    lv_obj_t * btnm = lv_btnmatrix_create(lv_scr_act());
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
    lv_obj_set_pos(btnm, ILI9488_WIDTH - 225, ILI9488_HEIGHT - 35);

    return btnm;
}


