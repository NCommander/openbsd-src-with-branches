/*	$OpenBSD: dkstats.c,v 1.11 2001/05/14 07:24:12 angelos Exp $	*/
/*	$NetBSD: dkstats.c,v 1.1 1996/05/10 23:19:27 thorpej Exp $	*/

/*
 * Copyright (c) 1996 John M. Vinopal
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
 *      This product includes software developed for the NetBSD Project
 *      by John M. Vinopal.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/time.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <sys/tty.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dkstats.h"

static struct nlist namelist[] = {
#define	X_TK_NIN	0
	{ "_tk_nin" },		/* tty characters in */
#define	X_TK_NOUT	1
	{ "_tk_nout" },		/* tty characters out */
#define	X_CP_TIME	2
	{ "_cp_time" },		/* system timer ticks */
#define	X_HZ		3
	{ "_hz" },		/* ticks per second */
#define	X_STATHZ	4
	{ "_stathz" },
#define X_DISK_COUNT	5
	{ "_disk_count" },	/* number of disks */
#define X_DISKLIST	6
	{ "_disklist" },	/* TAILQ of disks */
	{ NULL },
};

/* Structures to hold the statistics. */
struct _disk	cur, last;

/* Kernel pointers: nlistf and memf defined in calling program. */
static kvm_t	*kd = NULL;
extern char	*nlistf;
extern char	*memf;

/* Pointer to list of disks. */
static struct disk	*dk_drivehead = NULL;

/* Backward compatibility references. */
int	  	dk_ndrive = 0;
int		*dk_select;
char		**dr_name;

#define	KVM_ERROR(_string) {						\
	warnx("%s", (_string));						\
	errx(1, "%s", kvm_geterr(kd));					\
}

/*
 * Dereference the namelist pointer `v' and fill in the local copy 
 * 'p' which is of size 's'.
 */
#define deref_nl(v, p, s) deref_kptr((void *)namelist[(v)].n_value, (p), (s));

/* Missing from <sys/time.h> */
#define timerset(tvp, uvp) ((uvp)->tv_sec = (tvp)->tv_sec);		\
			   ((uvp)->tv_usec = (tvp)->tv_usec)

static void deref_kptr __P((void *, void *, size_t));

/*
 * Take the delta between the present values and the last recorded
 * values, storing the present values in the 'last' structure, and
 * the delta values in the 'cur' structure.
 */
void
dkswap()
{
#define SWAP(fld)		tmp = cur.fld;				\
				cur.fld -= last.fld;			\
				last.fld = tmp
	u_int64_t tmp;
	int	i;

	for (i = 0; i < dk_ndrive; i++) {
		struct timeval	tmp_timer;

		if (!cur.dk_select[i])
			continue;

		/* Delta Values. */
		SWAP(dk_xfer[i]);
		SWAP(dk_seek[i]);
		SWAP(dk_bytes[i]);

		/* Delta Time. */
		timerclear(&tmp_timer);
		timerset(&(cur.dk_time[i]), &tmp_timer);
		timersub(&tmp_timer, &(last.dk_time[i]), &(cur.dk_time[i]));
		timerclear(&(last.dk_time[i]));
		timerset(&tmp_timer, &(last.dk_time[i]));
	}
	for (i = 0; i < CPUSTATES; i++) {
		SWAP(cp_time[i]);
	}
	SWAP(tk_nin);
	SWAP(tk_nout);

#undef SWAP
}

/*
 * Read the disk statistics for each disk in the disk list. 
 * Also collect statistics for tty i/o and cpu ticks.
 */
