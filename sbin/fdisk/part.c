/*	$OpenBSD: part.c,v 1.1 1997/09/29 22:58:18 weingart Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <util.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <machine/param.h>
#include "disk.h"
#include "misc.h"
#include "mbr.h"


static struct part_type {
	int	type;
	char	*name;
} part_types[] = {
	{ 0x00, "unused"},
	{ 0x01, "Primary DOS with 12 bit FAT"},
	{ 0x02, "XENIX / filesystem"},
	{ 0x03, "XENIX /usr filesystem"},
	{ 0x04, "Primary DOS with 16 bit FAT"},
	{ 0x05, "Extended DOS"},
	{ 0x06, "Primary 'big' DOS (> 32MB)"},
	{ 0x07, "OS/2 HPFS, QNX or Advanced UNIX"},
	{ 0x08, "AIX filesystem"},
	{ 0x09, "AIX boot partition or Coherent"},
	{ 0x0A, "OS/2 Boot Manager or OPUS"},
	{ 0x0B, "Primary Windows 95 with 32 bit FAT"},
	{ 0x0E, "Primary DOS with 16-bit FAT, CHS-mapped"},
	{ 0x10, "OPUS"},
	{ 0x12, "Compaq Diagnostics"},
	{ 0x40, "VENIX 286"},
	{ 0x50, "DM"},
	{ 0x51, "DM"},
	{ 0x52, "CP/M or Microport SysV/AT"},
	{ 0x54, "Ontrack"},
	{ 0x56, "GB"},
	{ 0x61, "Speed"},
	{ 0x63, "ISC, System V/386, GNU HURD or Mach"},
	{ 0x64, "Novell Netware 2.xx"},
	{ 0x65, "Novell Netware 3.xx"},
	{ 0x75, "PCIX"},
	{ 0x80, "Minix 1.1 ... 1.4a"},
	{ 0x81, "Minix 1.4b ... 1.5.10"},
	{ 0x82, "Linux swap"},
	{ 0x83, "Linux filesystem"},
	{ 0x93, "Amoeba filesystem"},
	{ 0x94, "Amoeba bad block table"},
	{ 0xA5, "386BSD/FreeBSD/NetBSD"},
	{ 0xA6, "OpenBSD"},
	{ 0xA7, "NEXTSTEP"},
	{ 0xB7, "BSDI BSD/386 filesystem"},
	{ 0xB8, "BSDI BSD/386 swap"},
	{ 0xDB, "Concurrent CPM or C.DOS or CTOS"},
	{ 0xE1, "Speed"},
	{ 0xE3, "Speed"},
	{ 0xE4, "Speed"},
	{ 0xF1, "Speed"},
	{ 0xF2, "DOS 3.3+ Secondary"},
	{ 0xF4, "Speed"},
	{ 0xFF, "BBT (Bad Blocks Table)"},
};


char *
PRT_ascii_id(id)
	int id;
{
	static char unknown[] = "<Unknown ID>";
	int i;

	for(i = 0; i < sizeof(part_types)/sizeof(struct part_type); i++){
		if(part_types[i].type == id)
			return(part_types[i].name);
	}

	return(unknown);
}

void
PRT_parse(prt, partn)
	void *prt;
	prt_t *partn;
{
	unsigned char *p = prt;

	partn->flag = *p++;
	partn->shead = *p++;
	partn->ssect = (*p) & 0x3F;
	partn->scyl = ((*p << 2) & 0xFF00) | (*(p+1));
	p += 2;

	partn->id = *p++;
	partn->ehead = *p++;
	partn->esect = (*p) & 0x3F;
	partn->ecyl = ((*p << 2) & 0xFF00) | (*(p+1));
	p += 2;

	partn->bs = getlong(p);
	partn->ns = getlong(p+4);
}

void
PRT_make(partn, prt)
	prt_t *partn;
	void *prt;
{
	unsigned char *p = prt;

	*p++ = partn->flag & 0xFF;
	*p++ = partn->shead & 0xFF;
	*p++ = (partn->ssect & 0x3F) | ((partn->scyl & 0x300) >> 2);
	*p++ = partn->scyl & 0xFF;

	*p++ = partn->id & 0xFF;

	*p++ = partn->ehead & 0xFF;
	*p++ = (partn->esect & 0x3F) | ((partn->ecyl & 0x300) >> 2);
	*p++ = partn->ecyl & 0xFF;

	putlong(p, partn->bs);
	putlong(p+4, partn->ns);
}

void
PRT_print(num, partn)
	int num;
	prt_t *partn;
{

	if(partn == NULL){
		printf("         Starting        Ending\n");
		printf(" #: id  cyl  hd sec -  cyl  hd sec [     start -       size]\n");
		printf("-------------------------------------------------------------------\n");
	}else{
		printf("%c%1d: %.2X %4d %3d %3d - %4d %3d %3d [%10d - %10d] %s\n",
			(partn->flag == 0x80)?'*':' ',
			num, partn->id,
			partn->scyl, partn->shead, partn->ssect,
			partn->ecyl, partn->ehead, partn->esect,
			partn->bs, partn->ns,
			PRT_ascii_id(partn->id));
	}
}

void
PRT_fix_BN(disk, part)
	disk_t *disk;
	prt_t *part;
{
	int spt, tpc, spc;
	int start = 0;
	int end = 0;

	/* Disk metrics */
	spt = disk->bios->sectors;
	tpc = disk->bios->heads;
	spc = spt * tpc;

	start += part->scyl * spc;
	start += part->shead * spt;
	start += part->ssect - 1;

	end += part->ecyl * spc;
	end += part->ehead * spt;
	end += part->esect - 1;

	/* XXX - Should handle this... */
	if(start > end)
		warn("Start of partition after end!");

	part->bs = start;
	part->ns = (end - start) + 1;
}

void
PRT_fix_CHS(disk, part)
	disk_t *disk;
	prt_t *part;
{
	int spt, tpc, spc;
	int start, end, size;
	int cyl, head, sect;

	/* Disk metrics */
	spt = disk->bios->sectors;
	tpc = disk->bios->heads;
	spc = spt * tpc;

	start = part->bs;
	size = part->ns;
	end = (start + size) - 1;

	/* Figure out starting CHS values */
	cyl = (start / spc); start -= (cyl * spc);
	head = (start / spt); start -= (head * spt);
	sect = (start + 1);

	part->scyl = cyl;
	part->shead = head;
	part->ssect = sect;

	/* Figure out ending CHS values */
	cyl = (end / spc); end -= (cyl * spc);
	head = (end / spt); end -= (head * spt);
	sect = (end + 1);

	part->ecyl = cyl;
	part->ehead = head;
	part->esect = sect;
}

