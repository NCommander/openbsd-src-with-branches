/*	$OpenBSD: partition_map.h,v 1.12 2016/01/17 16:15:59 krw Exp $	*/

//
// partition_map.h - partition map routines
//
// Written by Eryk Vershen
//

/*
 * Copyright 1996,1998 by Apple Computer, Inc.
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

#ifndef __partition_map__
#define __partition_map__

#include "dpme.h"
#include "file_media.h"


//
// Defines
//


//
// Types
//
struct partition_map_header {
    struct file_media *m;
    char *name;
    struct partition_map * disk_order;
    struct partition_map * base_order;
    struct block0 *misc;
    int writable;
    int changed;
    int written;
    int physical_block;		// must be == sbBlockSize
    int logical_block;		// must be <= physical_block
    int blocks_in_map;
    int maximum_in_map;
    unsigned long media_size;	// in logical_blocks
};

struct partition_map {
    struct partition_map * next_on_disk;
    struct partition_map * prev_on_disk;
    struct partition_map * next_by_base;
    struct partition_map * prev_by_base;
    long disk_address;
    struct partition_map_header * the_map;
    int contains_driver;
    struct dpme *data;
    int HFS_kind;
};

/* Identifies the HFS kind. */
enum {
    kHFS_not       =   0,	// ' '
    kHFS_std       =   1,	// 'h'
    kHFS_embed     =   2,	// 'e'
    kHFS_plus      =   3	// '+'
};


//
// Global Constants
//
extern const char * kFreeType;
extern const char * kMapType;
extern const char * kUnixType;
extern const char * kHFSType;
extern const char * kFreeName;
extern const char * kPatchType;


//
// Global Variables
//
extern int rflag;
extern int dflag;


//
// Forward declarations
//
int add_partition_to_map(const char *name, const char *dptype, u32 base, u32 length, struct partition_map_header *map);
void close_partition_map(struct partition_map_header *map);
struct partition_map_header* create_partition_map(char *name, struct partition_map_header *oldmap);
void delete_partition_from_map(struct partition_map *entry);
struct partition_map* find_entry_by_disk_address(long, struct partition_map_header *);
struct partition_map* find_entry_by_type(const char *type_name, struct partition_map_header *map);
struct partition_map* find_entry_by_base(u32 base, struct partition_map_header *map);
struct partition_map_header* init_partition_map(char *name, struct partition_map_header* oldmap);
void move_entry_in_map(long, long, struct partition_map_header *);
struct partition_map_header* open_partition_map(char *name, int *valid_file);
void resize_map(long new_size, struct partition_map_header *map);
void write_partition_map(struct partition_map_header *map);
void dpme_init_flags(struct dpme *data);

#endif /* __partition_map__ */
