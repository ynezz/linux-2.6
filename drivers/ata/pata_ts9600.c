/*
 *  Technologic Systems TS-9600 PATA device driver.
 *
 * (c) Copyright 2008  Matthieu Crapet <mcrapet@gmail.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/libata.h>
#include <scsi/scsi_host.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <mach/ts72xx.h>

#define DRV_NAME  "pata_ts9600"
#define DRV_VERSION "0.21"

#define TS9600_IDE_IO	(TS72XX_PC104_8BIT_IO_PHYS_BASE + 0x1F0)
#define TS9600_IDE_DATA	(TS72XX_PC104_16BIT_IO_PHYS_BASE + 0x1F0)
#define TS9600_IDE_IRQ	IRQ_EP93XX_EXT3  // IRQ7 (no other possibility for arm)


static void pata_ts9600_release(struct device *dev)
{
	/* nothing to do (no kfree) because we have static struct */
}

static struct resource ts9600_resources[] = {
	[0] = {
		.start	= TS9600_IDE_IO,
		.end	= TS9600_IDE_IO + 8,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TS9600_IDE_IO + 0x206,
		.end	= TS9600_IDE_IO + 0x206 + 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= TS9600_IDE_DATA,
		.end	= TS9600_IDE_DATA + 2,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= TS9600_IDE_IRQ,
		.end	= TS9600_IDE_IRQ,
		.flags	= IORESOURCE_IRQ,
	}
};


static struct platform_device ts9600_device = {
	.name	= "ts72xx-ide",
	.id	= 9600,
	.dev	= {
		.dma_mask = &ts9600_device.dev.coherent_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.release	= pata_ts9600_release,
	},
	.num_resources	= ARRAY_SIZE(ts9600_resources),
	.resource	= ts9600_resources,
};


static __init int pata_ts9600_init(void)
{
	return platform_device_register(&ts9600_device);
}

static __exit void pata_ts9600_exit(void)
{
	platform_device_unregister(&ts9600_device);
}

module_init(pata_ts9600_init);
module_exit(pata_ts9600_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("TS-9600 PATA device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
