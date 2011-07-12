/*
 * arch/arm/mach-ep93xx/include/mach/system.h
 */

#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/ts72xx.h>

static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void arch_reset(char mode, const char *cmd)
{
	local_irq_disable();

	if (board_is_ts7200() || board_is_ts7250() || board_is_ts7260() ||
	    board_is_ts7300() || board_is_ts7400()) {
		/* We use more reliable CPLD watchdog to perform the reset */
		__raw_writeb(0x5, TS72XX_WDT_FEED_PHYS_BASE);
		__raw_writeb(0x1, TS72XX_WDT_CONTROL_PHYS_BASE);
	} else {
		/* Set then clear the SWRST bit to initiate a software reset */
		ep93xx_devcfg_set_bits(EP93XX_SYSCON_DEVCFG_SWRST);
		ep93xx_devcfg_clear_bits(EP93XX_SYSCON_DEVCFG_SWRST);
	}

	while (1)
		;
}
