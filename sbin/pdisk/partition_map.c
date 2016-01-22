/*	$OpenBSD: partition_map.c,v 1.49 2016/01/22 17:35:16 krw Exp $	*/

/*
 * partition_map.c - partition map routines
 *
 * Written by Eryk Vershen
 */

/*
 * Copyright 1996,1997,1998 by Apple Computer, Inc.
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

#include <sys/param.h>		/* DEV_BSIZE */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "partition_map.h"
#include "io.h"
#include "convert.h"
#include "file_media.h"

#define APPLE_HFS_FLAGS_VALUE	0x4000037f
#define get_align_long(x)	(*(x))
#define put_align_long(y, x)	((*(x)) = (y))

const char     *kFreeType = "Apple_Free";
const char     *kMapType = "Apple_partition_map";
const char     *kUnixType = "OpenBSD";
const char     *kHFSType = "Apple_HFS";
const char     *kPatchType = "Apple_Patches";

const char     *kFreeName = "Extra";

enum add_action {
	kReplace = 0,
	kAdd = 1,
	kSplit = 2
};

int		add_data_to_map(struct dpme *, long,
		    struct partition_map_header *);
int		coerce_block0(struct partition_map_header *);
int		contains_driver(struct partition_map *);
void		combine_entry(struct partition_map *);
struct dpme    *create_dpme(const char *, const char *, uint32_t, uint32_t);
void		delete_entry(struct partition_map *);
void		insert_in_base_order(struct partition_map *);
void		insert_in_disk_order(struct partition_map *);
int		read_partition_map(struct partition_map_header *);
void		remove_driver(struct partition_map *);
void		remove_from_disk_order(struct partition_map *);
void		renumber_disk_addresses(struct partition_map_header *);

struct partition_map_header *
open_partition_map(int fd, char *name, uint64_t mediasz)
{
	struct partition_map_header *map;

	map = malloc(sizeof(struct partition_map_header));
	if (map == NULL) {
		warn("can't allocate memory for open partition map");
		return NULL;
	}

	map->fd = fd;
	map->name = name;

	map->changed = 0;
	map->disk_order = NULL;
	map->base_order = NULL;
	map->physical_block = DEV_BSIZE;
	map->logical_block = DEV_BSIZE;
	map->blocks_in_map = 0;
	map->maximum_in_map = -1;

	map->block0 = malloc(DEV_BSIZE);
	if (map->block0 == NULL) {
		warn("can't allocate memory for block zero buffer");
		free(map);
		return NULL;
	}
	if (read_block(map->fd, 0, map->block0) == 0 ||
	    convert_block0(map->block0, 1) ||
	    coerce_block0(map)) {
		warnx("Can't read block 0 from '%s'", name);
		free_partition_map(map);
		return NULL;
	}

	if (read_partition_map(map) != -1)
		return map;

	if (!lflag) {
		my_ungetch('\n');
		printf("No valid partition map found on '%s'.\n", name);
		if (get_okay("Create default map? [n/y]: ", 0) == 1) {
			free_partition_map(map);
			map = create_partition_map(fd, name, mediasz);
			if (map)
				return (map);
		}
	}

	free_partition_map(map);
	return NULL;
}


void
free_partition_map(struct partition_map_header * map)
{
	struct partition_map *entry, *next;

	if (map) {
		free(map->block0);
		for (entry = map->disk_order; entry != NULL; entry = next) {
			next = entry->next_on_disk;
			free(entry->dpme);
			free(entry);
		}
		free(map);
	}
}

