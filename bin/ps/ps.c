/*	$OpenBSD: ps.c,v 1.33 2003/06/11 23:42:12 deraadt Exp $	*/
/*	$NetBSD: ps.c,v 1.15 1995/05/18 20:33:25 mycroft Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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
 * 3. Neither the name of the University nor the names of its contributors
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
"@(#) Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ps.c	8.4 (Berkeley) 4/2/94";
#else
static char rcsid[] = "$OpenBSD: ps.c,v 1.33 2003/06/11 23:42:12 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "ps.h"

extern char *__progname;

KINFO *kinfo;
struct varent *vhead, *vtail;

int	eval;			/* exit value */
int	rawcpu;			/* -C */
int	sumrusage;		/* -S */
int	termwidth;		/* width of screen (0 == infinity) */
int	totwidth;		/* calculated width of requested variables */

int	needcomm, needenv, commandonly;

enum sort { DEFAULT, SORTMEM, SORTCPU } sortby = DEFAULT;

static char	*kludge_oldps_options(char *);
static int	 pscomp(const void *, const void *);
static void	 saveuser(KINFO *);
static void	 scanvars(void);
static void	 usage(void);

char dfmt[] = "pid tt state time command";
char jfmt[] = "user pid ppid pgid sess jobc state tt time command";
char lfmt[] = "uid pid ppid cpu pri nice vsz rss wchan state tt time command";
char   o1[] = "pid";
char   o2[] = "tt state time command";
char ufmt[] = "user pid %cpu %mem vsz rss tt state start time command";
char vfmt[] = "pid state time sl re pagein vsz rss lim tsiz %cpu %mem command";

kvm_t *kd;
int kvm_sysctl_only;

int
main(int argc, char *argv[])
{
	struct kinfo_proc *kp;
	struct varent *vent;
	struct winsize ws;
	struct passwd *pwd;
	dev_t ttydev;
	pid_t pid;
	uid_t uid;
	int all, ch, flag, i, fmt, lineno, nentries, mib[4], mibcnt, nproc;
	int prtheader, wflag, kflag, what, xflg;
	char *nlistf, *memf, *swapf, errbuf[_POSIX2_LINE_MAX];
	size_t size;

	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	    ioctl(STDERR_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	    ioctl(STDIN_FILENO,  TIOCGWINSZ, (char *)&ws) == -1) ||
	    ws.ws_col == 0)
		termwidth = 79;
	else
		termwidth = ws.ws_col - 1;

	if (argc > 1)
		argv[1] = kludge_oldps_options(argv[1]);

	all = fmt = prtheader = wflag = kflag = xflg = 0;
	pid = -1;
	uid = (uid_t) -1;
	ttydev = NODEV;
	memf = nlistf = swapf = NULL;
	while ((ch = getopt(argc, argv,
	    "acCeghjkLlM:mN:O:o:p:rSTt:U:uvW:wx")) != -1)
		switch((char)ch) {
		case 'a':
			all = 1;
			break;
		case 'c':
			commandonly = 1;
			break;
		case 'e':			/* XXX set ufmt */
			needenv = 1;
			break;
		case 'C':
			rawcpu = 1;
			break;
		case 'g':
			break;			/* no-op */
		case 'h':
			prtheader = ws.ws_row > 5 ? ws.ws_row : 22;
			break;
		case 'j':
			parsefmt(jfmt);
			fmt = 1;
			jfmt[0] = '\0';
			break;
		case 'k':
			kflag++;
			break;
		case 'L':
			showkey();
			exit(0);
		case 'l':
			parsefmt(lfmt);
			fmt = 1;
			lfmt[0] = '\0';
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			sortby = SORTMEM;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'O':
			parsefmt(o1);
			parsefmt(optarg);
			parsefmt(o2);
			o1[0] = o2[0] = '\0';
			fmt = 1;
			break;
		case 'o':
			parsefmt(optarg);
			fmt = 1;
			break;
		case 'p':
			pid = atol(optarg);
			xflg = 1;
			break;
		case 'r':
			sortby = SORTCPU;
			break;
		case 'S':
			sumrusage = 1;
			break;
		case 'T':
			if ((optarg = ttyname(STDIN_FILENO)) == NULL)
				errx(1, "stdin: not a terminal");
			/* FALLTHROUGH */
		case 't': {
			struct stat sb;
			char *ttypath, pathbuf[MAXPATHLEN];

			if (strcmp(optarg, "co") == 0)
				ttypath = _PATH_CONSOLE;
			else if (*optarg != '/')
				(void)snprintf(ttypath = pathbuf,
				    sizeof(pathbuf), "%s%s", _PATH_TTY, optarg);
			else
				ttypath = optarg;
			if (stat(ttypath, &sb) == -1)
				err(1, "%s", ttypath);
			if (!S_ISCHR(sb.st_mode))
				errx(1, "%s: not a terminal", ttypath);
			ttydev = sb.st_rdev;
			break;
		}
		case 'U':
			pwd = getpwnam(optarg);
			if (pwd == NULL)
				errx(1, "%s: no such user", optarg);
			uid = pwd->pw_uid;
			endpwent();
			xflg = 1;
			break;
		case 'u':
			parsefmt(ufmt);
			sortby = SORTCPU;
			fmt = 1;
			ufmt[0] = '\0';
			break;
		case 'v':
			parsefmt(vfmt);
			sortby = SORTMEM;
			fmt = 1;
			vfmt[0] = '\0';
			break;
		case 'W':
			swapf = optarg;
			break;
		case 'w':
			if (wflag)
				termwidth = UNLIMITED;
			else if (termwidth < 131)
				termwidth = 131;
			wflag++;
			break;
		case 'x':
			xflg = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		nlistf = *argv;
		if (*++argv) {
			memf = *argv;
			if (*++argv)
				swapf = *argv;
		}
	}
