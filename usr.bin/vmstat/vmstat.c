/*	$NetBSD: vmstat.c,v 1.29.4.1 1996/06/05 00:21:05 cgd Exp $	*/
/*	$OpenBSD: vmstat.c,v 1.54 2001/06/23 21:59:44 art Exp $	*/

/*
 * Copyright (c) 1980, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1986, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)vmstat.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$NetBSD: vmstat.c,v 1.29.4.1 1996/06/05 00:21:05 cgd Exp $";
#endif
#endif /* not lint */

#define __POOL_EXPOSE

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <vm/vm.h>
#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <limits.h>
#include "dkstats.h"

#ifdef UVM
#include <uvm/uvm_extern.h>
#endif

struct nlist namelist[] = {
#if defined(UVM)
#define X_UVMEXP	0
	{ "_uvmexp" },
#else
#define X_SUM		0
	{ "_cnt" },
#endif
#define	X_BOOTTIME	1
	{ "_boottime" },
#define X_NCHSTATS	2
	{ "_nchstats" },
#define	X_INTRNAMES	3
	{ "_intrnames" },
#define	X_EINTRNAMES	4
	{ "_eintrnames" },
#define	X_INTRCNT	5
	{ "_intrcnt" },
#define	X_EINTRCNT	6
	{ "_eintrcnt" },
#define	X_KMEMSTAT	7
	{ "_kmemstats" },
#define	X_KMEMBUCKETS	8
	{ "_bucket" },
#define X_ALLEVENTS	9
	{ "_allevents" },
#define	X_FORKSTAT	10
	{ "_forkstat" },
#define X_POOLHEAD	11
	{ "_pool_head" },
#define X_NSELCOLL	12
	{ "_nselcoll" },
#define X_END		13
#if defined(__i386__)
#define	X_INTRHAND	(X_END)
	{ "_intrhand" },
#define	X_INTRSTRAY	(X_END+1)
	{ "_intrstray" },
#endif
	{ "" },
};

/* Objects defined in dkstats.c */
extern struct _disk	cur;
extern char	**dr_name;
extern int	*dk_select, dk_ndrive;

#ifdef UVM
struct	uvmexp uvmexp, ouvmexp;
#else
struct	vmmeter sum, osum;
#endif
int		ndrives;

int	winlines = 20;

kvm_t *kd;

#define	FORKSTAT	0x01
#define	INTRSTAT	0x02
#define	MEMSTAT		0x04
#define	SUMSTAT		0x08
#define	TIMESTAT	0x10
#define	VMSTAT		0x20

void	cpustats __P((void));
void	dkstats __P((void));
void	dointr __P((void));
void	domem __P((void));
void	dopool __P((void));
void	dosum __P((void));
void	dovmstat __P((u_int, int));
void	kread __P((int, void *, size_t));
void	usage __P((void));
void	dotimes __P((void));
void	doforkst __P((void));
void	printhdr __P((void));

char	**choosedrives __P((char **));

/* Namelist and memory file names. */
char	*nlistf, *memf;

extern char *__progname;

int
main(argc, argv)
	register int argc;
	register char **argv;
{
	extern int optind;
	extern char *optarg;
	register int c, todo;
	u_int interval;
	int reps;
	char errbuf[_POSIX2_LINE_MAX];

	memf = nlistf = NULL;
	interval = reps = todo = 0;
	while ((c = getopt(argc, argv, "c:fiM:mN:stw:")) != -1) {
		switch (c) {
		case 'c':
			reps = atoi(optarg);
			break;
		case 'f':
			todo |= FORKSTAT;
			break;
		case 'i':
			todo |= INTRSTAT;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			todo |= MEMSTAT;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 's':
			todo |= SUMSTAT;
			break;
		case 't':
			todo |= TIMESTAT;
			break;
		case 'w':
			interval = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (todo == 0)
		todo = VMSTAT;

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

	kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
	if (kd == 0)
		errx(1, "kvm_openfiles: %s", errbuf);

	if ((c = kvm_nlist(kd, namelist)) != 0) {

		setegid(getgid());
		setgid(getgid());

		if (c > 0) {
			(void)fprintf(stderr,
			    "%s: undefined symbols:", __progname);
			for (c = 0;
			    c < sizeof(namelist)/sizeof(namelist[0]); c++)
				if (namelist[c].n_type == 0)
					fprintf(stderr, " %s",
					    namelist[c].n_name);
			(void)fputc('\n', stderr);
			exit(1);
		} else
			errx(1, "kvm_nlist: %s", kvm_geterr(kd));
	}

	if (todo & VMSTAT) {
		struct winsize winsize;

		dkinit(0);	/* Initialize disk stats, no disks selected. */
		argv = choosedrives(argv);	/* Select disks. */
		winsize.ws_row = 0;
		(void) ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&winsize);
		if (winsize.ws_row > 0)
			winlines = winsize.ws_row;

	}

	setegid(getgid());
	setgid(getgid());

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv)
			reps = atoi(*argv);
	}
