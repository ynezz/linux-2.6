/*
 * EP93xx ethernet network device driver
 * Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 * Dedicated to Marija Kulikova.
 * Copyright (C) 2007 Herbert Valerio Riedel <hvr@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/hardware.h>

#define DRV_NAME		"ep93xx-eth"
#define DRV_VERSION		"0.13"

#define RX_QUEUE_ENTRIES	64
#define TX_QUEUE_ENTRIES	8

#define MAX_PKT_SIZE		2044
#define PKT_BUF_SIZE		2048

#define REG_RXCTL		0x0000
#define  REG_RXCTL_DEFAULT	0x00073800
#define REG_TXCTL		0x0004
#define  REG_TXCTL_ENABLE	0x00000001
#define REG_TESTCTL		0x0008
#define  REG_TESTCTL_MFDX	0x00000040
#define REG_MIICMD		0x0010
#define  REG_MIICMD_READ	0x00008000
#define  REG_MIICMD_WRITE	0x00004000
#define REG_MIIDATA		0x0014
#define REG_MIISTS		0x0018
#define  REG_MIISTS_BUSY	0x00000001
#define REG_SELFCTL		0x0020
#define  REG_SELFCTL_RESET	0x00000001
#define  REG_SELFCTL_MDCDIV_MSK	0x00007e00
#define  REG_SELFCTL_MDCDIV_OFS	9
#define  REG_SELFCTL_PSPRS	0x00000100
#define REG_INTEN		0x0024
#define  REG_INTEN_TX		0x00000008
#define  REG_INTEN_RX		0x00000007
#define REG_INTSTSP		0x0028
#define  REG_INTSTS_TX		0x00000008
#define  REG_INTSTS_RX		0x00000004
#define REG_INTSTSC		0x002c
#define REG_AFP			0x004c
#define REG_INDAD0		0x0050
#define REG_INDAD1		0x0051
#define REG_INDAD2		0x0052
#define REG_INDAD3		0x0053
#define REG_INDAD4		0x0054
#define REG_INDAD5		0x0055
#define REG_GIINTMSK		0x0064
#define  REG_GIINTMSK_ENABLE	0x00008000
#define REG_BMCTL		0x0080
#define  REG_BMCTL_ENABLE_TX	0x00000100
#define  REG_BMCTL_ENABLE_RX	0x00000001
#define REG_BMSTS		0x0084
#define  REG_BMSTS_RX_ACTIVE	0x00000008
#define REG_RXDQBADD		0x0090
#define REG_RXDQBLEN		0x0094
#define REG_RXDCURADD		0x0098
#define REG_RXDENQ		0x009c
#define REG_RXSTSQBADD		0x00a0
#define REG_RXSTSQBLEN		0x00a4
#define REG_RXSTSQCURADD	0x00a8
#define REG_RXSTSENQ		0x00ac
#define REG_TXDQBADD		0x00b0
#define REG_TXDQBLEN		0x00b4
#define REG_TXDQCURADD		0x00b8
#define REG_TXDENQ		0x00bc
#define REG_TXSTSQBADD		0x00c0
#define REG_TXSTSQBLEN		0x00c4
#define REG_TXSTSQCURADD	0x00c8
#define REG_MAXFRMLEN		0x00e8

struct ep93xx_rdesc
{
	u32	buf_addr;
	u32	rdesc1;
};

#define RDESC1_NSOF		0x80000000
#define RDESC1_BUFFER_INDEX	0x7fff0000
#define RDESC1_BUFFER_LENGTH	0x0000ffff

struct ep93xx_rstat
{
	u32	rstat0;
	u32	rstat1;
};

#define RSTAT0_RFP		0x80000000
#define RSTAT0_RWE		0x40000000
#define RSTAT0_EOF		0x20000000
#define RSTAT0_EOB		0x10000000
#define RSTAT0_AM		0x00c00000
#define RSTAT0_RX_ERR		0x00200000
#define RSTAT0_OE		0x00100000
#define RSTAT0_FE		0x00080000
#define RSTAT0_RUNT		0x00040000
#define RSTAT0_EDATA		0x00020000
#define RSTAT0_CRCE		0x00010000
#define RSTAT0_CRCI		0x00008000
#define RSTAT0_HTI		0x00003f00
#define RSTAT1_RFP		0x80000000
#define RSTAT1_BUFFER_INDEX	0x7fff0000
#define RSTAT1_FRAME_LENGTH	0x0000ffff

struct ep93xx_tdesc
{
	u32	buf_addr;
	u32	tdesc1;
};

#define TDESC1_EOF		0x80000000
#define TDESC1_BUFFER_INDEX	0x7fff0000
#define TDESC1_BUFFER_ABORT	0x00008000
#define TDESC1_BUFFER_LENGTH	0x00000fff

struct ep93xx_tstat
{
	u32	tstat0;
};

#define TSTAT0_TXFP		0x80000000
#define TSTAT0_TXWE		0x40000000
#define TSTAT0_FA		0x20000000
#define TSTAT0_LCRS		0x10000000
#define TSTAT0_OW		0x04000000
#define TSTAT0_TXU		0x02000000
#define TSTAT0_ECOLL		0x01000000
#define TSTAT0_NCOLL		0x001f0000
#define TSTAT0_BUFFER_INDEX	0x00007fff

struct ep93xx_descs
{
	struct ep93xx_rdesc	rdesc[RX_QUEUE_ENTRIES];
	struct ep93xx_tdesc	tdesc[TX_QUEUE_ENTRIES];
	struct ep93xx_rstat	rstat[RX_QUEUE_ENTRIES];
	struct ep93xx_tstat	tstat[TX_QUEUE_ENTRIES];
};

struct ep93xx_priv
{
	struct resource		*res;
	void __iomem		*base_addr;
	int			irq;

	struct ep93xx_descs	*descs;
	dma_addr_t		descs_dma_addr;

	void			*rx_buf[RX_QUEUE_ENTRIES];
	void			*tx_buf[TX_QUEUE_ENTRIES];

	spinlock_t		rx_lock;
	unsigned int		rx_pointer;
	unsigned int		tx_clean_pointer;
	unsigned int		tx_pointer;
	spinlock_t		tx_pending_lock;
	unsigned int		tx_pending;

	struct net_device	*dev;
	struct napi_struct	napi;

	struct mii_if_info	mii;
	u8			mdc_divisor;
	int     phy_supports_mfps:1;

	struct mii_bus    mii_bus;
	struct phy_device *phy_dev;
	int     speed;
	int     duplex;
	int     link;
};

#define rdb(ep, off)		__raw_readb((ep)->base_addr + (off))
#define rdw(ep, off)		__raw_readw((ep)->base_addr + (off))
#define rdl(ep, off)		__raw_readl((ep)->base_addr + (off))
#define wrb(ep, off, val)	__raw_writeb((val), (ep)->base_addr + (off))
#define wrw(ep, off, val)	__raw_writew((val), (ep)->base_addr + (off))
#define wrl(ep, off, val)	__raw_writel((val), (ep)->base_addr + (off))

/* common MII transactions should take < 100 iterations */
#define EP93XX_PHY_TIMEOUT 2000