void
dkreadstats()
{
	struct disk	cur_disk, *p;
	int		i, mib[3];
	size_t		size;

	if (nlistf == NULL && memf == NULL) {
		size = dk_ndrive * sizeof(struct disk);
		mib[0] = CTL_HW;
		mib[1] = HW_DISKSTATS;
		p = malloc(size);
		if (p == NULL)
			err(1, NULL);
		if (sysctl(mib, 2, p, &size, NULL, 0) < 0) {
			warn("could not read hw.diskstats");
			bzero(p, dk_ndrive * sizeof(struct disk));
		}

		for (i = 0; i < dk_ndrive; i++)	{
			cur.dk_xfer[i] = p[i].dk_xfer;
			cur.dk_seek[i] = p[i].dk_seek;
			cur.dk_bytes[i] = p[i].dk_bytes;
			timerset(&(p[i].dk_time), &(cur.dk_time[i]));
		}

	 	size = sizeof(cur.cp_time);
		mib[0] = CTL_KERN;
		mib[1] = KERN_CPTIME;
		if (sysctl(mib, 2, cur.cp_time, &size, NULL, 0) < 0) {
			warn("could not read kern.cp_time");
			bzero(cur.cp_time, sizeof(cur.cp_time));
		}
		size = sizeof(cur.tk_nin);
		mib[0] = CTL_KERN;
		mib[1] = KERN_TTY;
		mib[2] = KERN_TTY_TKNIN;
		if (sysctl(mib, 3, &cur.tk_nin, &size, NULL, 0) < 0) {
			warn("could not read kern.tty.tk_nin");
			cur.tk_nin = 0;
		}
		size = sizeof(cur.tk_nin);
		mib[0] = CTL_KERN;
		mib[1] = KERN_TTY;
		mib[2] = KERN_TTY_TKNOUT;
		if (sysctl(mib, 3, &cur.tk_nout, &size, NULL, 0) < 0) {
			warn("could not read kern.tty.tk_nout");
			cur.tk_nout = 0;
		}
	} else {
		p = dk_drivehead;

		for (i = 0; i < dk_ndrive; i++) {
			deref_kptr(p, &cur_disk, sizeof(cur_disk));
			cur.dk_xfer[i] = cur_disk.dk_xfer;
			cur.dk_seek[i] = cur_disk.dk_seek;
			cur.dk_bytes[i] = cur_disk.dk_bytes;
			timerset(&(cur_disk.dk_time), &(cur.dk_time[i]));
			p = cur_disk.dk_link.tqe_next;
		}

		deref_nl(X_CP_TIME, cur.cp_time, sizeof(cur.cp_time));
		deref_nl(X_TK_NIN, &cur.tk_nin, sizeof(cur.tk_nin));
		deref_nl(X_TK_NOUT, &cur.tk_nout, sizeof(cur.tk_nout));
	}
}

/*
 * Perform all of the initialization and memory allocation needed to
 * track disk statistics.
 */