int
read_partition_map(struct partition_map_header * map)
{
	struct dpme    *dpme;
	double d;
	int ix, old_logical;
	uint32_t limit;

	dpme = malloc(DEV_BSIZE);
	if (dpme == NULL) {
		warn("can't allocate memory for disk buffers");
		return -1;
	}
	if (read_block(map->fd, DEV_BSIZE, dpme) == 0) {
		warnx("Can't read block 1 from '%s'", map->name);
		free(dpme);
		return -1;
	} else if (convert_dpme(dpme, 1) ||
		   dpme->dpme_signature != DPME_SIGNATURE) {
		old_logical = map->logical_block;
		map->logical_block = 512;
		while (map->logical_block <= map->physical_block) {
			if (read_block(map->fd, DEV_BSIZE, dpme) == 0) {
				warnx("Can't read block 1 from '%s'",
				    map->name);
				free(dpme);
				return -1;
			} else if (convert_dpme(dpme, 1) == 0
				&& dpme->dpme_signature == DPME_SIGNATURE) {
				d = map->media_size;
				map->media_size = (d * old_logical) /
				    map->logical_block;
				break;
			}
			map->logical_block *= 2;
		}
		if (map->logical_block > map->physical_block) {
			warnx("No valid block 1 on '%s'", map->name);
			free(dpme);
			return -1;
		}
	}
	limit = dpme->dpme_map_entries;
	ix = 1;
	while (1) {
		if (add_data_to_map(dpme, ix, map) == 0) {
			free(dpme);
			return -1;
		}
		if (ix >= limit) {
			break;
		} else {
			ix++;
		}

		dpme = malloc(DEV_BSIZE);
		if (dpme == NULL) {
			warn("can't allocate memory for disk buffers");
			return -1;
		}
		if (read_block(map->fd, ix * DEV_BSIZE, dpme) == 0) {
			warnx("Can't read block %u from '%s'", ix, map->name);
			free(dpme);
			return -1;
		} else if (convert_dpme(dpme, 1) ||
			   (dpme->dpme_signature != DPME_SIGNATURE) ||
			   (dpme->dpme_map_entries != limit)) {
			warnx("Bad dpme in block %u from '%s'", ix, map->name);
			free(dpme);
			return -1;
		}
	}
	return 0;
}


void
write_partition_map(struct partition_map_header * map)
{
	struct partition_map *entry;
	char *block;
	int i = 0, result = 0;

	if (map->block0 != NULL) {
		convert_block0(map->block0, 0);
		result = write_block(map->fd, 0, map->block0);
		convert_block0(map->block0, 1);
	} else {
		block = calloc(1, DEV_BSIZE);
		if (block != NULL) {
			result = write_block(map->fd, 0, block);
			free(block);
		}
	}
	if (result == 0) {
		warn("Unable to write block zero");
	}
	for (entry = map->disk_order; entry != NULL;
	    entry = entry->next_on_disk) {
		convert_dpme(entry->dpme, 0);
		result = write_block(map->fd, entry->disk_address * DEV_BSIZE,
		    entry->dpme);
		convert_dpme(entry->dpme, 1);
		i = entry->disk_address;
		if (result == 0) {
			warn("Unable to write block %d", i);
		}
	}
}


int
add_data_to_map(struct dpme * dpme, long ix, struct partition_map_header * map)
{
	struct partition_map *entry;

	entry = malloc(sizeof(struct partition_map));
	if (entry == NULL) {
		warn("can't allocate memory for map entries");
		return 0;
	}
	entry->next_on_disk = NULL;
	entry->prev_on_disk = NULL;
	entry->next_by_base = NULL;
	entry->prev_by_base = NULL;
	entry->disk_address = ix;
	entry->the_map = map;
	entry->dpme = dpme;
	entry->contains_driver = contains_driver(entry);

	insert_in_disk_order(entry);
	insert_in_base_order(entry);

	map->blocks_in_map++;
	if (map->maximum_in_map < 0) {
		if (strncasecmp(dpme->dpme_type, kMapType, DPISTRLEN) == 0) {
			map->maximum_in_map = dpme->dpme_pblocks;
		}
	}
	return 1;
}

