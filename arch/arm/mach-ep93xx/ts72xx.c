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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/m48t86.h>
#include <linux/mtd/physmap.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

#include <mach/hardware.h>
#include <mach/ts72xx.h>

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
	},
	/* Use this for debug only. Each device will map its own PC/104 address space */
	///* PC/104 (8-bit) I/O bus */
	//{
	//  .virtual  = TS72XX_PC104_8BIT_IO_VIRT_BASE,
	//  .pfn    = __phys_to_pfn(TS72XX_PC104_8BIT_IO_PHYS_BASE),
	//  .length   = TS72XX_PC104_8BIT_IO_SIZE,
	//  .type   = MT_DEVICE,
	//},
	///* PC/104 (16-bit) I/O bus */
	//{
	//  .virtual  = TS72XX_PC104_16BIT_IO_VIRT_BASE,
	//  .pfn    = __phys_to_pfn(TS72XX_PC104_16BIT_IO_PHYS_BASE),
	//  .length   = TS72XX_PC104_16BIT_IO_SIZE,
	//  .type   = MT_DEVICE,
	//},
	///* PC/104 (8-bit) MEM bus */
	//{
	//  .virtual  = TS72XX_PC104_8BIT_MEM_VIRT_BASE,
	//  .pfn    = __phys_to_pfn(TS72XX_PC104_8BIT_MEM_PHYS_BASE),
	//  .length   = TS72XX_PC104_8BIT_MEM_SIZE,
	//  .type   = MT_DEVICE,
	//},
	///* PC/104 (16-bit) MEM bus */
	//{
	//  .virtual  = TS72XX_PC104_16BIT_MEM_VIRT_BASE,
	//  .pfn    = __phys_to_pfn(TS72XX_PC104_16BIT_MEM_PHYS_BASE),
	//  .length   = TS72XX_PC104_16BIT_MEM_SIZE,
	//  .type   = MT_DEVICE,
	//}
};

static struct map_desc ts72xx_nand_io_desc[] __initdata = {
	{
		.virtual	= TS72XX_NAND_DATA_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_NAND1_DATA_PHYS_BASE),
		.length		= TS72XX_NAND_DATA_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_NAND_CONTROL_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_NAND1_CONTROL_PHYS_BASE),
		.length		= TS72XX_NAND_CONTROL_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_NAND_BUSY_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_NAND1_BUSY_PHYS_BASE),
		.length		= TS72XX_NAND_BUSY_SIZE,
		.type		= MT_DEVICE,
	}
};

static struct map_desc ts72xx_alternate_nand_io_desc[] __initdata = {
	{
		.virtual	= TS72XX_NAND_DATA_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_NAND2_DATA_PHYS_BASE),
		.length		= TS72XX_NAND_DATA_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_NAND_CONTROL_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_NAND2_CONTROL_PHYS_BASE),
		.length		= TS72XX_NAND_CONTROL_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= TS72XX_NAND_BUSY_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_NAND2_BUSY_PHYS_BASE),
		.length		= TS72XX_NAND_BUSY_SIZE,
		.type		= MT_DEVICE,
	}
};

static void __init ts72xx_map_io(void)
{
	ep93xx_map_io();
	iotable_init(ts72xx_io_desc, ARRAY_SIZE(ts72xx_io_desc));

	/*
	 * The TS-7200 has NOR flash, the other models have NAND flash.
	 */
	if (!board_is_ts7200()) {
		if (is_ts9420_installed()) {
			iotable_init(ts72xx_alternate_nand_io_desc,
				ARRAY_SIZE(ts72xx_alternate_nand_io_desc));
		} else {
			iotable_init(ts72xx_nand_io_desc,
				ARRAY_SIZE(ts72xx_nand_io_desc));
		}
	}
}

/*************************************************************************
 * NOR flash (TS-7200 only)
 *************************************************************************/
static struct physmap_flash_data ts72xx_flash_data = {
	.width		= 2,
};

static struct resource ts72xx_flash_resource = {
	.start		= EP93XX_CS6_PHYS_BASE,
	.end		= EP93XX_CS6_PHYS_BASE + SZ_16M - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ts72xx_flash = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
		.platform_data	= &ts72xx_flash_data,
	},
	.num_resources	= 1,
	.resource	= &ts72xx_flash_resource,
};