int
dkinit(select)
int	select;
{
	struct disklist_head disk_head;
	struct disk	cur_disk, *p;
        char		errbuf[_POSIX2_LINE_MAX];
	static int	once = 0;
	extern int	hz;
	int		i, mib[2];
	size_t		size;
	struct clockinfo clkinfo;
	char		*disknames, *name, *bufpp;

	if (once)
		return(1);

	if (nlistf != NULL || memf != NULL) {
		/* Open the kernel. */
		if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
		    errbuf)) == NULL)
			errx(1, "kvm_openfiles: %s", errbuf);

		/* Obtain the namelist symbols from the kernel. */
		if (kvm_nlist(kd, namelist))
			KVM_ERROR("kvm_nlist failed to read symbols.");

		/* Get the number of attached drives. */
		deref_nl(X_DISK_COUNT, &dk_ndrive, sizeof(dk_ndrive));

		if (dk_ndrive < 0)
			errx(1, "invalid _disk_count %d.", dk_ndrive);

		/* Get a pointer to the first disk. */
		deref_nl(X_DISKLIST, &disk_head, sizeof(disk_head));
		dk_drivehead = disk_head.tqh_first;

		/* Get ticks per second. */
		deref_nl(X_STATHZ, &hz, sizeof(hz));
		if (!hz)
		  deref_nl(X_HZ, &hz, sizeof(hz));
	} else {
		/* Get the number of attached drives. */
		mib[0] = CTL_HW;
		mib[1] = HW_DISKCOUNT;
		size = sizeof(dk_ndrive);
		if (sysctl(mib, 2, &dk_ndrive, &size, NULL, 0) < 0 ) {
			warn("could not read hw.diskcount");
			dk_ndrive = 0;
		}

		/* Get ticks per second. */
		mib[0] = CTL_KERN;
		mib[1] = KERN_CLOCKRATE;
		size = sizeof(clkinfo);
		if (sysctl(mib, 2, &clkinfo, &size, NULL, 0) < 0) {
			warn("could not read kern.clockrate");
			hz = 0;
		} else
			hz = clkinfo.stathz;
	}

	/* allocate space for the statistics */
	cur.dk_time = calloc(dk_ndrive, sizeof(struct timeval));
	cur.dk_xfer = calloc(dk_ndrive, sizeof(u_int64_t));
	cur.dk_seek = calloc(dk_ndrive, sizeof(u_int64_t));
	cur.dk_bytes = calloc(dk_ndrive, sizeof(u_int64_t));
	last.dk_time = calloc(dk_ndrive, sizeof(struct timeval));
	last.dk_xfer = calloc(dk_ndrive, sizeof(u_int64_t));
	last.dk_seek = calloc(dk_ndrive, sizeof(u_int64_t));
	last.dk_bytes = calloc(dk_ndrive, sizeof(u_int64_t));
	cur.dk_select = calloc(dk_ndrive, sizeof(int));
	cur.dk_name = calloc(dk_ndrive, sizeof(char *));
	
	if (!cur.dk_time || !cur.dk_xfer || !cur.dk_seek || !cur.dk_bytes ||
	    !last.dk_time || !last.dk_xfer || !last.dk_seek ||
	    !last.dk_bytes || !cur.dk_select || !cur.dk_name)
		errx(1, "Memory allocation failure.");

	/* Set up the compatibility interfaces. */
	dk_select = cur.dk_select;
	dr_name = cur.dk_name;

	/* Read the disk names and set intial selection. */
	if (nlistf == NULL && memf == NULL) {
		mib[0] = CTL_HW;
		mib[1] = HW_DISKNAMES;
		size = 0;
		if (sysctl(mib, 2, NULL, &size, NULL, 0) < 0)
			err(1, "can't get hw.disknames");
		disknames = malloc(size);
		if (disknames == NULL)
			err(1, NULL);
		if (sysctl(mib, 2, disknames, &size, NULL, 0) < 0)
			err(1, "can't get hw.disknames");
		bufpp = disknames;
		i = 0;
		while ((name = strsep(&bufpp, ",")) != NULL) {
		    cur.dk_name[i] = name;
		    cur.dk_select[i++] = select;
		}
	} else {
		p = dk_drivehead;
		for (i = 0; i < dk_ndrive; i++) {
			char	buf[10];
			deref_kptr(p, &cur_disk, sizeof(cur_disk));
			deref_kptr(cur_disk.dk_name, buf, sizeof(buf));
			cur.dk_name[i] = strdup(buf);
			if (!cur.dk_name[i])
				errx(1, "Memory allocation failure.");
			cur.dk_select[i] = select;

			p = cur_disk.dk_link.tqe_next;
		}
	}

	/* Never do this initalization again. */
	once = 1;
	return(1);
}

/*
 * Dereference the kernel pointer `kptr' and fill in the local copy 
 * pointed to by `ptr'.  The storage space must be pre-allocated,
 * and the size of the copy passed in `len'.
 */
static void
deref_kptr(kptr, ptr, len)
	void *kptr, *ptr;
	size_t len;
{
	char buf[128];

	if (kvm_read(kd, (u_long)kptr, ptr, len) != len) {
		bzero(buf, sizeof(buf));
		snprintf(buf, (sizeof(buf) - 1),
		     "can't dereference kptr 0x%lx", (u_long)kptr);
		KVM_ERROR(buf);
	}
}
