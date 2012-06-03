/*
 * arch/arm/mach-ep93xx/ts72xx.c
 * Technologic Systems TS72xx SBC support.
 *
 * Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/m48t86.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/lis3lv02d.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <mach/hardware.h>
#include <mach/ts72xx.h>
#include <mach/ep93xx_spi.h>
#include <mach/gpio-ep93xx.h>

#include <asm/hardware/vic.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>


static struct map_desc ts72xx_io_desc[] __initdata = {
	{
		.virtual	= TS72XX_MODEL_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_MODEL_PHYS_BASE),
		.length		= TS72XX_MODEL_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_PLD_VERSION_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_PLD_VERSION_PHYS_BASE),
		.length		= TS72XX_PLD_VERSION_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_OPTIONS_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_OPTIONS_PHYS_BASE),
		.length		= TS72XX_OPTIONS_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_OPTIONS2_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_OPTIONS2_PHYS_BASE),
		.length		= TS72XX_OPTIONS2_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_RTC_INDEX_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_RTC_INDEX_PHYS_BASE),
		.length		= TS72XX_RTC_INDEX_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_RTC_DATA_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_RTC_DATA_PHYS_BASE),
		.length		= TS72XX_RTC_DATA_SIZE,
		.type		= MT_DEVICE,
	}
};

static void __init ts72xx_map_io(void)
{
	ep93xx_map_io();
	iotable_init(ts72xx_io_desc, ARRAY_SIZE(ts72xx_io_desc));
}


/*************************************************************************
 * NAND flash
 *************************************************************************/
#define TS72XX_NAND_CONTROL_ADDR_LINE	22	/* 0xN0400000 */
#define TS72XX_NAND_BUSY_ADDR_LINE	23	/* 0xN0800000 */

static void ts72xx_nand_hwcontrol(struct mtd_info *mtd,
				  int cmd, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {
		void __iomem *addr = chip->IO_ADDR_R;
		unsigned char bits;

		addr += (1 << TS72XX_NAND_CONTROL_ADDR_LINE);

		bits = __raw_readb(addr) & ~0x07;
		bits |= (ctrl & NAND_NCE) << 2;	/* bit 0 -> bit 2 */
		bits |= (ctrl & NAND_CLE);	/* bit 1 -> bit 1 */
		bits |= (ctrl & NAND_ALE) >> 2;	/* bit 2 -> bit 0 */

		__raw_writeb(bits, addr);
	}

	if (cmd != NAND_CMD_NONE)
		__raw_writeb(cmd, chip->IO_ADDR_W);
}

static int ts72xx_nand_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	void __iomem *addr = chip->IO_ADDR_R;

	addr += (1 << TS72XX_NAND_BUSY_ADDR_LINE);

	return !!(__raw_readb(addr) & 0x20);
}

static const char *ts72xx_nand_part_probes[] = { "cmdlinepart", NULL };

static struct mtd_partition ts72xx_nand_parts[] = {
	{
		.name		= "bootrom",
		.offset		= 0,
		.size		= 0x4000,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	}, {
		.name		= "rootfs",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x1d00000,
	}, {
		.name		= "redboot",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x40000,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	}, {
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x2b8000,
	}, {
		.name		= "redboot_fis",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x3000,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	}, {
		.name		= "redboot_config",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x1000,
	}
};

static struct platform_nand_data ts72xx_nand_data = {
	.chip = {
		.nr_chips	= 1,
		.chip_offset	= 0,
		.chip_delay	= 15,
		.part_probe_types = ts72xx_nand_part_probes,
		.partitions	= ts72xx_nand_parts,
		.nr_partitions	= ARRAY_SIZE(ts72xx_nand_parts),
	},
	.ctrl = {
		.cmd_ctrl	= ts72xx_nand_hwcontrol,
		.dev_ready	= ts72xx_nand_device_ready,
	},
};

