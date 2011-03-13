/*
 * TS SD Card device driver
 *
 * (c) Copyright 2010  Matthieu Crapet <mcrapet@gmail.com>
 * Based on Technologic Systems & Breton M. Saunders work
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Notes:
 *   - request processing method is: no request queue
 *   - no M2M DMA is used
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/delay.h>
#include <linux/hdreg.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include "sdcore2.h"

#define SDCARD_DEV_NAME          "tssd" /* will appear in /proc/partitions & /sys/class/block */
#define SD_SHIFT                 4      /* max 16 partitions = 1 << 4 */

#define KERN_SECTOR_SIZE         512    /* in bytes */
#define HARD_SECTOR_SIZE         512    /* in bytes */
#define HARD_2_KERN_SECTOR_RATIO 1      /* 1 kernel sector = 1 hardware sector */


struct ts72xx_sdcard_device {
	struct sdcore tssdcore;         /* Physical core layer */
	void __iomem *mmio_base;
	long size;                      /* Device size in (hardware) sectors */
	int id;
	int media_change;
	int users;

	spinlock_t lock;
	struct device *dev;
	struct request_queue *queue;
	struct gendisk *disk;
};


/*
 * Low level function to handle an I/O request
 */
static inline int sdcard_ll_transfer(struct ts72xx_sdcard_device *dev,
		unsigned long sector, unsigned long nsect, char *buffer, int rw)
{
	int ret;

	if ((sector + nsect) > (dev->size * HARD_2_KERN_SECTOR_RATIO)) {
		dev_err(dev->dev, "tranfer: beyond-end write (%ld %ld)\n", sector, nsect);
		return -1;
	}

	switch (rw) {
		case WRITE:
			ret = sdwrite(&dev->tssdcore, sector, buffer, nsect);
			if (ret && !dev->tssdcore.sd_wprot) {
				sdreset(&dev->tssdcore);
				ret = sdwrite(&dev->tssdcore, sector, buffer, nsect);
			}
			break;

		case READ:
		case READA:
			ret = sdread(&dev->tssdcore, sector, buffer, nsect);
			if (ret) {
				// SDCARD RESET may be printed when the core determines that the SD card has
				// f*ed up.this is not handled correctly yet; and should likely be inside a while loop
				dev_err(dev->dev, "transfer: SDCARD RESET\n");
				sdreset(&dev->tssdcore);
				ret = sdread(&dev->tssdcore, sector, buffer, nsect);
			}
			break;
	}

	return 0;
}

/*
 * The direct make request version.
 */
static int sdcard_make_request(struct request_queue *q, struct bio *bio)
{
	struct ts72xx_sdcard_device *dev = q->queuedata;

	struct bio_vec *bvec;
	sector_t sector;
	int i, rw;
	int err = -EIO;

	/* handle bio */
	sector = bio->bi_sector;
	rw = bio_rw(bio);

	bio_for_each_segment(bvec, bio, i) {
		char *buffer = __bio_kmap_atomic(bio, i, KM_USER0);
		unsigned int len = bvec->bv_len / HARD_SECTOR_SIZE;

		//printk("bvec: len=%d offt=%d page=%p\n", bvec->bv_len, bvec->bv_offset, bvec->bv_page);

		err = sdcard_ll_transfer(dev, sector, len, buffer, rw);
		if (err)
			break;

		sector += len;
		__bio_kunmap_atomic(bio, KM_USER0);
	}
	bio_endio(bio, err);

	return 0;
}

static void sdcard_delay(void *arg, unsigned int us)
{
	udelay(us);
}

static int sdcard_open(struct block_device *bdev, fmode_t mode)
{
	struct ts72xx_sdcard_device *dev = bdev->bd_disk->private_data;
	unsigned long flags;

	dev_dbg(dev->dev, "open() users=%i\n", dev->users + 1);

	spin_lock_irqsave(&dev->lock, flags);
	dev->users++;
	spin_unlock_irqrestore(&dev->lock, flags);

	check_disk_change(bdev);
	return 0;
};

