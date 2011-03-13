/* hd44780.c
 *
 * $Id: hd44780.c,v 1.181 2010/03/03 14:56:22 mjona Exp $
 *
 * LCD-Linux:
 * Driver for HD44780 compatible displays connected to the parallel port.
 *
 * Copyright (C) 2005 - 2009  Mattia Jona-Lasinio (mjona@users.sourceforge.net)
 * Based on the code for Sim.One Hardware by Nuccio Raciti (raciti.nuccio@gmail.com)
 * Modified for ts72xx hardware by Petr Stetiar (ynezz@true.cz)
 * (Only 8-bit mode supported for now.)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef CONFIG_PROC_FS
#define USE_PROC
#else
#undef USE_PROC
#endif

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/delay.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/ioport.h>

#ifdef USE_PROC
#include <linux/proc_fs.h>
#endif

#define LCD_LINUX_MAIN
#include <linux/hd44780.h>

#include "charmap.h"
#include "commands.h"
#include "config.h"

#define LCD_EN   	EP93XX_GPIO_LINE_H(3)
#define LCD_RS   	EP93XX_GPIO_LINE_H(4)
#define LCD_WR   	EP93XX_GPIO_LINE_H(5)
#define LCD_DATA0	EP93XX_GPIO_LINE_A(0)
#define LCD_DATA1	EP93XX_GPIO_LINE_A(1)
#define LCD_DATA2	EP93XX_GPIO_LINE_A(2)
#define LCD_DATA3	EP93XX_GPIO_LINE_A(3)
#define LCD_DATA4	EP93XX_GPIO_LINE_A(4)
#define LCD_DATA5	EP93XX_GPIO_LINE_A(5)
#define LCD_DATA6	EP93XX_GPIO_LINE_A(6)
#define LCD_DATA7	EP93XX_GPIO_LINE_A(7)

static struct gpio lcd_gpios[] = {
	{ LCD_EN, GPIOF_OUT_INIT_LOW, "LCD enable" },
	{ LCD_RS, GPIOF_OUT_INIT_LOW,  "LCD data" },
	{ LCD_WR, GPIOF_OUT_INIT_LOW,  "LCD r/w" },
	{ LCD_DATA0, GPIOF_OUT_INIT_LOW,  "LCD data0" },
	{ LCD_DATA1, GPIOF_OUT_INIT_LOW,  "LCD data1" },
	{ LCD_DATA2, GPIOF_OUT_INIT_LOW,  "LCD data2" },
	{ LCD_DATA3, GPIOF_OUT_INIT_LOW,  "LCD data3" },
	{ LCD_DATA4, GPIOF_OUT_INIT_LOW,  "LCD data4" },
	{ LCD_DATA5, GPIOF_OUT_INIT_LOW,  "LCD data5" },
	{ LCD_DATA6, GPIOF_OUT_INIT_LOW,  "LCD data6" },
	{ LCD_DATA7, GPIOF_OUT_INIT_LOW,  "LCD data7" },
};

/** Function prototypes **/
static void read_display(unsigned char *byte, unsigned char bitmask);
static void write_display(unsigned char byte, unsigned char bitmask);

/* Initialization */
static int hd44780_validate_driver(void);
static int hd44780_init_port(void);
static int hd44780_cleanup_port(void);
static int hd44780_init_display(void);
static int hd44780_cleanup_display(void);

/* Write */
static void hd44780_address_mode(int);
static void hd44780_clear_display(void);
static void hd44780_write_char(unsigned int, unsigned short);
static void hd44780_write_cgram_char(unsigned char, unsigned char *);

/* Read */
static void check_bf(unsigned char);
static void hd44780_read_char(unsigned int, unsigned short *);
static void hd44780_read_cgram_char(unsigned char, unsigned char *);

/* Input handling */
static int hd44780_handle_custom_char(unsigned int);
static int hd44780_handle_custom_ioctl(unsigned int, unsigned long, unsigned int);

/* Proc operations */
#ifdef USE_PROC
static void create_proc_entries(void);
static void remove_proc_entries(void);
#endif