static int ep93xx_mdio_wait(struct mii_bus *bus)
{
	struct ep93xx_priv *ep = bus->priv;
	unsigned int timeout = EP93XX_PHY_TIMEOUT;

	while ((rdl(ep, REG_MIISTS) & REG_MIISTS_BUSY)
			&& timeout--)
		cpu_relax();

	if (timeout <= 0) {
		dev_err(&bus->dev, "MII operation timed out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int ep93xx_mdio_read(struct mii_bus *bus, int mii_id, int reg)
{
	struct ep93xx_priv *ep = bus->priv;
	u32 selfctl;
	u32 data;

	if (ep93xx_mdio_wait(bus) < 0)
		return -ETIMEDOUT;

	selfctl = rdl(ep, REG_SELFCTL);

	if (ep->phy_supports_mfps)
		wrl(ep, REG_SELFCTL, selfctl | REG_SELFCTL_PSPRS);
	else
		wrl(ep, REG_SELFCTL, selfctl & ~REG_SELFCTL_PSPRS);

	wrl(ep, REG_MIICMD, REG_MIICMD_READ | (mii_id << 5) | reg);

	if (ep93xx_mdio_wait(bus) < 0)
		return -ETIMEDOUT;

	data =  rdl(ep, REG_MIIDATA);

	wrl(ep, REG_SELFCTL, selfctl);

 	return data;
}

static int ep93xx_mdio_write(struct mii_bus *bus, int mii_id, int reg, u16 data)
{
	struct ep93xx_priv *ep = bus->priv;
	u32 selfctl;

	if (ep93xx_mdio_wait(bus) < 0)
		return -ETIMEDOUT;

	selfctl = rdl(ep, REG_SELFCTL);

	if (ep->phy_supports_mfps)
		wrl(ep, REG_SELFCTL, selfctl | REG_SELFCTL_PSPRS);
	else
		wrl(ep, REG_SELFCTL, selfctl & ~REG_SELFCTL_PSPRS);

	wrl(ep, REG_MIIDATA, data);
	wrl(ep, REG_MIICMD, REG_MIICMD_WRITE | (mii_id << 5) | reg);

	if (ep93xx_mdio_wait(bus) < 0)
		return -ETIMEDOUT;

	wrl(ep, REG_SELFCTL, selfctl);

	return 0;
}

static int ep93xx_rx(struct net_device *dev, int processed, int budget)
{
	struct ep93xx_priv *ep = netdev_priv(dev);

	while (processed < budget) {
		int entry;
		struct ep93xx_rstat *rstat;
		u32 rstat0;
		u32 rstat1;
		int length;
		struct sk_buff *skb;

		entry = ep->rx_pointer;
		rstat = ep->descs->rstat + entry;

		rstat0 = rstat->rstat0;
		rstat1 = rstat->rstat1;
		if (!(rstat0 & RSTAT0_RFP) || !(rstat1 & RSTAT1_RFP))
			break;

		rstat->rstat0 = 0;
		rstat->rstat1 = 0;

		if (!(rstat0 & RSTAT0_EOF))
			pr_crit("not end-of-frame %.8x %.8x\n", rstat0, rstat1);
		if (!(rstat0 & RSTAT0_EOB))
			pr_crit("not end-of-buffer %.8x %.8x\n", rstat0, rstat1);
		if ((rstat1 & RSTAT1_BUFFER_INDEX) >> 16 != entry)
			pr_crit("entry mismatch %.8x %.8x\n", rstat0, rstat1);

		if (!(rstat0 & RSTAT0_RWE)) {
			dev->stats.rx_errors++;
			if (rstat0 & RSTAT0_OE)
				dev->stats.rx_fifo_errors++;
			if (rstat0 & RSTAT0_FE)
				dev->stats.rx_frame_errors++;
			if (rstat0 & (RSTAT0_RUNT | RSTAT0_EDATA))
				dev->stats.rx_length_errors++;
			if (rstat0 & RSTAT0_CRCE)
				dev->stats.rx_crc_errors++;
			goto err;
		}

		length = rstat1 & RSTAT1_FRAME_LENGTH;
		if (length > MAX_PKT_SIZE) {
			pr_notice("invalid length %.8x %.8x\n", rstat0, rstat1);
			goto err;
		}

		/* Strip FCS.  */
		if (rstat0 & RSTAT0_CRCI)
			length -= 4;

		skb = dev_alloc_skb(length + 2);
		if (likely(skb != NULL)) {
			skb_reserve(skb, 2);
			dma_sync_single_for_cpu(NULL, ep->descs->rdesc[entry].buf_addr,
						length, DMA_FROM_DEVICE);
			skb_copy_to_linear_data(skb, ep->rx_buf[entry], length);
			skb_put(skb, length);
			skb->protocol = eth_type_trans(skb, dev);

			netif_receive_skb(skb);

			dev->stats.rx_packets++;
			dev->stats.rx_bytes += length;
		} else {
			dev->stats.rx_dropped++;
		}

err:
		ep->rx_pointer = (entry + 1) & (RX_QUEUE_ENTRIES - 1);
		processed++;
	}

	return processed;
}

static int ep93xx_have_more_rx(struct ep93xx_priv *ep)
{
	struct ep93xx_rstat *rstat = ep->descs->rstat + ep->rx_pointer;
	return !!((rstat->rstat0 & RSTAT0_RFP) && (rstat->rstat1 & RSTAT1_RFP));
}

static int ep93xx_poll(struct napi_struct *napi, int budget)
{
	struct ep93xx_priv *ep = container_of(napi, struct ep93xx_priv, napi);
	struct net_device *dev = ep->dev;
	int rx = 0;

poll_some_more:
	rx = ep93xx_rx(dev, rx, budget);
	if (rx < budget) {
		int more = 0;

		spin_lock_irq(&ep->rx_lock);
		__napi_complete(napi);
		wrl(ep, REG_INTEN, REG_INTEN_TX | REG_INTEN_RX);
		if (ep93xx_have_more_rx(ep)) {
			wrl(ep, REG_INTEN, REG_INTEN_TX);
			wrl(ep, REG_INTSTSP, REG_INTSTS_RX);
			more = 1;
		}
		spin_unlock_irq(&ep->rx_lock);

		if (more && napi_reschedule(napi))
			goto poll_some_more;
	}

	if (rx) {
		wrw(ep, REG_RXDENQ, rx);
		wrw(ep, REG_RXSTSENQ, rx);
	}

	return rx;
}

static int ep93xx_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	int entry;

	if (unlikely(skb->len > MAX_PKT_SIZE)) {
		dev->stats.tx_dropped++;
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	entry = ep->tx_pointer;
	ep->tx_pointer = (ep->tx_pointer + 1) & (TX_QUEUE_ENTRIES - 1);

	ep->descs->tdesc[entry].tdesc1 =
		TDESC1_EOF | (entry << 16) | (skb->len & 0xfff);
	skb_copy_and_csum_dev(skb, ep->tx_buf[entry]);
	dma_sync_single_for_cpu(NULL, ep->descs->tdesc[entry].buf_addr,
				skb->len, DMA_TO_DEVICE);
	dev_kfree_skb(skb);

	spin_lock_irq(&ep->tx_pending_lock);
	ep->tx_pending++;
	if (ep->tx_pending == TX_QUEUE_ENTRIES)
		netif_stop_queue(dev);
	spin_unlock_irq(&ep->tx_pending_lock);

	wrl(ep, REG_TXDENQ, 1);

	return NETDEV_TX_OK;
}

static void ep93xx_tx_complete(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	int wake;

	wake = 0;

	spin_lock(&ep->tx_pending_lock);
	while (1) {
		int entry;
		struct ep93xx_tstat *tstat;
		u32 tstat0;

		entry = ep->tx_clean_pointer;
		tstat = ep->descs->tstat + entry;

		tstat0 = tstat->tstat0;
		if (!(tstat0 & TSTAT0_TXFP))
			break;

		tstat->tstat0 = 0;

		if (tstat0 & TSTAT0_FA)
			pr_crit("frame aborted %.8x\n", tstat0);
		if ((tstat0 & TSTAT0_BUFFER_INDEX) != entry)
			pr_crit("entry mismatch %.8x\n", tstat0);

		if (tstat0 & TSTAT0_TXWE) {
			int length = ep->descs->tdesc[entry].tdesc1 & 0xfff;

			dev->stats.tx_packets++;
			dev->stats.tx_bytes += length;
		} else {
			dev->stats.tx_errors++;
		}

		if (tstat0 & TSTAT0_OW)
			dev->stats.tx_window_errors++;
		if (tstat0 & TSTAT0_TXU)
			dev->stats.tx_fifo_errors++;
		dev->stats.collisions += (tstat0 >> 16) & 0x1f;

		ep->tx_clean_pointer = (entry + 1) & (TX_QUEUE_ENTRIES - 1);
		if (ep->tx_pending == TX_QUEUE_ENTRIES)
			wake = 1;
		ep->tx_pending--;
	}
	spin_unlock(&ep->tx_pending_lock);

	if (wake)
		netif_wake_queue(dev);
}

static irqreturn_t ep93xx_irq(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct ep93xx_priv *ep = netdev_priv(dev);
	u32 status;

	status = rdl(ep, REG_INTSTSC);
	if (status == 0)
		return IRQ_NONE;

	if (status & REG_INTSTS_RX) {
		spin_lock(&ep->rx_lock);
		if (likely(napi_schedule_prep(&ep->napi))) {
			wrl(ep, REG_INTEN, REG_INTEN_TX);
			__napi_schedule(&ep->napi);
		}
		spin_unlock(&ep->rx_lock);
	}

	if (status & REG_INTSTS_TX)
		ep93xx_tx_complete(dev);

	return IRQ_HANDLED;
}

static void ep93xx_free_buffers(struct ep93xx_priv *ep)
{
	int i;

	for (i = 0; i < RX_QUEUE_ENTRIES; i += 2) {
		dma_addr_t d;

		d = ep->descs->rdesc[i].buf_addr;
		if (d)
			dma_unmap_single(NULL, d, PAGE_SIZE, DMA_FROM_DEVICE);

		if (ep->rx_buf[i] != NULL)
			free_page((unsigned long)ep->rx_buf[i]);
	}

	for (i = 0; i < TX_QUEUE_ENTRIES; i += 2) {
		dma_addr_t d;

		d = ep->descs->tdesc[i].buf_addr;
		if (d)
			dma_unmap_single(NULL, d, PAGE_SIZE, DMA_TO_DEVICE);

		if (ep->tx_buf[i] != NULL)
			free_page((unsigned long)ep->tx_buf[i]);
	}

	dma_free_coherent(NULL, sizeof(struct ep93xx_descs), ep->descs,
							ep->descs_dma_addr);
}

/*
 * The hardware enforces a sub-2K maximum packet size, so we put
 * two buffers on every hardware page.
 */
static int ep93xx_alloc_buffers(struct ep93xx_priv *ep)
{
	int i;

	ep->descs = dma_alloc_coherent(NULL, sizeof(struct ep93xx_descs),
				&ep->descs_dma_addr, GFP_KERNEL | GFP_DMA);
	if (ep->descs == NULL)
		return 1;

	for (i = 0; i < RX_QUEUE_ENTRIES; i += 2) {
		void *page;
		dma_addr_t d;

		page = (void *)__get_free_page(GFP_KERNEL | GFP_DMA);
		if (page == NULL)
			goto err;

		d = dma_map_single(NULL, page, PAGE_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(NULL, d)) {
			free_page((unsigned long)page);
			goto err;
		}

		ep->rx_buf[i] = page;
		ep->descs->rdesc[i].buf_addr = d;
		ep->descs->rdesc[i].rdesc1 = (i << 16) | PKT_BUF_SIZE;

		ep->rx_buf[i + 1] = page + PKT_BUF_SIZE;
		ep->descs->rdesc[i + 1].buf_addr = d + PKT_BUF_SIZE;
		ep->descs->rdesc[i + 1].rdesc1 = ((i + 1) << 16) | PKT_BUF_SIZE;
	}

	for (i = 0; i < TX_QUEUE_ENTRIES; i += 2) {
		void *page;
		dma_addr_t d;

		page = (void *)__get_free_page(GFP_KERNEL | GFP_DMA);
		if (page == NULL)
			goto err;

		d = dma_map_single(NULL, page, PAGE_SIZE, DMA_TO_DEVICE);
		if (dma_mapping_error(NULL, d)) {
			free_page((unsigned long)page);
			goto err;
		}

		ep->tx_buf[i] = page;
		ep->descs->tdesc[i].buf_addr = d;

		ep->tx_buf[i + 1] = page + PKT_BUF_SIZE;
		ep->descs->tdesc[i + 1].buf_addr = d + PKT_BUF_SIZE;
	}

	return 0;

err:
	ep93xx_free_buffers(ep);
	return 1;
}

static int ep93xx_mdio_reset(struct mii_bus *bus)
{
	struct ep93xx_priv *ep = bus->priv;

	u32 selfctl = rdl(ep, REG_SELFCTL);

	selfctl &= ~(REG_SELFCTL_MDCDIV_MSK | REG_SELFCTL_PSPRS);

	selfctl |= (ep->mdc_divisor - 1) << REG_SELFCTL_MDCDIV_OFS;
	selfctl |= REG_SELFCTL_PSPRS;

	wrl(ep, REG_SELFCTL, selfctl);

	return 0;
}

static int ep93xx_start_hw(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	unsigned long addr;
	int i;

	wrl(ep, REG_SELFCTL, REG_SELFCTL_RESET);
	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_SELFCTL) & REG_SELFCTL_RESET) == 0)
			break;
		msleep(1);
	}

	if (i == 10) {
		pr_crit("hw failed to reset\n");
		return 1;
	}

	/* The reset cleared REG_SELFCTL, so set the MDC divisor again */
	ep93xx_mdio_reset(&ep->mii_bus);

	/* Receive descriptor ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, rdesc);
	wrl(ep, REG_RXDQBADD, addr);
	wrl(ep, REG_RXDCURADD, addr);
	wrw(ep, REG_RXDQBLEN, RX_QUEUE_ENTRIES * sizeof(struct ep93xx_rdesc));

	/* Receive status ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, rstat);
	wrl(ep, REG_RXSTSQBADD, addr);
	wrl(ep, REG_RXSTSQCURADD, addr);
	wrw(ep, REG_RXSTSQBLEN, RX_QUEUE_ENTRIES * sizeof(struct ep93xx_rstat));

	/* Transmit descriptor ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, tdesc);
	wrl(ep, REG_TXDQBADD, addr);
	wrl(ep, REG_TXDQCURADD, addr);
	wrw(ep, REG_TXDQBLEN, TX_QUEUE_ENTRIES * sizeof(struct ep93xx_tdesc));

	/* Transmit status ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, tstat);
	wrl(ep, REG_TXSTSQBADD, addr);
	wrl(ep, REG_TXSTSQCURADD, addr);
	wrw(ep, REG_TXSTSQBLEN, TX_QUEUE_ENTRIES * sizeof(struct ep93xx_tstat));

	wrl(ep, REG_BMCTL, REG_BMCTL_ENABLE_TX | REG_BMCTL_ENABLE_RX);
	wrl(ep, REG_INTEN, REG_INTEN_TX | REG_INTEN_RX);
	wrl(ep, REG_GIINTMSK, 0);

	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_BMSTS) & REG_BMSTS_RX_ACTIVE) != 0)
			break;
		msleep(1);
	}

	if (i == 10) {
		pr_crit("hw failed to start\n");
		return 1;
	}

	wrl(ep, REG_RXDENQ, RX_QUEUE_ENTRIES);
	wrl(ep, REG_RXSTSENQ, RX_QUEUE_ENTRIES);

	wrb(ep, REG_INDAD0, dev->dev_addr[0]);
	wrb(ep, REG_INDAD1, dev->dev_addr[1]);
	wrb(ep, REG_INDAD2, dev->dev_addr[2]);
	wrb(ep, REG_INDAD3, dev->dev_addr[3]);
	wrb(ep, REG_INDAD4, dev->dev_addr[4]);
	wrb(ep, REG_INDAD5, dev->dev_addr[5]);
	wrl(ep, REG_AFP, 0);

	wrl(ep, REG_MAXFRMLEN, (MAX_PKT_SIZE << 16) | MAX_PKT_SIZE);

	wrl(ep, REG_RXCTL, REG_RXCTL_DEFAULT);
	wrl(ep, REG_TXCTL, REG_TXCTL_ENABLE);

	return 0;
}

static void ep93xx_stop_hw(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	int i;

	wrl(ep, REG_SELFCTL, REG_SELFCTL_RESET);
	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_SELFCTL) & REG_SELFCTL_RESET) == 0)
			break;
		msleep(1);
	}

	if (i == 10)
		pr_crit("hw failed to reset\n");
}

static int ep93xx_open(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	int err;

	if (ep93xx_alloc_buffers(ep))
		return -ENOMEM;

	napi_enable(&ep->napi);

	if (ep93xx_start_hw(dev)) {
		napi_disable(&ep->napi);
		ep93xx_free_buffers(ep);
		return -EIO;
	}

	spin_lock_init(&ep->rx_lock);
	ep->rx_pointer = 0;
	ep->tx_clean_pointer = 0;
	ep->tx_pointer = 0;
	spin_lock_init(&ep->tx_pending_lock);
	ep->tx_pending = 0;

	err = request_irq(ep->irq, ep93xx_irq, IRQF_SHARED, dev->name, dev);
	if (err) {
		napi_disable(&ep->napi);
		ep93xx_stop_hw(dev);
		ep93xx_free_buffers(ep);
		return err;
	}

	wrl(ep, REG_GIINTMSK, REG_GIINTMSK_ENABLE);

	phy_start(ep->phy_dev);

	netif_start_queue(dev);

	return 0;
}

static int ep93xx_close(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);

	napi_disable(&ep->napi);
	netif_stop_queue(dev);

	if (ep->phy_dev)
		phy_stop(ep->phy_dev);

	wrl(ep, REG_GIINTMSK, 0);
	free_irq(ep->irq, dev);
	ep93xx_stop_hw(dev);
	ep93xx_free_buffers(ep);

	return 0;
}

static int ep93xx_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct ep93xx_priv *ep = netdev_priv(dev);

	return phy_mii_ioctl(ep->phy_dev, ifr, cmd);
}

static void ep93xx_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
        strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
        strlcpy(info->version, DRV_VERSION, sizeof(info->version));
        strlcpy(info->bus_info, dev_name(dev->dev.parent), sizeof(info->bus_info));
}

static int ep93xx_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	struct phy_device *phydev = ep->phy_dev;

	if (!phydev)
		return -ENODEV;

	return phy_ethtool_gset(phydev, cmd);
}

static int ep93xx_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	struct phy_device *phydev = ep->phy_dev;

	if (!phydev)
		return -ENODEV;

	return phy_ethtool_sset(phydev, cmd);
}

static const struct ethtool_ops ep93xx_ethtool_ops = {
	.get_drvinfo		= ep93xx_get_drvinfo,
	.get_settings		= ep93xx_get_settings,
	.set_settings		= ep93xx_set_settings,
	.get_link		= ethtool_op_get_link,
};

static const struct net_device_ops ep93xx_netdev_ops = {
	.ndo_open		= ep93xx_open,
	.ndo_stop		= ep93xx_close,
	.ndo_start_xmit		= ep93xx_xmit,
	.ndo_do_ioctl		= ep93xx_ioctl,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address	= eth_mac_addr,
};

static struct net_device *ep93xx_dev_alloc(struct ep93xx_eth_data *data)
{
	struct net_device *dev;

	dev = alloc_etherdev(sizeof(struct ep93xx_priv));
	if (dev == NULL)
		return NULL;

	memcpy(dev->dev_addr, data->dev_addr, ETH_ALEN);

	dev->ethtool_ops = &ep93xx_ethtool_ops;
	dev->netdev_ops = &ep93xx_netdev_ops;

	dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM;

	return dev;
}


static int ep93xx_eth_remove(struct platform_device *pdev)
{
	struct net_device *dev;
	struct ep93xx_priv *ep;

	dev = platform_get_drvdata(pdev);
	if (dev == NULL)
		return 0;
	platform_set_drvdata(pdev, NULL);

	ep = netdev_priv(dev);

	/* @@@ Force down.  */
	unregister_netdev(dev);
	ep93xx_free_buffers(ep);

	if (ep->base_addr != NULL)
		iounmap(ep->base_addr);

	if (ep->res != NULL) {
		release_resource(ep->res);
		kfree(ep->res);
	}

	free_netdev(dev);

	return 0;
}

