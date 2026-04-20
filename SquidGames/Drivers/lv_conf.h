/**
 * @file lv_conf.h
 * Minimal LVGL v8.3 configuration for ILI9488 on STM32L4R5ZI-P
 *
 * Place this file BESIDE the lvgl/ folder (not inside it).
 * e.g.:  Drivers/lv_conf.h
 *        Drivers/lvgl/
 */

#if 1 /* Set to 1 to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565) — LVGL renders internally in this format.
 * The flush callback converts to RGB666 for the ILI9488. */
#define LV_COLOR_DEPTH     16

/* Swap the 2 bytes of RGB565 color. 0 = no swap (correct for our SPI flush) */
#define LV_COLOR_16_SWAP    0

/* Chroma key color for transparency (default green) */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*====================
   MEMORY SETTINGS
 *====================*/

/* Size of the memory available for lv_mem_alloc() in bytes (>= 2KB)
 * 32KB is comfortable for labels + a few widgets */
#define LV_MEM_SIZE        (32U * 1024U)

/* Use the standard C malloc/free instead of LVGL's built-in allocator.
 * 0 = use LVGL's built-in (recommended for bare-metal) */
#define LV_MEM_CUSTOM       0

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period in ms.
 * LVGL will call the flush callback at most this often. */
#define LV_DISP_DEF_REFR_PERIOD   30

/* Input device read period in ms */
#define LV_INDEV_DEF_READ_PERIOD   30

/* Use a custom tick source?
 * 0 = we call lv_tick_inc() manually from SysTick (simplest approach) */
#define LV_TICK_CUSTOM     0

/*====================
   FEATURE CONFIGURATION
 * Disable everything we don't need to save flash
 *====================*/

/* Drawing */
#define LV_DRAW_COMPLEX     1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_IMG_CACHE_DEF_SIZE 0

/* GPU - none */
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0

/* Logging — disable for production */
#define LV_USE_LOG      0

/* Assertions — enable during development, disable for release */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* Others */
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0
#define LV_USE_REFR_DEBUG       0

/*====================
   FONT USAGE
 * Enable only what you need
 *====================*/

/* Disable loading fonts at runtime */
#define LV_USE_FONT_LOADER  0

/* Built-in fonts — Montserrat, various sizes */
#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_10   0
#define LV_FONT_MONTSERRAT_12   0
#define LV_FONT_MONTSERRAT_14   1   /* <-- our main font */
#define LV_FONT_MONTSERRAT_16   0
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   0
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   0

#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK   0
#define LV_FONT_UNSCII_8        0
#define LV_FONT_UNSCII_16       1

/* Default font */
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* Enable support for compressed fonts */
#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_SUBPX      0
#if LV_USE_FONT_SUBPX
#define LV_FONT_SUBPX_BGR      0
#endif


/*====================
   TEXT SETTINGS
 *====================*/
#define LV_TXT_ENC                  LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS          " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN  0
#define LV_TXT_COLOR_CMD            "#"

/*====================
   WIDGET USAGE
 * Enable only what you need for your project
 *====================*/

/* Basic widgets (always useful) */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0

/* Extra widgets */
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/* Layouts */
#define LV_USE_FLEX       1
#define LV_USE_GRID       0

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
#define LV_THEME_DEFAULT_DARK   0
#define LV_THEME_DEFAULT_GROW   0
#define LV_THEME_DEFAULT_TRANSITION_TIME 0  /* no animations = faster */
#endif
#define LV_USE_THEME_BASIC   0
#define LV_USE_THEME_MONO    0

/* File system — not needed */
#define LV_USE_FS_STDIO    0
#define LV_USE_FS_POSIX    0
#define LV_USE_FS_WIN32    0
#define LV_USE_FS_FATFS    0

/* Image decoders — not needed for text-only POC */
#define LV_USE_PNG         0
#define LV_USE_BMP         0
#define LV_USE_SJPG        0
#define LV_USE_GIF         0
#define LV_USE_QRCODE      0
#define LV_USE_FREETYPE    0
#define LV_USE_RLOTTIE     0
#define LV_USE_FFMPEG      0

/* Animations */
#define LV_USE_ANIM        1

/* Snapshot */
#define LV_USE_SNAPSHOT     0

/*====================
   COMPILER SETTINGS
 *====================*/
#define LV_BIG_ENDIAN_SYSTEM 0
#define LV_ATTRIBUTE_TICK_INC
#define LV_ATTRIBUTE_TIMER_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE  1
#define LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_DMA
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning
#define LV_USE_LARGE_COORD  0

#endif /* LV_CONF_H */

#endif /* Enable content */