struct partition_map_header *
create_partition_map(int fd, char *name, u_int64_t mediasz)
{
	struct partition_map_header *map;
	struct dpme *dpme;

	map = malloc(sizeof(struct partition_map_header));
	if (map == NULL) {
		warn("can't allocate memory for open partition map");
		return NULL;
	}
	map->name = name;
	map->fd = fd;
	map->changed = 1;
	map->disk_order = NULL;
	map->base_order = NULL;

	map->physical_block = DEV_BSIZE;
	map->logical_block = DEV_BSIZE;

	map->blocks_in_map = 0;
	map->maximum_in_map = -1;
	map->media_size = mediasz;
	sync_device_size(map);

	map->block0 = calloc(1, DEV_BSIZE);
	if (map->block0 == NULL) {
		warn("can't allocate memory for block zero buffer");
	} else {
		coerce_block0(map);

		dpme = calloc(1, DEV_BSIZE);
		if (dpme == NULL) {
			warn("can't allocate memory for disk buffers");
		} else {
			dpme->dpme_signature = DPME_SIGNATURE;
			dpme->dpme_map_entries = 1;
			dpme->dpme_pblock_start = 1;
			dpme->dpme_pblocks = map->media_size - 1;
			strncpy(dpme->dpme_name, kFreeName, DPISTRLEN);
			strncpy(dpme->dpme_type, kFreeType, DPISTRLEN);
			dpme->dpme_lblock_start = 0;
			dpme->dpme_lblocks = dpme->dpme_pblocks;
			dpme->dpme_flags = DPME_WRITABLE | DPME_READABLE |
			    DPME_VALID;

			if (add_data_to_map(dpme, 1, map) == 0) {
				free(dpme);
			} else {
				add_partition_to_map("Apple", kMapType,
				    1, (map->media_size <= 128 ? 2 : 63), map);
				return map;
			}
		}
	}

	free_partition_map(map);
	return NULL;
}


int
coerce_block0(struct partition_map_header * map)
{
	struct block0 *p;

	p = map->block0;
	if (p == NULL) {
		return 1;
	}
	if (p->sbSig != BLOCK0_SIGNATURE) {
		p->sbSig = BLOCK0_SIGNATURE;
		if (map->physical_block == 1) {
			p->sbBlkSize = DEV_BSIZE;
		} else {
			p->sbBlkSize = map->physical_block;
		}
		p->sbBlkCount = 0;
		p->sbDevType = 0;
		p->sbDevId = 0;
		p->sbData = 0;
		p->sbDrvrCount = 0;
	}
	return 0;
}


int
add_partition_to_map(const char *name, const char *dptype, uint32_t base, uint32_t length,
		     struct partition_map_header * map)
{
	struct partition_map *cur;
	struct dpme *dpme;
	enum add_action act;
	int limit;
	uint32_t adjusted_base = 0;
	uint32_t adjusted_length = 0;
	uint32_t new_base = 0;
	uint32_t new_length = 0;

	/* find a block that starts includes base and length */
	cur = map->base_order;
	while (cur != NULL) {
		if (cur->dpme->dpme_pblock_start <= base &&
		    (base + length) <=
		    (cur->dpme->dpme_pblock_start + cur->dpme->dpme_pblocks)) {
			break;
		} else {
			/*
			 * check if request is past end of existing
			 * partitions, but on disk
			 */
			if ((cur->next_by_base == NULL) &&
			    (base + length <= map->media_size)) {
				/* Expand final free partition */
				if ((strncasecmp(cur->dpme->dpme_type,
				    kFreeType, DPISTRLEN) == 0) &&
				    base >= cur->dpme->dpme_pblock_start) {
					cur->dpme->dpme_pblocks =
						map->media_size -
						cur->dpme->dpme_pblock_start;
					break;
				}
				/* create an extra free partition */
				if (base >= cur->dpme->dpme_pblock_start +
				    cur->dpme->dpme_pblocks) {
					if (map->maximum_in_map < 0) {
						limit = map->media_size;
					} else {
						limit = map->maximum_in_map;
					}
					if (map->blocks_in_map + 1 > limit) {
						printf("the map is not big "
						    "enough\n");
						return 0;
					}
					dpme = create_dpme(kFreeName, kFreeType,
					    cur->dpme->dpme_pblock_start +
					    cur->dpme->dpme_pblocks,
					    map->media_size -
					    (cur->dpme->dpme_pblock_start +
					    cur->dpme->dpme_pblocks));
					if (dpme != NULL) {
						if (add_data_to_map(dpme,
						    cur->disk_address, map) ==
						    0) {
							free(dpme);
						}
					}
				}
			}
			cur = cur->next_by_base;
		}
	}
	/* if it is not Extra then punt */
	if (cur == NULL ||
	    strncasecmp(cur->dpme->dpme_type, kFreeType, DPISTRLEN) != 0) {
		printf("requested base and length is not "
		       "within an existing free partition\n");
		return 0;
	}
	/* figure out what to do and sizes */
	dpme = cur->dpme;
	if (dpme->dpme_pblock_start == base) {
		/* replace or add */
		if (dpme->dpme_pblocks == length) {
			act = kReplace;
		} else {
			act = kAdd;
			adjusted_base = base + length;
			adjusted_length = dpme->dpme_pblocks - length;
		}
	} else {
		/* split or add */
		if (dpme->dpme_pblock_start + dpme->dpme_pblocks == base +
		    length) {
			act = kAdd;
			adjusted_base = dpme->dpme_pblock_start;
			adjusted_length = base - adjusted_base;
		} else {
			act = kSplit;
			new_base = dpme->dpme_pblock_start;
			new_length = base - new_base;
			adjusted_base = base + length;
			adjusted_length = dpme->dpme_pblocks - (length +
			    new_length);
		}
	}
	/* if the map will overflow then punt */
	if (map->maximum_in_map < 0) {
		limit = map->media_size;
	} else {
		limit = map->maximum_in_map;
	}
	if (map->blocks_in_map + act > limit) {
		printf("the map is not big enough\n");
		return 0;
	}
	dpme = create_dpme(name, dptype, base, length);
	if (dpme == NULL) {
		return 0;
	}
	if (act == kReplace) {
		free(cur->dpme);
		cur->dpme = dpme;
	} else {
		/* adjust this block's size */
		cur->dpme->dpme_pblock_start = adjusted_base;
		cur->dpme->dpme_pblocks = adjusted_length;
		cur->dpme->dpme_lblocks = adjusted_length;
		/* insert new with block address equal to this one */
		if (add_data_to_map(dpme, cur->disk_address, map) == 0) {
			free(dpme);
		} else if (act == kSplit) {
			dpme = create_dpme(kFreeName, kFreeType, new_base,
			    new_length);
			if (dpme != NULL) {
				/*
				 * insert new with block address equal to
				 * this one
				 */
				if (add_data_to_map(dpme, cur->disk_address,
				    map) == 0) {
					free(dpme);
				}
			}
		}
	}
	renumber_disk_addresses(map);
	map->changed = 1;
	return 1;
}