static void ep93xx_adjust_link(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	struct phy_device *phydev = ep->phy_dev;

	int status_change = 0;

	if (phydev->link) {
		if ((ep->speed != phydev->speed) ||
				(ep->duplex != phydev->duplex)) {
			/* speed and/or duplex state changed */
			u32 testctl = rdl(ep, REG_TESTCTL);

			if (DUPLEX_FULL == phydev->duplex)
				testctl |= REG_TESTCTL_MFDX;
			else
				testctl &= ~(REG_TESTCTL_MFDX);

			wrl(ep, REG_TESTCTL, testctl);

			ep->speed = phydev->speed;
			ep->duplex = phydev->duplex;
			status_change = 1;
		}
	}

	/* test for online/offline link transition */
	if (phydev->link != ep->link) {
		if (phydev->link) /* link went online */
			netif_tx_schedule_all(dev);
		else { /* link went offline */
			ep->speed = 0;
			ep->duplex = -1;
		}
		ep->link = phydev->link;

		status_change = 1;
	}

	if (status_change)
		phy_print_status(phydev);
}

static int ep93xx_mii_probe(struct net_device *dev, int phy_addr)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	struct phy_device *phydev = NULL;
	int val;

	if (phy_addr >= 0 && phy_addr < PHY_MAX_ADDR)
		phydev = ep->mii_bus.phy_map[phy_addr];

	if (!phydev) {
		pr_info("PHY not found at specified address,"
				" trying autodetection\n");

		/* find the first phy */
		for (phy_addr = 0; phy_addr < PHY_MAX_ADDR; phy_addr++) {
			if (ep->mii_bus.phy_map[phy_addr]) {
				phydev = ep->mii_bus.phy_map[phy_addr];
				break;
			}
		}
	}

	if (!phydev) {
		pr_err("no PHY found\n");
		return -ENODEV;
	}

	phydev = phy_connect(dev, dev_name(&phydev->dev),
			ep93xx_adjust_link, 0, PHY_INTERFACE_MODE_MII);

	if (IS_ERR(phydev)) {
		pr_err("Could not attach to PHY\n");
		return PTR_ERR(phydev);
	}

	ep->phy_supports_mfps = 0;

	val = phy_read(phydev, MII_BMSR);
	if (val < 0) {
		pr_err("failed to read MII register\n");
		return val;
	}

	if (val & 0x0040) {
		pr_info("PHY supports MII frame preamble suppression\n");
		ep->phy_supports_mfps = 1;
	}

	phydev->supported &= PHY_BASIC_FEATURES;

	phydev->advertising = phydev->supported;

	ep->link = 0;
	ep->speed = 0;
	ep->duplex = -1;
	ep->phy_dev = phydev;

	pr_info("attached PHY driver [%s] "
			"(mii_bus:phy_addr=%s, irq=%d)\n",
			phydev->drv->name, dev_name(&phydev->dev), phydev->irq);

	return 0;
}