#endif

	if (nlistf == NULL && memf == NULL && swapf == NULL) {
		kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
		kvm_sysctl_only = 1;
	} else {
		kd = kvm_openfiles(nlistf, memf, swapf, O_RDONLY, errbuf);
	}
	if (kd == NULL && (nlistf != NULL || memf != NULL || swapf != NULL))
		errx(1, "%s", errbuf);

	if (!fmt)
		parsefmt(dfmt);

	/* XXX - should be cleaner */
	if (!all && ttydev == NODEV && pid == -1 && uid == (uid_t)-1)
		uid = getuid();

	/*
	 * scan requested variables, noting what structures are needed,
	 * and adjusting header widths as appropriate.
	 */
	scanvars();
	/*
	 * get proc list
	 */
	if (uid != (uid_t) -1) {
		what = mib[2] = KERN_PROC_UID;
		flag = mib[3] = uid;
		mibcnt = 4;
	} else if (ttydev != NODEV) {
		what = mib[2] = KERN_PROC_TTY;
		flag = mib[3] = ttydev;
		mibcnt = 4;
	} else if (pid != -1) {
		what = mib[2] = KERN_PROC_PID;
		flag = mib[3] = pid;
		mibcnt = 4;
	} else if (kflag) {
		what = mib[2] = KERN_PROC_KTHREAD;
		flag = 0;
		mibcnt = 3;
	} else {
		what = mib[2] = KERN_PROC_ALL;
		flag = 0;
		mibcnt = 3;
	}
	/*
	 * select procs
	 */
	if (kd != NULL) {
		if ((kp = kvm_getprocs(kd, what, flag, &nentries)) == 0)
			errx(1, "%s", kvm_geterr(kd));
	}
	else {
		mib[0] = CTL_KERN;
		mib[1] = KERN_NPROCS;
		size = sizeof (nproc);
		if (sysctl(mib, 2, &nproc, &size, NULL, 0) < 0)
			err(1, "could not get kern.nproc");
		/* Allocate more memory than is needed, just in case */
		size = (5 * nproc * sizeof(struct kinfo_proc)) / 4;
		kp = calloc(size, sizeof(char));
		if (kp == NULL)
			err(1,
			    "failed to allocated memory for proc structures");
		mib[1] = KERN_PROC;
		if (sysctl(mib, mibcnt, kp, &size, NULL, 0) < 0)
			err(1, "could not read kern.proc");
		nentries = size / sizeof(struct kinfo_proc);
	}

	if ((kinfo = malloc(nentries * sizeof(*kinfo))) == NULL)
		err(1, NULL);
	for (i = nentries; --i >= 0; ++kp) {
		kinfo[i].ki_p = kp;
		saveuser(&kinfo[i]);
	}
	/*
	 * print header
	 */
	printheader();
	if (nentries == 0)
		exit(1);
	/*
	 * sort proc list
	 */
	qsort(kinfo, nentries, sizeof(KINFO), pscomp);
	/*
	 * for each proc, call each variable output function.
	 */
	for (i = lineno = 0; i < nentries; i++) {
		KINFO *ki = &kinfo[i];

		if (xflg == 0 && (KI_EPROC(ki)->e_tdev == NODEV ||
		    (KI_PROC(ki)->p_flag & P_CONTROLT ) == 0))
			continue;
		for (vent = vhead; vent; vent = vent->next) {
			(vent->var->oproc)(ki, vent);
			if (vent->next != NULL)
				(void)putchar(' ');
		}
		(void)putchar('\n');
		if (prtheader && lineno++ == prtheader - 4) {
			(void)putchar('\n');
			printheader();
			lineno = 0;
		}
	}
	exit(eval);
}

