/*	$OpenBSD: newfs.c,v 1.48 2004/06/26 18:21:36 otto Exp $	*/
/*	$NetBSD: newfs.c,v 1.20 1996/05/16 07:13:03 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1989, 1993, 1994
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
"@(#) Copyright (c) 1983, 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)newfs.c	8.8 (Berkeley) 4/18/94";
#else
static char rcsid[] = "$OpenBSD: newfs.c,v 1.48 2004/06/26 18:21:36 otto Exp $";
#endif
#endif /* not lint */

/*
 * newfs: friendly front end to mkfs
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <util.h>

#include "mntopts.h"
#include "pathnames.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_ASYNC,
	MOPT_UPDATE,
	{ NULL },
};

void	fatal(const char *fmt, ...);
void	usage(void);
void	mkfs(struct partition *, char *, int, int, mode_t, uid_t, gid_t);
void	rewritelabel(char *, int, struct disklabel *);
u_short	dkcksum(struct disklabel *);

#define	COMPAT			/* allow non-labeled disks */

/*
 * The following two constants set the default block and fragment sizes.
 * Both constants must be a power of 2 and meet the following constraints:
 *	MINBSIZE <= DESBLKSIZE <= MAXBSIZE
 *	sectorsize <= DESFRAGSIZE <= DESBLKSIZE
 *	DESBLKSIZE / DESFRAGSIZE <= 8
 */
#define	DFL_FRAGSIZE	2048
#define	DFL_BLKSIZE	16384

/*
 * Cylinder groups may have up to many cylinders. The actual
 * number used depends upon how much information can be stored
 * on a single cylinder. The default is to use as many as
 * possible.
 */
#define	DESCPG		65536	/* desired fs_cpg */

/*
 * ROTDELAY gives the minimum number of milliseconds to initiate
 * another disk transfer on the same cylinder. It is used in
 * determining the rotationally optimal layout for disk blocks
 * within a file; the default of fs_rotdelay is 0ms.
 */
#define ROTDELAY	0

/*
 * MAXBLKPG determines the maximum number of data blocks which are
 * placed in a single cylinder group. The default is one indirect
 * block worth of data blocks.
 */
#define MAXBLKPG(bsize)	((bsize) / sizeof(daddr_t))

/*
 * Each file system has a number of inodes statically allocated.
 * We allocate one inode slot per NFPI fragments, expecting this
 * to be far more than we will ever need.
 */
#define	NFPI		4

/*
 * For each cylinder we keep track of the availability of blocks at different
 * rotational positions, so that we can lay out the data to be picked
 * up with minimum rotational latency.  NRPOS is the default number of
 * rotational positions that we distinguish.  With NRPOS of 8 the resolution
 * of our summary information is 2ms for a typical 3600 rpm drive.  Caching
 * and zoning pretty much defeats rotational optimization, so we now use a
 * default of 1.
 */
#define	NRPOS		1	/* number distinct rotational positions */


int	mfs;			/* run as the memory based filesystem */
int	Nflag;			/* run without writing file system */
int	Oflag;			/* format as an 4.3BSD file system */
int	fssize;			/* file system size */
int	ntracks;		/* # tracks/cylinder */
int	nsectors;		/* # sectors/track */
int	nphyssectors;		/* # sectors/track including spares */
int	secpercyl;		/* sectors per cylinder */
int	trackspares = -1;	/* spare sectors per track */
int	cylspares = -1;		/* spare sectors per cylinder */
int	sectorsize;		/* bytes/sector */
int	realsectorsize;		/* bytes/sector in hardware */
int	rpm;			/* revolutions/minute of drive */
int	interleave;		/* hardware sector interleave */
int	trackskew = -1;		/* sector 0 skew, per track */
int	fsize = 0;		/* fragment size */
int	bsize = 0;		/* block size */
int	cpg;			/* cylinders/cylinder group */
int	cpgflg;			/* cylinders/cylinder group flag was given */
int	minfree = MINFREE;	/* free space threshold */
int	opt = DEFAULTOPT;	/* optimization preference (space or time) */
int	reqopt = -1;		/* opt preference has not been specified */
int	density;		/* number of bytes per inode */
int	maxcontig = 0;		/* max contiguous blocks to allocate */
int	rotdelay = ROTDELAY;	/* rotational delay between blocks */
int	maxbpg;			/* maximum blocks per file in a cyl group */
int	nrpos = NRPOS;		/* # of distinguished rotational positions */
int	avgfilesize = AVFILESIZ;/* expected average file size */
int	avgfilesperdir = AFPDIR;/* expected number of files per directory */
int	bbsize = BBSIZE;	/* boot block size */
int	sbsize = SBSIZE;	/* superblock size */
int	mntflags = MNT_ASYNC;	/* flags to be passed to mount */
int	quiet = 0;		/* quiet flag */
u_long	memleft;		/* virtual memory available */
caddr_t	membase;		/* start address of memory based filesystem */
#ifdef COMPAT
char	*disktype;
int	unlabeled;
#endif

