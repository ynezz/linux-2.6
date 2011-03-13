#ifndef __ASM_ARM_IOCTLS_H
#define __ASM_ARM_IOCTLS_H

#define FIOQSIZE	0x545E

#include <asm-generic/ioctls.h>

#define TIOC_SBCC485	0x545F /* TS72xx RTS/485 mode clear */
#define TIOC_SBCS485	0x5460 /* TS72xx RTS/485 mode set */

#endif