#endif

	if (interval) {
		if (!reps)
			reps = -1;
	} else if (reps)
		interval = 1;

	if (todo & FORKSTAT)
		doforkst();
	if (todo & MEMSTAT) {
		domem();
		dopool();
	}
	if (todo & SUMSTAT)
		dosum();
	if (todo & TIMESTAT)
		dotimes();
	if (todo & INTRSTAT)
		dointr();
	if (todo & VMSTAT)
		dovmstat(interval, reps);
	exit(0);
}

char **
choosedrives(argv)
	char **argv;
{
	register int i;

	/*
	 * Choose drives to be displayed.  Priority goes to (in order) drives
	 * supplied as arguments, default drives.  If everything isn't filled
	 * in and there are drives not taken care of, display the first few
	 * that fit.
	 */
#define BACKWARD_COMPATIBILITY
	for (ndrives = 0; *argv; ++argv) {
#ifdef	BACKWARD_COMPATIBILITY
		if (isdigit(**argv))
			break;
#endif
		for (i = 0; i < dk_ndrive; i++) {
			if (strcmp(dr_name[i], *argv))
				continue;
			dk_select[i] = 1;
			++ndrives;
			break;
		}
	}
	for (i = 0; i < dk_ndrive && ndrives < 4; i++) {
		if (dk_select[i])
			continue;
		dk_select[i] = 1;
		++ndrives;
	}
	return(argv);
}

time_t
getuptime()
{
	static time_t now;
	static struct timeval boottime;
	time_t uptime;
	int mib[2];
	size_t size;

	if (boottime.tv_sec == 0) {
		if (nlist == NULL && memf == NULL) {
			kread(X_BOOTTIME, &boottime, sizeof(boottime));
		} else {
			size = sizeof(boottime);
			mib[0] = CTL_KERN;
			mib[1] = KERN_BOOTTIME;
			if (sysctl(mib, 2, &boottime, &size, NULL, 0) < 0) {
				printf("Can't get kerninfo: %s\n",
				    strerror(errno));
				bzero(&boottime, sizeof(boottime));
			}
		}
	}
	(void)time(&now);
	uptime = now - boottime.tv_sec;
	if (uptime <= 0 || uptime > 60*60*24*365*10)
		errx(1, "time makes no sense; namelist must be wrong");

	return(uptime);
}

int	hz, hdrcnt;

void
dovmstat(interval, reps)
	u_int interval;
	int reps;
{
	struct vmtotal total;
	time_t uptime, halfuptime;
	void needhdr();
	int mib[2];
	struct clockinfo clkinfo;
	size_t size;

	uptime = getuptime();
	halfuptime = uptime / 2;
	(void)signal(SIGCONT, needhdr);

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	size = sizeof(clkinfo);
	if (sysctl(mib, 2, &clkinfo, &size, NULL, 0) < 0) {
		printf("Can't get kerninfo: %s\n", strerror(errno));
		return;
	}
	hz = clkinfo.stathz;

	for (hdrcnt = 1;;) {
		if (!--hdrcnt)
			printhdr();
		/* Read new disk statistics */
		dkreadstats();
#ifdef UVM
		if (nlist == NULL && memf == NULL) {
			kread(X_UVMEXP, &uvmexp, sizeof(uvmexp));
		} else {
			size = sizeof(uvmexp);
			mib[0] = CTL_VM;
			mib[1] = VM_UVMEXP;
			if (sysctl(mib, 2, &uvmexp, &size, NULL, 0) < 0) {
				printf("Can't get kerninfo: %s\n",
				    strerror(errno));
				bzero(&uvmexp, sizeof(uvmexp));
			}
		}
#else
		kread(X_SUM, &sum, sizeof(sum));
#endif
		size = sizeof(total);
		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		if (sysctl(mib, 2, &total, &size, NULL, 0) < 0) {
			printf("Can't get kerninfo: %s\n", strerror(errno));
			bzero(&total, sizeof(total));
		}
		(void)printf("%2u%2u%2u",
		    total.t_rq - 1, total.t_dw + total.t_pw, total.t_sw);
#define	rate(x)	(((x) + halfuptime) / uptime)	/* round */
#ifdef UVM
#define pgtok(a) ((a) * ((int)uvmexp.pagesize >> 10))
#else
#define pgtok(a) ((a) * ((int)sum.v_page_size >> 10))
#endif
		(void)printf("%7u%7u ",
		    pgtok(total.t_avm), pgtok(total.t_free));
#ifdef UVM
		(void)printf("%4u ", rate(uvmexp.faults - ouvmexp.faults));
		(void)printf("%3u ", rate(uvmexp.pdreact - ouvmexp.pdreact));
		(void)printf("%3u ", rate(uvmexp.pageins - ouvmexp.pageins));
		(void)printf("%3u %3u ",
		    rate(uvmexp.pdpageouts - ouvmexp.pdpageouts), 0);
		(void)printf("%3u ", rate(uvmexp.pdscans - ouvmexp.pdscans));
		dkstats();
		(void)printf("%4u %4u %3u ",
		    rate(uvmexp.intrs - ouvmexp.intrs),
		    rate(uvmexp.syscalls - ouvmexp.syscalls),
		    rate(uvmexp.swtch - ouvmexp.swtch));
#else
		(void)printf("%4u ", rate(sum.v_faults - osum.v_faults));
		(void)printf("%3u ",
		    rate(sum.v_reactivated - osum.v_reactivated));
		(void)printf("%3u ", rate(sum.v_pageins - osum.v_pageins));
		(void)printf("%3u %3u ",
		    rate(sum.v_pageouts - osum.v_pageouts), 0);
		(void)printf("%3u ", rate(sum.v_scan - osum.v_scan));
		dkstats();
		(void)printf("%4u %4u %3u ",
		    rate(sum.v_intr - osum.v_intr),
		    rate(sum.v_syscall - osum.v_syscall),
		    rate(sum.v_swtch - osum.v_swtch));
#endif
		cpustats();
		(void)printf("\n");
		(void)fflush(stdout);
		if (reps >= 0 && --reps <= 0)
			break;
#ifdef UVM
		ouvmexp = uvmexp;
#else
		osum = sum;
#endif
		uptime = interval;
		/*
		 * We round upward to avoid losing low-frequency events
		 * (i.e., >= 1 per interval but < 1 per second).
		 */
		halfuptime = uptime == 1 ? 0 : (uptime + 1) / 2;
		(void)sleep(interval);
	}
}

