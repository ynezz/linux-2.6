/*
 *  Technologic Systems TS-7200 Compact Flash PATA device driver.
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

#define DRV_NAME  "pata_ts7200_cf"
#define DRV_VERSION "0.21"


static void pata_ts7200_cf_release(struct device *dev)
{
	/* nothing to do (no kfree) because we have static struct */
}

static struct resource ts7200_cf_resources[] = {
	[0] = {
		.start	= TS7200_CF_CMD_PHYS_BASE,
		.end	= TS7200_CF_CMD_PHYS_BASE + 8,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TS7200_CF_AUX_PHYS_BASE,
		.end	= TS7200_CF_AUX_PHYS_BASE + 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= TS7200_CF_DATA_PHYS_BASE,
		.end	= TS7200_CF_DATA_PHYS_BASE + 2,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= IRQ_EP93XX_EXT0, /* pin 103 of EP9301 */
		.end	= IRQ_EP93XX_EXT0,
		.flags	= IORESOURCE_IRQ,
	}
};


static struct platform_device ts7200_cf_device = {
	.name	= "ts72xx-ide",
	.id	= 0,
	.dev	= {
		.dma_mask = &ts7200_cf_device.dev.coherent_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.release	= pata_ts7200_cf_release,
	},
	.num_resources	= ARRAY_SIZE(ts7200_cf_resources),
	.resource	= ts7200_cf_resources,
};


static __init int pata_ts7200_cf_init(void)
{
	return (board_is_ts7200()) ? \
		platform_device_register(&ts7200_cf_device) : -ENODEV;
}

static __exit void pata_ts7200_cf_exit(void)
{
	platform_device_unregister(&ts7200_cf_device);
}

module_init(pata_ts7200_cf_init);
module_exit(pata_ts7200_cf_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("TS-7200 CF PATA device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
