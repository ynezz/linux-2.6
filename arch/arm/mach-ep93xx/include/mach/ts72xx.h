/*
 * arch/arm/mach-ep93xx/include/mach/ts72xx.h
 */

/*
 * TS72xx memory map:
 *
 * virt		phys		size
 * febff000	22000000	4K	model number register
 * febfe000	22400000	4K	options register
 * febfd000	22800000	4K	options register #2 (JP6 and TS-9420 flags)
 * febfc000	[67]0000000	4K	NAND data register
 * febfb000	[67]0400000	4K	NAND control register
 * febfa000	[67]0800000	4K	NAND busy register
 * febf9000	10800000	4K	TS-5620 RTC index register
 * febf8000	11700000	4K	TS-5620 RTC data register
 * febf7000	23400000	4K	PLD version (3 bits)
 * febf6000	22c00000	4K	RS-485 control register
 * febf5000	23000000	4K	RS-485 mode register
 */

#define TS72XX_MODEL_PHYS_BASE		0x22000000
#define TS72XX_MODEL_VIRT_BASE		0xfebff000
#define TS72XX_MODEL_SIZE		0x00001000

#define TS7XXX_MODEL_TS7200		0x00
#define TS7XXX_MODEL_TS7250		0x01
#define TS7XXX_MODEL_TS7260		0x02
#define TS7XXX_MODEL_TS7300		0x03
#define TS7XXX_MODEL_TS7400		0x04
#define TS7XXX_MODEL_MASK		0x07


#define TS72XX_OPTIONS_PHYS_BASE	0x22400000
#define TS72XX_OPTIONS_VIRT_BASE	0xfebfe000
#define TS72XX_OPTIONS_SIZE		0x00001000
#define TS72XX_OPTIONS_COM2_RS485	0x02
#define TS72XX_OPTIONS_MAX197		0x01

#define TS72XX_OPTIONS2_PHYS_BASE	0x22800000
#define TS72XX_OPTIONS2_VIRT_BASE	0xfebfd000
#define TS72XX_OPTIONS2_SIZE		0x00001000
#define TS72XX_OPTIONS2_TS9420		0x04
#define TS72XX_OPTIONS2_TS9420_BOOT	0x02

#define TS72XX_RTC_INDEX_VIRT_BASE	0xfebf9000
#define TS72XX_RTC_INDEX_PHYS_BASE	0x10800000
#define TS72XX_RTC_INDEX_SIZE		0x00001000

#define TS72XX_RTC_DATA_VIRT_BASE	0xfebf8000
#define TS72XX_RTC_DATA_PHYS_BASE	0x11700000
#define TS72XX_RTC_DATA_SIZE		0x00001000

#define TS72XX_WDT_CONTROL_PHYS_BASE	0x23800000
#define TS72XX_WDT_FEED_PHYS_BASE	0x23c00000

#define TS72XX_PLD_VERSION_VIRT_BASE	0xfebf7000
#define TS72XX_PLD_VERSION_PHYS_BASE	0x23400000
#define TS72XX_PLD_VERSION_SIZE		0x00001000

#define TS72XX_JUMPERS_MAX197_PHYS_BASE	0x10800000 // jumpers/max197 busy bit/COM1 dcd register (8-bit, read only)
#define TS72XX_MAX197_SAMPLE_PHYS_BASE	0x10f00000 // max197 sample/control register (16-bit read/8-bit write)

/*
 * RS485 option
 */
#define TS72XX_RS485_CONTROL_VIRT_BASE	0xfebf6000
#define TS72XX_RS485_CONTROL_PHYS_BASE	0x22c00000
#define TS72XX_RS485_CONTROL_SIZE	0x00001000

#define TS72XX_RS485_MODE_VIRT_BASE	0xfebf5000
#define TS72XX_RS485_MODE_PHYS_BASE	0x23000000
#define TS72XX_RS485_MODE_SIZE		0x00001000

#define TS72XX_RS485_AUTO485FD		1
#define TS72XX_RS485_AUTO485HD		2
#define TS72XX_RS485_MODE_RS232		0x00
#define TS72XX_RS485_MODE_FD		0x01
#define TS72XX_RS485_MODE_9600_HD	0x04
#define TS72XX_RS485_MODE_19200_HD	0x05
#define TS72XX_RS485_MODE_57600_HD	0x06
#define TS72XX_RS485_MODE_115200_HD	0x07

/*
 * PC/104 8-bit & 16-bit bus
 *
 * virt		phys		size
 * febf0000	11e00000	4K	PC/104 8-bit I/O
 * febef000	21e00000	4K	PC/104 16-bit I/O
 * fea00000	11a00000	1MB	PC/104 8-bit memory
 * fe900000	21a00000	1MB	PC/104 16-bit memory
 */