void
printhdr()
{
	register int i;

	(void)printf(" procs   memory       page%*s", 20, "");
	if (ndrives > 0)
		(void)printf("%s %*sfaults   cpu\n",
		   ((ndrives > 1) ? "disks" : "disk"),
		   ((ndrives > 1) ? ndrives * 3 - 4 : 0), "");
	else
		(void)printf("%*s  faults   cpu\n",
		   ndrives * 3, "");

	(void)printf(" r b w    avm    fre  flt  re  pi  po  fr  sr ");
	for (i = 0; i < dk_ndrive; i++)
		if (dk_select[i])
			(void)printf("%c%c ", dr_name[i][0],
			    dr_name[i][strlen(dr_name[i]) - 1]);
	(void)printf("  in   sy  cs us sy id\n");
	hdrcnt = winlines - 2;
}

/*
 * Force a header to be prepended to the next output.
 */
void
needhdr()
{

	hdrcnt = 1;
}

void
dotimes()
{
	u_int pgintime, rectime;
	int mib[2];
	size_t size;

	pgintime = 0;
	rectime = 0;
#ifdef UVM
	if (nlist == NULL && memf == NULL) {
		kread(X_UVMEXP, &uvmexp, sizeof(uvmexp));
	} else {
		size = sizeof(uvmexp);
		mib[0] = CTL_VM;
		mib[1] = VM_UVMEXP;
		if (sysctl(mib, 2, &uvmexp, &size, NULL, 0) < 0) {
			printf("Can't get kerninfo: %s\n", strerror(errno));
			bzero(&uvmexp, sizeof(uvmexp));
		}
	}

	(void)printf("%u reactivates, %u total time (usec)\n",
	    uvmexp.pdreact, rectime);
	(void)printf("average: %u usec / reclaim\n", rectime / uvmexp.pdreact);
	(void)printf("\n");
	(void)printf("%u page ins, %u total time (msec)\n",
	    uvmexp.pageins, pgintime / 10);
	(void)printf("average: %8.1f msec / page in\n",
	    pgintime / (uvmexp.pageins * 10.0));
#else
	kread(X_SUM, &sum, sizeof(sum));
	(void)printf("%u reactivates, %u total time (usec)\n",
	    sum.v_reactivated, rectime);
	(void)printf("average: %u usec / reclaim\n", rectime / sum.v_reactivated);
	(void)printf("\n");
	(void)printf("%u page ins, %u total time (msec)\n",
	    sum.v_pageins, pgintime / 10);
	(void)printf("average: %8.1f msec / page in\n",
	    pgintime / (sum.v_pageins * 10.0));
#endif
}

int
pct(top, bot)
	long top, bot;
{
	long ans;

	if (bot == 0)
		return(0);
	ans = (quad_t)top * 100 / bot;
	return (ans);
}

#define	PCT(top, bot) pct((long)(top), (long)(bot))

