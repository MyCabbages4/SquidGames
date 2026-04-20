/**
 * xpt2046.c — Resistive touch driver for XPT2046 on SPI3
 * See xpt2046.h for overview.
 */

#include "xpt2046.h"

/* SPI handle from CubeMX-generated code */
extern SPI_HandleTypeDef hspi3;

/* Accessor defined in main.c — returns nonzero while an LVGL DMA flush
 * is in flight. Touch reads must bail out until DMA completes, otherwise
 * they corrupt the display transfer in progress on the shared bus. */
extern uint8_t is_display_dma_busy(void);

/* ── CS helpers ────────────────────────────────────────────────────────── */
#define TOUCH_CS_LOW()   HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_RESET)
#define TOUCH_CS_HIGH()  HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET)

/* ── SPI speed management ──────────────────────────────────────────────── */
static void touch_spi_slow(void) {
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;  /* ~1.25 MHz */
    HAL_SPI_Init(&hspi3);
}

static void touch_spi_fast(void) {
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;   /* 20 MHz (display) */
    HAL_SPI_Init(&hspi3);
}

/* ── Low-level 12-bit axis read ────────────────────────────────────────── */
static uint16_t xpt2046_read_axis(uint8_t cmd) {
    uint8_t tx[3] = { cmd, 0x00, 0x00 };
    uint8_t rx[3] = { 0 };
    HAL_SPI_TransmitReceive(&hspi3, tx, rx, 3, HAL_MAX_DELAY);
    /* Result sits in rx[1]:rx[2], left-aligned in 16 bits. Right-shift 3
     * to get the 12-bit value. */
    return ((rx[1] << 8) | rx[2]) >> 3;
}

/* Insertion sort + median — cheap for small N */
static uint16_t median_u16(uint16_t *arr, int n) {
    for (int i = 1; i < n; i++) {
        uint16_t v = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > v) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = v;
    }
    return arr[n / 2];
}

static int16_t map_i16(uint16_t val, uint16_t in_min, uint16_t in_max,
                       int16_t out_min, int16_t out_max) {
    if (in_max == in_min) return out_min;
    int32_t r = (int32_t)(val - in_min) * (out_max - out_min)
              / (int32_t)(in_max - in_min) + out_min;
    if (r < out_min) r = out_min;
    if (r > out_max) r = out_max;
    return (int16_t)r;
}

/* ── Public API ────────────────────────────────────────────────────────── */
void XPT2046_Init(void) {
    /* PF14 is configured as GPIO_Output (Initial High) by MX_GPIO_Init().
     * Nothing else required here — make sure that's set in CubeMX. */
    TOUCH_CS_HIGH();
}

bool XPT2046_ReadRaw(uint16_t *raw_x, uint16_t *raw_y) {
    /* No finger → don't even touch SPI. */
    if (!XPT2046_IS_TOUCHED()) return false;

    if (is_display_dma_busy()) return false;

    touch_spi_slow();
    TOUCH_CS_LOW();

    /* Skip Z1/Z2 — T_IRQ already told us there's a touch. */
    uint16_t xs[XPT2046_SAMPLES];
    uint16_t ys[XPT2046_SAMPLES];
    for (int i = 0; i < XPT2046_SAMPLES; i++) {
        xs[i] = xpt2046_read_axis(XPT2046_CMD_X);
        ys[i] = xpt2046_read_axis(XPT2046_CMD_Y);
    }

    TOUCH_CS_HIGH();
    touch_spi_fast();

    *raw_x = median_u16(xs, XPT2046_SAMPLES);
    *raw_y = median_u16(ys, XPT2046_SAMPLES);
    return true;
}

bool XPT2046_Read(int16_t *x, int16_t *y) {
    uint16_t rx, ry;
    if (!XPT2046_ReadRaw(&rx, &ry)) return false;

    /* Apply axis swap/invert for landscape (MADCTL 0xE8). */
    uint16_t a = rx, b = ry;
#if XPT2046_SWAP_XY
    uint16_t tmp = a; a = b; b = tmp;
#endif

    int16_t sx, sy;
    sx = map_i16(a, XPT2046_X_MIN_CAL, XPT2046_X_MAX_CAL, 0, XPT2046_SCREEN_W - 1);
    sy = map_i16(b, XPT2046_Y_MIN_CAL, XPT2046_Y_MAX_CAL, 0, XPT2046_SCREEN_H - 1);

#if XPT2046_INVERT_X
    sx = (XPT2046_SCREEN_W - 1) - sx;
#endif
#if XPT2046_INVERT_Y
    sy = (XPT2046_SCREEN_H - 1) - sy;
#endif

    *x = sx;
    *y = sy;
    return true;
}
