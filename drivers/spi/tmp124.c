/*
 *  TMP124 SPI protocol driver
 *
 * (c) Copyright 2008-2010  Matthieu Crapet <mcrapet@gmail.com>
 * Based on tle62x0.c by Ben Dooks, <ben@simtec.co.uk>
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
 * Note: The chip uses a '3-wire SPI' (miso and mosi are the same pin).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/sched.h>

struct tmp124_state {
	struct spi_device *bus;
	u8 tx_buff[2];
	u8 rx_buff[2];
};

static inline int tmp124_write_then_read(struct tmp124_state *st)
{
	struct spi_message msg;
	struct spi_transfer xfer[2] = {
		{
			.tx_buf		= st->tx_buff,
			.rx_buf		= NULL,
			.len		= 2,
		}, {
			.tx_buf		= NULL,
			.rx_buf		= st->rx_buff,
			.len		= 2,
		}
	};

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[0], &msg);
	spi_sync(st->bus, &msg);

	/* SPI_3WIRE is not handled by ep93xx_spi, the 2 messages must be
	 * splitted. We must wait to not confuse driver with read/write. */
	schedule_timeout(usecs_to_jiffies(1000));

	spi_message_init(&msg);
	spi_message_add_tail(&xfer[1], &msg);
	return spi_sync(st->bus, &msg);
}

static ssize_t tmp124_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmp124_state *st = dev_get_drvdata(dev);
	int ret;

	((short *)st->tx_buff)[0] = 0x8000;

	ret = tmp124_write_then_read(st);
	if (ret < 0) {
		dev_err(&st->bus->dev, "tmp124_write_then_read\n");
		ret = 0;
	} else {
		signed long val = ((short *)st->rx_buff)[0];

		val = val >> 3;

		/* 2 digit precision (0.0625*100) */
		val = (val * 50) / 8;
		ret = snprintf(buf, PAGE_SIZE, "%ld.%02ld\n", val/100, abs(val%100));
	}
	return ret;
}

static DEVICE_ATTR(temperature, S_IRUGO, tmp124_temperature_show, NULL);

static int __devinit tmp124_probe(struct spi_device *spi)
{
	struct tmp124_state *st;
	int ret;

	st = kzalloc(sizeof(struct tmp124_state), GFP_KERNEL);
	if (st == NULL) {
		dev_err(&spi->dev, "no memory for device state\n");
		return -ENOMEM;
	}

	/* required config */
	spi->bits_per_word = 16;
        spi->mode = SPI_MODE_0;

	ret = spi_setup(spi);
	if (ret) {
		dev_err(&spi->dev, "setup device\n");
		goto err;
	}

	ret = device_create_file(&spi->dev, &dev_attr_temperature);
	if (ret) {
		dev_err(&spi->dev, "cannot create temperature attribute\n");
		goto err;
	}

	st->bus = spi;
	spi_set_drvdata(spi, st);
	return 0;

err:
	kfree(st);
	return ret;
}

static int __devexit tmp124_remove(struct spi_device *spi)
{
	struct tmp124_state *st = spi_get_drvdata(spi);

	device_remove_file(&spi->dev, &dev_attr_temperature);
	kfree(st);
	return 0;
}

static struct spi_driver tmp124_driver = {
	.driver = {
		.name	= "tmp124",
		.owner	= THIS_MODULE,
	},
	.probe	= tmp124_probe,
	.remove	= __devexit_p(tmp124_remove),
};

static __init int tmp124_init(void)
{
	return spi_register_driver(&tmp124_driver);
}

static __exit void tmp124_exit(void)
{
	spi_unregister_driver(&tmp124_driver);
}

module_init(tmp124_init);
module_exit(tmp124_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("TMP124 SPI Protocol Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:tmp124");
MODULE_VERSION("0.2");
