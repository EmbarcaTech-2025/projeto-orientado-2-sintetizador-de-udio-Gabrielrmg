#ifndef _inc_ssd1306
#define _inc_ssd1306

#include <pico/stdlib.h>
#include <hardware/i2c.h>

typedef enum {
    SET_CONTRAST = 0x81,
    SET_ENTIRE_ON = 0xA4,
    SET_NORM_INV = 0xA6,
    SET_DISP = 0xAE,
    SET_MEM_ADDR = 0x20,
    SET_COL_ADDR = 0x21,
    SET_PAGE_ADDR = 0x22,
    SET_DISP_START_LINE = 0x40,
    SET_SEG_REMAP = 0xA0,
    SET_MUX_RATIO = 0xA8,
    SET_COM_OUT_DIR = 0xC0,
    SET_DISP_OFFSET = 0xD3,
    SET_COM_PIN_CFG = 0xDA,
    SET_DISP_CLK_DIV = 0xD5,
    SET_PRECHARGE = 0xD9,
    SET_VCOM_DESEL = 0xDB,
    SET_CHARGE_PUMP = 0x8D
} ssd1306_command_t;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t pages;
    uint8_t address;
    i2c_inst_t *i2c_i;
    bool external_vcc;
    uint8_t *buffer;                // Dados da tela
    uint8_t *buffer_with_control;  // Buffer com byte de controle I2C
    size_t bufsize;
} ssd1306_t;

bool ssd1306_init(ssd1306_t *p, uint16_t width, uint16_t height, uint8_t address, i2c_inst_t *i2c_instance);
void ssd1306_deinit(ssd1306_t *p);
void ssd1306_poweroff(ssd1306_t *p);
void ssd1306_poweron(ssd1306_t *p);
void ssd1306_contrast(ssd1306_t *p, uint8_t val);
void ssd1306_invert(ssd1306_t *p, uint8_t inv);
void ssd1306_clear(ssd1306_t *p);
void ssd1306_draw_pixel(ssd1306_t *p, uint32_t x, uint32_t y);
void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s);
void ssd1306_show(ssd1306_t *p);

#endif
