/*
 * arch/arm/mach-ep93xx/include/mach/dma.h
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#include <linux/list.h>
#include <linux/types.h>

struct ep93xx_dma_buffer {
	struct list_head	list;
	u32			bus_addr;
	u32			bus_addr2; /* only used by M2M */
	u16			size;
};

struct ep93xx_dma_m2p_client {
	char			*name;
	u8			flags;
	void			*cookie;
	void			(*buffer_started)(void *cookie,
					struct ep93xx_dma_buffer *buf);
	void			(*buffer_finished)(void *cookie,
					struct ep93xx_dma_buffer *buf,
					int bytes, int error);

	/* Internal to the DMA code.  */
	void			*channel;
};

/* flags (m2p client) */
#define EP93XX_DMA_M2P_PORT_I2S1	0x00
#define EP93XX_DMA_M2P_PORT_I2S2	0x01
#define EP93XX_DMA_M2P_PORT_AAC1	0x02
#define EP93XX_DMA_M2P_PORT_AAC2	0x03
#define EP93XX_DMA_M2P_PORT_AAC3	0x04
#define EP93XX_DMA_M2P_PORT_I2S3	0x05
#define EP93XX_DMA_M2P_PORT_UART1	0x06
#define EP93XX_DMA_M2P_PORT_UART2	0x07
#define EP93XX_DMA_M2P_PORT_UART3	0x08
#define EP93XX_DMA_M2P_PORT_IRDA	0x09
#define EP93XX_DMA_M2P_PORT_MASK	0x0f
#define EP93XX_DMA_M2P_TX		0x00
#define EP93XX_DMA_M2P_RX		0x10
#define EP93XX_DMA_M2P_ABORT_ON_ERROR	0x20
#define EP93XX_DMA_M2P_IGNORE_ERROR	0x40
#define EP93XX_DMA_M2P_ERROR_MASK	0x60


struct ep93xx_dma_m2m_client {
	char			*name;
	u32			flags;
	void			*cookie;
	void			(*buffer_started)(void *cookie,
					struct ep93xx_dma_buffer *buf);
	void			(*buffer_finished)(void *cookie,
					struct ep93xx_dma_buffer *buf,
					int bytes, int error);

	/* Internal to the DMA code.  */
	void			*channel;
};

/* flags (m2m client) */
#define EP93XX_DMA_M2M_RX		0x000	/* read from periph./memory */
#define EP93XX_DMA_M2M_TX		0x004	/* write to periph./memory */
#define EP93XX_DMA_M2M_DIR_MASK		0x004	/* direction mask */
#define EP93XX_DMA_M2M_DEV_EXT		0x000	/* external peripheral */
#define EP93XX_DMA_M2M_DEV_SSP		0x001	/* internal SSP */
#define EP93XX_DMA_M2M_DEV_IDE		0x002	/* internal IDE */
#define EP93XX_DMA_M2M_DEV_MEM		0x003	/* memory to memory transfer */
#define EP93XX_DMA_M2M_DEV_MASK		0x003   /* device mask */
#define EP93XX_DMA_M2M_EXT_FIFO		0x008	/* external peripheral is one location fifo */
#define EP93XX_DMA_M2M_EXT_NO_HDSK	0x010	/* external peripheral doesn't require regular handshaking protocol */
#define EP93XX_DMA_M2M_EXT_WIDTH_MASK	0x300
#define EP93XX_DMA_M2M_EXT_WIDTH_BYTE	0x000	/* external peripheral transfer is one byte width */
#define EP93XX_DMA_M2M_EXT_WIDTH_2BYTES	0x100
#define EP93XX_DMA_M2M_EXT_WIDTH_4BYTES	0x200
#define EP93XX_DMA_M2M_MEM_SPEED_FULL	0x000	/* M2M bandwidth control */
#define EP93XX_DMA_M2M_MEM_SPEED_HALF	0x040	/* half bus bandwidth */
#define EP93XX_DMA_M2M_MEM_SPEED_QUART	0x080	/* quarter bus bandwidth */
#define EP93XX_DMA_M2M_MEM_SPEED_SLOW	0x0c0	/* slowest speed */
#define EP93XX_DMA_M2M_MEM_SPEED_MASK	0x0c0   /* memory speed mask */
#define EP93XX_DMA_M2M_MEM_FILL		0x020	/* M2M is one location to block fill */

/* FIXME */
#define CTRL_PWSC_MASK		0xfe000000	/* peripheral wait states count */
#define CTRL_PWSC_SHIFT		25
#define EP93XX_DREQ_SHIFT	19
#define EP93XX_DREQ_MASK	0x00180000
#define EP93XX_DMA_M2M_DREQ_LS_L	(00 << EP93XX_DREQ_SHIFT)
#define EP93XX_DMA_M2M_DREQ_LS_H	(01 << EP93XX_DREQ_SHIFT)
#define EP93XX_DMA_M2M_DREQ_ES_L	(10 << EP93XX_DREQ_SHIFT)
#define EP93XX_DMA_M2M_DREQ_ES_H	(11 << EP93XX_DREQ_SHIFT)

/* See ep93xx_dma_m2m_client_register (channel_spec) */
#define EP93XX_DMA_M2M_REQUIRES_CH_ANY	0
#define EP93XX_DMA_M2M_REQUIRES_CH_0	1
#define EP93XX_DMA_M2M_REQUIRES_CH_1	2

int  ep93xx_dma_m2p_client_register(struct ep93xx_dma_m2p_client *m2p);
void ep93xx_dma_m2p_client_unregister(struct ep93xx_dma_m2p_client *m2p);
void ep93xx_dma_m2p_submit(struct ep93xx_dma_m2p_client *m2p,
			   struct ep93xx_dma_buffer *buf);
void ep93xx_dma_m2p_submit_recursive(struct ep93xx_dma_m2p_client *m2p,
				     struct ep93xx_dma_buffer *buf);
void ep93xx_dma_m2p_flush(struct ep93xx_dma_m2p_client *m2p);

int  ep93xx_dma_m2m_client_register(struct ep93xx_dma_m2m_client *m2m,
				    int channel_spec);
void ep93xx_dma_m2m_client_unregister(struct ep93xx_dma_m2m_client *m2m);
void ep93xx_dma_m2m_submit(struct ep93xx_dma_m2m_client *m2m,
			   struct ep93xx_dma_buffer *buf);
void ep93xx_dma_m2m_flush(struct ep93xx_dma_m2m_client *m2m);
void ep93xx_dma_m2m_start(struct ep93xx_dma_m2m_client *m2m);
void ep93xx_dma_m2m_stop(struct ep93xx_dma_m2m_client *m2m);
void ep93xx_dma_m2m_set_direction(struct ep93xx_dma_m2m_client *m2m,
				  int direction);

#endif /* __ASM_ARCH_DMA_H */