static struct resource ts72xx_nand_resource[] = {
	{
		.start		= 0,			/* filled in later */
		.end		= 0,			/* filled in later */
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device ts72xx_nand_flash = {
	.name			= "gen_nand",
	.id			= -1,
	.dev.platform_data	= &ts72xx_nand_data,
	.resource		= ts72xx_nand_resource,
	.num_resources		= ARRAY_SIZE(ts72xx_nand_resource),
};


static void __init ts72xx_register_flash(void)
{
	/*
	 * TS7200 has NOR flash all other TS72xx board have NAND flash.
	 */
	if (board_is_ts7200()) {
		ep93xx_register_flash(2, EP93XX_CS6_PHYS_BASE, SZ_16M);
	} else {
		resource_size_t start;

		if (is_ts9420_installed())
			start = EP93XX_CS7_PHYS_BASE;
		else
			start = EP93XX_CS6_PHYS_BASE;

		ts72xx_nand_resource[0].start = start;
		ts72xx_nand_resource[0].end = start + SZ_16M - 1;

		platform_device_register(&ts72xx_nand_flash);
	}
}


static unsigned char ts72xx_rtc_readbyte(unsigned long addr)
{
	__raw_writeb(addr, TS72XX_RTC_INDEX_VIRT_BASE);
	return __raw_readb(TS72XX_RTC_DATA_VIRT_BASE);
}

static void ts72xx_rtc_writebyte(unsigned char value, unsigned long addr)
{
	__raw_writeb(addr, TS72XX_RTC_INDEX_VIRT_BASE);
	__raw_writeb(value, TS72XX_RTC_DATA_VIRT_BASE);
}

static struct m48t86_ops ts72xx_rtc_ops = {
	.readbyte	= ts72xx_rtc_readbyte,
	.writebyte	= ts72xx_rtc_writebyte,
};

static struct platform_device ts72xx_rtc_device = {
	.name		= "rtc-m48t86",
	.id		= -1,
	.dev		= {
		.platform_data	= &ts72xx_rtc_ops,
	},
	.num_resources	= 0,
};

static struct resource ts72xx_wdt_resources[] = {
	{
		.start	= TS72XX_WDT_CONTROL_PHYS_BASE,
		.end	= TS72XX_WDT_CONTROL_PHYS_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= TS72XX_WDT_FEED_PHYS_BASE,
		.end	= TS72XX_WDT_FEED_PHYS_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ts72xx_wdt_device = {
	.name		= "ts72xx-wdt",
	.id		= -1,
	.num_resources 	= ARRAY_SIZE(ts72xx_wdt_resources),
	.resource	= ts72xx_wdt_resources,
};

static struct ep93xx_eth_data __initdata ts72xx_eth_data = {
	.phy_id		= 1,
};

//#define SPI_LIS3_IRQ		EP93XX_GPIO_LINE_EGPIO8
#ifdef SPI_LIS3_IRQ
 #define EP93XX_GPIO_IRQ_BASE	64
 #define EP93XX_GPIO_TO_IRQ(x)	EP93XX_GPIO_IRQ_BASE + x
#endif
#define SPI_LIS3_CHIPSELECT	EP93XX_GPIO_LINE_EGPIO9

static int ts72xx_spi_setup(struct spi_device *spi)
{
	int err;
	int gpio_cs = SPI_LIS3_CHIPSELECT;
#ifdef SPI_LIS3_IRQ
	int gpio_irq = SPI_LIS3_IRQ;
#endif

	err = gpio_request(gpio_cs, spi->modalias);
	if (err)
		return err;

	gpio_direction_output(gpio_cs, 1);

#ifdef SPI_LIS3_IRQ
	err = gpio_request(gpio_irq, spi->modalias);
	if (err)
		return err;

	gpio_direction_input(gpio_irq);
#endif

	return 0;
}

static void ts72xx_spi_cleanup(struct spi_device *spi)
{
	int gpio_cs = SPI_LIS3_CHIPSELECT;

#ifdef SPI_LIS3_IRQ
	int gpio_irq = SPI_LIS3_IRQ;
	gpio_free(gpio_irq);
#endif

	gpio_set_value(gpio_cs, 1);
	gpio_direction_input(gpio_cs);
	gpio_free(gpio_cs);
}

static void ts72xx_spi_cs_control(struct spi_device *spi, int value)
{
	gpio_set_value(SPI_LIS3_CHIPSELECT, value);
}

static struct ep93xx_spi_chip_ops ts72xx_spi_ops = {
	.setup = ts72xx_spi_setup,
	.cleanup = ts72xx_spi_cleanup,
	.cs_control = ts72xx_spi_cs_control,
};

static struct lis3lv02d_platform_data lis3_pdata = {
	.wakeup_flags	= LIS3_WAKEUP_X_LO | LIS3_WAKEUP_X_HI |
			  LIS3_WAKEUP_Y_LO | LIS3_WAKEUP_Y_HI |
			  LIS3_WAKEUP_Z_LO | LIS3_WAKEUP_Z_HI,
	.wakeup_thresh	= 10,
#ifdef SPI_LIS3_IRQ
	.irq_cfg	= LIS3_IRQ1_DATA_READY,
#endif
};

static struct spi_board_info ts72xx_spi_devices[] __initdata = {
	{
		.modalias = "lis3lv02d_spi",
		.controller_data = &ts72xx_spi_ops,
		.platform_data = &lis3_pdata,
		/*
		 * We use 10 MHz even though the maximum is 7.4 MHz. The driver
		 * will limit it automatically to max. frequency.
		 */
		.max_speed_hz = 10 * 1000 * 1000,
		.bus_num = 0,
		.chip_select = 0,
#ifdef SPI_LIS3_IRQ
		.irq = EP93XX_GPIO_TO_IRQ(SPI_LIS3_IRQ),
#endif
	},
};

static struct ep93xx_spi_info ts72xx_spi_info = {
	.num_chipselect = ARRAY_SIZE(ts72xx_spi_devices),
	.use_dma = true,
};

static void __init ts72xx_register_spi(void)
{
	ep93xx_register_spi(&ts72xx_spi_info, ts72xx_spi_devices,
			    ARRAY_SIZE(ts72xx_spi_devices));
}

static void __init ts72xx_init_machine(void)
{
	ep93xx_init_devices();
	ts72xx_register_flash();
	platform_device_register(&ts72xx_rtc_device);
	platform_device_register(&ts72xx_wdt_device);

	ep93xx_register_eth(&ts72xx_eth_data, 1);
	ts72xx_register_spi();
}

/* Use more reliable CPLD watchdog to perform the reset */
static void ts72xx_restart(char cmd, const char *mode)
{
	void __iomem *ctrl;
	void __iomem *feed;

	ctrl = ioremap(ts72xx_wdt_resources[0].start,
			  resource_size(&ts72xx_wdt_resources[0]));
	feed = ioremap(ts72xx_wdt_resources[1].start,
		       resource_size(&ts72xx_wdt_resources[1]));

	if (ctrl && feed) {
		__raw_writeb(0x5, feed);
		__raw_writeb(0x1, ctrl);

		while (1)
			;
	}

	ep93xx_restart(cmd, mode);
}

MACHINE_START(TS72XX, "Technologic Systems TS-72xx SBC")
	/* Maintainer: Lennert Buytenhek <buytenh@wantstofly.org> */
	.atag_offset	= 0x100,
	.map_io		= ts72xx_map_io,
	.init_irq	= ep93xx_init_irq,
	.handle_irq	= vic_handle_irq,
	.timer		= &ep93xx_timer,
	.init_machine	= ts72xx_init_machine,
	.restart	= ts72xx_restart,
MACHINE_END
