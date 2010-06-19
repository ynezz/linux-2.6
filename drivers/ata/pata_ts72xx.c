/*
 *  TS-72XX PATA driver for Technologic Systems boards.
 *
 *  Based on pata_platform.c by Paul Mundt &
 *      Alessandro Zummo <a.zummo@towertech.it>
 *  and old pata-ts72xx.c by Alessandro Zummo <a.zummo@towertech.it>
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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <scsi/scsi_host.h>
#include <linux/ata.h>
#include <linux/libata.h>

#define DRV_NAME  "pata_ts72xx"
#define DRV_VERSION "2.01"


/*
 * Provide our own set_mode() as we don't want to change anything that has
 * already been configured..
 */
static int ts72xx_set_mode(struct ata_link *link, struct ata_device **unused)
{
	struct ata_device *dev;

	ata_for_each_dev(dev, link, ENABLED) {
		if (ata_dev_enabled(dev)) {
			/* We don't really care */
			dev->pio_mode = dev->xfer_mode = XFER_PIO_0;
			dev->xfer_shift = ATA_SHIFT_PIO;
			dev->flags |= ATA_DFLAG_PIO;
			ata_dev_printk(dev, KERN_INFO, "configured for PIO\n");
		}
	}
	return 0;
}

static struct scsi_host_template ts72xx_sht = {
	ATA_PIO_SHT(DRV_NAME),
};

static struct ata_port_operations ts72xx_port_ops = {
	.inherits	= &ata_sff_port_ops,
	.set_mode	= ts72xx_set_mode,
};

static __devinit int ts72xx_pata_probe(struct platform_device *pdev)
{
	struct ata_host *host;
	struct ata_port *ap;
	int irq;

	struct resource *pata_cmd  = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct resource *pata_aux  = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	struct resource *pata_data = platform_get_resource(pdev, IORESOURCE_MEM, 2);

	if (!pata_cmd || !pata_aux || !pata_data) {
		dev_err(&pdev->dev, "missing resource(s)\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		irq = 0;  /* no irq */

	/*
	 * Now that that's out of the way, wire up the port
	 */
	host = ata_host_alloc(&pdev->dev, 1);
	if (!host)
		return -ENOMEM;
	ap = host->ports[0];

	ap->ops = &ts72xx_port_ops;
	ap->pio_mask = 0x1f; /* PIO0-4 */
	ap->flags |= ATA_FLAG_SLAVE_POSS;

	/*
	 * Use polling mode if there's no IRQ
	 */
	if (!irq) {
		ap->flags |= ATA_FLAG_PIO_POLLING;
		ata_port_desc(ap, "no IRQ, using PIO polling");
	}

	ap->ioaddr.cmd_addr = devm_ioremap(&pdev->dev, pata_cmd->start,
			pata_cmd->end - pata_cmd->start + 1);
	ap->ioaddr.ctl_addr = devm_ioremap(&pdev->dev, pata_aux->start,
			pata_aux->end - pata_aux->start + 1);

	if (!ap->ioaddr.cmd_addr || !ap->ioaddr.ctl_addr) {
		dev_err(&pdev->dev, "failed to map IO/CTL base\n");
		return -ENOMEM;
	}

	ap->ioaddr.altstatus_addr = ap->ioaddr.ctl_addr;

	ata_sff_std_ports(&ap->ioaddr);
	ap->ioaddr.data_addr = devm_ioremap(&pdev->dev, pata_data->start,
			pata_data->end - pata_data->start + 1);

	ata_port_desc(ap, "mmio cmd 0x%llx ctl 0x%llx",
			(unsigned long long)pata_cmd->start,
			(unsigned long long)pata_aux->start);

	return ata_host_activate(host, irq, irq ? ata_sff_interrupt : NULL,
			0 /* irq flags */, &ts72xx_sht);
}

static __devexit int ts72xx_pata_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ata_host *host = dev_get_drvdata(dev);

	ata_host_detach(host);

	return 0;
}

static struct platform_driver ts72xx_pata_platform_driver = {
	.probe	= ts72xx_pata_probe,
	.remove	= __devexit_p(ts72xx_pata_remove),
	.driver	= {
		.name	= "ts72xx-ide",
		.owner	= THIS_MODULE,
	},
};

static int __init ts72xx_pata_init(void)
{
	return platform_driver_register(&ts72xx_pata_platform_driver);
}

static void __exit ts72xx_pata_exit(void)
{
	platform_driver_unregister(&ts72xx_pata_platform_driver);
}

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("low-level driver for TS-72xx device PATA");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_init(ts72xx_pata_init);
module_exit(ts72xx_pata_exit);