void
dosum()
{
	struct nchstats nchstats;
	long nchtotal;
	size_t size;
	int mib[2], nselcoll;

#ifdef UVM
	if (nlist == NULL && memf == NULL) {
		kread(X_UVMEXP, &nchstats, sizeof(uvmexp));
	} else {
		size = sizeof(uvmexp);
		mib[0] = CTL_VM;
		mib[1] = VM_UVMEXP;
		if (sysctl(mib, 2, &uvmexp, &size, NULL, 0) < 0) {
			printf("Can't get kerninfo: %s\n", strerror(errno));
			bzero(&uvmexp, sizeof(uvmexp));
		}
	}

	/* vm_page constants */
	(void)printf("%11u bytes per page\n", uvmexp.pagesize);

	/* vm_page counters */
	(void)printf("%11u pages managed\n", uvmexp.npages);
	(void)printf("%11u pages free\n", uvmexp.free);
	(void)printf("%11u pages active\n", uvmexp.active);
	(void)printf("%11u pages inactive\n", uvmexp.inactive);
	(void)printf("%11u pages being paged out\n", uvmexp.paging);
	(void)printf("%11u pages wired\n", uvmexp.wired);
	(void)printf("%11u pages reserved for pagedaemon\n",
		     uvmexp.reserve_pagedaemon);
	(void)printf("%11u pages reserved for kernel\n",
		     uvmexp.reserve_kernel);

	/* swap */
	(void)printf("%11u swap pages\n", uvmexp.swpages);
	(void)printf("%11u swap pages in use\n", uvmexp.swpginuse);
	(void)printf("%11u total anon's in system\n", uvmexp.nanon);
	(void)printf("%11u free anon's\n", uvmexp.nfreeanon);

	/* stat counters */
	(void)printf("%11u page faults\n", uvmexp.faults);
	(void)printf("%11u traps\n", uvmexp.traps);
	(void)printf("%11u interrupts\n", uvmexp.intrs);
	(void)printf("%11u cpu context switches\n", uvmexp.swtch);
	(void)printf("%11u software interrupts\n", uvmexp.softs);
	(void)printf("%11u syscalls\n", uvmexp.syscalls);
	(void)printf("%11u pagein operations\n", uvmexp.pageins);
	(void)printf("%11u swap ins\n", uvmexp.swapins);
	(void)printf("%11u swap outs\n", uvmexp.swapouts);
	(void)printf("%11u forks\n", uvmexp.forks);
	(void)printf("%11u forks where vmspace is shared\n",
		     uvmexp.forks_sharevm);

	/* daemon counters */
	(void)printf("%11u number of times the pagedeamon woke up\n",
		     uvmexp.pdwoke);
	(void)printf("%11u revolutions of the clock hand\n", uvmexp.pdrevs);
	(void)printf("%11u pages freed by pagedaemon\n", uvmexp.pdfreed);
	(void)printf("%11u pages scanned by pagedaemon\n", uvmexp.pdscans);
	(void)printf("%11u pages reactivated by pagedaemon\n", uvmexp.pdreact);
	(void)printf("%11u busy pages found by pagedaemon\n", uvmexp.pdbusy);
#else
	kread(X_SUM, &sum, sizeof(sum));
	(void)printf("%11u cpu context switches\n", sum.v_swtch);
	(void)printf("%11u device interrupts\n", sum.v_intr);
	(void)printf("%11u software interrupts\n", sum.v_soft);
	(void)printf("%11u traps\n", sum.v_trap);
	(void)printf("%11u system calls\n", sum.v_syscall);
	(void)printf("%11u total faults taken\n", sum.v_faults);
	(void)printf("%11u swap ins\n", sum.v_swpin);
	(void)printf("%11u swap outs\n", sum.v_swpout);
	(void)printf("%11u pages swapped in\n", sum.v_pswpin);
	(void)printf("%11u pages swapped out\n", sum.v_pswpout);
	(void)printf("%11u page ins\n", sum.v_pageins);
	(void)printf("%11u page outs\n", sum.v_pageouts);
	(void)printf("%11u pages paged in\n", sum.v_pgpgin);
	(void)printf("%11u pages paged out\n", sum.v_pgpgout);
	(void)printf("%11u pages reactivated\n", sum.v_reactivated);
	(void)printf("%11u intransit blocking page faults\n", sum.v_intrans);
	(void)printf("%11u zero fill pages created\n", sum.v_nzfod);
	(void)printf("%11u zero fill page faults\n", sum.v_zfod);
	(void)printf("%11u pages examined by the clock daemon\n", sum.v_scan);
	(void)printf("%11u revolutions of the clock hand\n", sum.v_rev);
	(void)printf("%11u VM object cache lookups\n", sum.v_lookups);
	(void)printf("%11u VM object hits\n", sum.v_hits);
	(void)printf("%11u total VM faults taken\n", sum.v_vm_faults);
	(void)printf("%11u copy-on-write faults\n", sum.v_cow_faults);
	(void)printf("%11u pages freed by daemon\n", sum.v_dfree);
	(void)printf("%11u pages freed by exiting processes\n", sum.v_pfree);
	(void)printf("%11u pages free\n", sum.v_free_count);
	(void)printf("%11u pages wired down\n", sum.v_wire_count);
	(void)printf("%11u pages active\n", sum.v_active_count);
	(void)printf("%11u pages inactive\n", sum.v_inactive_count);
	(void)printf("%11u bytes per page\n", sum.v_page_size);
#endif

	if (nlist == NULL && memf == NULL) {
		kread(X_NCHSTATS, &nchstats, sizeof(nchstats));
	} else {
		size = sizeof(nchstats);
		mib[0] = CTL_KERN;
		mib[1] = KERN_NCHSTATS;
		if (sysctl(mib, 2, &nchstats, &size, NULL, 0) < 0) {
		    	printf("Can't get kerninfo: %s\n", strerror(errno));
			bzero(&nchstats, sizeof(nchstats));
		}
	}

	nchtotal = nchstats.ncs_goodhits + nchstats.ncs_neghits +
	    nchstats.ncs_badhits + nchstats.ncs_falsehits +
	    nchstats.ncs_miss + nchstats.ncs_long;
	(void)printf("%11ld total name lookups\n", nchtotal);
	(void)printf("%11s cache hits (%d%% pos + %d%% neg) system %d%% "
	    "per-directory\n",
	    "", PCT(nchstats.ncs_goodhits, nchtotal),
	    PCT(nchstats.ncs_neghits, nchtotal),
	    PCT(nchstats.ncs_pass2, nchtotal));
	(void)printf("%11s deletions %d%%, falsehits %d%%, toolong %d%%\n", "",
	    PCT(nchstats.ncs_badhits, nchtotal),
	    PCT(nchstats.ncs_falsehits, nchtotal),
	    PCT(nchstats.ncs_long, nchtotal));

	if (nlist == NULL && memf == NULL) {
		kread(X_NSELCOLL, &nselcoll, sizeof(nselcoll));
	} else {
		size = sizeof(nselcoll);
		mib[0] = CTL_KERN;
		mib[1] = KERN_NSELCOLL;
		if (sysctl(mib, 2, &nselcoll, &size, NULL, 0) < 0) {
		    	printf("Can't get kerninfo: %s\n", strerror(errno));
			nselcoll = 0;
		}
	}
	(void)printf("%11d select collisions\n", nselcoll);
}