static int ep93xx_eth_probe(struct platform_device *pdev)
{
	struct ep93xx_eth_data *data;
	struct net_device *dev;
	struct ep93xx_priv *ep;
	struct resource *mem;
	int irq;
	int err, i;

	if (pdev == NULL)
		return -ENODEV;
	data = pdev->dev.platform_data;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!mem || irq < 0)
		return -ENXIO;

	dev = ep93xx_dev_alloc(data);
	if (dev == NULL) {
		err = -ENOMEM;
		goto err_out;
	}
	ep = netdev_priv(dev);
	ep->dev = dev;
	netif_napi_add(dev, &ep->napi, ep93xx_poll, 64);

	platform_set_drvdata(pdev, dev);

	ep->res = request_mem_region(mem->start, resource_size(mem),
				     dev_name(&pdev->dev));
	if (ep->res == NULL) {
		dev_err(&pdev->dev, "Could not reserve memory region\n");
		err = -ENOMEM;
		goto err_out_request_mem_region;
	}

	ep->base_addr = ioremap(mem->start, resource_size(mem));
	if (ep->base_addr == NULL) {
		dev_err(&pdev->dev, "Failed to ioremap ethernet registers\n");
		err = -EIO;
		goto err_out_ioremap;
	}
	ep->irq = irq;

	/* mdio/mii bus */
	ep->mii_bus.state = MDIOBUS_ALLOCATED; /* see mdiobus_alloc */
	ep->mii_bus.name = "ep93xx_mii_bus";
	snprintf(ep->mii_bus.id, MII_BUS_ID_SIZE, "0");

	ep->mii_bus.read = ep93xx_mdio_read;
	ep->mii_bus.write = ep93xx_mdio_write;
	ep->mii_bus.reset = ep93xx_mdio_reset;

	ep->mii_bus.phy_mask = 0;

	ep->mii_bus.priv = ep;
	ep->mii_bus.dev = dev->dev;

	ep->mii_bus.irq = kmalloc(sizeof(int)*PHY_MAX_ADDR, GFP_KERNEL);
	if (NULL == ep->mii_bus.irq) {
		dev_err(&pdev->dev, "Could not allocate memory\n");
		err = -ENOMEM;
		goto err_out_mii_bus_irq_kmalloc;
	}

	for (i = 0; i < PHY_MAX_ADDR; i++)
		ep->mii_bus.irq[i] = PHY_POLL;

	ep->mdc_divisor = 40;	/* Max HCLK 100 MHz, min MDIO clk 2.5 MHz.  */
	ep->phy_supports_mfps = 0;	/* probe without preamble suppression */

	if (is_zero_ether_addr(dev->dev_addr))
		random_ether_addr(dev->dev_addr);

	err = register_netdev(dev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register netdev\n");
		goto err_out_register_netdev;
	}

	err = mdiobus_register(&ep->mii_bus);
	if (err) {
		dev_err(&dev->dev, "Could not register MII bus\n");
		goto err_out_mdiobus_register;
	}

	err = ep93xx_mii_probe(dev, data->phy_id);
	if (err) {
		dev_err(&dev->dev, "failed to probe MII bus\n");
		goto err_out_mii_probe;
	}

	dev_info(&dev->dev, "ep93xx on-chip ethernet, IRQ %d, %pM\n",
			ep->irq, dev->dev_addr);

	return 0;

err_out_mii_probe:
	mdiobus_unregister(&ep->mii_bus);
err_out_mdiobus_register:
	unregister_netdev(dev);
err_out_register_netdev:
	kfree(ep->mii_bus.irq);
err_out_mii_bus_irq_kmalloc:
	iounmap(ep->base_addr);
err_out_ioremap:
	release_resource(ep->res);
	kfree(ep->res);
err_out_request_mem_region:
	free_netdev(dev);
err_out:
	ep93xx_eth_remove(pdev);
	return err;
}


static struct platform_driver ep93xx_eth_driver = {
	.probe		= ep93xx_eth_probe,
	.remove		= ep93xx_eth_remove,
	.driver		= {
		.name	= "ep93xx-eth",
		.owner	= THIS_MODULE,
	},
};

static int __init ep93xx_eth_init_module(void)
{
	return platform_driver_register(&ep93xx_eth_driver);
}

static void __exit ep93xx_eth_cleanup_module(void)
{
	platform_driver_unregister(&ep93xx_eth_driver);
}

module_init(ep93xx_eth_init_module);
module_exit(ep93xx_eth_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EP93XX Ethernet driver");
MODULE_ALIAS("platform:" DRV_NAME);
