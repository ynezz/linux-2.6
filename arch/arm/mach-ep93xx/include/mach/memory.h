/*
 * arch/arm/mach-ep93xx/include/mach/memory.h
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#if defined(CONFIG_EP93XX_SDCE3_SYNC_PHYS_OFFSET)
#define PLAT_PHYS_OFFSET		UL(0x00000000)
#elif defined(CONFIG_EP93XX_SDCE0_PHYS_OFFSET)
#define PLAT_PHYS_OFFSET		UL(0xc0000000)
#elif defined(CONFIG_EP93XX_SDCE1_PHYS_OFFSET)
#define PLAT_PHYS_OFFSET		UL(0xd0000000)
#elif defined(CONFIG_EP93XX_SDCE2_PHYS_OFFSET)
#define PLAT_PHYS_OFFSET		UL(0xe0000000)
#elif defined(CONFIG_EP93XX_SDCE3_ASYNC_PHYS_OFFSET)
#define PLAT_PHYS_OFFSET		UL(0xf0000000)
#else
#error "Kconfig bug: No EP93xx PHYS_OFFSET set"
#endif

/*
 * Non-linear mapping like so:
 * phys       => virt
 * 0x00000000 => 0xc0000000
 * 0x01000000 => 0xc1000000
 * 0x04000000 => 0xc4000000
 * 0x05000000 => 0xc5000000
 * 0xe0000000 => 0xc8000000
 * 0xe1000000 => 0xc9000000
 * 0xe4000000 => 0xcc000000
 * 0xe5000000 => 0xcd000000
 *
 * As suggested here: http://marc.info/?l=linux-arm&m=122754446724900&w=2
 */
#if defined(CONFIG_SPARSEMEM)

#define __phys_to_virt(p) \
	(((p) & 0x07ffffff) | (((p) & 0xe0000000) ? 0x08000000 : 0) | PAGE_OFFSET)

#define __virt_to_phys(v) \
	(((v) & 0x07ffffff) | (((v) & 0x08000000) ? 0xe0000000 : 0 ))

#define SECTION_SIZE_BITS 24
#define MAX_PHYSMEM_BITS 32

#endif /* CONFIG_SPARSEMEM */

#endif