void
doforkst()
{
	struct forkstat fks;
	size_t size;
	int mib[2];

	if (nlist == NULL && memf == NULL) {
		kread(X_FORKSTAT, &fks, sizeof(struct forkstat));
	} else {
		size = sizeof(struct forkstat);
		mib[0] = CTL_KERN;
		mib[1] = KERN_FORKSTAT;
		if (sysctl(mib, 2, &fks, &size, NULL, 0) < 0) {
		    	printf("Can't get kerninfo: %s\n", strerror(errno));
			bzero(&fks, sizeof(struct forkstat));
		}
	}

	(void)printf("%d forks, %d pages, average %.2f\n",
	    fks.cntfork, fks.sizfork, (double)fks.sizfork / fks.cntfork);
	(void)printf("%d vforks, %d pages, average %.2f\n",
	    fks.cntvfork, fks.sizvfork, (double)fks.sizvfork / (fks.cntvfork ? fks.cntvfork : 1));
	(void)printf("%d rforks, %d pages, average %.2f\n",
	    fks.cntrfork, fks.sizrfork, (double)fks.sizrfork / (fks.cntrfork ? fks.cntrfork : 1));
	(void)printf("%d kthread creations, %d pages, average %.2f\n",
	    fks.cntkthread, fks.sizkthread, (double)fks.sizkthread / (fks.cntkthread ? fks.cntkthread : 1));
}

void
dkstats()
{
	register int dn, state;
	double etime;

	/* Calculate disk stat deltas. */
	dkswap();
	etime = 0;
	for (state = 0; state < CPUSTATES; ++state) {
		etime += cur.cp_time[state];
	}
	if (etime == 0)
		etime = 1;
	etime /= hz;
	for (dn = 0; dn < dk_ndrive; ++dn) {
		if (!dk_select[dn])
			continue;
		(void)printf("%2.0f ", cur.dk_xfer[dn] / etime);
	}
}

void
cpustats()
{
	register int state;
	double pct, total;

	total = 0;
	for (state = 0; state < CPUSTATES; ++state)
		total += cur.cp_time[state];
	if (total)
		pct = 100 / total;
	else
		pct = 0;
	(void)printf("%2.0f ", (cur.cp_time[CP_USER] + cur.cp_time[CP_NICE]) * pct);
	(void)printf("%2.0f ", (cur.cp_time[CP_SYS] + cur.cp_time[CP_INTR]) * pct);
	(void)printf("%2.0f", cur.cp_time[CP_IDLE] * pct);
}

