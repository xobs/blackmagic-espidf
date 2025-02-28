/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PLATFORM_H
#define __PLATFORM_H

#undef PRIx32
#undef PRIX32
#define PRIx32 "x"
#define PRIX32 "x"

#undef SCNx32
#define SCNx32 "x"

#include "esp_log.h"
#include "esp_attr.h"
#include "timing.h"
#include "driver/gpio.h"

void platform_buffer_flush(void);
void platform_set_baud(uint32_t baud);

#define SET_RUN_STATE(state)
#define SET_IDLE_STATE(state)
#define SET_ERROR_STATE(state) gpio_set_level(CONFIG_LED_GPIO, !state)

#if 1
#define ENABLE_DEBUG 1
#define DEBUG(x, ...)                        \
	do {                                     \
		TRIM(out, x);                        \
		ESP_LOGD("BMP", out, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG(x, ...)
#endif

#define SWDIO_MODE_FLOAT() gpio_set_direction(SWDIO_PIN, GPIO_MODE_INPUT)

#define SWDIO_MODE_DRIVE() gpio_set_direction(SWDIO_PIN, GPIO_MODE_OUTPUT)

#define TMS_SET_MODE()                                                   \
	do {                                                                 \
		gpio_reset_pin(CONFIG_TDI_GPIO);                                 \
		gpio_reset_pin(CONFIG_TDO_GPIO);                                 \
		gpio_reset_pin(CONFIG_TMS_SWDIO_GPIO);                           \
		gpio_reset_pin(CONFIG_TCK_SWCLK_GPIO);                           \
		gpio_reset_pin(CONFIG_TMS_SWDIO_DIR_GPIO);                       \
                                                                         \
		gpio_set_direction(CONFIG_TDI_GPIO, GPIO_MODE_OUTPUT);           \
		gpio_set_direction(CONFIG_TDO_GPIO, GPIO_MODE_INPUT);            \
		gpio_set_direction(CONFIG_TMS_SWDIO_GPIO, GPIO_MODE_OUTPUT);     \
		gpio_set_direction(CONFIG_TCK_SWCLK_GPIO, GPIO_MODE_OUTPUT);     \
		gpio_set_direction(CONFIG_TMS_SWDIO_DIR_GPIO, GPIO_MODE_OUTPUT); \
	} while (0)

#define TMS_PIN CONFIG_TMS_SWDIO_GPIO
#define TCK_PIN CONFIG_TCK_SWCLK_GPIO
#define TDI_PIN CONFIG_TDI_GPIO
#define TDO_PIN CONFIG_TDO_GPIO

#define SWDIO_PIN CONFIG_TMS_SWDIO_GPIO
#define SWCLK_PIN CONFIG_TCK_SWCLK_GPIO
#define SRST_PIN  CONFIG_SRST_GPIO

#define SWCLK_PORT 0
#define SWDIO_PORT 0

#define gpio_set(port, pin)     \
	do {                        \
		gpio_set_level(pin, 1); \
	} while (0)
#define gpio_clear(port, pin)   \
	do {                        \
		gpio_set_level(pin, 0); \
	} while (0)
#define gpio_get(port, pin) gpio_get_level(pin)
#define gpio_set_val(port, pin, value) \
	if (value) {                       \
		gpio_set(port, pin);           \
	} else {                           \
		gpio_clear(port, pin);         \
	}

#define GPIO_INPUT  GPIO_MODE_INPUT
#define GPIO_OUTPUT GPIO_MODE_OUTPUT

#define PLATFORM_HAS_DEBUG
#define PLATFORM_IDENT "esp32"
#endif

#define PLATFORM_HAS_TRACESWO
#define NUM_TRACE_PACKETS (128) /* This is an 8K buffer */
#define TRACESWO_PROTOCOL 2     /* 1 = Manchester, 2 = NRZ / async */

	extern uint32_t swd_delay_cnt;