struct dpme*
create_dpme(const char *name, const char *dptype, uint32_t base,
    uint32_t length)
{
	struct dpme *dpme;

	dpme = calloc(1, DEV_BSIZE);
	if (dpme == NULL) {
		warn("can't allocate memory for disk buffers");
	} else {
		dpme->dpme_signature = DPME_SIGNATURE;
		dpme->dpme_map_entries = 1;
		dpme->dpme_pblock_start = base;
		dpme->dpme_pblocks = length;
		strncpy(dpme->dpme_name, name, DPISTRLEN);
		strncpy(dpme->dpme_type, dptype, DPISTRLEN);
		dpme->dpme_lblock_start = 0;
		dpme->dpme_lblocks = dpme->dpme_pblocks;
		dpme_init_flags(dpme);
	}
	return dpme;
}

void
dpme_init_flags(struct dpme * dpme)
{
	if (strncasecmp(dpme->dpme_type, kHFSType, DPISTRLEN) == 0) {
		/* XXX this is gross, fix it! */
		dpme->dpme_flags = APPLE_HFS_FLAGS_VALUE;
	} else {
		dpme->dpme_flags = DPME_WRITABLE | DPME_READABLE |
		    DPME_ALLOCATED | DPME_VALID;
	}
}

void
renumber_disk_addresses(struct partition_map_header * map)
{
	struct partition_map *cur;
	long ix;

	/* reset disk addresses */
	cur = map->disk_order;
	ix = 1;
	while (cur != NULL) {
		cur->disk_address = ix++;
		cur->dpme->dpme_map_entries = map->blocks_in_map;
		cur = cur->next_on_disk;
	}
}

void
sync_device_size(struct partition_map_header * map)
{
	struct block0  *p;
	unsigned long size;
	double d;

	p = map->block0;
	if (p == NULL) {
		return;
	}
	d = map->media_size;
	size = (d * map->logical_block) / p->sbBlkSize;
	if (p->sbBlkCount != size) {
		p->sbBlkCount = size;
	}
}