char	device[MAXPATHLEN];

extern	char *__progname;
struct disklabel *getdisklabel(char *, int);

static int do_exec(const char *, const char *, char *const[]);
static int isdir(const char *);
static void copy(char *, char *);
static int gettmpmnt(char *, size_t);

int
main(int argc, char *argv[])
{
	int ch;
	struct partition *pp;
	struct disklabel *lp;
	struct disklabel mfsfakelabel;
	struct partition oldpartition;
	struct stat st;
	struct statfs *mp;
	struct rlimit rl;
	int fsi = -1, fso, len, n, ncyls, maxpartitions;
	char *cp, *s1, *s2, *special, *opstring;
#ifdef MFS
	char mountfromname[BUFSIZ];
	char *pop = NULL;
	pid_t pid, res;
	struct statfs sf;
	struct stat mountpoint;
	int status;
#endif
	uid_t mfsuid;
	gid_t mfsgid;
	mode_t mfsmode;
	char *fstype = NULL;
	char **saveargv = argv;
	int ffs = 1;

	if (strstr(__progname, "mfs"))
		mfs = Nflag = quiet = 1;

	maxpartitions = getmaxpartitions();
	if (maxpartitions > 26)
		fatal("insane maxpartitions value %d", maxpartitions);

	opstring = mfs ?
	    "P:T:a:b:c:d:e:f:i:m:o:s:" :
	    "NOS:T:a:b:c:d:e:f:g:h:i:k:l:m:n:o:p:qr:s:t:u:x:z:";
	while ((ch = getopt(argc, argv, opstring)) != -1) {
		switch (ch) {
		case 'N':
			Nflag = 1;
			break;
		case 'O':
			Oflag = 1;
			break;
		case 'S':
			if ((sectorsize = atoi(optarg)) <= 0)
				fatal("%s: bad sector size", optarg);
			break;
#ifdef COMPAT
		case 'T':
			disktype = optarg;
			break;
#endif
		case 'a':
			if ((maxcontig = atoi(optarg)) <= 0)
				fatal("%s: bad maximum contiguous blocks\n",
				    optarg);
			break;
		case 'b':
			if ((bsize = atoi(optarg)) < MINBSIZE)
				fatal("%s: bad block size", optarg);
			break;
		case 'c':
			if ((cpg = atoi(optarg)) <= 0)
				fatal("%s: bad cylinders/group", optarg);
			cpgflg++;
			break;
		case 'd':
			if ((rotdelay = atoi(optarg)) < 0)
				fatal("%s: bad rotational delay\n", optarg);
			break;
		case 'e':
			if ((maxbpg = atoi(optarg)) <= 0)
		fatal("%s: bad blocks per file in a cylinder group\n",
				    optarg);
			break;
		case 'f':
			if ((fsize = atoi(optarg)) <= 0)
				fatal("%s: bad fragment size", optarg);
			break;
		case 'g':
			if ((avgfilesize = atoi(optarg)) <= 0)
				fatal("%s: bad average file size", optarg);
			break;
		case 'h':
			if ((avgfilesperdir = atoi(optarg)) <= 0)
				fatal("%s: bad average files per dir", optarg);
			break;
		case 'i':
			if ((density = atoi(optarg)) <= 0)
				fatal("%s: bad bytes per inode\n", optarg);
			break;
		case 'k':
			if ((trackskew = atoi(optarg)) < 0)
				fatal("%s: bad track skew", optarg);
			break;
		case 'l':
			if ((interleave = atoi(optarg)) <= 0)
				fatal("%s: bad interleave", optarg);
			break;
		case 'm':
			if ((minfree = atoi(optarg)) < 0 || minfree > 99)
				fatal("%s: bad free space %%\n", optarg);
			break;
		case 'n':
			if ((nrpos = atoi(optarg)) <= 0)
				fatal("%s: bad rotational layout count\n",
				    optarg);
			break;
		case 'o':
			if (mfs)
				getmntopts(optarg, mopts, &mntflags);
			else {
				if (strcmp(optarg, "space") == 0)
					reqopt = opt = FS_OPTSPACE;
				else if (strcmp(optarg, "time") == 0)
					reqopt = opt = FS_OPTTIME;
				else
					fatal("%s: unknown optimization "
					    "preference: use `space' or `time'.");
			}
			break;
		case 'p':
			if ((trackspares = atoi(optarg)) < 0)
				fatal("%s: bad spare sectors per track",
				    optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			if ((rpm = atoi(optarg)) <= 0)
				fatal("%s: bad revolutions/minute\n", optarg);
			break;
		case 's':
			if ((fssize = atoi(optarg)) <= 0)
				fatal("%s: bad file system size", optarg);
			break;
		case 'z':
			if ((ntracks = atoi(optarg)) <= 0)
				fatal("%s: bad total tracks", optarg);
			break;
		case 't':
			fstype = optarg;
			if (strcmp(fstype, "ffs"))
				ffs = 0;
			break;
		case 'u':
			if ((nsectors = atoi(optarg)) <= 0)
				fatal("%s: bad sectors/track", optarg);
			break;
		case 'x':
			if ((cylspares = atoi(optarg)) < 0)
				fatal("%s: bad spare sectors per cylinder",
				    optarg);
			break;
#ifdef MFS
		case 'P':
			pop = optarg;
			break;
#endif
		case '?':
		default:
			usage();
		}
		if (!ffs)
			break;
	}
	argc -= optind;
	argv += optind;

	if (ffs && argc - mfs != 1)
		usage();

	/* Increase our data size to the max */
	if (getrlimit(RLIMIT_DATA, &rl) == 0) {
		rl.rlim_cur = rl.rlim_max;
		(void)setrlimit(RLIMIT_DATA, &rl);
	}

	special = argv[0];
	if (!mfs) {
		char execname[MAXPATHLEN], name[MAXPATHLEN];

		if (fstype == NULL)
			fstype = readlabelfs(special, 0);
		if (fstype != NULL && strcmp(fstype, "ffs")) {
			snprintf(name, sizeof name, "newfs_%s", fstype);
			saveargv[0] = name;
			snprintf(execname, sizeof execname, "%s/newfs_%s",
			    _PATH_SBIN, fstype);
			(void)execv(execname, saveargv);
			snprintf(execname, sizeof execname, "%s/newfs_%s",
			    _PATH_USRSBIN, fstype);
			(void)execv(execname, saveargv);
			err(1, "%s not found", name);
		}
	}

	if (mfs && !strcmp(special, "swap")) {
		/*
		 * it's an MFS, mounted on "swap."  fake up a label.
		 * XXX XXX XXX
		 */
		fso = -1;	/* XXX; normally done below. */

		memset(&mfsfakelabel, 0, sizeof(mfsfakelabel));
		mfsfakelabel.d_secsize = 512;
		mfsfakelabel.d_nsectors = 64;
		mfsfakelabel.d_ntracks = 16;
		mfsfakelabel.d_ncylinders = 16;
		mfsfakelabel.d_secpercyl = 1024;
		mfsfakelabel.d_secperunit = 16384;
		mfsfakelabel.d_rpm = 3600;
		mfsfakelabel.d_interleave = 1;
		mfsfakelabel.d_npartitions = 1;
		mfsfakelabel.d_partitions[0].p_size = 16384;
		mfsfakelabel.d_partitions[0].p_fsize = 1024;
		mfsfakelabel.d_partitions[0].p_frag = 8;
		mfsfakelabel.d_partitions[0].p_cpg = 16;

		lp = &mfsfakelabel;
		pp = &mfsfakelabel.d_partitions[0];

		goto havelabel;
	}
	cp = strrchr(special, '/');
	if (cp == NULL) {
		/*
		 * No path prefix; try /dev/r%s then /dev/%s.
		 */
		(void)snprintf(device, sizeof(device), "%sr%s",
			       _PATH_DEV, special);
		if (stat(device, &st) == -1)
			(void)snprintf(device, sizeof(device), "%s%s",
				       _PATH_DEV, special);
		special = device;
	}
	if (Nflag) {
		fso = -1;
	} else {
		fso = open(special, O_WRONLY);
		if (fso < 0)
			fatal("%s: %s", special, strerror(errno));

		/* Bail if target special is mounted */
		n = getmntinfo(&mp, MNT_NOWAIT);
		if (n == 0)
			fatal("%s: getmntinfo: %s", special, strerror(errno));

		len = sizeof(_PATH_DEV) - 1;
		s1 = special;
		if (strncmp(_PATH_DEV, s1, len) == 0)
			s1 += len;

		while (--n >= 0) {
			s2 = mp->f_mntfromname;
			if (strncmp(_PATH_DEV, s2, len) == 0) {
				s2 += len - 1;
				*s2 = 'r';
			}
			if (strcmp(s1, s2) == 0 || strcmp(s1, &s2[1]) == 0)
				fatal("%s is mounted on %s",
				    special, mp->f_mntonname);
			++mp;
		}
	}
#ifdef COMPAT
	if (mfs && disktype != NULL) {
		lp = (struct disklabel *)getdiskbyname(disktype);
		if (lp == NULL)
			fatal("%s: unknown disk type", disktype);
		pp = &lp->d_partitions[1];
	} else
#endif
	{
		fsi = open(special, O_RDONLY);
		if (fsi < 0)
			fatal("%s: %s", special, strerror(errno));
		if (fstat(fsi, &st) < 0)
			fatal("%s: %s", special, strerror(errno));
		if (!S_ISCHR(st.st_mode) && !mfs)
			printf("%s: %s: not a character-special device\n",
			    __progname, special);
		cp = strchr(argv[0], '\0') - 1;
		if (cp == NULL || ((*cp < 'a' || *cp > ('a' + maxpartitions - 1))
		    && !isdigit(*cp)))
			fatal("%s: can't figure out file system partition",
			    argv[0]);
		lp = getdisklabel(special, fsi);
		if (isdigit(*cp))
			pp = &lp->d_partitions[0];
		else
			pp = &lp->d_partitions[*cp - 'a'];
		if (pp->p_size == 0)
			fatal("%s: `%c' partition is unavailable",
			    argv[0], *cp);
		if (pp->p_fstype == FS_BOOT)
			fatal("%s: `%c' partition overlaps boot program",
			      argv[0], *cp);
	}
havelabel:
	if (fssize == 0)
		fssize = pp->p_size;
	if (fssize > pp->p_size && !mfs)
	       fatal("%s: maximum file system size on the `%c' partition is %d",
			argv[0], *cp, pp->p_size);
	if (rpm == 0) {
		rpm = lp->d_rpm;
		if (rpm <= 0)
			rpm = 3600;
	}
	if (ntracks == 0) {
		ntracks = lp->d_ntracks;
		if (ntracks <= 0)
			fatal("%s: no default #tracks", argv[0]);
	}
	if (nsectors == 0) {
		nsectors = lp->d_nsectors;
		if (nsectors <= 0)
			fatal("%s: no default #sectors/track", argv[0]);
	}
	if (sectorsize == 0) {
		sectorsize = lp->d_secsize;
		if (sectorsize <= 0)
			fatal("%s: no default sector size", argv[0]);
	}
	if (trackskew == -1) {
		trackskew = lp->d_trackskew;
		if (trackskew < 0)
			trackskew = 0;
	}
	if (interleave == 0) {
		interleave = lp->d_interleave;
		if (interleave <= 0)
			interleave = 1;
	}
	if (fsize == 0) {
		fsize = pp->p_fsize;
		if (fsize <= 0)
			fsize = MAX(DFL_FRAGSIZE, lp->d_secsize);
	}
	if (bsize == 0) {
		bsize = pp->p_frag * pp->p_fsize;
		if (bsize <= 0)
			bsize = MIN(DFL_BLKSIZE, 8 * fsize);
	}
	/*
	 * Maxcontig sets the default for the maximum number of blocks
	 * that may be allocated sequentially. With filesystem clustering
	 * it is possible to allocate contiguous blocks up to the maximum
	 * transfer size permitted by the controller or buffering.
	 */
	if (maxcontig == 0)
		maxcontig = MAX(1, MIN(MAXPHYS, MAXBSIZE) / bsize - 1);
	if (density == 0)
		density = NFPI * fsize;
	if (minfree < MINFREE && opt != FS_OPTSPACE && reqopt == -1) {
		fprintf(stderr, "Warning: changing optimization to space ");
		fprintf(stderr, "because minfree is less than %d%%\n", MINFREE);
		opt = FS_OPTSPACE;
	}
	if (trackspares == -1) {
		trackspares = lp->d_sparespertrack;
		if (trackspares < 0)
			trackspares = 0;
	}
	nphyssectors = nsectors + trackspares;
	if (cylspares == -1) {
		cylspares = lp->d_sparespercyl;
		if (cylspares < 0)
			cylspares = 0;
	}
	secpercyl = nsectors * ntracks - cylspares;
	if (secpercyl != lp->d_secpercyl)
		fprintf(stderr, "%s (%d) %s (%lu)\n",
		    "Warning: calculated sectors per cylinder", secpercyl,
		    "disagrees with disk label",
		    (unsigned long)lp->d_secpercyl);
	if (maxbpg == 0)
		maxbpg = MAXBLKPG(bsize);
#ifdef notdef /* label may be 0 if faked up by kernel */
	bbsize = lp->d_bbsize;
	sbsize = lp->d_sbsize;
#endif
	oldpartition = *pp;
	realsectorsize = sectorsize;
	if (sectorsize < DEV_BSIZE) {
		int secperblk = DEV_BSIZE / sectorsize;

		sectorsize = DEV_BSIZE;
		nsectors /= secperblk;
		nphyssectors /= secperblk;
		secpercyl /= secperblk;
		fssize /= secperblk;
		pp->p_size /= secperblk;
	} else if (sectorsize > DEV_BSIZE) {
		int blkpersec = sectorsize / DEV_BSIZE;

		sectorsize = DEV_BSIZE;
		nsectors *= blkpersec;
		nphyssectors *= blkpersec;
		secpercyl *= blkpersec;
		fssize *= blkpersec;
		pp->p_size *= blkpersec;
	}
	ncyls = fssize / secpercyl;
	if (ncyls < 2)
		ncyls = 2;
	if (cpg == 0)
		cpg = DESCPG < ncyls ? DESCPG : ncyls;
	else if (cpg > ncyls) {
		cpg = ncyls;
		printf("Number of cylinders restricts cylinders per group "
		    "to %d.\n", cpg);
	}
#ifdef MFS
	if (mfs) {
		if (stat(argv[1], &mountpoint) < 0)
			err(88, "stat %s", argv[1]);
		mfsuid = mountpoint.st_uid;
		mfsgid = mountpoint.st_gid;
		mfsmode = mountpoint.st_mode & ALLPERMS;
	}
#endif

	mkfs(pp, special, fsi, fso, mfsmode, mfsuid, mfsgid);
	if (realsectorsize < DEV_BSIZE)
		pp->p_size *= DEV_BSIZE / realsectorsize;
	else if (realsectorsize > DEV_BSIZE)
		pp->p_size /= realsectorsize / DEV_BSIZE;
	if (!Nflag && memcmp(pp, &oldpartition, sizeof(oldpartition)))
		rewritelabel(special, fso, lp);
	if (!Nflag)
		close(fso);
	close(fsi);
#ifdef MFS
	if (mfs) {
		struct mfs_args args;

		switch (pid = fork()) {
		case -1:
			err(10, "mfs");
		case 0:
			snprintf(mountfromname, sizeof(mountfromname),
			    "mfs:%d", getpid());
			break;
		default:
			snprintf(mountfromname, sizeof(mountfromname),
			    "mfs:%d", pid);
			for (;;) {
				/*
				 * spin until the mount succeeds
				 * or the child exits
				 */
				usleep(1);

				/*
				 * XXX Here is a race condition: another process
				 * can mount a filesystem which hides our
				 * ramdisk before we see the success.
				 */
				if (statfs(argv[1], &sf) < 0)
					err(88, "statfs %s", argv[1]);
				if (!strcmp(sf.f_mntfromname, mountfromname) &&
				    !strncmp(sf.f_mntonname, argv[1],
					     MNAMELEN) &&
				    !strcmp(sf.f_fstypename, "mfs")) {
					if (pop != NULL)
						copy(pop, argv[1]);
					exit(0);
				}
				res = waitpid(pid, &status, WNOHANG);
				if (res == -1)
					err(11, "waitpid");
				if (res != pid)
					continue;
				if (WIFEXITED(status)) {
					if (WEXITSTATUS(status) == 0)
						exit(0);
					errx(1, "%s: mount: %s", argv[1],
					     strerror(WEXITSTATUS(status)));
				} else
					errx(11, "abnormal termination");
			}
			/* NOTREACHED */
		}

		(void) setsid();
		(void) close(0);
		(void) close(1);
		(void) close(2);
		(void) chdir("/");

		args.fspec = mountfromname;
		args.export_info.ex_root = -2;
		if (mntflags & MNT_RDONLY)
			args.export_info.ex_flags = MNT_EXRDONLY;
		else
			args.export_info.ex_flags = 0;
		args.base = membase;
		args.size = fssize * sectorsize;
		if (mount(MOUNT_MFS, argv[1], mntflags, &args) < 0)
			exit(errno); /* parent prints message */
	}
#endif
	exit(0);
}

#ifdef COMPAT
char lmsg[] = "%s: can't read disk label; disk type must be specified";
#else
char lmsg[] = "%s: can't read disk label";
#endif

struct disklabel *
getdisklabel(char *s, int fd)
{
	static struct disklabel lab;

	if (ioctl(fd, DIOCGDINFO, (char *)&lab) < 0) {
#ifdef COMPAT
		if (disktype) {
			struct disklabel *lp;

			unlabeled++;
			lp = getdiskbyname(disktype);
			if (lp == NULL)
				fatal("%s: unknown disk type", disktype);
			return (lp);
		}
#endif
		warn("ioctl (GDINFO)");
		fatal(lmsg, s);
	}
	return (&lab);
}

void
rewritelabel(char *s, int fd, struct disklabel *lp)
{
#ifdef COMPAT
	if (unlabeled)
		return;
#endif
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	if (ioctl(fd, DIOCWDINFO, (char *)lp) < 0) {
		warn("ioctl (WDINFO)");
		fatal("%s: can't rewrite disk label", s);
	}
#ifdef __vax__
	if (lp->d_type == DTYPE_SMD && lp->d_flags & D_BADSECT) {
		int i;
		int cfd;
		daddr_t alt;
		char specname[64];
		char blk[1024];
		char *cp;

		/*
		 * Make name for 'c' partition.
		 */
		strncpy(specname, s, sizeof(specname) - 1);
		specname[sizeof(specname) - 1] = '\0';
		cp = specname + strlen(specname) - 1;
		if (!isdigit(*cp))
			*cp = 'c';
		cfd = open(specname, O_WRONLY);
		if (cfd < 0)
			fatal("%s: %s", specname, strerror(errno));
		memset(blk, 0, sizeof(blk));
		*(struct disklabel *)(blk + LABELOFFSET) = *lp;
		alt = lp->d_ncylinders * lp->d_secpercyl - lp->d_nsectors;
		for (i = 1; i < 11 && i < lp->d_nsectors; i += 2) {
			off_t offset;

			offset = alt + i;
			offset *= lp->d_secsize;
			if (lseek(cfd, offset, SEEK_SET) == -1)
				fatal("lseek to badsector area: %s",
				    strerror(errno));
			if (write(cfd, blk, lp->d_secsize) < lp->d_secsize)
				warn("alternate label %d write", i/2);
		}
		close(cfd);
	}
#endif	/*__vax__*/
}

/*VARARGS*/
void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (fcntl(STDERR_FILENO, F_GETFL) < 0) {
		openlog(__progname, LOG_CONS, LOG_DAEMON);
		vsyslog(LOG_ERR, fmt, ap);
		closelog();
	} else {
		vwarnx(fmt, ap);
	}
	va_end(ap);
	exit(1);
	/*NOTREACHED*/
}

