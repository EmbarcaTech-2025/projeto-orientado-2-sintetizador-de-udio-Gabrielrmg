#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ssd1306.h"
#include "font.h"

inline static void fancy_write(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, const char *label) {
    int result = i2c_write_blocking(i2c, addr, src, len, false);
    if (result < 0) {
        printf("[%s] erro I2C %d para addr 0x%02X\n", label, result, addr);
    }
}

bool ssd1306_init(ssd1306_t *p, uint16_t width, uint16_t height, uint8_t address, i2c_inst_t *i2c_instance) {
    p->width = width;
    p->height = height;
    p->pages = height / 8;
    p->address = address;
    p->i2c_i = i2c_instance;
    p->bufsize = p->pages * p->width;

    p->buffer_with_control = malloc(p->bufsize + 1);
    if (!p->buffer_with_control) return false;

    p->buffer_with_control[0] = 0x40;  // Control byte
    p->buffer = p->buffer_with_control + 1;

    uint8_t cmds[] = {
        SET_DISP,
        SET_DISP_CLK_DIV, 0x80,
        SET_MUX_RATIO, height - 1,
        SET_DISP_OFFSET, 0x00,
        SET_DISP_START_LINE,
        SET_CHARGE_PUMP, p->external_vcc ? 0x10 : 0x14,
        SET_SEG_REMAP | 0x01,
        SET_COM_OUT_DIR | 0x08,
        SET_COM_PIN_CFG, width > 2 * height ? 0x02 : 0x12,
        SET_CONTRAST, 0x7F,
        SET_PRECHARGE, p->external_vcc ? 0x22 : 0xF1,
        SET_VCOM_DESEL, 0x30,
        SET_ENTIRE_ON,
        SET_NORM_INV,
        SET_DISP | 0x01,
        SET_MEM_ADDR, 0x00
    };

    for (size_t i = 0; i < sizeof(cmds); ++i) {
        uint8_t cmd[2] = {0x00, cmds[i]};
        fancy_write(p->i2c_i, p->address, cmd, 2, "init");
    }

    return true;
}

void ssd1306_show(ssd1306_t *p) {
    uint8_t addr[] = { SET_COL_ADDR, 0, p->width - 1, SET_PAGE_ADDR, 0, p->pages - 1 };
    for (size_t i = 0; i < sizeof(addr); ++i) {
        uint8_t cmd[2] = {0x00, addr[i]};
        fancy_write(p->i2c_i, p->address, cmd, 2, "addr");
    }

    memcpy(p->buffer_with_control + 1, p->buffer, p->bufsize);
    fancy_write(p->i2c_i, p->address, p->buffer_with_control, p->bufsize + 1, "ssd1306_show");
}

void ssd1306_clear(ssd1306_t *p) {
    memset(p->buffer, 0, p->bufsize);
}

void ssd1306_deinit(ssd1306_t *p) {
    free(p->buffer_with_control);
    p->buffer = NULL;
    p->buffer_with_control = NULL;
}

void ssd1306_poweroff(ssd1306_t *p) {
    uint8_t cmd[2] = {0x00, SET_DISP | 0x00};
    fancy_write(p->i2c_i, p->address, cmd, 2, "poweroff");
}

void ssd1306_poweron(ssd1306_t *p) {
    uint8_t cmd[2] = {0x00, SET_DISP | 0x01};
    fancy_write(p->i2c_i, p->address, cmd, 2, "poweron");
}

void ssd1306_contrast(ssd1306_t *p, uint8_t val) {
    uint8_t cmds[] = {0x00, SET_CONTRAST, 0x00, val};
    fancy_write(p->i2c_i, p->address, cmds + 1, 2, "contrast");
}

void ssd1306_invert(ssd1306_t *p, uint8_t inv) {
    uint8_t cmd[2] = {0x00, SET_NORM_INV | (inv & 1)};
    fancy_write(p->i2c_i, p->address, cmd, 2, "invert");
}

void ssd1306_draw_pixel(ssd1306_t *p, uint32_t x, uint32_t y) {
    if (x >= p->width || y >= p->height) return;
    p->buffer[x + p->width * (y >> 3)] |= (1 << (y & 0x07));
}

void ssd1306_draw_char_with_font(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t font[][5], char c) {
    if (c < 32 || c > 127) c = '?';
    for (int i = 0; i < 5; i++) {
        uint8_t line = font[c - 32][i];
        for (int j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                for (int dx = 0; dx < scale; dx++) {
                    for (int dy = 0; dy < scale; dy++) {
                        ssd1306_draw_pixel(p, x + i * scale + dx, y + j * scale + dy);
                    }
                }
            }
        }
    }
}

void ssd1306_draw_string_with_font(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t font[][5], const char *s) {
    while (*s) {
        ssd1306_draw_char_with_font(p, x, y, scale, font, *s++);
        x += 6 * scale;
    }
}

void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s) {
    ssd1306_draw_string_with_font(p, x, y, scale, font, s);
}