void
delete_partition_from_map(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct dpme *dpme;

	if (strncasecmp(entry->dpme->dpme_type, kMapType, DPISTRLEN) == 0) {
		printf("Can't delete entry for the map itself\n");
		return;
	}
	if (entry->contains_driver) {
		printf("This program can't install drivers\n");
		if (get_okay("are you sure you want to delete this driver? "
		    "[n/y]: ", 0) != 1) {
			return;
		}
	}
	/* if past end of disk, delete it completely */
	if (entry->next_by_base == NULL &&
	    entry->dpme->dpme_pblock_start >= entry->the_map->media_size) {
		if (entry->contains_driver) {
			remove_driver(entry);	/* update block0 if necessary */
		}
		delete_entry(entry);
		return;
	}
	/* If at end of disk, incorporate extra disk space to partition */
	if (entry->next_by_base == NULL) {
		entry->dpme->dpme_pblocks = entry->the_map->media_size -
		    entry->dpme->dpme_pblock_start;
	}
	dpme = create_dpme(kFreeName, kFreeType,
		 entry->dpme->dpme_pblock_start, entry->dpme->dpme_pblocks);
	if (dpme == NULL) {
		return;
	}
	if (entry->contains_driver) {
		remove_driver(entry);	/* update block0 if necessary */
	}
	free(entry->dpme);
	entry->dpme = dpme;
	combine_entry(entry);
	map = entry->the_map;
	renumber_disk_addresses(map);
	map->changed = 1;
}


int
contains_driver(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct block0  *p;
	struct ddmap   *m;
	int i, f;
	uint32_t start;

	map = entry->the_map;
	p = map->block0;
	if (p == NULL) {
		return 0;
	}
	if (p->sbSig != BLOCK0_SIGNATURE) {
		return 0;
	}
	if (map->logical_block > p->sbBlkSize) {
		return 0;
	} else {
		f = p->sbBlkSize / map->logical_block;
	}
	if (p->sbDrvrCount > 0) {
		m = (struct ddmap *) p->sbMap;
		for (i = 0; i < p->sbDrvrCount; i++) {
			start = get_align_long(&m[i].ddBlock);
			if (entry->dpme->dpme_pblock_start <= f * start &&
			    f * (start + m[i].ddSize) <=
			    (entry->dpme->dpme_pblock_start +
			    entry->dpme->dpme_pblocks)) {
				return 1;
			}
		}
	}
	return 0;
}


void
combine_entry(struct partition_map * entry)
{
	struct partition_map *p;
	uint32_t end;

	if (entry == NULL
	|| strncasecmp(entry->dpme->dpme_type, kFreeType, DPISTRLEN) != 0) {
		return;
	}
	if (entry->next_by_base != NULL) {
		p = entry->next_by_base;
		if (strncasecmp(p->dpme->dpme_type, kFreeType, DPISTRLEN) != 0) {
			/* next is not free */
		} else if (entry->dpme->dpme_pblock_start +
			   entry->dpme->dpme_pblocks !=
			   p->dpme->dpme_pblock_start) {
			/* next is not contiguous (XXX this is bad) */
			printf("next entry is not contiguous\n");
			/* start is already minimum */
			/* new end is maximum of two ends */
			end = p->dpme->dpme_pblock_start +
			    p->dpme->dpme_pblocks;
			if (end > entry->dpme->dpme_pblock_start +
			    entry->dpme->dpme_pblocks) {
				entry->dpme->dpme_pblocks = end -
				    entry->dpme->dpme_pblock_start;
			}
			entry->dpme->dpme_lblocks = entry->dpme->dpme_pblocks;
			delete_entry(p);
		} else {
			entry->dpme->dpme_pblocks += p->dpme->dpme_pblocks;
			entry->dpme->dpme_lblocks = entry->dpme->dpme_pblocks;
			delete_entry(p);
		}
	}
	if (entry->prev_by_base != NULL) {
		p = entry->prev_by_base;
		if (strncasecmp(p->dpme->dpme_type, kFreeType, DPISTRLEN) != 0) {
			/* previous is not free */
		} else if (p->dpme->dpme_pblock_start + p->dpme->dpme_pblocks
			   != entry->dpme->dpme_pblock_start) {
			/* previous is not contiguous (XXX this is bad) */
			printf("previous entry is not contiguous\n");
			/* new end is maximum of two ends */
			end = p->dpme->dpme_pblock_start +
			    p->dpme->dpme_pblocks;
			if (end < entry->dpme->dpme_pblock_start +
			    entry->dpme->dpme_pblocks) {
				end = entry->dpme->dpme_pblock_start +
				    entry->dpme->dpme_pblocks;
			}
			entry->dpme->dpme_pblocks = end -
			    p->dpme->dpme_pblock_start;
			/* new start is previous entry's start */
			entry->dpme->dpme_pblock_start =
			    p->dpme->dpme_pblock_start;
			entry->dpme->dpme_lblocks = entry->dpme->dpme_pblocks;
			delete_entry(p);
		} else {
			entry->dpme->dpme_pblock_start =
			    p->dpme->dpme_pblock_start;
			entry->dpme->dpme_pblocks += p->dpme->dpme_pblocks;
			entry->dpme->dpme_lblocks = entry->dpme->dpme_pblocks;
			delete_entry(p);
		}
	}
	entry->contains_driver = contains_driver(entry);
}


