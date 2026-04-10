#include "ili9488.h"
#include <string.h>

static SPI_HandleTypeDef *_hspi;

// ---- Low-level SPI helpers ----

static void LCD_SendCommand(uint8_t cmd) {
    LCD_DC_LOW();   // command mode
    LCD_CS_LOW();
    HAL_SPI_Transmit(_hspi, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

static void LCD_SendData(uint8_t *data, uint16_t len) {
    LCD_DC_HIGH();  // data mode
    LCD_CS_LOW();
    HAL_SPI_Transmit(_hspi, data, len, HAL_MAX_DELAY);
    LCD_CS_HIGH();
}

static void LCD_SendData8(uint8_t val) {
    LCD_SendData(&val, 1);
}

static void LCD_WriteReg(uint8_t cmd, uint8_t *data, uint16_t len) {
    LCD_SendCommand(cmd);
    if (len > 0) {
        LCD_SendData(data, len);
    }
}

// ---- Initialization Sequence ----

void ILI9488_Init(SPI_HandleTypeDef *hspi) {
    _hspi = hspi;

    // Software reset only (hardware RESET pin is tied to 3.3V)
    LCD_SendCommand(ILI9488_SWRESET);
    HAL_Delay(150);

    // Sleep out
    LCD_SendCommand(ILI9488_SLPOUT);
    HAL_Delay(150);

    // Pixel format: 18-bit/pixel (RGB666) for SPI mode
    LCD_SendCommand(ILI9488_COLMOD);
    LCD_SendData8(0x66);

    // Power Control 1
    LCD_WriteReg(0xC0, (uint8_t[]){0x17, 0x15}, 2);

    // Power Control 2
    LCD_WriteReg(0xC1, (uint8_t[]){0x41}, 1);

    // VCOM Control
    LCD_WriteReg(0xC5, (uint8_t[]){0x00, 0x12, 0x80}, 3);

    // Frame Rate Control (60Hz)
    LCD_WriteReg(0xB1, (uint8_t[]){0xA0, 0x11}, 2);

    // Display Inversion Control
    LCD_WriteReg(0xB4, (uint8_t[]){0x02}, 1);

    // Display Function Control
    LCD_WriteReg(0xB6, (uint8_t[]){0x02, 0x02}, 2);

    // Interface Control
    LCD_WriteReg(0xF6, (uint8_t[]){0x01, 0x00, 0x06}, 3);

    // Positive Gamma Control
    LCD_WriteReg(0xE0, (uint8_t[]){
        0x00, 0x03, 0x09, 0x08, 0x16,
        0x0A, 0x3F, 0x78, 0x4C, 0x09,
        0x0A, 0x08, 0x16, 0x1A, 0x0F
    }, 15);

    // Negative Gamma Control
    LCD_WriteReg(0xE1, (uint8_t[]){
        0x00, 0x16, 0x19, 0x03, 0x0F,
        0x05, 0x32, 0x45, 0x46, 0x04,
        0x0E, 0x0D, 0x35, 0x37, 0x0F
    }, 15);


    // Memory Access Control
    LCD_SendCommand(ILI9488_MADCTL);
//    LCD_SendData8(0x48); // portrait
    LCD_SendData8(0xE8); // landscape

    // Display ON
    LCD_SendCommand(ILI9488_DISPON);
    HAL_Delay(50);
}

// ---- Set Address Window (public — used by LVGL flush) ----

void ILI9488_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    LCD_SendCommand(ILI9488_CASET);
    uint8_t ca[] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    LCD_SendData(ca, 4);

    LCD_SendCommand(ILI9488_PASET);
    uint8_t ra[] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    LCD_SendData(ra, 4);

    LCD_SendCommand(ILI9488_RAMWR);
}

//// ---- Drawing Functions ----
//
void ILI9488_FillScreen(ILI9488_Color color) {
    ILI9488_FillRect(0, 0, ILI9488_WIDTH, ILI9488_HEIGHT, color);
}

void ILI9488_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, ILI9488_Color color) {
    if (x >= ILI9488_WIDTH || y >= ILI9488_HEIGHT) return;
    if (x + w > ILI9488_WIDTH)  w = ILI9488_WIDTH - x;
    if (y + h > ILI9488_HEIGHT) h = ILI9488_HEIGHT - y;

    ILI9488_SetWindow(x, y, x + w - 1, y + h - 1);

    uint8_t line_buf[ILI9488_WIDTH * 3];
    uint16_t pixels_per_line = w;
    for (uint16_t i = 0; i < pixels_per_line; i++) {
        line_buf[i * 3 + 0] = color.r;
        line_buf[i * 3 + 1] = color.g;
        line_buf[i * 3 + 2] = color.b;
    }

    LCD_DC_HIGH();
    LCD_CS_LOW();
    for (uint16_t row = 0; row < h; row++) {
        HAL_SPI_Transmit(_hspi, line_buf, pixels_per_line * 3, HAL_MAX_DELAY);
    }
    LCD_CS_HIGH();
}

void ILI9488_DrawPixel(uint16_t x, uint16_t y, ILI9488_Color color) {
    if (x >= ILI9488_WIDTH || y >= ILI9488_HEIGHT) return;

    ILI9488_SetWindow(x, y, x, y);

    uint8_t data[] = { color.r, color.g, color.b };
    LCD_SendData(data, 3);
}

//void ILI9488_SetRotation(uint8_t rotation) {
//    LCD_SendCommand(ILI9488_MADCTL);
//    switch (rotation % 4) {
//        case 0: LCD_SendData8(0x48); break;
//        case 1: LCD_SendData8(0x28); break;
//        case 2: LCD_SendData8(0x88); break;
//        case 3: LCD_SendData8(0xE8); break;
//    }
//}
