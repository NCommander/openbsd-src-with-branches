/*	$OpenBSD: ncheck_ffs.c,v 1.3 1996/06/30 05:45:55 tholo Exp $	*/

/*-
 * Copyright (c) 1995, 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
 *      This product includes software developed by SigmaSoft, Th. Lockert
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: ncheck_ffs.c,v 1.3 1996/06/30 05:45:55 tholo Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ufs/ffs/fs.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fstab.h>
#include <errno.h>
#include <err.h>

#define MAXINOPB	(MAXBSIZE / sizeof(struct dinode))

char	*disk;		/* name of the disk file */
int	diskfd;		/* disk file descriptor */
struct	fs *sblock;	/* the file system super block */
char	sblock_buf[MAXBSIZE];
long	dev_bsize;	/* block size of underlying disk device */
int	dev_bshift;	/* log2(dev_bsize) */
ino_t	*ilist;		/* list of inodes to check */
int	ninodes;	/* number of inodes in list */
int	sflag;		/* only suid and special files */
int	aflag;		/* print the . and .. entries too */
int	mflag;		/* verbose output */
int	iflag;		/* specific inode */

struct icache_s {
	ino_t		ino;
	struct dinode	di;
} *icache;
int	nicache;

void addinode __P((ino_t inum));
struct dinode *getino __P((ino_t inum));
void findinodes __P((ino_t));
void bread __P((daddr_t, char *, int));
void usage __P((void));
void scanonedir __P((ino_t, const char *));
void dirindir __P((ino_t, daddr_t, int, long *, const char *));
void searchdir __P((ino_t, daddr_t, long, long, const char *));
int matchino __P((const void *, const void *));
int matchcache __P((const void *, const void *));
void cacheino __P((ino_t, struct dinode *));
struct dinode *cached __P((ino_t));
int main __P((int, char *[]));
char *rawname __P((char *));

/*
 * Check to see if the indicated inodes are the same
 */
int
matchino(key, val)
	const void *key, *val;
{
	ino_t k = *(ino_t *)key;
	ino_t v = *(ino_t *)val;

	if (k < v)
		return -1;
	else if (k > v)
		return 1;
	return 0;
}

/*
 * Check if the indicated inode match the entry in the cache
 */
int matchcache(key, val)
	const void *key, *val;
{
	ino_t		ino = *(ino_t *)key;
	struct icache_s	*ic = (struct icache_s *)val;

	if (ino < ic->ino)
		return -1;
	else if (ino > ic->ino)
		return 1;
	return 0;
}

/*
 * Add an inode to the cached entries
 */
void
cacheino(ino, ip)
	ino_t ino;
	struct dinode *ip;
{
	if (nicache)
		icache = realloc(icache, (nicache + 1) * sizeof(struct icache_s));
	else
		icache = malloc(sizeof(struct icache_s));
	icache[nicache].ino = ino;
	icache[nicache++].di = *ip;
}

/*
 * Get a cached inode
 */
struct dinode *
cached(ino)
	ino_t ino;
{
	struct icache_s *ic;

	ic = (struct icache_s *)bsearch(&ino, icache, nicache, sizeof(struct icache_s), matchcache);
	return ic ? &ic->di : NULL;
}

/*
 * Walk the inode list for a filesystem to find all allocated inodes
 * Remember inodes we want to give information about and cache all
 * inodes pointing to directories
 */
void
findinodes(maxino)
	ino_t maxino;
{
	register ino_t ino;
	register struct dinode *dp;
	mode_t mode;

	for (ino = ROOTINO; ino < maxino; ino++) {
		dp = getino(ino);
		mode = dp->di_mode & IFMT;
		if (!mode)
			continue;
		if (mode == IFDIR)
			cacheino(ino, dp);
		if (iflag ||
		    (sflag &&
		     (((dp->di_mode & (ISGID | ISUID)) == 0) &&
		      ((mode == IFREG) || (mode == IFDIR) || (mode == IFLNK)))))
			continue;
		addinode(ino);
	}
}

/*
 * Get a specified inode from disk.  Attempt to minimize reads to once
 * per cylinder group
 */
struct dinode *
getino(inum)
	ino_t inum;
{
	static struct dinode *itab = NULL;
	static daddr_t iblk = -1;
	struct dinode *ip;

	if (inum < ROOTINO || inum >= sblock->fs_ncg * sblock->fs_ipg)
		return NULL;
	if ((ip = cached(inum)) != NULL)
		return ip;
	if ((inum / sblock->fs_ipg) != iblk || itab == NULL) {
		iblk = inum / sblock->fs_ipg;
		if (itab == NULL &&
		    (itab = calloc(sizeof(struct dinode), sblock->fs_ipg)) == NULL)
			errx(1, "no memory for inodes");
		bread(fsbtodb(sblock, cgimin(sblock, iblk)), (char *)itab,
		      sblock->fs_ipg * sizeof(struct dinode));
	}
	return &itab[inum % sblock->fs_ipg];
}

