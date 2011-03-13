/**
 * DOC: EP93xx DMA M2P memory to peripheral and peripheral to memory engine
 *
 * The EP93xx DMA M2P subsystem handles DMA transfers between memory and
 * peripherals. DMA M2P channels are available for audio, UARTs and IrDA.
 * See chapter 10 of the EP93xx users guide for full details on the DMA M2P
 * engine.
 *
 * See sound/soc/ep93xx/ep93xx-pcm.c for an example use of the DMA M2P code.
 *
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#include <linux/list.h>
#include <linux/types.h>

/**
 * struct ep93xx_dma_buffer - Information about a buffer to be transferred
 * using the DMA M2P engine
 *
 * @list: Entry in DMA buffer list
 * @bus_addr: Physical address of the buffer
 * @size: Size of the buffer in bytes
 */
struct ep93xx_dma_buffer {
	struct list_head	list;
	u32			bus_addr;
	u32			bus_addr2; /* only used by M2M */
	u16			size;
};

/**
 * struct ep93xx_dma_m2p_client - Information about a DMA M2P client
 *
 * @name: Unique name for this client
 * @flags: Client flags
 * @cookie: User data to pass to callback functions
 * @buffer_started: Non NULL function to call when a transfer is started.
 * 			The arguments are the user data cookie and the DMA
 *			buffer which is starting.
 * @buffer_finished: Non NULL function to call when a transfer is completed.
 *			The arguments are the user data cookie, the DMA buffer
 *			which has completed, and a boolean flag indicating if
 *			the transfer had an error.
 */
struct ep93xx_dma_m2p_client {
	char			*name;
	u8			flags;
	void			*cookie;
	void			(*buffer_started)(void *cookie,
					struct ep93xx_dma_buffer *buf);
	void			(*buffer_finished)(void *cookie,
					struct ep93xx_dma_buffer *buf,
					int bytes, int error);

	/* private: Internal use only */
	void			*channel;
};

/* DMA M2P ports */
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

/* DMA M2P client flags */
#define EP93XX_DMA_M2P_TX		0x00	/* Memory to peripheral */
#define EP93XX_DMA_M2P_RX		0x10	/* Peripheral to memory */

/*
 * DMA M2P client error handling flags. See the EP93xx users guide
 * documentation on the DMA M2P CONTROL register for more details
 */
#define EP93XX_DMA_M2P_ABORT_ON_ERROR	0x20	/* Abort on peripheral error */
#define EP93XX_DMA_M2P_IGNORE_ERROR	0x40	/* Ignore peripheral errors */
#define EP93XX_DMA_M2P_ERROR_MASK	0x60	/* Mask of error bits */

/**
 * ep93xx_dma_m2p_client_register - Register a client with the DMA M2P
 * subsystem
 *
 * @m2p: Client information to register
 * returns 0 on success
 *
 * The DMA M2P subsystem allocates a channel and an interrupt line for the DMA
 * client
 */
int ep93xx_dma_m2p_client_register(struct ep93xx_dma_m2p_client *m2p);

/**
 * ep93xx_dma_m2p_client_unregister - Unregister a client from the DMA M2P
 * subsystem
 *
 * @m2p: Client to unregister
 *
 * Any transfers currently in progress will be completed in hardware, but
 * ignored in software.
 */
void ep93xx_dma_m2p_client_unregister(struct ep93xx_dma_m2p_client *m2p);

/**
 * ep93xx_dma_m2p_submit - Submit a DMA M2P transfer
 *
 * @m2p: DMA Client to submit the transfer on
 * @buf: DMA Buffer to submit
 *
 * If the current or next transfer positions are free on the M2P client then
 * the transfer is started immediately. If not, the transfer is added to the
 * list of pending transfers. This function must not be called from the
 * buffer_finished callback for an M2P channel.
 *
 */
void ep93xx_dma_m2p_submit(struct ep93xx_dma_m2p_client *m2p,
			   struct ep93xx_dma_buffer *buf);

/**
 * ep93xx_dma_m2p_submit_recursive - Put a DMA transfer on the pending list
 * for an M2P channel
 *
 * @m2p: DMA Client to submit the transfer on
 * @buf: DMA Buffer to submit
 *
 * This function must only be called from the buffer_finished callback for an
 * M2P channel. It is commonly used to add the next transfer in a chained list
 * of DMA transfers.
 */
void ep93xx_dma_m2p_submit_recursive(struct ep93xx_dma_m2p_client *m2p,
				     struct ep93xx_dma_buffer *buf);

/**
 * ep93xx_dma_m2p_flush - Flush all pending transfers on a DMA M2P client
 *
 * @m2p: DMA client to flush transfers on
 *
 * Any transfers currently in progress will be completed in hardware, but
 * ignored in software.
 *
 */
void ep93xx_dma_m2p_flush(struct ep93xx_dma_m2p_client *m2p);

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