#if defined(__i386__)
/* To get struct intrhand */
#define _KERNEL
#include <machine/psl.h>
#undef _KERNEL
void
dointr()
{
	struct intrhand *intrhand[16], *ihp, ih;
	u_long inttotal;
	time_t uptime;
	u_long intrstray[16];
	char iname[17], fname[31];
	int i;

	iname[16] = '\0';
	uptime = getuptime();
	kread(X_INTRHAND, intrhand, sizeof(intrhand));
	kread(X_INTRSTRAY, intrstray, sizeof(intrstray));

	(void)printf("interrupt             total     rate\n");
	inttotal = 0;
	for (i = 0; i < 16; i++) {
		ihp = intrhand[i];
		while (ihp) {
			if (kvm_read(kd, (u_long)ihp, &ih, sizeof(ih)) != sizeof(ih))
				errx(1, "vmstat: ih: %s", kvm_geterr(kd));
			if (kvm_read(kd, (u_long)ih.ih_what, iname, 16) != 16)
				errx(1, "vmstat: ih_what: %s", kvm_geterr(kd));
			snprintf(fname, sizeof fname, "irq%d/%s", i, iname);
			printf("%-16.16s %10lu %8lu\n", fname, ih.ih_count,
			    ih.ih_count / uptime);
			inttotal += ih.ih_count;
			ihp = ih.ih_next;
		}
	}
	for (i = 0; i < 16; i++)
		if (intrstray[i]) {
			printf("Stray irq %-2d     %10lu %8lu\n",
			    i, intrstray[i], intrstray[i] / uptime);
			inttotal += intrstray[i];
		}
	printf("Total            %10lu %8lu\n", inttotal, inttotal / uptime);
}
#else
void
dointr()
{
	register long *intrcnt, inttotal;
	time_t uptime;
	register int nintr, inamlen;
	register char *intrname;
	struct evcntlist allevents;
	struct evcnt evcnt, *evptr;
	struct device dev;

	uptime = getuptime();
	nintr = namelist[X_EINTRCNT].n_value - namelist[X_INTRCNT].n_value;
	inamlen =
	    namelist[X_EINTRNAMES].n_value - namelist[X_INTRNAMES].n_value;
	intrcnt = malloc((size_t)nintr);
	intrname = malloc((size_t)inamlen);
	if (intrcnt == NULL || intrname == NULL)
		err(1, "malloc");
	kread(X_INTRCNT, intrcnt, (size_t)nintr);
	kread(X_INTRNAMES, intrname, (size_t)inamlen);
	(void)printf("interrupt             total     rate\n");
	inttotal = 0;
	nintr /= sizeof(long);
	while (--nintr >= 0) {
		if (*intrcnt)
			(void)printf("%-14s %12ld %8ld\n", intrname,
			    *intrcnt, *intrcnt / uptime);
		intrname += strlen(intrname) + 1;
		inttotal += *intrcnt++;
	}
	kread(X_ALLEVENTS, &allevents, sizeof allevents);
	evptr = allevents.tqh_first;
	while (evptr) {
		if (kvm_read(kd, (long)evptr, (void *)&evcnt,
		    sizeof evcnt) != sizeof evcnt)
			errx(1, "event chain trashed: %s", kvm_geterr(kd));
		if (strcmp(evcnt.ev_name, "intr") == 0) {
			if (kvm_read(kd, (long)evcnt.ev_dev, (void *)&dev,
			    sizeof dev) != sizeof dev)
				errx(1, "event chain trashed: %s", kvm_geterr(kd));
			if (evcnt.ev_count)
				(void)printf("%-14s %12d %8ld\n", dev.dv_xname,
				    evcnt.ev_count, (long)(evcnt.ev_count / uptime));
			inttotal += evcnt.ev_count++;
		}
		evptr = evcnt.ev_list.tqe_next;
	}
	(void)printf("Total          %12ld %8ld\n", inttotal, inttotal / uptime);
}
#endif

/*
 * These names are defined in <sys/malloc.h>.
 */
char *kmemnames[] = INITKMEMNAMES;

