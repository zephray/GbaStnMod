//
// Copyright 2021 Wenting Zhang <zephray@outlook.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "vinfsm.pio.h"
#include "vin.h"
#include "font.h"

#define VIN_IN_PIN  11
#define VIN_SM (0)

PIO vin_pio = pio1;
static uint sm_offset;

uint8_t color_map[4096];

void vin_init() {
    sm_offset = pio_add_program(vin_pio, &vinfsm_program);

    pio_sm_config c = vinfsm_program_get_default_config(sm_offset);

    // TBD
    sm_config_set_in_pins(&c, 28);
    // Set the pin directions to input at the PIO
    pio_sm_set_consecutive_pindirs(vin_pio, VIN_SM, VIN_IN_PIN, 15, false);
    // Connect these GPIOs to this PIO block
    for (int i = 0; i < 15; i++) {
        pio_gpio_init(vin_pio, VIN_IN_PIN + i);
    }

    // Shifting to left matches the customary MSB-first ordering of SPI.
    sm_config_set_in_shift(
        &c,
        false, // Shift-to-right = false (i.e. shift to left)
        true,  // Autopush enabled
        16     // Autopush threshold = 8
    );

    // We only receive, so disable the TX FIFO to make the RX FIFO deeper.
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    // Load our configuration, and start the program from the beginning
    pio_sm_init(vin_pio, VIN_SM, sm_offset, &c);
    //pio_sm_set_enabled(vin_pio, VIN_SM, true);

    for (uint32_t i = 0; i < 4096; i++) {
        // Use only RGB444
        // Y = (9R + 30G + 3B) / 10
        uint16_t b = (~i >> 0) & 0xf;
        uint16_t g = (~i >> 4) & 0xf;
        uint16_t r = (~i >> 8) & 0xf;
        uint16_t s = (9 * r + 30 * g + 3 * b);
        color_map[i] = s / 10;
    }
}

static unsigned char *lcd_flip() {
    multicore_fifo_push_blocking(0x1234); // Push something to request flip
    return (unsigned char *)multicore_fifo_pop_blocking();
}

#define HBLANK 1

void __no_inline_not_in_flash_func(vin_processing)(uint8_t *buffer) {
    size_t ptr = 0;
    // Discard first one. Vsync
    uint16_t rxdata = pio_sm_get_blocking(vin_pio, VIN_SM);

    for (int i = 0; i < 160; i++) {
        for (int j = 0; j < 240; j++) {
            rxdata = pio_sm_get_blocking(vin_pio, VIN_SM);
            buffer[ptr++] = color_map[rxdata >> 4];
            //buffer[ptr++] = rxdata >> 4;
        }
        ptr += (256-240);
        for (int j = 0; j < HBLANK; j++) {
            rxdata = pio_sm_get_blocking(vin_pio, VIN_SM);
            // discard
        }
    }
}

static void putpixel(uint8_t *buf, int x, int y, uint8_t c) {
    buf[256 * y + x] = c;
}

void put_char(unsigned char *buf, int x, int y, char c, int fg, int bg) {
    int i, j, p;
	for(i = 0; i < 6; i++)
	{
		for(j = 8; j > 0; j--)
		{
			p = charMap_ascii_mini[(unsigned char)c][i] << j ;
			if (p & 0x80)
                putpixel(buf, x + i, y + 8 - j, fg);
            else
                putpixel(buf, x + i, y + 8 - j, bg);
		}
	}
}

void put_string(unsigned char *buf, int x, int y, char *str, int fg, int bg) {
    while (*str) {
        char c = *str++;
        if ((c == '\n')) {
            y += 8;
            x = 0;
        }
        if (c != '\n')
            put_char(buf, x, y, c, fg, bg);
        x += 6;
        if (x > 240) {
            y += 8;
            x = 0;
        }
    }
}

void vin_run() {
    uint8_t *buffer;
    char ystr[5];

    while (1) {
        buffer = lcd_flip();
        pio_sm_set_enabled(vin_pio, VIN_SM, false);
        pio_sm_clear_fifos(vin_pio, VIN_SM);
        pio_sm_restart(vin_pio, VIN_SM);
        pio_sm_exec(vin_pio, VIN_SM, pio_encode_jmp(sm_offset));
        pio_sm_set_enabled(vin_pio, VIN_SM, true);

        vin_processing(buffer);

        update_contrast();
    }
}