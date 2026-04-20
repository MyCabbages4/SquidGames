#include "screen_main.h"
#include "ui_theme.h"

static lv_obj_t* active_screen = NULL;	// currently active screen
static lv_obj_t* select_bmat = NULL;	// screen select button matrix
static lv_obj_t* screen_label = NULL;
static bool first = true;

void screen_main_init(void) {
	lv_obj_t* root = lv_scr_act();
	ui_theme_apply_root(root);

	printf("screen_main_init()\n\r");

	// status bar
	select_bmat = screen_select_btnmatrix();
	screen_label = lv_label_create(root);
	lv_label_set_text(screen_label, screen_labels[current]);
	ui_theme_apply_label(screen_label);
    lv_obj_align(screen_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);

	// child screen area
	active_screen = lv_obj_create(root);
	ui_theme_apply_panel(active_screen);
	lv_obj_set_size(active_screen, 480, ILI9488_HEIGHT - 35);
	lv_obj_align(active_screen, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_clear_flag(active_screen, LV_OBJ_FLAG_SCROLLABLE);

	// swap to placeholder 0
	swap_screen(SCR_MOTORS, (void *)(intptr_t)1);
}

void swap_screen(screen_id_t id, void* ctx) {
	if(!first && current == id) return;	// TODO unsure if this is needed or not
	first = false;
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
//    bool prev = id == 0 ? true : false;
//    bool next = id == 6 ? true : false;
//    if(prev || next) {
//        /*Find the checked button*/
//        uint32_t i;
//        for(i = 1; i < 7; i++) {
//            if(lv_btnmatrix_has_btn_ctrl(obj, i, LV_BTNMATRIX_CTRL_CHECKED)) break;
//        }
//
//        if(prev && i > 1) i--;
//        else if(next && i < 5) i++;
//
//        lv_btnmatrix_set_btn_ctrl(obj, i, LV_BTNMATRIX_CTRL_CHECKED);
////        swap_screen(SCR_PLACEHOLDER, (void *)(intptr_t)i);
//    }
//    } else {
//    	swap_screen(SCR_PLACEHOLDER, (void *)(intptr_t)id);
//    }

    id = lv_btnmatrix_get_selected_btn(obj);
    swap_screen(bmat_map[id], (void *)(intptr_t)id);
}

/**
 * Make a button group (pagination)
 */
lv_obj_t* screen_select_btnmatrix(void) {
    static const char * map[] = {"TELEMET", "OPTIONS", ""};

    lv_obj_t * btnm = lv_btnmatrix_create(lv_scr_act());
    lv_btnmatrix_set_map(btnm, map);
    ui_theme_apply_btnmatrix(btnm);
    lv_obj_add_event_cb(btnm, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(btnm, 225, 35);

    lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
//    lv_btnmatrix_clear_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECKABLE);

    lv_btnmatrix_set_one_checked(btnm, true);
    lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECKED);

    lv_obj_set_pos(btnm, ILI9488_WIDTH - 225, ILI9488_HEIGHT - 35);
    return btnm;
}