void
domem()
{
	register struct kmembuckets *kp;
	register struct kmemstats *ks;
	register int i, j;
	int len, size, first;
	u_long totuse = 0, totfree = 0;
	quad_t totreq = 0;
	char *name;
	struct kmemstats kmemstats[M_LAST];
	struct kmembuckets buckets[MINBUCKET + 16];
	int mib[4];
	size_t siz;
	char buf[BUFSIZ], *bufp, *ap;

	if (memf == NULL && nlistf == NULL) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_MALLOCSTATS;
		mib[2] = KERN_MALLOC_BUCKETS;
		siz = sizeof(buf);
		if (sysctl(mib, 3, buf, &siz, NULL, 0) < 0) {
			printf("Could not acquire information on kernel memory bucket sizes.\n");
			return;
		}

		bufp = buf;
		mib[2] = KERN_MALLOC_BUCKET;
		siz = sizeof(struct kmembuckets);
		i = 0;
		while ((ap = strsep(&bufp, ",")) != NULL) {
			mib[3] = atoi(ap);

			if (sysctl(mib, 4, &buckets[MINBUCKET + i], &siz,
			    NULL, 0) < 0) {
				printf("Failed to read statistics for bucket %d.\n",
				    mib[3]);
				return;
			}
			i++;
		}
	} else {
		kread(X_KMEMBUCKETS, buckets, sizeof(buckets));
	}

	for (first = 1, i = MINBUCKET, kp = &buckets[i]; i < MINBUCKET + 16;
	     i++, kp++) {
		if (kp->kb_calls == 0)
			continue;
		if (first) {
			(void)printf("Memory statistics by bucket size\n");
			(void)printf(
		"    Size   In Use   Free           Requests  HighWater  Couldfree\n");
			first = 0;
		}
		size = 1 << i;
		(void)printf("%8d %8qu %6qu %18qu %7qu %10qu\n", size,
			kp->kb_total - kp->kb_totalfree,
			kp->kb_totalfree, kp->kb_calls,
			kp->kb_highwat, kp->kb_couldfree);
		totfree += size * kp->kb_totalfree;
	}

	/*
	 * If kmem statistics are not being gathered by the kernel,
	 * first will still be 1.
	 */
	if (first) {
		printf(
		    "Kmem statistics are not being gathered by the kernel.\n");
		return;
	}

	if (memf == NULL && nlistf == NULL) {
		bzero(kmemstats, sizeof(kmemstats));
		for (i = 0; i < M_LAST; i++) {
			mib[0] = CTL_KERN;
			mib[1] = KERN_MALLOCSTATS;
			mib[2] = KERN_MALLOC_KMEMSTATS;
			mib[3] = i;
			siz = sizeof(struct kmemstats);

			/* 
			 * Skip errors -- these are presumed to be unallocated
			 * entries.
			 */
			if (sysctl(mib, 4, &kmemstats[i], &siz, NULL, 0) < 0)
				continue;
		}
	} else {
		kread(X_KMEMSTAT, kmemstats, sizeof(kmemstats));
	}

	(void)printf("\nMemory usage type by bucket size\n");
	(void)printf("    Size  Type(s)\n");
	kp = &buckets[MINBUCKET];
	for (j =  1 << MINBUCKET; j < 1 << (MINBUCKET + 16); j <<= 1, kp++) {
		if (kp->kb_calls == 0)
			continue;
		first = 1;
		len = 8;
		for (i = 0, ks = &kmemstats[0]; i < M_LAST; i++, ks++) {
			if (ks->ks_calls == 0)
				continue;
			if ((ks->ks_size & j) == 0)
				continue;
			name = kmemnames[i] ? kmemnames[i] : "undefined";
			len += 2 + strlen(name);
			if (first)
				printf("%8d  %s", j, name);
			else
				printf(",");
			if (len >= 80) {
				printf("\n\t ");
				len = 10 + strlen(name);
			}
			if (!first)
				printf(" %s", name);
			first = 0;
		}
		printf("\n");
	}

	(void)printf(
	   "\nMemory statistics by type                           Type  Kern\n");
	(void)printf(
"          Type InUse MemUse HighUse  Limit Requests Limit Limit Size(s)\n");
	for (i = 0, ks = &kmemstats[0]; i < M_LAST; i++, ks++) {
		if (ks->ks_calls == 0)
			continue;
		(void)printf("%14s%6ld%6ldK%7ldK%6ldK%9ld%5u%6u",
		    kmemnames[i] ? kmemnames[i] : "undefined",
		    ks->ks_inuse, (ks->ks_memuse + 1023) / 1024,
		    (ks->ks_maxused + 1023) / 1024,
		    (ks->ks_limit + 1023) / 1024, ks->ks_calls,
		    ks->ks_limblocks, ks->ks_mapblocks);
		first = 1;
		for (j =  1 << MINBUCKET; j < 1 << (MINBUCKET + 16); j <<= 1) {
			if ((ks->ks_size & j) == 0)
				continue;
			if (first)
				printf("  %d", j);
			else
				printf(",%d", j);
			first = 0;
		}
		printf("\n");
		totuse += ks->ks_memuse;
		totreq += ks->ks_calls;
	}
	(void)printf("\nMemory Totals:  In Use    Free    Requests\n");
	(void)printf("              %7luK %6luK    %8qu\n",
	     (totuse + 1023) / 1024, (totfree + 1023) / 1024, totreq);
}

