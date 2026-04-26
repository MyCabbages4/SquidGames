#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

/* amber palette */
#define UI_COL_BG         lv_color_hex(0x000000)
#define UI_COL_BG_PANEL   lv_color_hex(0x0A0A0A)
#define UI_COL_BG_SUNKEN  lv_color_hex(0x1A0F00)

#define UI_COL_AMBER      lv_color_hex(0xFF9900)
#define UI_COL_AMBER_HI   lv_color_hex(0xFFCC33)
#define UI_COL_AMBER_DIM  lv_color_hex(0x996600)

#define UI_COL_TEXT       lv_color_hex(0xFFAA33)
#define UI_COL_TEXT_DIM   lv_color_hex(0x805500)
#define UI_COL_TEXT_HI    lv_color_hex(0xFFEEAA)

#define UI_COL_WARN       lv_color_hex(0xFF3300)

#define UI_FONT_BODY      (&lv_font_montserrat_14)
#define UI_FONT_TITLE     (&lv_font_montserrat_14)

#define UI_RADIUS         0
#define UI_BORDER_W       1
#define UI_PAD            6

void ui_theme_apply_root(lv_obj_t *obj);
void ui_theme_apply_panel(lv_obj_t *obj);
void ui_theme_apply_label(lv_obj_t *label);
void ui_theme_apply_label_dim(lv_obj_t *label);
void ui_theme_apply_bar(lv_obj_t *bar);
void ui_theme_apply_button(lv_obj_t *btn);
void ui_theme_apply_slider(lv_obj_t *slider);
void ui_theme_apply_btnmatrix(lv_obj_t *btnm);

#endif
