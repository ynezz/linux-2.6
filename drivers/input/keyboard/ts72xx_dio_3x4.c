/*
 *  TS-72xx (3x4) keypad device driver for DIO1 header (DIO_0 thru DIO_6)
 *
 * (c) Copyright 2010  Matthieu Crapet <mcrapet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input/matrix_keypad.h>
#include <mach/gpio.h>

static const uint32_t ts72xx_kbd_keymap[] = {
	KEY(0, 0, KEY_1),
	KEY(0, 1, KEY_2),
	KEY(0, 2, KEY_3),

	KEY(1, 0, KEY_4),
	KEY(1, 1, KEY_5),
	KEY(1, 2, KEY_6),

	KEY(2, 0, KEY_7),
	KEY(2, 1, KEY_8),
	KEY(2, 2, KEY_9),

	KEY(3, 0, KEY_KPASTERISK),
	KEY(3, 1, KEY_0),
	KEY(3, 2, KEY_ENTER),
};

static struct matrix_keymap_data ts72xx_kbd_keymap_data = {
	.keymap		= ts72xx_kbd_keymap,
	.keymap_size	= ARRAY_SIZE(ts72xx_kbd_keymap),
};

static const int ts72xx_kbd_row_gpios[] = {
	EP93XX_GPIO_LINE_EGPIO14,	// DIO_6 (row0)
	EP93XX_GPIO_LINE_EGPIO13,
	EP93XX_GPIO_LINE_EGPIO12,
	EP93XX_GPIO_LINE_EGPIO11,
};

static const int ts72xx_kbd_col_gpios[] = {
	EP93XX_GPIO_LINE_EGPIO10,	// DIO_2 (col0)
	EP93XX_GPIO_LINE_EGPIO9,
	EP93XX_GPIO_LINE_EGPIO8,
};

static struct matrix_keypad_platform_data ts72xx_kbd_pdata = {
	.keymap_data		= &ts72xx_kbd_keymap_data,
	.row_gpios		= ts72xx_kbd_row_gpios,
	.col_gpios		= ts72xx_kbd_col_gpios,
	.num_row_gpios		= ARRAY_SIZE(ts72xx_kbd_row_gpios),
	.num_col_gpios		= ARRAY_SIZE(ts72xx_kbd_col_gpios),
	.col_scan_delay_us	= 20,
	.debounce_ms		= 20,
	.wakeup			= 1,
	.active_low		= 1,
	//.no_autorep		= 1,
};

static void ts72xx_kbd_release(struct device *dev)
{
}

static struct platform_device ts72xx_kbd_device = {
	.name		= "matrix-keypad",
	.id		= -1,
	.dev		= {
		.platform_data	= &ts72xx_kbd_pdata,
		.release	= ts72xx_kbd_release,
	},
};

static int __init ts72xx_dio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ts72xx_kbd_row_gpios); i++) {
		int irq = gpio_to_irq(ts72xx_kbd_row_gpios[i]);

		ep93xx_gpio_int_debounce(irq, 1);
	}

	return platform_device_register(&ts72xx_kbd_device);
}

static void __exit ts72xx_dio_exit(void)
{
	platform_device_unregister(&ts72xx_kbd_device);
}

module_init(ts72xx_dio_init);
module_exit(ts72xx_dio_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("Platform device 3x4 keypad");
MODULE_LICENSE("GPL");