struct fsoptions {
	char *str;
	int mfs_too;
} fsopts[] = {
	{ "-N do not create file system, just print out parameters", 0 },
	{ "-O create a 4.3BSD format filesystem", 0 },
#ifdef MFS
	{ "-P src populate mfs filesystem", 2 },
#endif
	{ "-S sector size", 0 },
#ifdef COMPAT
	{ "-T disktype", 0 },
#endif
	{ "-a maximum contiguous blocks", 1 },
	{ "-b block size", 1 },
	{ "-c cylinders/group", 1 },
	{ "-d rotational delay between contiguous blocks", 1 },
	{ "-e maximum blocks per file in a cylinder group", 1 },
	{ "-f frag size", 1 },
	{ "-g average file size", 0 },
	{ "-h average files per directory", 0 },
	{ "-i number of bytes per inode", 1 },
	{ "-k sector 0 skew, per track", 0 },
	{ "-l hardware sector interleave", 0 },
	{ "-m minimum free space %%", 1 },
	{ "-n number of distinguished rotational positions", 0 },
	{ "-o optimization preference (`space' or `time')", 1 },
	{ "-p spare sectors per track", 0 },
	{ "-r revolutions/minute", 0 },
	{ "-s file system size (sectors)", 1 },
	{ "-t file system type", 0 },
	{ "-u sectors/track", 0 },
	{ "-x spare sectors per cylinder", 0 },
	{ "-z tracks/cylinder", 0 },
	{ NULL, NULL }
};