/*
 * Read a chunk of data from the disk. Try to recover from hard errors by
 * reading in sector sized pieces.  Error recovery is attempted at most
 * BREADEMAX times before seeking consent from the operator to continue.
 */
int	breaderrors = 0;		
#define	BREADEMAX 32

void
bread(blkno, buf, size)
	daddr_t blkno;
	char *buf;
	int size;	
{
	int cnt, i;

loop:
	if (lseek(diskfd, ((off_t)blkno << dev_bshift), 0) < 0)
		warnx("bread: lseek fails\n");
	if ((cnt = read(diskfd, buf, size)) == size)
		return;
	if (blkno + (size / dev_bsize) > fsbtodb(sblock, sblock->fs_size)) {
		/*
		 * Trying to read the final fragment.
		 *
		 * NB - dump only works in TP_BSIZE blocks, hence
		 * rounds `dev_bsize' fragments up to TP_BSIZE pieces.
		 * It should be smarter about not actually trying to
		 * read more than it can get, but for the time being
		 * we punt and scale back the read only when it gets
		 * us into trouble. (mkm 9/25/83)
		 */
		size -= dev_bsize;
		goto loop;
	}
	if (cnt == -1)
		warnx("read error from %s: %s: [block %d]: count=%d\n",
			disk, strerror(errno), blkno, size);
	else
		warnx("short read error from %s: [block %d]: count=%d, got=%d\n",
			disk, blkno, size, cnt);
	if (++breaderrors > BREADEMAX)
		errx(1, "More than %d block read errors from %s\n", BREADEMAX, disk);
	/*
	 * Zero buffer, then try to read each sector of buffer separately.
	 */
	memset(buf, 0, size);
	for (i = 0; i < size; i += dev_bsize, buf += dev_bsize, blkno++) {
		if (lseek(diskfd, ((off_t)blkno << dev_bshift), 0) < 0)
			warnx("bread: lseek2 fails!\n");
		if ((cnt = read(diskfd, buf, (int)dev_bsize)) == dev_bsize)
			continue;
		if (cnt == -1) {
			warnx("read error from %s: %s: [sector %d]: count=%d\n",
				disk, strerror(errno), blkno, dev_bsize);
			continue;
		}
		warnx("short read error from %s: [sector %d]: count=%d, got=%d\n",
			disk, blkno, dev_bsize, cnt);
	}
}

/*
 * Add an inode to the in-memory list of inodes to dump
 */
void
addinode(ino)
	ino_t ino;
{
	if (ninodes)
		ilist = realloc(ilist, sizeof(ino_t) * (ninodes + 1));
	else
		ilist = malloc(sizeof(ino_t));
	if (ilist == NULL)
		errx(4, "not enough memory to allocate tables");
	ilist[ninodes] = ino;
	ninodes++;
}

/*
 * Scan the directory pointer at by ino
 */
void
scanonedir(ino, path)
	ino_t ino;
	const char *path;
{
	struct dinode *dp;
	long filesize;
	int i;

	if ((dp = cached(ino)) == NULL)
		return;
	filesize = dp->di_size;
	for (i = 0; filesize > 0 && i < NDADDR; i++) {
		if (dp->di_db[i])
			searchdir(ino, dp->di_db[i], dblksize(sblock, dp, i), filesize, path);
		filesize -= sblock->fs_bsize;
	}
	for (i = 0; filesize > 0 && i < NIADDR; i++) {
		if (dp->di_ib[i])
			dirindir(ino, dp->di_ib[i], i, &filesize, path);
	}
}

/*
 * Read indirect blocks, and pass the data blocks to be searched
 * as directories. Quit as soon as any entry is found that will
 * require the directory to be dumped.
 */
void
dirindir(ino, blkno, ind_level, filesize, path)
	ino_t ino;
	daddr_t blkno;
	int ind_level;
	long *filesize;
	const char *path;
{
	daddr_t idblk[MAXBSIZE / sizeof(daddr_t)];
	int i;

	bread(fsbtodb(sblock, blkno), (char *)idblk, (int)sblock->fs_bsize);
	if (ind_level <= 0) {
		for (i = 0; *filesize > 0 && i < NINDIR(sblock); i++) {
			blkno = idblk[i];
			if (blkno)
				searchdir(ino, blkno, sblock->fs_bsize, *filesize, path);
		}
		return;
	}
	ind_level--;
	for (i = 0; *filesize > 0 && NINDIR(sblock); i++) {
		blkno = idblk[i];
		if (blkno)
			dirindir(ino, blkno, ind_level, filesize, path);
	}
}

