#include "ui_theme.h"

void ui_theme_apply_root(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, UI_COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

void ui_theme_apply_panel(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, UI_COL_BG_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, UI_COL_AMBER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, UI_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, UI_PAD, LV_PART_MAIN);
}

void ui_theme_apply_label(lv_obj_t *label) {
    lv_obj_set_style_text_color(label, UI_COL_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, UI_FONT_BODY, LV_PART_MAIN);
}

void ui_theme_apply_label_dim(lv_obj_t *label) {
    lv_obj_set_style_text_color(label, UI_COL_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, UI_FONT_BODY, LV_PART_MAIN);
}

void ui_theme_apply_bar(lv_obj_t *bar) {
    lv_obj_set_style_bg_color(bar, UI_COL_BG_SUNKEN, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, UI_COL_AMBER_DIM, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(bar, UI_COL_AMBER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
}

void ui_theme_apply_button(lv_obj_t *btn) {
    /* idle */
    lv_obj_set_style_bg_color(btn, UI_COL_BG_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, UI_COL_AMBER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, UI_COL_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(btn, UI_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, UI_PAD, LV_PART_MAIN);

    /* pressed — invert: amber fill, black text */
    lv_obj_set_style_bg_color(btn, UI_COL_AMBER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, UI_COL_BG, LV_PART_MAIN | LV_STATE_PRESSED);

    /* checked (for checkable/btnmatrix) */
    lv_obj_set_style_bg_color(btn, UI_COL_AMBER_DIM, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btn, UI_COL_TEXT_HI, LV_PART_MAIN | LV_STATE_CHECKED);

    /* disabled */
    lv_obj_set_style_border_color(btn, UI_COL_AMBER_DIM, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_text_color(btn, UI_COL_TEXT_DIM, LV_PART_MAIN | LV_STATE_DISABLED);
}

void ui_theme_apply_slider(lv_obj_t *slider) {
    /* track */
    lv_obj_set_style_bg_color(slider, UI_COL_BG_SUNKEN, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(slider, UI_COL_AMBER_DIM, LV_PART_MAIN);
    lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 0, LV_PART_MAIN);

    /* fill */
    lv_obj_set_style_bg_color(slider, UI_COL_AMBER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 0, LV_PART_INDICATOR);

    /* knob */
    lv_obj_set_style_bg_color(slider, UI_COL_AMBER_HI, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, UI_COL_BG, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 1, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);

    /* pressed knob — warning amber */
    lv_obj_set_style_bg_color(slider, UI_COL_WARN, LV_PART_KNOB | LV_STATE_PRESSED);
}

void ui_theme_apply_btnmatrix(lv_obj_t *btnm) {
    /* container */
    lv_obj_set_style_bg_color(btnm, UI_COL_BG_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btnm, UI_COL_AMBER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnm, UI_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_radius(btnm, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnm, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(btnm, 0, LV_PART_MAIN);

    /* buttons — idle */
    lv_obj_set_style_bg_color(btnm, UI_COL_BG_PANEL, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(btnm, UI_COL_AMBER_DIM, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(btnm, LV_BORDER_SIDE_INTERNAL, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, UI_COL_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, UI_FONT_BODY, LV_PART_ITEMS);

    /* pressed */
    lv_obj_set_style_bg_color(btnm, UI_COL_AMBER, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btnm, UI_COL_BG, LV_PART_ITEMS | LV_STATE_PRESSED);

    /* checked */
    lv_obj_set_style_bg_color(btnm, UI_COL_AMBER_DIM, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btnm, UI_COL_TEXT_HI, LV_PART_ITEMS | LV_STATE_CHECKED);
}
