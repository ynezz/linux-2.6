/*
 * ts7200_flash.c - mapping for TS-7200 SBCs (8mb NOR flash)
 * No platform_device resource is used here. All is hardcoded.
 *
 * (c) Copyright 2006-2010  Matthieu Crapet <mcrapet@gmail.com>
 * Based on ts5500_flash.c by Sean Young <sean@mess.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <mach/ep93xx-regs.h>


static struct mtd_info *mymtd;

static struct map_info ts7200nor_map = {
	.name		= "Full TS-7200 NOR flash",
	.size		= 0,		/* filled in later */
	.bankwidth	= 2,
	.phys		= EP93XX_CS6_PHYS_BASE,
};

/*
 * MTD partitioning stuff
 */
#ifdef CONFIG_MTD_PARTITIONS

#define TS7200_BOOTROM_PART_SIZE	(SZ_128K)
#define TS7200_REDBOOT_PART_SIZE	(15*SZ_128K)

static struct mtd_partition ts7200_nor_parts[] =
{
	{
		.name		= "TS-BOOTROM",
		.offset		= 0,
		.size		= TS7200_BOOTROM_PART_SIZE,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "RootFS",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0,			/* filled in later */
	},
	{
		.name		= "Redboot",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,	/* up to the end */
	}
};
#endif

static int __init ts7200_nor_init(void)
{
	if (CONFIG_MTD_TS7200_NOR_SIZE <= 8)
		ts7200nor_map.size = SZ_8M;
	else
		ts7200nor_map.size = SZ_16M;

	printk(KERN_NOTICE "TS-7200 flash mapping: %ldmo at 0x%x\n",
			ts7200nor_map.size / SZ_1M, ts7200nor_map.phys);

	ts7200nor_map.virt = ioremap(ts7200nor_map.phys, ts7200nor_map.size - 1);
	if (!ts7200nor_map.virt) {
		printk("ts7200_flash: failed to ioremap\n");
		return -EIO;
	}

	simple_map_init(&ts7200nor_map);
	mymtd = do_map_probe("cfi_probe", &ts7200nor_map);
	if (mymtd) {
		mymtd->owner = THIS_MODULE;
		add_mtd_device(mymtd);
		#ifdef CONFIG_MTD_PARTITIONS
		ts7200_nor_parts[1].size = ts7200nor_map.size - TS7200_REDBOOT_PART_SIZE;
		return add_mtd_partitions(mymtd, ts7200_nor_parts, ARRAY_SIZE(ts7200_nor_parts));
		#else
		return 0;
		#endif
	}

	iounmap(ts7200nor_map.virt);
	return -ENXIO;
}

static void __exit ts7200_nor_exit(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
		mymtd = NULL;
	}
	if (ts7200nor_map.virt) {
		iounmap(ts7200nor_map.virt);
		ts7200nor_map.virt = 0;
	}
}

module_init(ts7200_nor_init);
module_exit(ts7200_nor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("MTD map driver for TS-7200 board");
MODULE_VERSION("0.1");