static void __init ts72xx_register_flash(void)
{
	if (board_is_ts7200())
		platform_device_register(&ts72xx_flash);
}

/*************************************************************************
 * SD Card (TS-7260 only)
 *************************************************************************/

static struct resource ts72xx_sdcard_resource = {
	.start		= TS7260_SDCARD_PHYS_BASE,
	.end		= TS7260_SDCARD_PHYS_BASE + 0x20,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ts72xx_sdcard = {
	.name		= "ts72xx-sdcard",
	.id		= 0,
	.num_resources	= 1,
	.resource	= &ts72xx_sdcard_resource,
};

static void __init ts72xx_register_sdcard(void)
{
	if (board_is_ts7260() || board_is_ts7400() || board_is_ts7300())
		platform_device_register(&ts72xx_sdcard);
}

/*************************************************************************
 * RTC
 *************************************************************************/
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

/*************************************************************************
 * MAX197 (8 * 12-bit A/D converter) option
 *************************************************************************/
static struct resource ts72xx_max197_resources[] = {
	[0] = { /* sample/control register */
		.start	= TS72XX_MAX197_SAMPLE_PHYS_BASE,
		.end	= TS72XX_MAX197_SAMPLE_PHYS_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = { /* busy bit */
		.start	= TS72XX_JUMPERS_MAX197_PHYS_BASE,
		.end	= TS72XX_JUMPERS_MAX197_PHYS_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device ts72xx_max197_device = {
	.name		= "ts72xx-max197",
	.id		= -1,
	.dev		= {
		.platform_data	= NULL,
	},
	.num_resources	= ARRAY_SIZE(ts72xx_max197_resources),
	.resource	= ts72xx_max197_resources,
};

/*************************************************************************
 * Ethernet
 *************************************************************************/
static struct ep93xx_eth_data ts72xx_eth_data = {
	.phy_id		= 1,
};

/*************************************************************************
 * I2C (make access through TS-72XX "DIO" 2x8 header)
 *************************************************************************/
static struct i2c_gpio_platform_data ts72xx_i2c_gpio_data = {
	.sda_pin		= EP93XX_GPIO_LINE_EGPIO14, // DIO_6
	.sda_is_open_drain	= 0,
	.scl_pin		= EP93XX_GPIO_LINE_EGPIO15, // DIO_7
	.scl_is_open_drain	= 0,
	.udelay			= 0,	/* default is 100 kHz */
	.timeout		= 0,	/* default is 100 ms */
};

static struct i2c_board_info __initdata ts72xx_i2c_board_info[] = {
};

static void __init ts72xx_init_machine(void)
{
	ep93xx_init_devices();
	ts72xx_register_flash();
	ts72xx_register_sdcard();
	platform_device_register(&ts72xx_rtc_device);
	platform_device_register(&ts72xx_wdt_device);

	ep93xx_register_eth(&ts72xx_eth_data, 1);
	ep93xx_register_i2c(&ts72xx_i2c_gpio_data,
			ts72xx_i2c_board_info,
			ARRAY_SIZE(ts72xx_i2c_board_info));

	if (is_max197_installed()) {
		platform_device_register(&ts72xx_max197_device);
	}

	/* PWM1 is DIO_6 on TS-72xx header */
	ep93xx_register_pwm(0, 1);
}

MACHINE_START(TS72XX, "Technologic Systems TS-72xx SBC")
	/* Maintainer: Lennert Buytenhek <buytenh@wantstofly.org> */
	.phys_io	= EP93XX_APB_PHYS_BASE,
	.io_pg_offst	= ((EP93XX_APB_VIRT_BASE) >> 18) & 0xfffc,
	.boot_params	= EP93XX_SDCE3_PHYS_BASE_SYNC + 0x100,
	.map_io		= ts72xx_map_io,
	.init_irq	= ep93xx_init_irq,
	.timer		= &ep93xx_timer,
	.init_machine	= ts72xx_init_machine,
MACHINE_END
