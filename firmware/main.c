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
#include "hardware/pwm.h"
#include "hardware/vreg.h"
#include "lcd.h" // For resolution definition, should not call any functions
#include "core1.h"
#include "font.h"
#include "vin.h"

//#define USE_LUT

// Overclock
#define VREG_VSEL   VREG_VOLTAGE_1_20
#define SYSCLK_KHZ  200000

uint8_t contrast = 165;

#define BTN_START 27
#define BTN_SEL 28
#define BTN_L 29
#define BTN_R 26

void set_contrast(uint8_t val) {
    uint slice_num = pwm_gpio_to_slice_num(10);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, val);
}

void update_contrast() {
    static uint8_t cntr = 0;
    if (cntr != 0) {
        cntr--;
        return;
    }
    else {
        cntr = 2;
    }
    if (gpio_get(BTN_START) == false) {
        if (gpio_get(BTN_L) == false) {
            contrast++;
            set_contrast(contrast);
        }
        else if (gpio_get(BTN_R) == false) {
            contrast--;
            set_contrast(contrast);
        }
    }
}

int main()
{
    vreg_set_voltage(VREG_VSEL);
	sleep_ms(10);
    set_sys_clock_khz(SYSCLK_KHZ, true);

    multicore_launch_core1(core1_entry);
    uint32_t g = multicore_fifo_pop_blocking();

    //gpio_put(LED_PIN, 0);
    //sleep_ms(250);

    /*gpio_init(10);
    gpio_set_dir(10, GPIO_OUT);
    gpio_put(10, 0);*/

    gpio_set_function(10, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(10);
    pwm_set_clkdiv(slice_num, 10.0f);
    pwm_set_wrap(slice_num, 255);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, contrast);
    pwm_set_enabled(slice_num, true);

    for (int i = 26; i <= 29; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }

    gpio_init(9);
    gpio_set_dir(9, GPIO_OUT);
    gpio_put(9, 0);

    vin_init();
    vin_run();

    return 0;
}