/* hd44780_flags */
#define _CHECK_BF	0	/* Do busy-flag checking */
#define _4BITS_BUS	1	/* The bus is 4 bits long */
#define _5X10_FONT	2	/* Use 5x10 font */
#define CURSOR_BLINK	3	/* Make the cursor blinking */
#define SHOW_CURSOR	4	/* Make the cursor visible */
#define DISPLAY_ON	5	/* Display status: on or off */
#define INC_ADDR	6	/* Increment address after data read/write */
#define BACKLIGHT	7	/* Display backlight: on or off */
#define CGRAM_STATE	9	/* Controller status bitmask (bits 9->15): DDRAM or CGRAM access */

/* hd44780 access */
#define ACCESS_TO_READ    0
#define ACCESS_TO_WRITE   1
#define ACCESS_TO_DATA    2

#define ESC_MASK	0x00ff0000
#define PROC_MASK	0x0f000000

#define SET_STATE(state, mask)	(hd44780_flags = (hd44780_flags & ~(mask)) | ((state) & (mask)))
#define SET_ESC_STATE(state)	SET_STATE((state) << 16, ESC_MASK)
#define SET_PROC_LEVEL(level)	SET_STATE((level) << 24, PROC_MASK)
#define ESC_STATE		((hd44780_flags & ESC_MASK) >> 16)
#define PROC_LEVEL		((hd44780_flags & PROC_MASK) >> 24)

/* globals */
static unsigned int disp_size;			/* Display size (rows*columns) */
static unsigned int disp_offset[1];		/* Physical cursor position on the display */
static unsigned long hd44780_flags;		/* Driver flags for internal use only */

static struct lcd_parameters par = {
	.name		= HD44780_STRING,
	.minor		= HD44780_MINOR,
	.flags		= DFLT_FLAGS,
	.tabstop	= DFLT_TABSTOP,
	.num_cntr	= 1,
	.cntr_rows	= DFLT_CNTR_ROWS,
	.cntr_cols	= DFLT_CNTR_COLS,
	.vs_rows	= DFLT_VS_ROWS,
	.vs_cols	= DFLT_VS_COLS,
	.cgram_chars	= 8,
	.cgram_bytes	= 8,
	.cgram_char0	= 0,
};
/* End of globals */

#ifdef MODULE
#include <linux/device.h>
MODULE_ALIAS_CHARDEV(LCD_MAJOR, HD44780_MINOR);
#include <linux/kmod.h>

static unsigned short flags	= DFLT_FLAGS;
static unsigned short tabstop	= DFLT_TABSTOP;
static unsigned short cntr_rows	= DFLT_CNTR_ROWS;
static unsigned short cntr_cols	= DFLT_CNTR_COLS;
static unsigned short vs_rows	= DFLT_VS_ROWS;
static unsigned short vs_cols	= DFLT_VS_COLS;
static unsigned short minor	= HD44780_MINOR;

