/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"

#include "shared-bindings/microcontroller/Pin.h"

#include "supervisor/shared/rgb_led_status.h"

#include "src/rp2_common/hardware_gpio/include/hardware/gpio.h"

#ifdef MICROPY_HW_NEOPIXEL
bool neopixel_in_use;
#endif
#ifdef MICROPY_HW_APA102_MOSI
bool apa102_sck_in_use;
bool apa102_mosi_in_use;
#endif
#ifdef SPEAKER_ENABLE_PIN
bool speaker_enable_in_use;
#endif

STATIC uint32_t never_reset_pins;

void reset_all_pins(void) {
    for (size_t i = 0; i < 30; i++) {
        if ((never_reset_pins & (1 << i)) != 0) {
            continue;
        }
        reset_pin_number(i);
    }
}

void never_reset_pin_number(uint8_t pin_number) {
    if (pin_number >= 32) {
        return;
    }

    never_reset_pins |= 1 << pin_number;
}

void reset_pin_number(uint8_t pin_number) {
    if (pin_number >= 32
#if TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
    // Pin 15 is used for Errata so we don't mess with it.
        || pin_number == 15
#endif
        ) {
        return;
    }

    never_reset_pins &= ~(1 << pin_number);

    // We are very aggressive in shutting down the pad fully. Both pulls are
    // disabled and both buffers are as well.
    gpio_init(pin_number);
    hw_clear_bits(&padsbank0_hw->io[pin_number], PADS_BANK0_GPIO0_IE_BITS |
                                                 PADS_BANK0_GPIO0_PUE_BITS |
                                                 PADS_BANK0_GPIO0_PDE_BITS);
    hw_set_bits(&padsbank0_hw->io[pin_number], PADS_BANK0_GPIO0_OD_BITS);

    #ifdef MICROPY_HW_NEOPIXEL
    if (pin_number == MICROPY_HW_NEOPIXEL->number) {
        neopixel_in_use = false;
        rgb_led_status_init();
        return;
    }
    #endif
    #ifdef MICROPY_HW_APA102_MOSI
    if (pin_number == MICROPY_HW_APA102_MOSI->number ||
        pin_number == MICROPY_HW_APA102_SCK->number) {
        apa102_mosi_in_use = apa102_mosi_in_use && pin_number != MICROPY_HW_APA102_MOSI->number;
        apa102_sck_in_use = apa102_sck_in_use && pin_number != MICROPY_HW_APA102_SCK->number;
        if (!apa102_sck_in_use && !apa102_mosi_in_use) {
            rgb_led_status_init();
        }
        return;
    }
    #endif

    #ifdef SPEAKER_ENABLE_PIN
    if (pin_number == SPEAKER_ENABLE_PIN->number) {
        speaker_enable_in_use = false;
    }
    #endif
}

void common_hal_never_reset_pin(const mcu_pin_obj_t* pin) {
    never_reset_pin_number(pin->number);
}

void common_hal_reset_pin(const mcu_pin_obj_t* pin) {
    reset_pin_number(pin->number);
}

void claim_pin(const mcu_pin_obj_t* pin) {
    #ifdef MICROPY_HW_NEOPIXEL
    if (pin == MICROPY_HW_NEOPIXEL) {
        neopixel_in_use = true;
    }
    #endif
    #ifdef MICROPY_HW_APA102_MOSI
    if (pin == MICROPY_HW_APA102_MOSI) {
        apa102_mosi_in_use = true;
    }
    if (pin == MICROPY_HW_APA102_SCK) {
        apa102_sck_in_use = true;
    }
    #endif

    #ifdef SPEAKER_ENABLE_PIN
    if (pin == SPEAKER_ENABLE_PIN) {
        speaker_enable_in_use = true;
    }
    #endif
}

bool pin_number_is_free(uint8_t pin_number) {
    if (pin_number >= 30) {
        return false;
    }
#if TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
    // Pin 15 is used for Errata so we don't mess with it.
    if (pin_number == 15) {
        return true;
    }
#endif
    uint32_t pad_state = padsbank0_hw->io[pin_number];
    return (pad_state & PADS_BANK0_GPIO0_IE_BITS) == 0 &&
           (pad_state & PADS_BANK0_GPIO0_OD_BITS) != 0;
}

bool common_hal_mcu_pin_is_free(const mcu_pin_obj_t* pin) {
    #ifdef MICROPY_HW_NEOPIXEL
    if (pin == MICROPY_HW_NEOPIXEL) {
        return !neopixel_in_use;
    }
    #endif
    #ifdef MICROPY_HW_APA102_MOSI
    if (pin == MICROPY_HW_APA102_MOSI) {
        return !apa102_mosi_in_use;
    }
    if (pin == MICROPY_HW_APA102_SCK) {
        return !apa102_sck_in_use;
    }
    #endif

    #ifdef SPEAKER_ENABLE_PIN
    if (pin == SPEAKER_ENABLE_PIN) {
        return !speaker_enable_in_use;
    }
    #endif

    return pin_number_is_free(pin->number);
}

uint8_t common_hal_mcu_pin_number(const mcu_pin_obj_t* pin) {
    return pin->number;
}

void common_hal_mcu_pin_claim(const mcu_pin_obj_t* pin) {
    return claim_pin(pin);
}

void common_hal_mcu_pin_reset_number(uint8_t pin_no) {
    reset_pin_number(pin_no);
}