static void
scanvars(void)
{
	struct varent *vent;
	VAR *v;
	int i;

	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		i = strlen(v->header);
		if (v->width < i)
			v->width = i;
		totwidth += v->width + 1;	/* +1 for space */
		if (v->flag & COMM)
			needcomm = 1;
	}
	totwidth--;
}

static void
saveuser(KINFO *ki)
{
	struct usave *usp;

	usp = &ki->ki_u;
	usp->u_valid = KI_EPROC(ki)->e_pstats_valid;
	if (!usp->u_valid)
		return;
	usp->u_start = KI_EPROC(ki)->e_pstats.p_start;
	usp->u_ru = KI_EPROC(ki)->e_pstats.p_ru;
	usp->u_cru = KI_EPROC(ki)->e_pstats.p_cru;
}

static int
pscomp(const void *a, const void *b)
{
	int i;
#define VSIZE(k) (KI_EPROC(k)->e_vm.vm_dsize + KI_EPROC(k)->e_vm.vm_ssize + \
		  KI_EPROC(k)->e_vm.vm_tsize)
#define STARTTIME(k) (k->ki_u.u_start.tv_sec)
#define STARTuTIME(k) (k->ki_u.u_start.tv_usec)

	if (sortby == SORTCPU)
		return (getpcpu((KINFO *)b) - getpcpu((KINFO *)a));
	if (sortby == SORTMEM)
		return (VSIZE((KINFO *)b) - VSIZE((KINFO *)a));
	i =  KI_EPROC((KINFO *)a)->e_tdev - KI_EPROC((KINFO *)b)->e_tdev;
	if (i == 0)
		i = STARTTIME(((KINFO *)a)) - STARTTIME(((KINFO *)b));
		if (i == 0)
			i = STARTuTIME(((KINFO *)a)) - STARTuTIME(((KINFO *)b));
	return (i);
}

/*
 * ICK (all for getopt), would rather hide the ugliness
 * here than taint the main code.
 *
 *  ps foo -> ps -foo
 *  ps 34 -> ps -p34
 *
 * The old convention that 't' with no trailing tty arg means the users
 * tty, is only supported if argv[1] doesn't begin with a '-'.  This same
 * feature is available with the option 'T', which takes no argument.
 */
static char *
kludge_oldps_options(char *s)
{
	size_t len;
	char *newopts, *ns, *cp;

	len = strlen(s);
	if ((newopts = ns = malloc(2 + len + 1)) == NULL)
		err(1, NULL);
	/*
	 * options begin with '-'
	 */
	if (*s != '-')
		*ns++ = '-';	/* add option flag */

	/*
	 * gaze to end of argv[1]
	 */
	cp = s + len - 1;
	/*
	 * if last letter is a 't' flag with no argument (in the context
	 * of the oldps options -- option string NOT starting with a '-' --
	 * then convert to 'T' (meaning *this* terminal, i.e. ttyname(0)).
	 */
	if (*cp == 't' && *s != '-')
		*cp = 'T';
	else {
		/*
		 * otherwise check for trailing number, which *may* be a
		 * pid.
		 */
		while (cp >= s && isdigit(*cp))
			--cp;
	}
	cp++;
	memmove(ns, s, (size_t)(cp - s));	/* copy up to trailing number */
	ns += cp - s;
	/*
	 * if there's a trailing number, and not a preceding 'p' (pid) or
	 * 't' (tty) flag, then assume it's a pid and insert a 'p' flag.
	 */
	if (isdigit(*cp) && (cp == s || (cp[-1] != 't' && cp[-1] != 'p' &&
	    (cp - 1 == s || cp[-2] != 't'))))
		*ns++ = 'p';
	/* and append the number */
	(void)strlcpy(ns, cp, newopts + len + 3 - ns);

	return (newopts);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-][acCehjklmrSTuvwx] [-O|o fmt] [-p pid] [-t tty] [-U user]\n",
	    __progname);	
	(void)fprintf(stderr,
	    "%-*s[-M core] [-N system] [-W swap]\n", strlen(__progname) + 8, "");
	(void)fprintf(stderr, "       %s [-L]\n", __progname);
	exit(1);
}