void
delete_entry(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *p;

	map = entry->the_map;
	map->blocks_in_map--;

	remove_from_disk_order(entry);

	p = entry->next_by_base;
	if (map->base_order == entry) {
		map->base_order = p;
	}
	if (p != NULL) {
		p->prev_by_base = entry->prev_by_base;
	}
	if (entry->prev_by_base != NULL) {
		entry->prev_by_base->next_by_base = p;
	}
	free(entry->dpme);
	free(entry);
}


struct partition_map *
find_entry_by_disk_address(long ix, struct partition_map_header * map)
{
	struct partition_map *cur;

	cur = map->disk_order;
	while (cur != NULL) {
		if (cur->disk_address == ix) {
			break;
		}
		cur = cur->next_on_disk;
	}
	return cur;
}


struct partition_map *
find_entry_by_type(const char *type_name, struct partition_map_header * map)
{
	struct partition_map *cur;

	cur = map->base_order;
	while (cur != NULL) {
		if (strncasecmp(cur->dpme->dpme_type, type_name, DPISTRLEN) ==
		    0) {
			break;
		}
		cur = cur->next_by_base;
	}
	return cur;
}

struct partition_map *
find_entry_by_base(uint32_t base, struct partition_map_header * map)
{
	struct partition_map *cur;

	cur = map->base_order;
	while (cur != NULL) {
		if (cur->dpme->dpme_pblock_start == base) {
			break;
		}
		cur = cur->next_by_base;
	}
	return cur;
}


void
move_entry_in_map(long old_index, long ix, struct partition_map_header * map)
{
	struct partition_map *cur;

	cur = find_entry_by_disk_address(old_index, map);
	if (cur == NULL) {
		printf("No such partition\n");
	} else {
		remove_from_disk_order(cur);
		cur->disk_address = ix;
		insert_in_disk_order(cur);
		renumber_disk_addresses(map);
		map->changed = 1;
	}
}


void
remove_from_disk_order(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *p;

	map = entry->the_map;
	p = entry->next_on_disk;
	if (map->disk_order == entry) {
		map->disk_order = p;
	}
	if (p != NULL) {
		p->prev_on_disk = entry->prev_on_disk;
	}
	if (entry->prev_on_disk != NULL) {
		entry->prev_on_disk->next_on_disk = p;
	}
	entry->next_on_disk = NULL;
	entry->prev_on_disk = NULL;
}


void
insert_in_disk_order(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *cur;

	/* find position in disk list & insert */
	map = entry->the_map;
	cur = map->disk_order;
	if (cur == NULL || entry->disk_address <= cur->disk_address) {
		map->disk_order = entry;
		entry->next_on_disk = cur;
		if (cur != NULL) {
			cur->prev_on_disk = entry;
		}
		entry->prev_on_disk = NULL;
	} else {
		for (cur = map->disk_order; cur != NULL;
		    cur = cur->next_on_disk) {
			if (cur->disk_address <= entry->disk_address &&
			    (cur->next_on_disk == NULL ||
			    entry->disk_address <=
			    cur->next_on_disk->disk_address)) {
				entry->next_on_disk = cur->next_on_disk;
				cur->next_on_disk = entry;
				entry->prev_on_disk = cur;
				if (entry->next_on_disk != NULL) {
					entry->next_on_disk->prev_on_disk =
					    entry;
				}
				break;
			}
		}
	}
}


