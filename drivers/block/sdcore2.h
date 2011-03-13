/*
 * Copyright (c) 2006-2008, Technologic Systems
 * All rights reserved.
 */

#ifndef _SDCORE_H_
#define _SDCORE_H_

// Additional missing defs
#define SDCMD 0                // cmd register
#define SDDAT 1                // data register
#define SDSTATE 2              // state register
#define SDCTRL 3               // ctrl register


// this bit is set when no card inserted
#define SDCTRL_CARD_ABSENT 0x08



/* public bits for sd_state bitfield, can be read from client code.
 * Do not write!  Other bits are used internally.
 */
#define SDDAT_RX	(1<<0)
#define SDDAT_TX	(1<<1)
#define SDCMD_RX	(1<<2)
#define SDCMD_TX	(1<<3)

// used to disable CRC calculations in write mode
#define SDCRC_DISABLE   (1 << 4)



// used to choose between 4 bit crc mode and 1 bit crc mode

// note - likely set in sdreset when configuring interface bit width
#define SDSSP_4BIT_MODE (1 << 5)

// SD_ADDRESSING_DIRECT means that sd card addresses
// will be communicated in read/write mode using
// full offsets, not 512 byte block offsets.
// the core will mulitply the address by 512 if this
// bit is cleared
#define SD_ADDRESSING_DIRECT (1 << 6)


/* These structs should start intialized to all 0's (bzero()'ed).  Proper
 * operation can be assured by setting sd_regstart, and the os_delay
 * callback.  sdreset() should be called to initialize the core, then
 * sdread() and sdwrite() can be used.
 */
struct sdcore {
	/* virtual address of SD block register start, to be filled in
	 * by client code before calling any sdcore functions.
	 */
  // 0-3
	unsigned char* sd_regstart;



  // 4-7
	unsigned int sd_state;

	/* Erase hint for subsequent sdwrite() call, used to optimize
	 * write throughput on multi-sector writes by pre-erasing this
	 * many sectors.
	 */
  // 8-11
	unsigned int sd_erasehint;

	/* Following this comment are 5 function pointer declarations to
	 * OS helper functions.  The 'os_arg' member is passed as the
	 * first argument to the helpers and should be set by
	 * client code before issueing sdreset()
	 *
	 * os_dmastream(os_arg, buf, buflen)
	 * This function should look at sd_state and set up and run an
	 * appropriate DMA transfer.  If buf is NULL, callee doesn't care
	 * about the actual data sent/received and helper function
	 * can do whatever it wants.  Should return 0 when DMA transfer was
	 * run and completed successfully.  If this function pointer is
	 * NULL, PIO methods of transfer will be used instead of DMA.
	 *
	 * os_dmaprep(os_arg, buf, buflen)
	 * This function is used to prepare an area of memory for a possible
	 * DMA transfer.  This function is called once per distinct buffer
	 * passed in.  After this function is called, os_dmastream() may be
	 * called one or more times (for sequential addresses) on subregions
	 * of the address range passed here.  Should write-back or invalidate
	 * L1 cache lines and possibly look up physical addresses for buf
	 * passed in if I/O buffers.  If 'os_dmaprep' is set to NULL, function
	 * call will not happen. (though os_dmastream() calls may still)
	 *
	 * os_delay(os_arg, microseconds)
	 * This function is supposed to delay or stall the processor for
	 * the passed in value number of microseconds.
	 *
	 * os_irqwait(os_arg, type)
	 * Called at certain times to request to be put to sleep/block until
	 * an SD interrupt occurs.  It is not critical to set this function.
	 * When NULL, the sdcore routines simply busy-wait.
	 *
	 * os_powerok(os_arg)
	 * Experimental callback function -- set to NULL for now.
	 */
  // 12-15
	void *os_arg;
  // 16-19
	int (*os_dmastream)(void *, unsigned char *, unsigned int);
  // 20-23
	void (*os_dmaprep)(void *, unsigned char *, unsigned int);
  // 24-27
	void (*os_delay)(void *, unsigned int);
  // 28-31
	void (*os_irqwait)(void *, unsigned int);
  // 32-35
	int (*os_powerok)(void *);

	int (*os_timeout)(void *);
	int (*os_reset_timeout)(void *);

	/* If the SD card last successfully reset is write protected, this
	 * member will be non-zero.
	 */
  // 36-39
	unsigned int sd_wprot;

	/* If this card may have been already initialized by TS-SDBOOT, place
	 * the magic token it placed in the EP93xx SYSCON ScratchReg1 here
	 * before calling sdreset() to avoid re-initialization.
	 */
  // 40-43
	unsigned int sdboot_token;

	/* CRC hint for subsequent sdwrite() call, used to optimize
	 * write throughput while using DMA by pre-calculating CRC's for
	 * next write.  NULL means no hint supplied.
	 */
  // 44-47
	unsigned char *sd_crchint;

	/* The block size of the memory device.  Normally 512, but can be 1024
	 * for larger cards.  Read-only member and actually not very useful.
	 */
  // 48-51
	unsigned int sd_blocksize;

	/* Password for auto-unlocking in sdreset()
	 */
  // 52-55
	unsigned char *sd_pwd;

	/* If the SD card was password locked, this will be non-zero after
	 * unsuccessful sdreset().
	 */
  // 56-59
	unsigned int sd_locked;