void
usage(void)
{
	struct fsoptions *fsopt;

	if (mfs) {
		fprintf(stderr,
		    "usage: %s [ -fsoptions ] special-device mount-point\n",
			__progname);
	} else {
		fprintf(stderr,
		    "usage: %s [ -fsoptions ] special-device\n", __progname);
	}
	fprintf(stderr, "where fsoptions are:\n");
	for (fsopt = fsopts; fsopt->str; fsopt++) {
		if (!mfs || fsopt->mfs_too == 1 || (mfs && fsopt->mfs_too == 2))
			fprintf(stderr, "\t%s\n", fsopt->str);
	}
	exit(1);
}

#ifdef MFS

static int
do_exec(const char *dir, const char *cmd, char *const argv[])
{
	pid_t pid;
	int ret, status;
	sig_t intsave, quitsave;

	switch (pid = fork()) {
	case -1:
		err(1, "fork");
	case 0:
		if (dir != NULL && chdir(dir) != 0)
			err(1, "chdir");
		if (execv(cmd, argv) != 0)
			err(1, "%s", cmd);
		break;
	default:
		intsave = signal(SIGINT, SIG_IGN);
		quitsave = signal(SIGQUIT, SIG_IGN);
		for (;;) {
			ret = waitpid(pid, &status, 0);
			if (ret == -1)
				err(11, "waitpid");
			if (WIFEXITED(status)) {
				status = WEXITSTATUS(status);
				if (status != 0)
					warnx("%s: exited", cmd);
				break;
			} else if (WIFSIGNALED(status)) {
				warnx("%s: %s", cmd,
				    strsignal(WTERMSIG(status)));
				status = 1;
				break;
			}
		}
		signal(SIGINT, intsave);
		signal(SIGQUIT, quitsave);
		return (status);
	}
	/* NOTREACHED */
	return (-1);
}

