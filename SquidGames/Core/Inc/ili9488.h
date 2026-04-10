#ifndef ILI9488_H
#define ILI9488_H

#include "stm32l4xx_hal.h"

// ------ Pin Macros (match your CubeMX labels) ------
#define LCD_CS_PORT   GPIOF
#define LCD_CS_PIN    GPIO_PIN_13
#define LCD_DC_PORT   GPIOF
#define LCD_DC_PIN    GPIO_PIN_15
// RESET is tied to 3.3V — no GPIO needed

#define LCD_CS_LOW()   HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define LCD_DC_LOW()   HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_HIGH()  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)

// ------ Display Dimensions ------
// portrait
//#define ILI9488_WIDTH   320
//#define ILI9488_HEIGHT  480
// landscape
#define ILI9488_WIDTH   480
#define ILI9488_HEIGHT  320

// ------ ILI9488 Commands ------
#define ILI9488_NOP       0x00
#define ILI9488_SWRESET   0x01
#define ILI9488_SLPOUT    0x11
#define ILI9488_DISPON    0x29
#define ILI9488_CASET     0x2A
#define ILI9488_PASET     0x2B
#define ILI9488_RAMWR     0x2C
#define ILI9488_MADCTL    0x36
#define ILI9488_COLMOD    0x3A

// ------ Color Helpers (RGB666 — 3 bytes per pixel over SPI) ------
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ILI9488_Color;

#define ILI9488_RED     (ILI9488_Color){0xFC, 0x00, 0x00}
#define ILI9488_GREEN   (ILI9488_Color){0x00, 0xFC, 0x00}
#define ILI9488_BLUE    (ILI9488_Color){0x00, 0x00, 0xFC}
#define ILI9488_WHITE   (ILI9488_Color){0xFC, 0xFC, 0xFC}
#define ILI9488_BLACK   (ILI9488_Color){0x00, 0x00, 0x00}
#define ILI9488_YELLOW  (ILI9488_Color){0xFC, 0xFC, 0x00}
#define ILI9488_CYAN    (ILI9488_Color){0x00, 0xFC, 0xFC}

// ------ Function Prototypes ------
void ILI9488_Init(SPI_HandleTypeDef *hspi);
void ILI9488_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ILI9488_FillScreen(ILI9488_Color color);
void ILI9488_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, ILI9488_Color color);
void ILI9488_DrawPixel(uint16_t x, uint16_t y, ILI9488_Color color);
void ILI9488_SetRotation(uint8_t rotation);

#endif