	/* Whether or not writes can be parked.  Definitely should be set to 1
	 * as writes are very slow without it.
	 */
  // 60-63
	unsigned int sd_writeparking;

	/* Logical unit number.  Some SD cores will have multiple card slots.
	 * LUN #0 is the first.
	 */
  // 64-67
	unsigned int sd_lun;

	/* The rest of these members are for private internal use and should
	 * not be of interest to client code.
	 */


  // 68-71
  unsigned int rca;  // relative card address


  unsigned int sd_csd[17];
  /*


  // 72 -75    0
  unsigned int unknown72;      // one of the csds?

  // 76 -79    1
  unsigned int unknown76;      // csd 0x00
  // 80 -83    2
  unsigned int unknown04;      // csd 0x01
  // 84 - 87   3
  unsigned int unknown05;      // csd 0x02
  // 88 - 91   4
  unsigned int unknown06;      // csd 0x03
  // 92 - 95   5
  unsigned int unknown92;      // csd 0x04
  // 96 - 100  6
  unsigned int unknown96;      // csd 0x05
  // 100 - 103 7
  unsigned int unknown100;     // csd 0x06
  // 104 - 107 8
  unsigned int unknown104;     // csd 0x07
  // 108 - 111 9
  unsigned int unknown108;     // csd 0x08
  // 112 - 115 10
  unsigned int unknown112;     // csd 0x09
  // 116 - 119 11
  unsigned int unknown116;     // csd 0x0a
  // 120       12
  unsigned int unknown24;      // csd 0x0b
  // 124       13
  unsigned int unknown25;      // csd 0x0c
  // 128       14
  unsigned int unknown26;      // csd 0x0d
  // 132       15
  unsigned int unknown132;      // csd 0x0e
  // 136       16
  unsigned int unknown28;      // csd 0x0f
  */


  // 140
  unsigned int sd_crc_shift;

  // 144 + 4 + 4
  unsigned short s_crc_table[4]; // 4 shorts
  // 152 + 4 + 4 + 4 + 4
  unsigned int   l_crc_table[4]; // 4 longs

  // 168
  unsigned int sd_timeout;       // used to busy wait

  // 172
  unsigned int sd_cur_sector;    // stop indicator - if zero , then stop procedure will be skipped

  // 176
  unsigned int sdcore_version;   // hardware version
  // 180
  unsigned int unknown39;
  // 184
  unsigned int unknown40;
  // 188
  unsigned int sdcore_sdsize;



  unsigned int unknown42;
  unsigned int unknown43;
  unsigned int unknown44;
  unsigned int unknown45;
  unsigned int unknown46;
  unsigned int unknown47;













};

/* I believe sdcores is a table mapping
   id -> sdcore struct.  The table is
   64 long, meaning that one could build a ts device with
   64 sdcores on it.
*/
//extern unsigned char sdcores[256];




/* For sdreadv() / sdwritev() */
struct sdiov {
  unsigned char *sdiov_base;
  unsigned int sdiov_nsect;
};




int sdreset(struct sdcore *);

int sdsize(struct sdcore* sdcore);


int sdread(struct sdcore* sdcore,
	   unsigned int sector,
	   unsigned char* buffer,
	   int nsect);

int sdwrite(struct sdcore *, unsigned int, unsigned char *, int);


// same signature as do_read
int do_read(struct sdcore*, unsigned int, struct sdiov*, int);
int sdreadv(struct sdcore * sdcore,
	    unsigned int sector,
	    struct sdiov * sdiov,
	    int nsdiov);

// same signature as do_write
int do_write(struct sdcore* sdcore,
	     unsigned int sector,
	     struct sdiov* sdiov,
	     int nsdiov);

int sdwritev(struct sdcore *, unsigned int, struct sdiov *, int);


void sd_1bit_feedcrc(struct sdcore*, unsigned int);
void sd_4bit_feedcrc(struct sdcore*, unsigned int);

unsigned char sd_1bit_getcrc(struct sdcore*);
unsigned char sd_4bit_getcrc(struct sdcore*);

/** stop takes only sdcore parameters */
int stop(struct sdcore*);


int tend_ssp(struct sdcore* sdcore,
	     unsigned int** unknown_r1,      // r1
	     unsigned char** unknown_r2);


int datssp_stream(struct sdcore* sdcore,
		  unsigned char** data,
		  int count);

/*
 * @param cmd is the command - I believe that the lower byte is the command, and
 *       the upper one is the crc
 *
 * @param data is a character buffer for data received in the ssp dat register, as
 * a result of a command execution.
 */

int sdcmd(struct sdcore* sdcore,
	  unsigned short cmd,
	  unsigned int sdargs,
	  unsigned int* response,
	  unsigned char** data); // command response buffer?


/**
 * Error tests if a sdcommand error has been received.
 * It does this by checking that the command was
 * correctly returned by the card (the first byte in buffer),
 * and that a CRC error has not occurred.  IF one has occurred
 * it will attempt a 1bit fix. (suspected)
 */
int error(unsigned int* buffer, unsigned int cmd);



int sdsetwprot(struct sdcore *, unsigned int);
#define SDLOCK_UNLOCK	0
#define SDLOCK_SETPWD	1
#define SDLOCK_CLRPWD	2
#define SDLOCK_ERASE	8

int sdlockctl(struct sdcore *,
	      unsigned int,      // op code
	      unsigned char *,
	      unsigned char *);

#endif