/*
 * Scan a disk block containing directory information looking to see if
 * any of the entries are on the inode list and to see if the directory
 * contains any subdirectories.  Display entries for marked inodes.
 * Pass inodes pointing to directories back to scanonedir().
 */
void
searchdir(ino, blkno, size, filesize, path)
	ino_t ino;
	daddr_t blkno;
	long size;
	long filesize;
	const char *path;
{
	char dblk[MAXBSIZE];
	struct direct *dp;
	struct dinode *di;
	mode_t mode;
	char *npath;
	long loc;

	bread(fsbtodb(sblock, blkno), dblk, (int)size);
	if (filesize < size)
		size = filesize;
	for (loc = 0; loc < size;) {
		dp = (struct direct *)(dblk + loc);
		if (dp->d_reclen == 0) {
			warnx("corrupted directory, inode %lu", ino);
			break;
		}
		loc += dp->d_reclen;
		if (!dp->d_ino)
			continue;
		if (dp->d_name[0] == '.') {
			if (!aflag && (dp->d_name[1] == '\0' ||
			    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
				continue;
		}
		di = getino(dp->d_ino);
		mode = di->di_mode & IFMT;
		if (bsearch(&dp->d_ino, ilist, ninodes, sizeof(*ilist), matchino)) {
			if (mflag)
				printf("mode %-6o uid %-5lu gid %-5lu ino ", di->di_mode, di->di_uid, di->di_gid);
			printf("%-7lu %s/%s%s\n", dp->d_ino, path, dp->d_name, mode == IFDIR ? "/." : "");
		}
		if (mode == IFDIR) {
			if (dp->d_name[0] == '.') {
				if (dp->d_name[1] == '\0' ||
				    (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))
				continue;
			}
			npath = malloc(strlen(path) + strlen(dp->d_name) + 2);
			strcpy(npath, path);
			strcat(npath, "/");
			strcat(npath, dp->d_name);
			scanonedir(dp->d_ino, npath);
			free(npath);
		}
	}
}

char *
rawname(name)
	char *name;
{
	static char newname[MAXPATHLEN];
	char *p;

	if ((p = strrchr(name, '/')) == NULL)
		return name;
	*p = '\0';
	strcpy(newname, name);
	*p++ = '/';
	strcat(newname, "/r");
	strcat(newname, p);
	return(newname);
}

void
usage()
{
	fprintf(stderr, "Usage: ncheck_ffs [-i numbers] [-ams] filesystem\n");
	exit(3);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat stblock;
	struct fstab *fsp;
	int c;
	ino_t ino;

	while ((c = getopt(argc, argv, "ai:ms")) != EOF)
		switch (c) {
			case 'a':
				aflag++;
				break;
			case 'i':
				iflag++;
				addinode(strtoul(optarg, NULL, 10));
				while (optind < argc && (ino = strtoul(argv[optind], NULL, 10)) != 0) {
					addinode(ino);
					optind++;
				}
				break;
			case 'm':
				mflag++;
				break;
			case 's':
				sflag++;
				break;
			case '?':
				exit(2);
		}
	if (optind != argc - 1)
		usage();

	disk = argv[optind];

	if (stat(disk, &stblock) < 0)
		err(1, "cannot stat %s", disk);

        if (S_ISBLK(stblock.st_mode)) {
		disk = rawname(disk);
	}
	else if (!S_ISCHR(stblock.st_mode)) {
		if ((fsp = getfsfile(disk)) == NULL)
			err(1, "cound not find file system %s", disk);
                disk = rawname(fsp->fs_spec);
        }

	if ((diskfd = open(disk, O_RDONLY)) < 0)
		err(1, "cannot open %s", disk);
	sblock = (struct fs *)sblock_buf;
	bread(SBOFF, (char *)sblock, SBSIZE);
	if (sblock->fs_magic != FS_MAGIC)
		errx(1, "not a file system");
	dev_bsize = sblock->fs_fsize / fsbtodb(sblock, 1);
	dev_bshift = ffs(dev_bsize) - 1;
	if (dev_bsize != (1 << dev_bshift))
		errx(2, "blocksize (%d) not a power of 2", dev_bsize);
	findinodes(sblock->fs_ipg * sblock->fs_ncg);
	printf("%s:\n", disk);
	scanonedir(ROOTINO, "");
	close(diskfd);
	return 0;
}