static int
isdir(const char *path)
{
	struct stat st;

	if (stat(path, &st) != 0)
		err(1, "cannot stat %s", path);
	if (!S_ISDIR(st.st_mode) && !S_ISBLK(st.st_mode))
		errx(1, "%s: not a dir or a block device", path);
	return (S_ISDIR(st.st_mode));
}

static void
copy(char *src, char *dst)
{
	int ret, dir, created = 0;
	struct ufs_args mount_args;
	char mountpoint[MNAMELEN];
	char *const argv[] = { "pax", "-rw", "-pe", ".", dst, NULL } ;

	dir = isdir(src);
	if (dir)
		strlcpy(mountpoint, src, sizeof(mountpoint));
	else {
		created = gettmpmnt(mountpoint, sizeof(mountpoint));
		memset(&mount_args, 0, sizeof(mount_args));
		mount_args.fspec = src;
		ret = mount(MOUNT_FFS, mountpoint, MNT_RDONLY, &mount_args);
		if (ret != 0) {
			if (created && rmdir(mountpoint) != 0)
				warn("rmdir %s", mountpoint);
			if (unmount(dst, 0) != 0)
				warn("unmount %s", dst);
			err(1, "mount %s %s", src, mountpoint);
		}
	}
	ret = do_exec(mountpoint, "/bin/pax", argv);
	if (!dir && unmount(mountpoint, 0) != 0)
		warn("unmount %s", mountpoint);
	if (created && rmdir(mountpoint) != 0)
		warn("rmdir %s", mountpoint);
	if (ret != 0) {
		if (unmount(dst, 0) != 0)
			warn("unmount %s", dst);
		errx(1, "copy %s to %s failed", mountpoint, dst);
	}
}

