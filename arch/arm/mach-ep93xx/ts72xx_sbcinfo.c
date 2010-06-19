/*
 *  Technologic Systems TS-72XX sbc /proc/driver/sbcinfo entry.
 *
 *  Original idea by Liberty Young (Technologic Systems).
 *
 *	(c) Copyright 2008  Matthieu Crapet <mcrapet@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <mach/hardware.h>
#include <mach/ts72xx.h>

struct infos {
	const char	*cpu_rev;
	int		model, pld, wdt;
	int		option_ad;
	int		option_rs485;
	unsigned char	jumpers[6]; // 0=off,1=on,2=error

	/* Power management : TS-7260 only */
	int		pm;
};

static const char *revisions[] = { "A", "B", "C", "D0", "D1", "E0", "E1", "E2", "??" };


static void get_sbcinfo(struct infos *data)
{
	void __iomem *p;
	short rev;

	/* CPU revision */
	rev = __raw_readl(EP93XX_SYSCON_CHIPID) >> 28;
	if (rev > ARRAY_SIZE(revisions))
		rev = ARRAY_SIZE(revisions) - 1;
	data->cpu_rev = revisions[rev];

	/* Board model */
	if (board_is_ts7200())
		data->model = 7200;
	else if (board_is_ts7250())
		data->model = 7250;
	else if (board_is_ts7260())
		data->model = 7260;
	else if (board_is_ts7300())
		data->model = 7300;
	else if (board_is_ts7400())
		data->model = 7400;
	else
		data->model = 0;

	data->pld = get_ts72xx_pld_version();

	/* A/D converter (8 x 12-bit channels) */
	if (data->model == 7200 || data->model == 7250) {
		data->option_ad = is_max197_installed();
	} else {
		data->option_ad = 0;
	}

	/* COM2 RS-485 */
	if (is_rs485_installed()) {
		data->option_rs485 = 1;
	} else {
		data->option_rs485 = 0;
	}

	/* jumpers */
	p = ioremap(TS72XX_JUMPERS_MAX197_PHYS_BASE, SZ_4K - 1);
	if (p) {
		unsigned char c = __raw_readb(p);

		data->jumpers[0] = 2;                // JP1 (bootstrap)
		data->jumpers[1] = !!(c & 0x01);     // JP2 (enable serial console)
		data->jumpers[2] = !!(c & 0x02);     // JP3 (flash write enable)
		data->jumpers[3] = !(c & 0x08);      // JP4 (console on COM2)
		data->jumpers[4] = !(c & 0x10);      // JP5 (test)
		data->jumpers[5] = !!(is_jp6_set()); // JP6 (user jumper)

		iounmap(p);
	} else {
		data->jumpers[0] = data->jumpers[1] = data->jumpers[2] = 2;
		data->jumpers[3] = data->jumpers[4] = data->jumpers[5] = 2;
	}

	/* cpld watchdog */
	p = ioremap(TS72XX_WDT_CONTROL_PHYS_BASE, SZ_4K - 1);
	if (p) {
		data->wdt = __raw_readb(p) & 0x7;
		iounmap(p);
	} else {
		data->wdt = 8;
	}

	/* power management */
	data->pm = -1;
	if (data->model == 7260) {
		p = ioremap(TS7260_POWER_MANAGEMENT_PHYS_BASE, SZ_4K - 1);
		if (p) {
			data->pm = __raw_readb(p);
			iounmap(p);
		}
	}
}

static char *get_pm_string(int reg, char *buffer, size_t size)
{
	static const char *pm_state = "rs232=%d usb=%d lcd=%d pc104=%d ttl=%d";

	if (reg < 0) {
		strncpy(buffer, "n/a", size);
	} else {
		/* 1 means on/enabled */
		snprintf(buffer, size, pm_state,
				reg & TS7260_PM_RS232_LEVEL_CONVERTER,
				!!(reg & TS7260_PM_USB),
				!!(reg & TS7260_PM_LCD),
				!(reg & TS7260_PM_PC104_CLOCK),
				!!(reg & TS7260_PM_TTL_UART_ENABLE));
	}
	return buffer;
}

static int ts72xx_sbcinfo_read_proc(char *buffer, char **start, off_t offset,
		int count, int *eof, void *data)
{
	int len, size = count;
	char *p = buffer;
	char temp[64];
	struct infos nfo;

	static const char jpc[3] = { 'n', 'y', '?' };
	static const char *wdt[9] = { "disabled", "250ms", "500ms", "1s", "reserved", "2s", "4s", "8s", "n/a" };

	get_sbcinfo(&nfo);
	len = scnprintf(p, size,
			"Model             : TS-%d (CPU rev %s) (PLD rev %c)\n"
			"Option max197 A/D : %s\n"
			"Option RS-485     : %s\n"
			"Jumpers           : JP2=%c JP3=%c JP4=%c JP5=%c JP6=%c\n"
			"CPLD Watchdog     : %s\n"
			"Power management  : %s\n",
			nfo.model, nfo.cpu_rev, nfo.pld + 0x40,
			(nfo.option_ad ? "yes" : "no"),
			(nfo.option_rs485 ? "yes" : "no"),
			jpc[nfo.jumpers[1]], jpc[nfo.jumpers[2]], jpc[nfo.jumpers[3]], jpc[nfo.jumpers[4]],
			jpc[nfo.jumpers[5]], wdt[nfo.wdt],
			get_pm_string(nfo.pm, &temp[0], sizeof(temp)));

	if (len <= offset + count)
		*eof = 1;

	*start = buffer + offset;
	len -= offset;

	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}

static int __init ts72xx_sbcinfo_init(void)
{
	struct proc_dir_entry *entry;
	int ret = 0;

	entry = create_proc_read_entry("driver/sbcinfo", 0,
			NULL, ts72xx_sbcinfo_read_proc, NULL);

	if (!entry) {
		printk(KERN_ERR "sbcinfo: can't create /proc/driver/sbcinfo\n");
		ret = -ENOMEM;
	}

	return ret;
}

static void __exit ts72xx_sbcinfo_exit(void)
{
	remove_proc_entry("driver/sbcinfo", NULL);
}

module_init(ts72xx_sbcinfo_init);
module_exit(ts72xx_sbcinfo_exit);

MODULE_AUTHOR("Matthieu Crapet <mcrapet@gmail.com>");
MODULE_DESCRIPTION("Show information of Technologic Systems TS-72XX sbc");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.04");
