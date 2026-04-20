/**
 * xpt2046.h — Resistive touch driver for XPT2046 on SPI3
 *
 * Shares SPI3 bus with the ILI9488 display (see DISPLAY_SUBSYSTEM_DOCS.md).
 * CS on PF14. The driver temporarily drops SPI3 prescaler from /4 (20 MHz)
 * to /64 (~1.25 MHz) for each touch read, since XPT2046 maxes at ~2 MHz.
 *
 * IMPORTANT: Never call XPT2046_Read*() while an LVGL DMA flush is in flight.
 * Read functions check is_display_dma_busy() and bail out if so.
 */

#ifndef XPT2046_H
#define XPT2046_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Pin ───────────────────────────────────────────────────────────────── */
#define TOUCH_CS_PORT           GPIOF
#define TOUCH_CS_PIN            GPIO_PIN_14
#define TOUCH_IRQ_PORT          GPIOD
#define TOUCH_IRQ_PIN           GPIO_PIN_2
/* T_IRQ is active-low: LOW = finger down, HIGH = idle */
#define XPT2046_IS_TOUCHED()    (HAL_GPIO_ReadPin(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN) == GPIO_PIN_RESET)

/* ── XPT2046 command bytes (differential, 12-bit, PD0=PD1=0) ──────────── */
#define XPT2046_CMD_X           0xD0
#define XPT2046_CMD_Y           0x90
#define XPT2046_CMD_Z1          0xB0
#define XPT2046_CMD_Z2          0xC0

/* ── Tuning ────────────────────────────────────────────────────────────── */
#define XPT2046_SAMPLES         5     /* median filter depth (must be odd)   */

/* Screen in landscape (MADCTL 0xE8) */
#define XPT2046_SCREEN_W        480
#define XPT2046_SCREEN_H        320

/* ── Calibration (replace after running the calibration procedure) ─────── */
/* These are placeholders — actual values come from touching known corners
 * and reading the raw ADC values via XPT2046_ReadRaw(). */
#define XPT2046_X_MIN_CAL       240
#define XPT2046_X_MAX_CAL       3900
#define XPT2046_Y_MIN_CAL       300
#define XPT2046_Y_MAX_CAL       3900

/* Axis orientation flags — set to match how the physical touch panel maps
 * onto the landscape display. Determine empirically during calibration. */
#define XPT2046_SWAP_XY         1
#define XPT2046_INVERT_X        0
#define XPT2046_INVERT_Y        0

/* ── API ───────────────────────────────────────────────────────────────── */
void XPT2046_Init(void);

/* Returns true and fills x/y with mapped screen coords if a touch is detected.
 * Returns false if not touched or if the display DMA is busy. */
bool XPT2046_Read(int16_t *x, int16_t *y);

/* Returns true and fills raw_x/raw_y (0-4095) if a touch is detected.
 * Use for calibration only. */
bool XPT2046_ReadRaw(uint16_t *raw_x, uint16_t *raw_y);

#endif /* XPT2046_H */