MODULE_DESCRIPTION("LCD ts72xx  driver for HD44780 compatible controllers.");
MODULE_AUTHOR("Petr Stetiar <ynezz@true.cz>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
module_param(flags,	ushort, 0444);
module_param(cntr_rows,	ushort, 0444);
module_param(cntr_cols,	ushort, 0444);
module_param(vs_rows,	ushort, 0444);
module_param(vs_cols,	ushort, 0444);
module_param(tabstop,	ushort, 0444);
module_param(minor,	ushort, 0444);
MODULE_PARM_DESC(flags,		"Various flags (see Documentation)");
MODULE_PARM_DESC(cntr_rows,	"Number of rows per controller on the LCD: 1, 2, 4 (default: " string(DFLT_CNTR_ROWS) ")");
MODULE_PARM_DESC(cntr_cols,	"Number of columns on the LCD (default: " string(DFLT_CNTR_COLS) ", max: " string(MAX_CNTR_COLS) ")");
MODULE_PARM_DESC(vs_rows,	"Number of rows of the virtual screen (default: " string(DFLT_VS_ROWS) ")");
MODULE_PARM_DESC(vs_cols,	"Number of columns of the virtual screen (default: " string(DFLT_VS_COLS) ")");
MODULE_PARM_DESC(tabstop,	"Tab character length (default: " string(DFLT_TABSTOP) ")");
MODULE_PARM_DESC(minor,		"Assigned minor number (default: " string(HD44780_MINOR) ")");
#else

/*
 * Parse boot command line
 *
 * hd44780=cntr_rows,cntr_cols,vs_rows,vs_cols,flags,minor,tabstop
 */
static int __init hd44780_boot_init(char *cmdline)
{
	char *str = cmdline;
	int idx = 0;
	unsigned short *args[] = {
		&par.cntr_rows,
		&par.cntr_cols,
		&par.vs_rows,
		&par.vs_cols,
		(ushort *) &par.flags,
		&par.num_cntr,
		&par.minor,
		&par.tabstop,
	};

	while (*cmdline && idx < (sizeof(args)/sizeof(unsigned short *))) {
		switch (*str) {
		case ',':
			*str++ = 0;
		case 0:
			if (strlen(cmdline))
				*args[idx] = simple_strtoul(cmdline, NULL, 0);
			++idx;
			cmdline = str;
			break;
		default:
			++str;
			break;
		}
	}

	return (1);
}

__setup("hd44780=", hd44780_boot_init);
#endif /* MODULE */

/* Macros for iterator handling */
static inline unsigned int iterator_inc_(unsigned int iterator, const unsigned int module)
{
	return ((++iterator)%module);
}

static inline unsigned int iterator_dec_(unsigned int iterator, const unsigned int module)
{
	return (iterator ? --iterator : module-1);
}

#define iterator_inc(iterator, module)		(iterator = iterator_inc_(iterator, module))
#define iterator_dec(iterator, module)		(iterator = iterator_dec_(iterator, module))

static inline void set_lines(unsigned char bitmask)
{
	gpio_set_value(LCD_EN, 0);		/* Disable */

	if (bitmask & ACCESS_TO_WRITE ) {
		gpio_direction_output(LCD_DATA0, 0);
		gpio_direction_output(LCD_DATA1, 0);
		gpio_direction_output(LCD_DATA2, 0);
		gpio_direction_output(LCD_DATA3, 0);
		gpio_direction_output(LCD_DATA4, 0);
		gpio_direction_output(LCD_DATA5, 0);
		gpio_direction_output(LCD_DATA6, 0);
		gpio_direction_output(LCD_DATA7, 0);
		gpio_set_value(LCD_WR, 0);	/* Write */
	} else {
		gpio_direction_input(LCD_DATA0);
		gpio_direction_input(LCD_DATA1);
		gpio_direction_input(LCD_DATA2);
		gpio_direction_input(LCD_DATA3);
		gpio_direction_input(LCD_DATA4);
		gpio_direction_input(LCD_DATA5);
		gpio_direction_input(LCD_DATA6);
		gpio_direction_input(LCD_DATA7);
		gpio_set_value(LCD_WR, 1);	/* Read */
	}

	if (bitmask & ACCESS_TO_DATA )
		gpio_set_value(LCD_RS, 1);	/* Data */
	else
		gpio_set_value(LCD_RS, 0);	/* Cmds*/
}

static inline void read_display(unsigned char *byte, unsigned char bitmask)
{
	unsigned char ret;
	if (bitmask)
		check_bf(bitmask);

	set_lines(bitmask);

	ndelay(T_AS);			/* Address set-up time */
	gpio_set_value(LCD_EN, 1); 	/* Enable */
	ndelay(T_EH);			/* Enable high time */

	ret =  (gpio_get_value(LCD_DATA0) << 0);
	ret |= (gpio_get_value(LCD_DATA1) << 1);
	ret |= (gpio_get_value(LCD_DATA2) << 2);
	ret |= (gpio_get_value(LCD_DATA3) << 3);
	ret |= (gpio_get_value(LCD_DATA4) << 4);
	ret |= (gpio_get_value(LCD_DATA5) << 5);
	ret |= (gpio_get_value(LCD_DATA6) << 6);
	ret |= (gpio_get_value(LCD_DATA7) << 7);
 
	gpio_set_value(LCD_EN, 0); 	/* Disable */
	ndelay(T_EL);			/* Enable low time */
	*byte = ret;
}

/* Low level write to the display */
static inline void write_display(unsigned char data, unsigned char bitmask)
{
	check_bf(bitmask);
	set_lines(bitmask);

	gpio_set_value(LCD_DATA0, (data >> 0) & 1);
	gpio_set_value(LCD_DATA1, (data >> 1) & 1);
	gpio_set_value(LCD_DATA2, (data >> 2) & 1);
	gpio_set_value(LCD_DATA3, (data >> 3) & 1);
	gpio_set_value(LCD_DATA4, (data >> 4) & 1);
	gpio_set_value(LCD_DATA5, (data >> 5) & 1);
	gpio_set_value(LCD_DATA6, (data >> 6) & 1);
	gpio_set_value(LCD_DATA7, (data >> 7) & 1);

	ndelay(T_AS);				/* Address set-up time */
	gpio_set_value(LCD_EN, 1);
	ndelay(T_EH);				/* Enable high time */

	gpio_set_value(LCD_EN, 0);		/* Disable */
	ndelay(T_EL);				/* Enable low time */
}

/* Read Address Counter AC from the display */
static unsigned char read_ac(unsigned char bitmask)
{
	unsigned char byte;

	read_display(&byte, bitmask);

	return (byte);
}

static void check_bf(unsigned char bitmask)
{
	unsigned int timeout = 20;
	static unsigned char do_check_bf = 5;

	gpio_set_value(LCD_EN, 0);		/* Disable */

	gpio_direction_input(LCD_DATA0);
	gpio_direction_input(LCD_DATA1);
	gpio_direction_input(LCD_DATA2);
	gpio_direction_input(LCD_DATA3);
	gpio_direction_input(LCD_DATA4);
	gpio_direction_input(LCD_DATA5);
	gpio_direction_input(LCD_DATA6);
	gpio_direction_input(LCD_DATA7);
 
	gpio_set_value(LCD_WR, 1);  		/* Read */
	gpio_set_value(LCD_RS, 0);  		/* Instru */

	ndelay(T_AS);				/* Address set-up time */
	gpio_set_value(LCD_EN, 1);		/* Enable */
	ndelay(T_EH);				/* Enable high time */

	do {
		udelay(T_BF);
	} while (gpio_get_value(LCD_DATA7) && --timeout);

	if (!timeout) {
		if (!--do_check_bf) {
			printk(KERN_NOTICE "hd44780 error: is LCD connected?\n");
		}
	}

	gpio_set_value(LCD_EN, 0);		/* Disable */
	ndelay(T_EL);				/* Enable low time */
}

/* Send commands to the display */
static void write_command(unsigned char command)
{
	write_display(command, ACCESS_TO_WRITE);

	if (command <= 0x03)
		mdelay(2);
}

static inline void set_cursor(unsigned int offset)
{
	unsigned int disp_number = offset/disp_size;
	unsigned int local_offset = offset%disp_size;

	if (disp_offset[disp_number] != local_offset || test_bit(CGRAM_STATE+disp_number, &hd44780_flags)) {
		unsigned int disp_row = local_offset/par.cntr_cols;
		unsigned int disp_column = local_offset%par.cntr_cols;

		write_command(DDRAM_IO | ((disp_row%2)*0x40) | (((disp_row >= 2)*par.cntr_cols)+disp_column));
		clear_bit(CGRAM_STATE+disp_number, &hd44780_flags);
		disp_offset[disp_number] = local_offset;
	}
}

/* HD44780 DDRAM addresses are consecutive only when
 * the cursor moves on the same row of the display.
 * Every time the row of the cursor changes we invalidate
 * the cursor position to force hardware cursor repositioning.
 */
static inline void mov_cursor(unsigned int disp_number)
{
	if (test_bit(INC_ADDR, &hd44780_flags)) {
		iterator_inc(disp_offset[disp_number], disp_size);
		if (disp_offset[disp_number]%par.cntr_cols == 0)
			disp_offset[disp_number] = disp_size;
	} else {
		iterator_dec(disp_offset[disp_number], disp_size);
		if (disp_offset[disp_number]%par.cntr_cols == par.cntr_cols-1)
			disp_offset[disp_number] = disp_size;
	}
}

static struct lcd_driver hd44780 = {
	.read_char		= hd44780_read_char,
	.read_cgram_char	= hd44780_read_cgram_char,
	.write_char		= hd44780_write_char,
	.write_cgram_char	= hd44780_write_cgram_char,
	.address_mode		= hd44780_address_mode,
	.clear_display		= hd44780_clear_display,
	.validate_driver	= hd44780_validate_driver,
	.init_display		= hd44780_init_display,
	.cleanup_display	= hd44780_cleanup_display,
	.init_port		= hd44780_init_port,
	.cleanup_port		= hd44780_cleanup_port,
	.handle_custom_char	= hd44780_handle_custom_char,
	.handle_custom_ioctl	= hd44780_handle_custom_ioctl,

	.charmap		= charmap,
};

static void hd44780_read_char(unsigned int offset, unsigned short *data)
{
	unsigned int disp_number = offset/disp_size;
	unsigned char tmp;

	set_cursor(offset);
	read_display(&tmp, ACCESS_TO_DATA);
	*data = tmp;
	mov_cursor(disp_number);
}

static void hd44780_read_cgram_char(unsigned char index, unsigned char *pixels)
{
	unsigned int i;

	write_command(CGRAM_IO | (index << 3));
	set_bit(CGRAM_STATE+0, &hd44780_flags);

	for (i = 0; i < 8; ++i) {
		read_display(pixels+i, ACCESS_TO_DATA);
		pixels[i] &= 0x1f;
	}

}

static void hd44780_write_char(unsigned int offset, unsigned short data)
{
	unsigned int disp_number = offset/disp_size;

	set_cursor(offset);
	write_display(data & 0xff, ACCESS_TO_WRITE | ACCESS_TO_DATA);
	mov_cursor(disp_number);
}

static void hd44780_write_cgram_char(unsigned char index, unsigned char *pixels)
{
	unsigned int i;

	/* Move address pointer to index in CGRAM */
	write_command(CGRAM_IO | (index << 3));
	set_bit(CGRAM_STATE+0, &hd44780_flags);

	for (i = 0; i < 8; ++i) {
		pixels[i] &= 0x1f;
		write_display(pixels[i], ACCESS_TO_WRITE | ACCESS_TO_DATA );
	}
}

/* Increment/decrement address mode after a data read/write */
static void hd44780_address_mode(int mode)
{
	if (mode > 0 && ! test_bit(INC_ADDR, &hd44780_flags)) {
		write_command(CURS_INC | DISP_SHIFT_OFF);
		set_bit(INC_ADDR, &hd44780_flags);
	} else if (mode < 0 && test_bit(INC_ADDR, &hd44780_flags)) {
		write_command(CURS_DEC | DISP_SHIFT_OFF);
		clear_bit(INC_ADDR, &hd44780_flags);
	}
}

static void hd44780_clear_display(void)
{
	write_command(CLR_DISP);
	if (! test_bit(INC_ADDR, &hd44780_flags))
		write_command(CURS_DEC | DISP_SHIFT_OFF);
	memset(disp_offset, 0, sizeof(disp_offset));
}

static int hd44780_validate_driver(void)
{
	if (par.cntr_rows != 1 && par.cntr_rows != 2 && par.cntr_rows != 4)
		par.cntr_rows = DFLT_CNTR_ROWS;

	if (par.cntr_rows != 1)
		par.flags &= ~HD44780_5X10_FONT;

	if (! par.cntr_cols || par.cntr_cols > MAX_CNTR_COLS/par.cntr_rows)
		par.cntr_cols = MAX_CNTR_COLS/par.cntr_rows;

	disp_size = par.cntr_rows*par.cntr_cols;

	/* These parameters depend on the hardware and cannot be changed */
	par.cgram_chars = 8;
	par.cgram_bytes = 8;
	par.cgram_char0 = 0;

	return (0);
}

/* Send init commands to the display */
static void write_init_command(void)
{
	unsigned char command;
	command = BUS_8_BITS;
	command |= ((par.cntr_rows == 1) ? DISP_1_LINE : DISP_2_LINES);
	command |= (test_bit(_5X10_FONT, &hd44780_flags) ? FONT_5X10 : FONT_5X8);

	write_display(command, ACCESS_TO_WRITE);
	mdelay(20);	/* Wait more than 4.1 ms */

	write_display(command, ACCESS_TO_WRITE);
	udelay(200);	/* Wait more than 100 us */

	write_display(command, ACCESS_TO_WRITE);
	udelay(200);	/* Wait more than 100 us */

	write_command(command);
}

static int hd44780_init_display(void)
{
	if (par.flags & HD44780_CHECK_BF)
		set_bit(_CHECK_BF, &hd44780_flags);
	else
		clear_bit(_CHECK_BF, &hd44780_flags);

	if (par.flags & HD44780_4BITS_BUS)
		set_bit(_4BITS_BUS, &hd44780_flags);
	else
		clear_bit(_4BITS_BUS, &hd44780_flags);

	if (par.flags & HD44780_5X10_FONT)
		set_bit(_5X10_FONT, &hd44780_flags);
	else
		clear_bit(_5X10_FONT, &hd44780_flags);

	write_init_command();
	hd44780_address_mode(1);
	hd44780_clear_display();
	write_command(DISP_ON | CURS_OFF | BLINK_OFF);
	set_bit(DISPLAY_ON, &hd44780_flags);
	clear_bit(SHOW_CURSOR, &hd44780_flags);
	clear_bit(CURSOR_BLINK, &hd44780_flags);

	/* Set the CGRAM to default values */
	hd44780_write_cgram_char(0, cg0);
	hd44780_write_cgram_char(1, cg1);
	hd44780_write_cgram_char(2, cg2);
	hd44780_write_cgram_char(3, cg3);
	hd44780_write_cgram_char(4, cg4);
	hd44780_write_cgram_char(5, cg5);
	hd44780_write_cgram_char(6, cg6);
	hd44780_write_cgram_char(7, cg7);
	init_charmap();

	return (0);
}

static int hd44780_cleanup_display(void)
{
	hd44780_clear_display();

	return (0);
}

static int hd44780_init_port(void)
{
	int err = gpio_request_array(lcd_gpios, ARRAY_SIZE(lcd_gpios));
	if (err) {
		printk(KERN_ERR "hd44780: error while requesting GPIO pins\n");
		return 1;
	}

	return 0;
}

static int hd44780_cleanup_port(void)
{
	gpio_direction_input(LCD_RS);
	gpio_direction_input(LCD_EN);
	gpio_direction_input(LCD_DATA0);
	gpio_direction_input(LCD_DATA1);
	gpio_direction_input(LCD_DATA2);
	gpio_direction_input(LCD_DATA3);
	gpio_direction_input(LCD_DATA4);
	gpio_direction_input(LCD_DATA5);
	gpio_direction_input(LCD_DATA6);
	gpio_direction_input(LCD_DATA7);

	gpio_free_array(lcd_gpios, ARRAY_SIZE(lcd_gpios));

	return 0;
}

static void display_attr(unsigned char input)
{
	unsigned char command;

	switch (ESC_STATE) {
	case 'a':	/* Turn on/off the display cursor */
		if (input == '1')
			set_bit(SHOW_CURSOR, &hd44780_flags);
		else if (input == '0')
			clear_bit(SHOW_CURSOR, &hd44780_flags);
		break;
	case 'b':	/* Turn on/off the display cursor blinking */
		if (input == '1')
			set_bit(CURSOR_BLINK, &hd44780_flags);
		else if (input == '0')
			clear_bit(CURSOR_BLINK, &hd44780_flags);
		break;
	case 'h':	/* Turn on/off the display */
		if (input == '1')
			set_bit(DISPLAY_ON, &hd44780_flags);
		else if (input == '0')
			clear_bit(DISPLAY_ON, &hd44780_flags);
		break;
	}

	command = (test_bit(DISPLAY_ON, &hd44780_flags) ? DISP_ON : DISP_OFF);
	command |= (test_bit(SHOW_CURSOR, &hd44780_flags) ? CURS_ON : CURS_OFF);
	command |= (test_bit(CURSOR_BLINK, &hd44780_flags) ? BLINK_ON : BLINK_OFF);

	if (ESC_STATE == 'h')
		write_command(command);
}

static int hd44780_handle_custom_char(unsigned int _input)
{
	unsigned char input = _input & 0xff;

	if (_input & (~0xff)) {
		switch (ESC_STATE) {
		case 'a':	/* Turn on/off the display cursor */
		case 'b':	/* Turn on/off the display cursor blinking */
		case 'h':	/* Turn on/off the the display */
			display_attr(input);
			return (0);
		case 'l':	/* Turn on/off the backlight */
			if (input == '1')
				set_bit(BACKLIGHT, &hd44780_flags);
			else if (input == '0')
				clear_bit(BACKLIGHT, &hd44780_flags);
			read_ac(ACCESS_TO_READ);
			return (0);
		}
	}

	switch (input) {
	case 'a':	/* Turn on/off the display cursor */
	case 'b':	/* Turn on/off the display cursor blinking */
	case 'h':	/* Turn on/off the display */
	case 'l':	/* Turn on/off the backlight */
		SET_ESC_STATE(input);
		return (1);
	case 'd':	/* Shift display cursor Right */
		write_command(SHIFT_CURS | SHIFT_RIGHT);
		return (0);
	case 'e':	/* Shift display cursor Left */
		write_command(SHIFT_CURS | SHIFT_LEFT);
		return (0);
	case 'f':	/* Shift display Right */
		write_command(SHIFT_DISP | SHIFT_RIGHT);
		return (0);
	case 'g':	/* Shift display Left */
		write_command(SHIFT_DISP | SHIFT_LEFT);
		return (0);
	}

	return (-1);
}

static int hd44780_handle_custom_ioctl(unsigned int num, unsigned long arg, unsigned int user_space)
{
	unsigned char *buffer = (unsigned char *)arg;

	if (num != HD44780_READ_AC)
		return (-ENOIOCTLCMD);

	if (user_space)
		put_user(read_ac(ACCESS_TO_READ), buffer);
	else
		buffer[0] = read_ac(ACCESS_TO_READ);

	return (0);
}

#ifdef USE_PROC
static int hd44780_proc_status(char *buffer, char **start, off_t offset, int size, int *eof, void *data)
{
	char *temp = buffer;

	/* Print display configuration */
	temp += sprintf(temp,
			"Interface:\t%u bits\n"
			"Display rows:\t%d\n"
			"Display cols:\t%d\n"
			"Screen rows:\t%d\n"
			"Screen cols:\t%d\n"
			"Read:\t\t%sabled\n"
			"Busy flag chk:\t%sabled\n"
			"Assigned minor:\t%u\n",
			(test_bit(_4BITS_BUS, &hd44780_flags) ? 4 : 8),
			par.cntr_rows, par.cntr_cols,
			par.vs_rows, par.vs_cols,
			(hd44780.read_char ? "En" : "Dis"),
			(test_bit(_CHECK_BF, &hd44780_flags) ? "En" : "Dis"),
			par.minor);

	return (temp-buffer);
}

static int hd44780_proc_cgram(char *buffer, char **start, off_t offset, int size, int *eof, void *data)
{
	char *temp = buffer;
	unsigned int i;

	temp += sprintf(temp,	"static void init_charmap(void)\n{\n"
				"\t/*\n"
				"\t * charmap[char mapped to cg0] = 0;\n"
				"\t * charmap[char mapped to cg1] = 1;\n"
				"\t * charmap[char mapped to cg2] = 2;\n"
				"\t * charmap[char mapped to cg3] = 3;\n"
				"\t * charmap[char mapped to cg4] = 4;\n"
				"\t * charmap[char mapped to cg5] = 5;\n"
				"\t * charmap[char mapped to cg6] = 6;\n"
				"\t * charmap[char mapped to cg7] = 7;\n"
				"\t */\n"
				"}\n\n");

	for (i = 0; i < 8; ++i) {
		unsigned int j;
		unsigned char cgram_buffer[8];

		temp += sprintf(temp, "static unsigned char cg%u[] = { ", i);
		hd44780_read_cgram_char(i, cgram_buffer);
		for (j = 0; j < 8; ++j)
			temp += sprintf(temp, "0x%.2x%s", cgram_buffer[j], (j == 7 ? " };\n" : ", "));
	}

	return (temp-buffer);
}

static void create_proc_entries(void)
{
	SET_PROC_LEVEL(0);
	if (create_proc_read_entry("status", 0, hd44780.driver_proc_root, hd44780_proc_status, NULL) == NULL) {
		printk(KERN_ERR "hd44780: cannot create /proc/lcd/%s/status\n", par.name);
		return;
	}
	SET_PROC_LEVEL(1);
	if (create_proc_read_entry("cgram.h", 0, hd44780.driver_proc_root, hd44780_proc_cgram, NULL) == NULL) {
		printk(KERN_ERR "hd44780: cannot create /proc/lcd/%s/cgram.h\n", par.name);
		return;
	}
	SET_PROC_LEVEL(2);
}

static void remove_proc_entries(void)
{
	switch (PROC_LEVEL) {
	case 2:
		remove_proc_entry("cgram.h", hd44780.driver_proc_root);
	case 1:
		remove_proc_entry("status", hd44780.driver_proc_root);
	}
	SET_PROC_LEVEL(0);
}
#endif

/* Initialization */
static int __init hd44780_init_module(void)
{
	int ret;

#ifdef MODULE
	if ((ret = request_module("lcd-linux"))) {
		printk(KERN_ERR "hd44780: request_module() returned %d\n", ret);
		if (ret < 0) {
			if (ret != -ENOSYS) {
				printk(KERN_ERR "hd44780: failure while loading module lcd-linux\n");
				return (ret);
			}
			printk(KERN_ERR "hd44780: your kernel does not have kmod or kerneld support;\n");
			printk(KERN_ERR "hd44780: remember to load the lcd-linux module before\n");
			printk(KERN_ERR "hd44780: loading the hd44780 module\n");
		}
	}

	if (flags	!= DFLT_FLAGS)		par.flags	= flags;
	if (tabstop	!= DFLT_TABSTOP)	par.tabstop	= tabstop;
	if (cntr_rows	!= DFLT_CNTR_ROWS)	par.cntr_rows	= cntr_rows;
	if (cntr_cols	!= DFLT_CNTR_COLS)	par.cntr_cols	= cntr_cols;
	if (vs_rows	!= DFLT_VS_ROWS)	par.vs_rows	= vs_rows;
	if (vs_cols	!= DFLT_VS_COLS)	par.vs_cols	= vs_cols;
	if (minor	!= HD44780_MINOR)	par.minor	= minor;
#endif

	lcd_driver_setup(&hd44780);
	if ((ret = lcd_register_driver(&hd44780, &par)))
		return (ret);

#ifdef USE_PROC
	if (hd44780.driver_proc_root)
		create_proc_entries();
#endif

	printk(KERN_INFO "hd44780: ts72xx driver loaded\n" );

	return (0);
}

static void __exit hd44780_cleanup_module(void)
{
#ifdef USE_PROC
	if (hd44780.driver_proc_root)
		remove_proc_entries();
#endif

	lcd_unregister_driver(&hd44780, &par);
}

module_init(hd44780_init_module)
module_exit(hd44780_cleanup_module)