#define TS72XX_PC104_8BIT_IO_VIRT_BASE	0xfebf0000
#define TS72XX_PC104_8BIT_IO_PHYS_BASE	0x11e00000
#define TS72XX_PC104_8BIT_IO_SIZE	0x00001000
#define TS72XX_PC104_8BIT_MEM_VIRT_BASE	0xfea00000
#define TS72XX_PC104_8BIT_MEM_PHYS_BASE	0x11a00000
#define TS72XX_PC104_8BIT_MEM_SIZE	0x00100000

#define TS72XX_PC104_16BIT_IO_VIRT_BASE		0xfebef000
#define TS72XX_PC104_16BIT_IO_PHYS_BASE		0x21e00000
#define TS72XX_PC104_16BIT_IO_SIZE		0x00001000
#define TS72XX_PC104_16BIT_MEM_VIRT_BASE	0xfe900000
#define TS72XX_PC104_16BIT_MEM_PHYS_BASE	0x21a00000
#define TS72XX_PC104_16BIT_MEM_SIZE		0x00100000

/*
 * TS7200 specific : CompactFlash memory map
 *
 * phys		size	description
 * 11000000	7	CF registers (8-bit each), starting at 11000001
 * 10400006	2	CF aux registers (8-bit)
 * 21000000	2	CF data register (16-bit)
 */
#define TS7200_CF_CMD_PHYS_BASE		0x11000000
#define TS7200_CF_AUX_PHYS_BASE		0x10400006
#define TS7200_CF_DATA_PHYS_BASE	0x21000000

/*
 * TS7260 specific : SD card & Power Management
 *
 * phys		size	description
 * 12000000	4K	Power management register (8-bit)
 * 13000000	4K	SD card registers (4 x 8-bit)
 */
#define TS7260_POWER_MANAGEMENT_PHYS_BASE	0x12000000
#define TS7260_PM_RS232_LEVEL_CONVERTER	0x01
#define TS7260_PM_USB			0x02
#define TS7260_PM_LCD			0x04
#define TS7260_PM_5V_SWITCHER		0x08
#define TS7260_PM_PC104_CLOCK		0x10
#define TS7260_PM_PC104_FAST_STROBES	0x20
#define TS7260_PM_TTL_UART_ENABLE	0x40
#define TS7260_PM_SCRATCH_BIT		0x80

#define TS7260_SDCARD_PHYS_BASE		0x13000000

#define  TS7300_ETHOC_PHYS_BASE 		0x72100000
#define  TS7300_ETHOC_IO_BASE 			0x72102000

#ifndef __ASSEMBLY__

static inline int board_is_ts7200(void)
{
	return (__raw_readb(TS72XX_MODEL_VIRT_BASE) &
			TS7XXX_MODEL_MASK) == TS7XXX_MODEL_TS7200;
}

static inline int board_is_ts7250(void)
{
	return (__raw_readb(TS72XX_MODEL_VIRT_BASE) &
			TS7XXX_MODEL_MASK) == TS7XXX_MODEL_TS7250;
}

static inline int board_is_ts7260(void)
{
	return (__raw_readb(TS72XX_MODEL_VIRT_BASE) &
			TS7XXX_MODEL_MASK) == TS7XXX_MODEL_TS7260;
}

static inline int board_is_ts7300(void)
{
	return (__raw_readb(TS72XX_MODEL_VIRT_BASE) &
			TS7XXX_MODEL_MASK) == TS7XXX_MODEL_TS7300;
}

static inline int board_is_ts7400(void)
{
	return (__raw_readb(TS72XX_MODEL_VIRT_BASE) &
			TS7XXX_MODEL_MASK) == TS7XXX_MODEL_TS7400;
}

static inline int is_max197_installed(void)
{
	return !!(__raw_readb(TS72XX_OPTIONS_VIRT_BASE) &
			TS72XX_OPTIONS_MAX197);
}

static inline int is_ts9420_installed(void)
{
	return !!(__raw_readb(TS72XX_OPTIONS2_VIRT_BASE) &
			TS72XX_OPTIONS2_TS9420);
}

static inline int is_rs485_installed(void)
{
	return !!(__raw_readb(TS72XX_OPTIONS_VIRT_BASE) &
			TS72XX_OPTIONS_COM2_RS485);
}

static inline int get_ts72xx_pld_version(void)
{
	return (__raw_readb(TS72XX_PLD_VERSION_VIRT_BASE) & 0x7);
}

/* User jumper */
static inline int is_jp6_set(void)
{
	return (__raw_readb(TS72XX_OPTIONS2_VIRT_BASE) & 0x1);
}

#endif
