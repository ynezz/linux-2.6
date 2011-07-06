/*
 * linux/arch/arm/mm/cache-v4wb-init.c
 *
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2011 Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Runtime initialization for the code in cache-v4wb.S.
 */

#include <linux/init.h>
#include <asm/sizes.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>

#define FLUSH_BASE		0xfffe0000
#define FLUSH_OFF_ALT		SZ_32K
#define FLUSH_BASE_ALT		(FLUSH_BASE + FLUSH_OFF_ALT)
#define UNCACHEABLE_ADDR 	0xfffee000

static struct map_desc cache_v4wb_map[] __initdata = {
	{
		.virtual	= FLUSH_BASE,
		.length		= SZ_16K,
		.type		= MT_CACHECLEAN,
	}, {
		.virtual	= FLUSH_BASE + SZ_16K,
		.length		= PAGE_SIZE,
		.type		= MT_MINICLEAN,
	}, {
		.virtual	= FLUSH_BASE_ALT,
		.length		= SZ_16K,
		.type		= MT_CACHECLEAN,
	}, {
		.virtual	= FLUSH_BASE_ALT + SZ_16K,
		.length		= PAGE_SIZE,
		.type		= MT_MINICLEAN,
	}, {
		.virtual	= UNCACHEABLE_ADDR,
		.length		= PAGE_SIZE,
		.type		= MT_UNCACHED,
	}
};

/* shared with cache-v4wb.S */
struct {
	unsigned long addr;
	unsigned long size;
} cache_v4wb_params;

/*
 * This sets up cache flushing areas for the SA110 and SA11x0 processors.
 * For convenience, uncached_phys_addr is used here to create an uncacheable
 * mapping as needed by proc-sa110.S and proc-sa1100.S.
 */
void __init cache_v4wb_init(unsigned long flush_phys_addr,
			    unsigned long uncached_phys_addr,
			    enum cache_v4wb_cputype cpu_type)
{
	cache_v4wb_map[0].pfn = __phys_to_pfn(flush_phys_addr);
	cache_v4wb_map[1].pfn = __phys_to_pfn(flush_phys_addr + SZ_16K);
	cache_v4wb_map[2].pfn = __phys_to_pfn(flush_phys_addr + FLUSH_OFF_ALT);
	cache_v4wb_map[3].pfn = __phys_to_pfn(flush_phys_addr + FLUSH_OFF_ALT + SZ_16K);
	cache_v4wb_map[4].pfn = __phys_to_pfn(uncached_phys_addr);

	/*
	 * SA110 has a 16KB cache and no minicache.
	 * SA1100 has a 8KB cache and a 512-byte minicache.
	 * To make the flush code straight forward, we simply pretend
	 * that the SA1100 has a cache of 8704 bytes in size and its
	 * flush area is located at an offset of 8192 bytes into
	 * the mapped area.
	 */
	switch (cpu_type) {
	case CACHE_CPU_SA110:
		cache_v4wb_params.addr = FLUSH_BASE;
		cache_v4wb_params.size = SZ_16K;
		break;
	case CACHE_CPU_SA1100:
		cache_v4wb_params.addr = FLUSH_BASE + SZ_8K;
		cache_v4wb_params.size = SZ_8K + 512;
		break;
	}

	iotable_init(cache_v4wb_map, ARRAY_SIZE(cache_v4wb_map));
}