void
insert_in_base_order(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct partition_map *cur;

	/* find position in base list & insert */
	map = entry->the_map;
	cur = map->base_order;
	if (cur == NULL
	|| entry->dpme->dpme_pblock_start <= cur->dpme->dpme_pblock_start) {
		map->base_order = entry;
		entry->next_by_base = cur;
		if (cur != NULL) {
			cur->prev_by_base = entry;
		}
		entry->prev_by_base = NULL;
	} else {
		for (cur = map->base_order; cur != NULL;
		    cur = cur->next_by_base) {
			if (cur->dpme->dpme_pblock_start <=
			    entry->dpme->dpme_pblock_start &&
			    (cur->next_by_base == NULL ||
			    entry->dpme->dpme_pblock_start <=
			    cur->next_by_base->dpme->dpme_pblock_start)) {
				entry->next_by_base = cur->next_by_base;
				cur->next_by_base = entry;
				entry->prev_by_base = cur;
				if (entry->next_by_base != NULL) {
					entry->next_by_base->prev_by_base =
					    entry;
				}
				break;
			}
		}
	}
}


void
resize_map(long new_size, struct partition_map_header * map)
{
	struct partition_map *entry;
	struct partition_map *next;
	int incr;

	entry = find_entry_by_type(kMapType, map);

	if (entry == NULL) {
		printf("Couldn't find entry for map!\n");
		return;
	}
	next = entry->next_by_base;

	if (new_size == entry->dpme->dpme_pblocks) {
		return;
	}
	/* make it smaller */
	if (new_size < entry->dpme->dpme_pblocks) {
		if (next == NULL ||
		    strncasecmp(next->dpme->dpme_type, kFreeType, DPISTRLEN) !=
		    0) {
			incr = 1;
		} else {
			incr = 0;
		}
		if (new_size < map->blocks_in_map + incr) {
			printf("New size would be too small\n");
			return;
		}
		goto doit;
	}
	/* make it larger */
	if (next == NULL ||
	    strncasecmp(next->dpme->dpme_type, kFreeType, DPISTRLEN) != 0) {
		printf("No free space to expand into\n");
		return;
	}
	if (entry->dpme->dpme_pblock_start + entry->dpme->dpme_pblocks
	    != next->dpme->dpme_pblock_start) {
		printf("No contiguous free space to expand into\n");
		return;
	}
	if (new_size > entry->dpme->dpme_pblocks + next->dpme->dpme_pblocks) {
		printf("No enough free space\n");
		return;
	}
doit:
	entry->dpme->dpme_type[0] = 0;
	delete_partition_from_map(entry);
	add_partition_to_map("Apple", kMapType, 1, new_size, map);
	map->maximum_in_map = new_size;
}


void
remove_driver(struct partition_map * entry)
{
	struct partition_map_header *map;
	struct block0  *p;
	struct ddmap   *m;
	int i, j, f;
	uint32_t start;

	map = entry->the_map;
	p = map->block0;
	if (p == NULL) {
		return;
	}
	if (p->sbSig != BLOCK0_SIGNATURE) {
		return;
	}
	if (map->logical_block > p->sbBlkSize) {
		/* this is not supposed to happen, but let's just ignore it. */
		return;
	} else {
		/*
		 * compute the factor to convert the block numbers in block0
		 * into partition map block numbers.
		 */
		f = p->sbBlkSize / map->logical_block;
	}
	if (p->sbDrvrCount > 0) {
		m = (struct ddmap *) p->sbMap;
		for (i = 0; i < p->sbDrvrCount; i++) {
			start = get_align_long(&m[i].ddBlock);

			/*
			 * zap the driver if it is wholly contained in the
			 * partition
			 */
			if (entry->dpme->dpme_pblock_start <= f * start &&
			    f * (start + m[i].ddSize) <=
			    (entry->dpme->dpme_pblock_start
				+ entry->dpme->dpme_pblocks)) {
				/* delete this driver */
				/*
				 * by copying down later ones and zapping the
				 * last
				 */
				for (j = i + 1; j < p->sbDrvrCount; j++, i++) {
					put_align_long(get_align_long(
					    &m[j].ddBlock), &m[i].ddBlock);
					m[i].ddSize = m[j].ddSize;
					m[i].ddType = m[j].ddType;
				}
				put_align_long(0, &m[i].ddBlock);
				m[i].ddSize = 0;
				m[i].ddType = 0;
				p->sbDrvrCount -= 1;
				return;	/* XXX if we continue we will delete
					 * other drivers? */
			}
		}
	}
}