static int
gettmpmnt(char *mountpoint, size_t len)
{
	const char *tmp;
	const char *mnt = _PATH_MNT;
	struct statfs fs;
	size_t n;

	tmp = getenv("TMPDIR");
	if (tmp == NULL || *tmp == '\0')
		tmp = _PATH_TMP;

	if (statfs(tmp, &fs) != 0)
		err(1, "statfs %s", tmp);
	if (fs.f_flags & MNT_RDONLY) {
		if (statfs(mnt, &fs) != 0)
			err(1, "statfs %s", mnt);
		if (strcmp(fs.f_mntonname, "/") != 0)
			errx(1, "tmp mountpoint %s busy", mnt);
		if (strlcpy(mountpoint, mnt, len) >= len)
			errx(1, "tmp mountpoint %s too long", mnt);
		return (0);
	}
	n = strlcpy(mountpoint, tmp, len);
	if (n >= len)
		errx(1, "tmp mount point too long");
	if (mountpoint[n - 1] != '/')
		strlcat(mountpoint, "/", len);
	n = strlcat(mountpoint, "mntXXXXXXXXXX", len);
	if (n >= len)
		errx(1, "tmp mount point too long");
	if (mkdtemp(mountpoint) == NULL)
		err(1, "mkdtemp %s", mountpoint);
	return (1);
}

#endif /* MFS */
