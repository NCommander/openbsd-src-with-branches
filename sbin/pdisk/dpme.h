/*	$OpenBSD: dpme.h,v 1.6 2016/01/15 16:39:20 krw Exp $	*/

//
// dpme.h - Disk Partition Map Entry (dpme)
//
// Written by Eryk Vershen
//
// This file describes structures and values related to the standard
// Apple SCSI disk partitioning scheme.
//
// Each entry is (and shall remain) 512 bytes long.
//
// For more information see:
//	"Inside Macintosh: Devices" pages 3-12 to 3-15.
//	"Inside Macintosh - Volume V" pages V-576 to V-582
//	"Inside Macintosh - Volume IV" page IV-292
//
// There is a kernel file with much of the same info (under different names):
//	/usr/src/mklinux-1.0DR2/osfmk/src/mach_kernel/ppc/POWERMAC/mac_label.h
//

/*
 * Copyright 1996 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __dpme__
#define __dpme__

//
// Defines
//
#define	BLOCK0_SIGNATURE	0x4552	/* i.e. 'ER' */

#define	DPISTRLEN	32
#define	DPME_SIGNATURE	0x504D		/* i.e. 'PM' */

//
// Types
//
typedef	unsigned char	u8;
typedef	unsigned short	u16;
typedef	unsigned long	u32;


// Physical block zero of the disk has this format
struct Block0 {
    u16 	sbSig;		/* unique value for SCSI block 0 */
    u16 	sbBlkSize;	/* block size of device */
    u32 	sbBlkCount;	/* number of blocks on device */
    u16 	sbDevType;	/* device type */
    u16 	sbDevId;	/* device id */
    u32 	sbData;		/* not used */
    u16 	sbDrvrCount;	/* driver descriptor count */
    u16 	sbMap[247];	/* descriptor map */
};
typedef struct Block0 Block0;

// Where &sbMap[0] is actually an array DDMap[sbDrvrCount]
// kludge to get around alignment junk
struct DDMap {
    u32 	ddBlock;	/* 1st driver's starting block (in sbBlkSize blocks!) */
    u16 	ddSize;		/* size of 1st driver (512-byte blks) */
    u16 	ddType;		/* system type (1 for Mac+) */
};
typedef struct DDMap DDMap;


// Each partition map entry (blocks 1 through n) has this format
struct dpme {
    u16     dpme_signature          ;
    u16     dpme_reserved_1         ;
    u32     dpme_map_entries        ;
    u32     dpme_pblock_start       ;
    u32     dpme_pblocks            ;
    char    dpme_name[DPISTRLEN]    ;  /* name of partition */
    char    dpme_type[DPISTRLEN]    ;  /* type of partition */
    u32     dpme_lblock_start       ;
    u32     dpme_lblocks            ;
    u32     dpme_flags		    ;
#define	DPME_DISKDRIVER		(1<<9)
#define	DPME_CHAINABLE		(1<<8)
#define	DPME_OS_SPECIFIC_1	(1<<8)
#define	DPME_OS_SPECIFIC_2	(1<<7)
#define	DPME_OS_PIC_CODE	(1<<6)
#define	DPME_WRITABLE		(1<<5)
#define	DPME_READABLE		(1<<4)
#define	DPME_BOOTABLE		(1<<3)
#define	DPME_IN_USE		(1<<2)
#define	DPME_ALLOCATED		(1<<1)
#define	DPME_VALID		(1<<0)
    u32     dpme_boot_block         ;
    u32     dpme_boot_bytes         ;
    u8     *dpme_load_addr          ;
    u8     *dpme_load_addr_2        ;
    u8     *dpme_goto_addr          ;
    u8     *dpme_goto_addr_2        ;
    u32     dpme_checksum           ;
    char    dpme_process_id[16]     ;
    u32     dpme_boot_args[32]      ;
    u32     dpme_reserved_3[62]     ;
};
typedef struct dpme DPME;


//
// Global Constants
//


//
// Global Variables
//


//
// Forward declarations
//

#endif /* __dpme__ */
