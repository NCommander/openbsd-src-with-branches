/*	$OpenBSD$	*/
/*	$NetBSD: boot.c,v 1.6 1994/10/27 04:21:49 cgd Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef lint
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifdef lint
#ifdef notdef
static char sccsid[] = "@(#)boot.c	7.3 (Berkeley) 5/4/91";
#endif
static char rcsid[] = "$NetBSD: boot.c,v 1.6 1994/10/27 04:21:49 cgd Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/reboot.h>
#include <a.out.h>
#include <sys/disklabel.h>
#include <stand.h>

/*
 * Boot program, loaded by boot block from remaing 7.5K of boot area.
 * Sifts through disklabel and attempts to load an program image of
 * a standalone program off the disk. If keyboard is hit during load,
 * or if an error is encounter, try alternate files.
 */

char *kernels[] = { "bsd.z", "obsd.z", "bsd.old.z",
		  "bsd", "obsd", "bsd.old",
		  "vmunix", "ovmunix", "vmunix.old",
		  NULL };

int	retry = 0;
extern struct disklabel disklabel;
extern	int bootdev, boothowto, cyloffset;
extern	int cnvmem, extmem;
extern	char version[];
static unsigned char *biosparams = (char *) 0x9ff00; /* XXX */
int	esym;

#ifndef HZ
#define HZ 100
#endif
int	hz = HZ;

void	copyunix (char*, int, long);
void	wait (int);

struct disklabel disklabel;

/*
 * Boot program... loads /boot out of filesystem indicated by arguements.
 * We assume an autoboot unless we detect a misconfiguration.
 */
void
boot(dev, unit, off)
{
	register struct disklabel *lp;
	register int io;
	register char **bootfile = kernels;
	int howto = 0;

	/* init system clock */
	/* startrtclock(); */

	/* are we a disk, if so look at disklabel and do things */
	lp = &disklabel;

#ifdef	DEBUG
	printf("cyl %x %x hd %x sect %x ",
		biosparams[0], biosparams[1], biosparams[2], biosparams[0xe]);
	printf("dev %x unit %x off %d\n", dev, unit, off);
#endif

	printf("\n"
		">> OpenBSD BOOT: %d/%d k [%s]\n"
		"use ? for file list, or carriage return for defaults\n"
		"use hd(1,a)/bsd to boot sd0 when wd0 is also installed\n",
		cnvmem, extmem, version);

	if (lp->d_magic == DISKMAGIC) {
	    /*
	     * Synthesize bootdev from dev, unit, type and partition
	     * information from the block 0 bootstrap.
	     * It's dirty work, but someone's got to do it.
	     * This will be used by the filesystem primatives, and
	     * drivers. Ultimately, opendev will be created corresponding
	     * to which drive to pass to top level bootstrap.
	     */
	    for (io = 0; io < lp->d_npartitions; io++) {
		int sn;

		if (lp->d_partitions[io].p_size == 0)
			continue;
		if (lp->d_type == DTYPE_SCSI)
			sn = off;
		else
			sn = off * lp->d_secpercyl;
		if (lp->d_partitions[io].p_offset == sn)
			break;
	    }

	    if (io == lp->d_npartitions) goto screwed;
            cyloffset = off;
	} else {
screwed:
		/* probably a bad or non-existant disklabel */
		io = 0 ;
		howto |= RB_SINGLE|RB_ASKNAME ;
	}

	/* construct bootdev */
	/* currently, PC has no way of booting off alternate controllers */
	bootdev = MAKEBOOTDEV(/*i_dev*/ dev, /*i_adapt*/0, /*i_ctlr*/0,
	    unit, /*i_part*/io);

	for (;;) {

		copyunix(*bootfile, howto, off);

		if(*++bootfile == NULL) bootfile = kernels;
		printf("will try %s\n", *bootfile);

		wait(1<<((retry++) + 10));
	}
}

/*ARGSUSED*/
void
copyunix(name, howto, off)
	char	*name;
	int	howto;
	long	off;
{
	int	f;
	struct stat sb;
	char	*addr = NULL, *loadaddr;

	printf("loading %s: ", name);

	/* read file */
	if (stat(name, &sb) < 0)
		return;

	if ((f = open(name, 1)) < 0)
		return;

	/* exec */
	execz(addr, loadaddr, 0);

	return;
}

int
zread(f, loadaddr, size)
	int	f;
	void	*loadaddr;
	size_t	size;
{

	return -1;
}
