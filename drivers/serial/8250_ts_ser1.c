/*
 *  linux/drivers/serial/8250_ts_ser1.c
 *  Technologic Systems TS-SER1 support.
 *
 * (c) Copyright 2006-2008  Matthieu Crapet <mcrapet@gmail.com>
 * Data taken from include/asm-i386/serial.h
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
 * Pin Number:
 * 1 DCD
 * 2 Receive data
 * 3 Trasmit data
 * 4 DTR
 * 5 Signal Ground
 * 6 DSR
 * 7 RTS
 * 8 CTS
 * 9 RI
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/serial_8250.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/ts72xx.h>
#include <mach/gpio.h>

#define TS72XX_SER1_IO_PHYS_BASE	(TS72XX_PC104_8BIT_IO_PHYS_BASE)
#define TS72XX_SER1_IO_SIZE		(TS72XX_PC104_8BIT_IO_SIZE)

#define TS_SER1_PORT_COM3	0x3E8
#define TS_SER1_PORT_COM4	0x2E8
#define TS_SER1_PORT_COM5	0x3A8

/* Value to write in 16550A scratch register */
#define MARKER_BYTE 0xAA /* or 0x55 */

#define PORT(_base,_irq)		\
{					\
	.iobase   = _base,		\
	.membase  = (void __iomem *)0,	\
	.irq      = _irq,		\
	.uartclk  = 1843200,		\
	.iotype   = UPIO_PORT,		\
	.flags    = UPF_BOOT_AUTOCONF,	\
}
/* Note: IRQ can be shared (see CONFIG_SERIAL_8250_SHARE_IRQ) */


static struct plat_serial8250_port ts72xx_ser1_data_com3[] = {
	PORT(TS_SER1_PORT_COM3, 0),
	{ },
};

static struct plat_serial8250_port ts72xx_ser1_data_com4[] = {
	PORT(TS_SER1_PORT_COM4, 0),
	{ },
};

static struct plat_serial8250_port ts72xx_ser1_data_com5[] = {
	PORT(TS_SER1_PORT_COM5, 0),
	{ },
};


static int ts_ser1_irq = CONFIG_SERIAL_8250_TS_SER1_IRQ; // 5, 6 or 7
static struct platform_device *serial8250_ts_ser1_dev;


static int __init ts_ser1_init(void)
{
	struct plat_serial8250_port *comX = NULL;
	void __iomem *iomem;

	int ret = -ENODEV;
	int n = 0; // COM number as printed on TS-SER1 pcb

	iomem = ioremap(TS72XX_SER1_IO_PHYS_BASE, TS72XX_SER1_IO_SIZE);

	if (iomem != NULL) {
		__raw_writeb(MARKER_BYTE, iomem + TS_SER1_PORT_COM3 + 7);
		if (__raw_readb(iomem + TS_SER1_PORT_COM3 + 7) == MARKER_BYTE) {
			comX = ts72xx_ser1_data_com3;
			n = 3;
		} else {
			__raw_writeb(MARKER_BYTE, iomem + TS_SER1_PORT_COM4 + 7);
			if (__raw_readb(iomem + TS_SER1_PORT_COM4 + 7) == MARKER_BYTE) {
				comX = ts72xx_ser1_data_com4;
				n = 4;
			} else {
				__raw_writeb(MARKER_BYTE, iomem + TS_SER1_PORT_COM5 + 7);
				if (__raw_readb(iomem + TS_SER1_PORT_COM5 + 7) == MARKER_BYTE) {
					comX = ts72xx_ser1_data_com5;
					n = 5;
				}
			}
		}

		if (comX) {
			switch (ts_ser1_irq) {
				case 5:
					ret = gpio_request(EP93XX_GPIO_LINE_F(3), "TS-SER1");
					if (ret < 0) {
						pr_err("gpio_request failed, try another irq\n");
						goto init_error;
					}
					gpio_direction_input(EP93XX_GPIO_LINE_F(3));
					comX->irq = gpio_to_irq(EP93XX_GPIO_LINE_F(3));
					set_irq_type(comX->irq, IRQ_TYPE_EDGE_RISING);
					break;
				case 6:
					comX->irq = IRQ_EP93XX_EXT1;
					break;
				case 7:
					comX->irq = IRQ_EP93XX_EXT3;
					break;
				default:
					pr_err("wrong specified irq\n");
					goto init_error;
			}

			comX->iobase += (unsigned long)iomem; // virtual address

		} else {
			pr_err("can't detect COM number\n");
			goto init_error;
		}

		/* create platform_device structure */
		serial8250_ts_ser1_dev = platform_device_alloc("serial8250", n);
		if (!serial8250_ts_ser1_dev) {
			ret = -ENOMEM;
			goto init_error;
		}

                ret = platform_device_add_data(serial8250_ts_ser1_dev, comX,
				2 * sizeof(struct plat_serial8250_port));
		if (ret) {
			platform_device_put(serial8250_ts_ser1_dev);
			goto init_error;
		}

		ret = platform_device_add(serial8250_ts_ser1_dev);
		if (ret) {
			platform_device_put(serial8250_ts_ser1_dev);
			goto init_error;
		}

		platform_set_drvdata(serial8250_ts_ser1_dev, iomem);
		return 0;
	}

init_error:
	if (iomem) {
		iounmap(iomem);
		iomem = NULL;
	}
	return ret;
}

static void __exit ts_ser1_exit(void)
{
	struct platform_device *pdev = serial8250_ts_ser1_dev;
	void __iomem *iomem = platform_get_drvdata(pdev);

	serial8250_ts_ser1_dev = NULL;

	platform_device_unregister(pdev);

	iounmap(iomem);
	if (ts_ser1_irq == 5)
		gpio_free(EP93XX_GPIO_LINE_F(3));
}

module_init(ts_ser1_init);
module_exit(ts_ser1_exit);

module_param(ts_ser1_irq, int, 0);
MODULE_PARM_DESC(ts_ser1_irq, "TS-SER1 IRQ, default=" __MODULE_STRING(CONFIG_SERIAL_8250_TS_SER1_IRQ) ")");

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("8250 serial probe module for TS-SER1 (TS-72xx)");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.5");