static void
print_pool(struct pool *pp, char *name)
{
	static int first = 1;
	int ovflw;
	char maxp[32];

	if (first) {
		(void)printf("Memory resource pool statistics\n");
		(void)printf(
		    "%-11s%5s%9s%5s%9s%6s%6s%6s%6s%6s%6s%5s\n",
		    "Name",
		    "Size",
		    "Requests",
		    "Fail",
		    "Releases",
		    "Pgreq",
		    "Pgrel",
		    "Npage",
		    "Hiwat",
		    "Minpg",
		    "Maxpg",
		    "Idle");
		first = 0;
	}
	if (pp->pr_maxpages == UINT_MAX)
		sprintf(maxp, "inf");
	else
		sprintf(maxp, "%u", pp->pr_maxpages);
/*
 * Print single word.  `ovflow' is number of characters didn't fit
 * on the last word.  `fmt' is a format string to print this word.
 * It must contain asterisk for field width.  `width' is a width
 * occupied by this word.  `fixed' is a number of constant chars in
 * `fmt'.  `val' is a value to be printed using format string `fmt'.
 */
#define	PRWORD(ovflw, fmt, width, fixed, val) do {	\
	(ovflw) += printf((fmt),			\
	    (width) - (fixed) - (ovflw) > 0 ?		\
	    (width) - (fixed) - (ovflw) : 0,		\
	    (val)) - (width);				\
	if ((ovflw) < 0)				\
		(ovflw) = 0;				\
} while (/* CONSTCOND */0)

	ovflw = 0;
	PRWORD(ovflw, "%-*s", 11, 0, name);
	PRWORD(ovflw, " %*u", 5, 1, pp->pr_size);
	PRWORD(ovflw, " %*lu", 9, 1, pp->pr_nget);
	PRWORD(ovflw, " %*lu", 5, 1, pp->pr_nfail);
	PRWORD(ovflw, " %*lu", 9, 1, pp->pr_nput);
	PRWORD(ovflw, " %*lu", 6, 1, pp->pr_npagealloc);
	PRWORD(ovflw, " %*lu", 6, 1, pp->pr_npagefree);
	PRWORD(ovflw, " %*d", 6, 1, pp->pr_npages);
	PRWORD(ovflw, " %*d", 6, 1, pp->pr_hiwat);
	PRWORD(ovflw, " %*d", 6, 1, pp->pr_minpages);
	PRWORD(ovflw, " %*s", 6, 1, maxp);
	PRWORD(ovflw, " %*lu\n", 5, 1, pp->pr_nidle);	
}

static void dopool_kvm(void);
static void dopool_sysctl(void);

void
dopool(void)
{
	if (nlistf == NULL && memf == NULL)
		dopool_sysctl();
	else
		dopool_kvm();
}

void
dopool_sysctl(void)
{
	struct pool pool;
	size_t size;
	int mib[4];
	int npools, i;

	mib[0] = CTL_KERN;
	mib[1] = KERN_POOL;
	mib[2] = KERN_POOL_NPOOLS;
	size = sizeof(npools);
	if (sysctl(mib, 3, &npools, &size, NULL, 0) < 0) {
		printf("Can't figure out number of pools in kernel: %s\n",
			strerror(errno));
		return;
	}

	for (i = 1; npools; i++) {
		char name[32];

		mib[0] = CTL_KERN;
		mib[1] = KERN_POOL;
		mib[2] = KERN_POOL_POOL;
		mib[3] = i;
		size = sizeof(struct pool);
		if (sysctl(mib, 4, &pool, &size, NULL, 0) < 0) {
			if (errno == ENOENT)
				continue;
			printf("error getting pool: %s\n", strerror(errno));
			return;
		}
		npools--;
		mib[2] = KERN_POOL_NAME;
		size = sizeof(name);
		if (sysctl(mib, 4, &name, &size, NULL, 0) < 0) {
			printf("error getting pool name: %s\n",
				strerror(errno));
			return;
		}
		print_pool(&pool, name);
	}
}

void
dopool_kvm(void)
{
	long addr;
	long total = 0, inuse = 0;
	TAILQ_HEAD(,pool) pool_head;
	struct pool pool, *pp = &pool;

	kread(X_POOLHEAD, &pool_head, sizeof(pool_head));
	addr = (long)TAILQ_FIRST(&pool_head);

	while (addr != 0) {
		char name[32];
		if (kvm_read(kd, addr, (void *)pp, sizeof *pp) != sizeof *pp) {
			(void)fprintf(stderr,
			    "vmstat: pool chain trashed: %s\n",
			    kvm_geterr(kd));
			exit(1);
		}
		if (kvm_read(kd, (long)pp->pr_wchan, name, sizeof name) < 0) {
			(void)fprintf(stderr,
			    "vmstat: pool name trashed: %s\n",
			    kvm_geterr(kd));
			exit(1);
		}
		name[31] = '\0';

		print_pool(pp, name);

		inuse += (pp->pr_nget - pp->pr_nput) * pp->pr_size;
		total += pp->pr_npages * pp->pr_pagesz;
		addr = (long)TAILQ_NEXT(pp, pr_poollist);
	}

	inuse /= 1024;
	total /= 1024;
	printf("\nIn use %ldK, total allocated %ldK; utilization %.1f%%\n",
	    inuse, total, (double)(100 * inuse) / total);
}

/*
 * kread reads something from the kernel, given its nlist index.
 */
void
kread(nlx, addr, size)
	int nlx;
	void *addr;
	size_t size;
{
	char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "symbol %s not defined", sym);
	}
	if (kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "%s: %s", sym, kvm_geterr(kd));
	}
}

void
usage()
{
	(void)fprintf(stderr, "usage: %s [-fimst] [-c count] [-M core] "
	    "[-N system] [-w wait] [disks]\n", __progname);
	exit(1);
}