static int sdcard_release(struct gendisk *disk, fmode_t mode)
{
	struct ts72xx_sdcard_device *dev = disk->private_data;
	unsigned long flags;

	dev_dbg(dev->dev, "release() users=%i\n", dev->users - 1);

	spin_lock_irqsave(&dev->lock, flags);
	dev->users--;
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

static int sdcard_media_changed(struct gendisk *disk)
{
	struct ts72xx_sdcard_device *dev = disk->private_data;

	char buf[HARD_SECTOR_SIZE];
	dev->media_change = sdread(&dev->tssdcore, 1, buf, 1);

	dev_dbg(dev->dev, "media_changed() %i\n", dev->media_change);
	return dev->media_change;
}

static int sdcard_revalidate(struct gendisk *disk)
{
	struct ts72xx_sdcard_device *dev = disk->private_data;
	int ret = 0;

	dev_dbg(dev->dev, "revalidate() %i\n", dev->media_change);
	if (dev->media_change) {
		dev->size = sdreset(&dev->tssdcore);
		set_disk_ro(dev->disk, !!(dev->tssdcore.sd_wprot));
		if (dev->size > 0) {
			set_capacity(dev->disk, dev->size * HARD_2_KERN_SECTOR_RATIO);
			dev->media_change = 0;
		} else {
			dev_err(dev->dev, "revalidate() no card found\n");
			ret = -1;
		}
	}
	return ret;
}

static int sdcard_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct gendisk *disk = bdev->bd_disk;
	struct ts72xx_sdcard_device *dev = disk->private_data;

	/* We don't have real geometry info, but let's at least return
	 * values consistent with the size of the device */
	geo->heads = 16;
	geo->sectors = 32;
	geo->cylinders = get_capacity(disk) / (16 * 32);

	dev_dbg(dev->dev, "getgeo() %d heads, %d sectors, %d cylinders\n",
			geo->heads, geo->sectors, geo->cylinders);
	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations ts72xx_sdcard_ops = {
	.owner			= THIS_MODULE,
	.open			= sdcard_open,
	.release		= sdcard_release,
	.media_changed		= sdcard_media_changed,
	.revalidate_disk	= sdcard_revalidate,
	.getgeo			= sdcard_getgeo
};

static int sdcard_major;

/* ---------------------------------------------------------------------
 * Device setup
 */

static int ts72xx_sdcard_setup(const char *name, struct ts72xx_sdcard_device *dev)
{
	int rc;

	spin_lock_init(&dev->lock);

	/*
	 * Initialize the request queue
	 */
	dev->queue = blk_alloc_queue(GFP_KERNEL);
	if (!dev->queue)
		goto err_alloc_queue;

	dev->queue->queuedata = dev;
	blk_queue_make_request(dev->queue, sdcard_make_request);
	blk_queue_logical_block_size(dev->queue, HARD_SECTOR_SIZE);

	dev->tssdcore.sd_regstart = (unsigned char *)dev->mmio_base;
	dev->tssdcore.os_arg       = dev;
	dev->tssdcore.os_delay     = sdcard_delay;
	dev->tssdcore.os_dmastream = NULL;
	dev->tssdcore.os_dmaprep   = NULL;

	// don't want to write park
	dev->tssdcore.sd_writeparking = 1;
	// I do want to pre-erase blocks - 8 blocks pre-erase
	dev->tssdcore.sd_erasehint = 8;
	dev->tssdcore.sdboot_token = 0;

	dev->disk = alloc_disk(1 << SD_SHIFT);
	if (!dev->disk) {
		goto err_alloc_disk;
	}

	dev->disk->major = sdcard_major;
	dev->disk->first_minor = dev->id << SD_SHIFT;
	dev->disk->flags = GENHD_FL_REMOVABLE;
	dev->disk->fops = &ts72xx_sdcard_ops;
	dev->disk->queue = dev->queue;
	dev->disk->private_data = dev;
	snprintf(dev->disk->disk_name, 32, SDCARD_DEV_NAME "%c", dev->id + 'a');

	/* SD Card size and Reset
	 * (set_disk_ro, set_capacity will be called) */
	dev->media_change = 1;
	rc = sdcard_revalidate(dev->disk);
	if (rc) {
		dev_info(dev->dev, "No SD card detected!\n");
		goto err_alloc_disk;
	}

	dev_info(dev->dev, "SD card hardware revision: %08x\n",
			dev->tssdcore.sdcore_version);
	dev_info(dev->dev, "block device major number = %d\n",
			sdcard_major);
	dev_info(dev->dev, "New SD card detected, name=%s size=%ld (sectors)\n",
			dev->disk->disk_name, dev->size);

	/* Make the sysace device 'live' */
	add_disk(dev->disk);

	return 0;

err_alloc_disk:
	blk_cleanup_queue(dev->queue);
err_alloc_queue:
	return -ENOMEM;
}


/* ---------------------------------------------------------------------
 * Platform drivers functons
 */

static int __init ts72xx_sdcard_probe(struct platform_device *pdev)
{
	struct ts72xx_sdcard_device *dev;
	struct resource *res;
	int rc;

	dev = kzalloc(sizeof(struct ts72xx_sdcard_device), GFP_KERNEL);
	if (!dev) {
		rc = -ENOMEM;
		goto fail_no_mem;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		rc = -ENXIO;
		goto fail_no_mem_resource;
	}

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (res == NULL) {
		rc = -EBUSY;
		goto fail_no_mem_resource;
	}

	dev->mmio_base = ioremap(res->start, resource_size(res));
	if (dev->mmio_base == NULL) {
		rc = -ENXIO;
		goto fail_no_ioremap;
	}

	dev->dev = &pdev->dev;
	dev->id = pdev->id;
	platform_set_drvdata(pdev, dev);

	rc = ts72xx_sdcard_setup(SDCARD_DEV_NAME, dev);
	if (rc) {
		dev_err(dev->dev, "ts72xx_sdcard_setup failed\n");
		goto fail_sdcard_setup;
	}

	return 0;

fail_sdcard_setup:
	iounmap(dev->mmio_base);
fail_no_ioremap:
	release_mem_region(res->start, resource_size(res));
fail_no_mem_resource:
	kfree(dev);
fail_no_mem:
	return rc;
}

static int __exit ts72xx_sdcard_remove(struct platform_device *pdev)
{
	struct ts72xx_sdcard_device *dev = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	platform_set_drvdata(pdev, NULL);
	iounmap(dev->mmio_base);
	release_mem_region(res->start, resource_size(res));
	blk_cleanup_queue(dev->queue);
	del_gendisk(dev->disk);
	put_disk(dev->disk);
	kfree(dev);

	return 0;
}

static struct platform_driver ts72xx_sdcard_driver = {
	.driver		= {
		.name	= "ts72xx-sdcard",
		.owner	= THIS_MODULE,
	},
	.remove		= __exit_p(ts72xx_sdcard_remove),
};


/* ---------------------------------------------------------------------
 * Module init/exit routines
 */

static int __init ts72xx_sdcard_init(void)
{
	int rc;

	sdcard_major = rc = register_blkdev(sdcard_major, SDCARD_DEV_NAME);
	if (rc <= 0) {
		printk(KERN_ERR "%s:%u: register_blkdev failed %d\n", __func__,
				__LINE__, rc);
		return rc;
	}

	rc = platform_driver_probe(&ts72xx_sdcard_driver, ts72xx_sdcard_probe);
	if (rc)
		unregister_blkdev(sdcard_major, SDCARD_DEV_NAME);

	return rc;
}

static void __exit ts72xx_sdcard_exit(void)
{
	unregister_blkdev(sdcard_major, SDCARD_DEV_NAME);
	platform_driver_unregister(&ts72xx_sdcard_driver);
}

module_init(ts72xx_sdcard_init);
module_exit(ts72xx_sdcard_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("TS72xx SD Card block driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK0_MAJOR);
MODULE_ALIAS("tssd");
