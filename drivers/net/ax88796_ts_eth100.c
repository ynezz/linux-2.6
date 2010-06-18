/*
 *  linux/drivers/net/ax88796_ts_eth100.c
 *  Technologic Systems TS-ETH100 support.
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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <net/ax88796.h>
#include <mach/ts72xx.h>
#include <mach/gpio.h>

#define TS72XX_ETH100_IO8_PHYS_BASE	(TS72XX_PC104_8BIT_IO_PHYS_BASE)
#define TS72XX_ETH100_IO8_SIZE		(TS72XX_PC104_8BIT_IO_SIZE)
#define TS72XX_ETH100_IO16_PHYS_BASE	(TS72XX_PC104_16BIT_IO_PHYS_BASE)
#define TS72XX_ETH100_IO16_SIZE		(TS72XX_PC104_16BIT_IO_SIZE)

/* Technologic systems I/O space */
#define TS_ETH100_PLD_0	0x100
#define TS_ETH100_PLD_1	0x110
#define TS_ETH100_PLD_2	0x120
#define TS_ETH100_PLD_3	0x130

/* NE2000 I/O space */
#define TS_ETH100_MAC_0	0x200
#define TS_ETH100_MAC_1	0x240
#define TS_ETH100_MAC_2	0x300
#define TS_ETH100_MAC_3	0x340

/* Board identifier must be 5 ; PLD revision should be 1 */
#define is_eth100_present(__iomem, __offset) \
	(((__raw_readb(__iomem + __offset) & 0xF) == 0x5) && \
	 ((__raw_readb(__iomem + __offset + 4) & 0xF) == 0x1))

/* Jumpers status (SRAM control register) */
#define read_irq(__iomem, __offset) \
	(__raw_readb(__iomem + __offset + 8) & 0xE)


static u32 offsets[0x20] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

static struct ax_plat_data ts72xx_eth100_asix_data = {
	.flags		= AXFLG_HAS_93CX6,
	.wordlength	= 2,
	.dcr_val	= 0x48,
	.rcr_val	= 0x40,
	.reg_offsets	= offsets,
};

static struct resource ts72xx_eth100_resource[] = {
	[0] = {
		.start	= TS72XX_ETH100_IO8_PHYS_BASE,
		.end	= TS72XX_ETH100_IO8_PHYS_BASE + 0x40 - 1,
		.flags	= IORESOURCE_MEM
	},
	[1] = { /* 0x10 is NE_DATAPORT is 16-bit access */
		.start	= TS72XX_ETH100_IO16_PHYS_BASE,
		.end	= TS72XX_ETH100_IO16_PHYS_BASE + 0x40 - 1,
		.flags	= IORESOURCE_MEM
	},
	[2] = {
		.start	= IRQ_EP93XX_EXT1,
		.end	= IRQ_EP93XX_EXT1,
		.flags	= IORESOURCE_IRQ
	}
};

static int ts_eth100_irq; // 2 [IRQ 5], 4 [IRQ 6] or 8 [IRQ 7] (jumper configuration)


static void ts72xx_eth100_release(struct device *dev)
{
	/* nothing to do (no kfree) because we have static struct */
}

static struct platform_device ts72xx_eth100_device_asix = {
	.name	= "ax88796",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(ts72xx_eth100_resource),
	.resource	= ts72xx_eth100_resource,
	.dev	= {
		.platform_data	= &ts72xx_eth100_asix_data,
		.release	= ts72xx_eth100_release,
	}
};

static int __init ts_eth100_init(void)
{
	void __iomem *iomem;
	struct platform_device *ethX = NULL;

	iomem = ioremap(TS72XX_ETH100_IO8_PHYS_BASE, TS72XX_ETH100_IO8_SIZE);
	if (iomem != NULL) {
		ethX = &ts72xx_eth100_device_asix;

		if (is_eth100_present(iomem, TS_ETH100_PLD_0)) {
			ethX->resource[0].start += TS_ETH100_MAC_0;
			ethX->resource[0].end   += TS_ETH100_MAC_0;
			ethX->resource[1].start += TS_ETH100_MAC_0;
			ethX->resource[1].end   += TS_ETH100_MAC_0;
			ts_eth100_irq = read_irq(iomem, TS_ETH100_PLD_0);
		} else if(is_eth100_present(iomem, TS_ETH100_PLD_1)) {
			ethX->resource[0].start += TS_ETH100_MAC_1;
			ethX->resource[0].end   += TS_ETH100_MAC_1;
			ethX->resource[1].start += TS_ETH100_MAC_1;
			ethX->resource[1].end   += TS_ETH100_MAC_1;
			ts_eth100_irq = read_irq(iomem, TS_ETH100_PLD_1);
		} else if(is_eth100_present(iomem, TS_ETH100_PLD_2)) {
			ethX->resource[0].start += TS_ETH100_MAC_2;
			ethX->resource[0].end   += TS_ETH100_MAC_2;
			ethX->resource[1].start += TS_ETH100_MAC_2;
			ethX->resource[1].end   += TS_ETH100_MAC_2;
			ts_eth100_irq = read_irq(iomem, TS_ETH100_PLD_2);
		} else if(is_eth100_present(iomem, TS_ETH100_PLD_3)) {
			ethX->resource[0].start += TS_ETH100_MAC_3;
			ethX->resource[0].end   += TS_ETH100_MAC_3;
			ethX->resource[1].start += TS_ETH100_MAC_3;
			ethX->resource[1].end   += TS_ETH100_MAC_3;
			ts_eth100_irq = read_irq(iomem, TS_ETH100_PLD_3);
		} else {
			ethX = NULL;
		}

		/* Translate IRQ number */
		if (ethX != NULL) {
			int ret, irq = 0;
			switch (ts_eth100_irq) {
				case 0x2: /* IRQ5 */
					irq = gpio_to_irq(EP93XX_GPIO_LINE_F(3));
					ret = gpio_request(irq, "TS-ETH100");
					if (ret < 0) {
						ethX = NULL;
						goto init_error;
					} else {
						gpio_direction_input(irq);
						set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
					}
					break;
				case 0x4: /* IRQ6 */
					irq =  IRQ_EP93XX_EXT1;
					break;
				case 0x8: /* IRQ7 */
				default:
					irq =  IRQ_EP93XX_EXT3;
			}
			ethX->resource[2].start = irq;
			ethX->resource[2].end   = irq;
		}
init_error:
		iounmap(iomem);
	}

	return ((ethX == NULL) ? -ENODEV :
			platform_device_register(&ts72xx_eth100_device_asix));
}


static void __exit ts_eth100_exit(void)
{
	platform_device_unregister(&ts72xx_eth100_device_asix);
	if (ts_eth100_irq == 2)
		gpio_free(EP93XX_GPIO_LINE_F(3));
}

module_init(ts_eth100_init);
module_exit(ts_eth100_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("Asix 88796 ethernet probe module for TS-ETH100 (TS-72xx)");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.21");
