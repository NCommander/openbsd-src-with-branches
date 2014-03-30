/*	$OpenBSD: rcp.c,v 1.53 2013/11/12 04:36:02 deraadt Exp $	*/
/*	$NetBSD: rcp.c,v 1.9 1995/03/21 08:19:06 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1990, 1992, 1993
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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"
#include "extern.h"

#ifdef KERBEROS
#include <des.h>
#include <kerberosIV/krb.h>

char	dst_realm_buf[REALM_SZ];
char	*dest_realm = NULL;
int	use_kerberos = 1;
CREDENTIALS	cred;
Key_schedule	schedule;
extern	char	*krb_realmofhost();
int	doencrypt = 0;
#define	OPTIONS	"dfKk:prtx"
#else
#define	OPTIONS "dfprt"
#endif

struct passwd *pwd;
u_short	port;
uid_t	userid;
gid_t	groupid;
int errs, rem;
int pflag, iamremote, iamrecursive, targetshouldbedirectory;

#define	CMDNEEDS	64
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */

#ifdef KERBEROS
int	 kerberos(char **, char *, char *, char *);
void	 oldw(const char *, ...);
/* XXX from ../../usr.bin/rlogin/krcmd.c */
int krcmd(char **, u_short, char *, char *, int *, char *);
int krcmd_mutual(char **, u_short, char *, char *, int *,
		 char *, CREDENTIALS *, Key_schedule);
#endif
int	 response(void);
void	 rsource(char *, struct stat *);
void	 sink(int, char *[]);
void	 source(int, char *[]);
void	 tolocal(int, char *[]);
void	 toremote(char *, int, char *[]);
int	 do_times(int fd, const struct stat *sb);
void	 usage(void);

int
main(int argc, char *argv[])
{
	struct servent *sp;
	int ch, fflag, tflag;
	char *targ, *shell;

	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch(ch) {			/* User-visible flags. */
		case 'K':
#ifdef KERBEROS
			use_kerberos = 0;
#endif
			break;
#ifdef	KERBEROS
		case 'k':
			dest_realm = dst_realm_buf;
			strlcpy(dst_realm_buf, optarg, sizeof(dst_realm_buf));
			break;
		case 'x':
			doencrypt = 1;
			/* des_set_key(cred.session, schedule); */
			break;
#endif
		case 'p':
			pflag = 1;
			break;
		case 'r':
			iamrecursive = 1;
			break;
						/* Server options. */
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':			/* "from" */
			iamremote = 1;
			fflag = 1;
			break;
		case 't':			/* "to" */
			iamremote = 1;
			tflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#ifdef KERBEROS
	if (use_kerberos) {
		shell = doencrypt ? "ekshell" : "kshell";
		if ((sp = getservbyname(shell, "tcp")) == NULL) {
			use_kerberos = 0;
			oldw("can't get entry for %s/tcp service", shell);
			sp = getservbyname(shell = "shell", "tcp");
		}
	} else
		sp = getservbyname(shell = "shell", "tcp");
#else
	sp = getservbyname(shell = "shell", "tcp");
#endif
	if (sp == NULL)
		errx(1, "%s/tcp: unknown service", shell);
	port = sp->s_port;

	if ((pwd = getpwuid(userid = getuid())) == NULL)
		errx(1, "unknown user %u", userid);
	groupid = pwd->pw_gid;

	unsetenv("RSH");		/* Force the use of /usr/bin/rsh */

	rem = STDIN_FILENO;		/* XXX */

	if (fflag) {			/* Follow "protocol", send data. */
		(void)response();
		(void)setresgid(groupid, groupid, groupid);
		(void)setgroups(1, &groupid);
		(void)setresuid(userid, userid, userid);
		source(argc, argv);
		exit(errs != 0);
	}

	if (tflag) {			/* Receive data. */
		(void)setresgid(groupid, groupid, groupid);
		(void)setgroups(1, &groupid);
		(void)setresuid(userid, userid, userid);
		sink(argc, argv);
		exit(errs != 0);
	}

	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

	rem = -1;
	/* Command to be executed on remote system using "rsh". */
#ifdef	KERBEROS
	(void)snprintf(cmd, sizeof(cmd),
	    "rcp%s%s%s%s", iamrecursive ? " -r" : "",
	    (doencrypt && use_kerberos ? " -x" : ""),
	    pflag ? " -p" : "", targetshouldbedirectory ? " -d" : "");
#else
	(void)snprintf(cmd, sizeof(cmd), "rcp%s%s%s",
	    iamrecursive ? " -r" : "", pflag ? " -p" : "",
	    targetshouldbedirectory ? " -d" : "");
#endif

	(void)signal(SIGPIPE, lostconn);

	if ((targ = colon(argv[argc - 1])))	/* Dest is remote host. */
		toremote(targ, argc, argv);
	else {
		tolocal(argc, argv);		/* Dest is local host. */
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs != 0);
}

void
toremote(char *targ, int argc, char *argv[])
{
	int i, tos;
	char *bp, *host, *src, *suser, *thost, *tuser, *user, *arg;
	arglist alist;

	memset(&alist, '\0', sizeof(alist));
	alist.list = NULL;

	if ((user = strdup(pwd->pw_name)) == NULL)
		err(1, "strdup");

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	arg = strdup(argv[argc - 1]);
	if (!arg)
		err(1, "strdup");
	if ((thost = strchr(arg, '@'))) {
		/* user@host */
		*thost++ = 0;
		tuser = arg;
		if (*tuser == '\0')
			tuser = NULL;
		else if (!okname(tuser))
			exit(1);
	} else {
		thost = arg;
		tuser = NULL;
	}

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {			/* remote to remote */
			freeargs(&alist);
			addargs(&alist, "%s", _PATH_RSH);
			addargs(&alist, "%s", "-n");

			*src++ = 0;
			if (*src == 0)
				src = ".";

			host = strchr(argv[i], '@');
			if (host) {
				*host++ = 0;
				suser = argv[i];
				if (*suser == '\0')
					suser = user;
				else if (!okname(suser))
					continue;

				addargs(&alist, "-l");
				addargs(&alist, "%s", suser);
			} else
				host = argv[1];
			addargs(&alist, "%s", host);
			addargs(&alist, "%s", cmd);
			addargs(&alist, "%s", src);
			addargs(&alist, "%s%s%s:%s",
			    tuser ? tuser : "", tuser ? "@" : "",
			    thost, targ);
			do_local_cmd(&alist, userid, groupid);
		} else {			/* local to remote */
			if (rem == -1) {
				if (asprintf(&bp, "%s -t %s", cmd, targ) == -1)
					err(1, NULL);
				host = thost;
#ifdef KERBEROS
				if (use_kerberos)
					rem = kerberos(&host, bp, user,
					    tuser ? tuser : pwd->pw_name);
				else
#endif
					rem = rcmd(&host, port, user,
					    tuser ? tuser : user, bp, 0);
				if (rem < 0)
					exit(1);
				tos = IPTOS_THROUGHPUT;
				if (setsockopt(rem, IPPROTO_IP, IP_TOS,
				    &tos, sizeof(int)) < 0 &&
				    errno != ENOPROTOOPT)
					warn("TOS (ignored)");
				if (response() < 0)
					exit(1);
				(void)free(bp);
				(void)setresgid(groupid, groupid, groupid);
				(void)setgroups(1, &groupid);
				(void)setresuid(userid, userid, userid);
			}
			source(1, argv+i);
		}
	}
	free(user);
	free(arg);
}

void
tolocal(int argc, char *argv[])
{
	int i, tos;
	char *bp, *host, *src, *suser, *user;
	arglist alist;

	memset(&alist, '\0', sizeof(alist));
	alist.list = NULL;

	if ((user = strdup(pwd->pw_name)) == NULL)
		err(1, "strdup");

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {		/* Local to local. */
			freeargs(&alist);
			addargs(&alist, "%s", _PATH_CP);
			if (iamrecursive)
				addargs(&alist, "-R");
			if (pflag)
				addargs(&alist, "-p");
			addargs(&alist, "%s", argv[i]);
			addargs(&alist, "%s", argv[argc-1]);
			if (do_local_cmd(&alist, userid, groupid))
				++errs;
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		if ((host = strchr(argv[i], '@')) == NULL) {
			host = argv[i];
			suser = user;
		} else {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
				suser = user;
			else if (!okname(suser))
				continue;
		}
		if (asprintf(&bp, "%s -f %s", cmd, src) == -1)
			err(1, NULL);
		rem =
#ifdef KERBEROS
		    use_kerberos ?
			kerberos(&host, bp, user, suser) :
#endif
			rcmd(&host, port, user, suser, bp, 0);
		(void)free(bp);
		if (rem < 0) {
			++errs;
			continue;
		}
		(void)seteuid(userid);
		tos = IPTOS_THROUGHPUT;
		if (setsockopt(rem, IPPROTO_IP, IP_TOS, &tos, sizeof(int)) < 0
		    && errno != ENOPROTOOPT)
			warn("TOS (ignored)");
		sink(1, argv + argc - 1);
		(void)seteuid(0);
		(void)close(rem);
		rem = -1;
	}
	free(user);
}

int
do_times(int fd, const struct stat *sb)
{
	/* strlen(2^64) == 20; strlen(10^6) == 7 */
	char buf[(20 + 7 + 2) * 2 + 2];

	(void)snprintf(buf, sizeof(buf), "T%llu 0 %llu 0\n",
	    (unsigned long long) (sb->st_mtime < 0 ? 0 : sb->st_mtime),
	    (unsigned long long) (sb->st_atime < 0 ? 0 : sb->st_atime));
	(void)write(fd, buf, strlen(buf));
	return (response());
}

void
source(int argc, char *argv[])
{
	struct stat stb;
	static BUF buffer;
	BUF *bp;
	off_t i;
	int amt, fd = -1, haderr, indx, result;
	char *last, *name, buf[BUFSIZ];
	int len;

	for (indx = 0; indx < argc; ++indx) {
		name = argv[indx];
		len = strlen(name);
		while (len > 1 && name[len-1] == '/')
			name[--len] = '\0';
		if (strchr(name, '\n') != NULL) {
			run_err("%s: skipping, filename contains a newline",
			    name);
			goto next;
		}
		if ((fd = open(name, O_RDONLY, 0)) < 0)
			goto syserr;
		if (fstat(fd, &stb)) {
syserr:
			run_err("%s: %s", name, strerror(errno));
			goto next;
		}
		switch (stb.st_mode & S_IFMT) {
		case S_IFREG:
			break;
		case S_IFDIR:
			if (iamrecursive) {
				rsource(name, &stb);
				goto next;
			}
			/* FALLTHROUGH */
		default:
			run_err("%s: not a regular file", name);
			goto next;
		}
		if ((last = strrchr(name, '/')) == NULL)
			last = name;
		else
			++last;
		if (pflag) {
			if (do_times(rem, &stb) < 0)
				goto next;
		}
#define	MODEMASK	(S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
		(void)snprintf(buf, sizeof(buf), "C%04o %qd %s\n",
		    stb.st_mode & MODEMASK, stb.st_size, last);
		(void)write(rem, buf, strlen(buf));
		if (response() < 0)
			goto next;
		if ((bp = allocbuf(&buffer, fd, BUFSIZ)) == NULL) {
next:			if (fd != -1) {
				(void)close(fd);
				fd = -1;
			}
			continue;
		}

		/* Keep writing after an error so that we stay sync'd up. */
		for (haderr = i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (!haderr) {
				result = read(fd, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
			if (haderr)
				(void)write(rem, bp->buf, amt);
			else {
				result = write(rem, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
		}
		if (fd != -1) {
			if (close(fd) && !haderr)
				haderr = errno;
			fd = -1;
		}
		if (!haderr)
			(void)write(rem, "", 1);
		else
			run_err("%s: %s", name, strerror(haderr));
		(void)response();
	}
}

void
rsource(char *name, struct stat *statp)
{
	DIR *dirp;
	struct dirent *dp;
	char *last, *vect[1], path[MAXPATHLEN];

	if (!(dirp = opendir(name))) {
		run_err("%s: %s", name, strerror(errno));
		return;
	}
	last = strrchr(name, '/');
	if (last == NULL)
		last = name;
	else
		last++;
	if (pflag) {
		if (do_times(rem, statp) < 0) {
			closedir(dirp);
			return;
		}
	}
	(void)snprintf(path, sizeof(path),
	    "D%04o %d %s\n", statp->st_mode & MODEMASK, 0, last);
	(void)write(rem, path, strlen(path));
	if (response() < 0) {
		closedir(dirp);
		return;
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= MAXPATHLEN - 1) {
			run_err("%s/%s: name too long", name, dp->d_name);
			continue;
		}
		(void)snprintf(path, sizeof(path), "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	(void)closedir(dirp);
	(void)write(rem, "E\n", 2);
	(void)response();
}

void
sink(int argc, char *argv[])
{
	static BUF buffer;
	struct stat stb;
	struct timeval tv[2];
	enum { YES, NO, DISPLAYED } wrerr;
	BUF *bp;
	off_t i, j, size;
	unsigned long long ull;
	int amt, count, exists, first, mask, mode, ofd, omode;
	int setimes, targisdir, wrerrno = 0;
	char ch, *cp, *np, *targ, *why, *vect[1], buf[BUFSIZ];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		(void)umask(mask);
	if (argc != 1) {
		run_err("ambiguous target");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	(void)write(rem, "", 1);
	if (stat(targ, &stb) == 0 && S_ISDIR(stb.st_mode))
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;
		if (read(rem, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[BUFSIZ - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void)write(STDERR_FILENO,
				    buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			++errs;
			continue;
		}
		if (buf[0] == 'E') {
			(void)write(rem, "", 1);
			return;
		}

		if (ch == '\n')
			*--cp = 0;

		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			if (!isdigit((unsigned char)*cp))
				SCREWUP("mtime.sec not present");
			ull = strtoull(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			if ((time_t)ull < 0 || (time_t)ull != ull)
				setimes = 0;	/* out of range */
			mtime.tv_sec = ull;
			mtime.tv_usec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != ' ' || mtime.tv_usec < 0 ||
			    mtime.tv_usec > 999999)
				SCREWUP("mtime.usec not delimited");
			if (!isdigit((unsigned char)*cp))
				SCREWUP("atime.sec not present");
			ull = strtoull(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			if ((time_t)ull < 0 || (time_t)ull != ull)
				setimes = 0;	/* out of range */
			atime.tv_sec = ull;
			atime.tv_usec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != '\0' || atime.tv_usec < 0 ||
			    atime.tv_usec > 999999)
				SCREWUP("atime.usec not delimited");
			(void)write(rem, "", 1);
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				run_err("%s", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");

		for (size = 0; isdigit((unsigned char)*cp);)
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if ((strchr(cp, '/') != NULL) || (strcmp(cp, "..") == 0)) {
			run_err("error: unexpected filename: %s", cp);
			exit(1);
		}
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			size_t need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (!(namebuf = malloc(need)))
					run_err("%s", strerror(errno));
			}
			(void)snprintf(namebuf, need, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
			np = namebuf;
		} else
			np = targ;
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			int mod_flag = pflag;
			if (!iamrecursive)
				SCREWUP("received directory without -r");
			if (exists) {
				if (!S_ISDIR(stb.st_mode)) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void)chmod(np, mode);
			} else {
				/* Handle copying from a read-only directory */
				mod_flag = 1;
				if (mkdir(np, mode | S_IRWXU) < 0)
					goto bad;
			}
			vect[0] = np;
			sink(1, vect);
			if (setimes) {
				setimes = 0;
				if (utimes(np, tv) < 0)
				    run_err("%s: set times: %s",
					np, strerror(errno));
			}
			if (mod_flag)
				(void)chmod(np, mode);
			continue;
		}
		omode = mode;
		mode |= S_IWUSR;
		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:			run_err("%s: %s", np, strerror(errno));
			continue;
		}
		(void)write(rem, "", 1);
		if ((bp = allocbuf(&buffer, ofd, BUFSIZ)) == NULL) {
			(void)close(ofd);
			continue;
		}
		cp = bp->buf;
		wrerr = NO;
		for (count = i = 0; i < size; i += BUFSIZ) {
			amt = BUFSIZ;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = read(rem, cp, amt);
				if (j <= 0) {
					run_err("%s", j ? strerror(errno) :
					    "dropped connection");
					exit(1);
				}
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				/* Keep reading so we stay sync'd up. */
				if (wrerr == NO) {
					j = write(ofd, bp->buf, count);
					if (j != count) {
						wrerr = YES;
						wrerrno = j >= 0 ? EIO : errno;
					}
				}
				count = 0;
				cp = bp->buf;
			}
		}
		if (count != 0 && wrerr == NO &&
		    (j = write(ofd, bp->buf, count)) != count) {
			wrerr = YES;
			wrerrno = j >= 0 ? EIO : errno;
		}
		if (wrerr == NO && ftruncate(ofd, size) != 0) {
			run_err("%s: truncate: %s", np, strerror(errno));
			wrerr = DISPLAYED;
		}
		if (pflag) {
			if (exists || omode != mode)
				if (fchmod(ofd, omode)) {
					run_err("%s: set mode: %s",
					    np, strerror(errno));
					wrerr = DISPLAYED;
				}
		} else {
			if (!exists && omode != mode)
				if (fchmod(ofd, omode & ~mask)) {
					run_err("%s: set mode: %s",
					    np, strerror(errno));
					wrerr = DISPLAYED;
				}
		}
		(void)close(ofd);
		(void)response();
		if (setimes && wrerr == NO) {
			setimes = 0;
			if (utimes(np, tv) < 0) {
				run_err("%s: set times: %s",
				    np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
		switch(wrerr) {
		case YES:
			run_err("%s: %s", np, strerror(wrerrno));
			break;
		case NO:
			(void)write(rem, "", 1);
			break;
		case DISPLAYED:
			break;
		}
	}
screwup:
	run_err("protocol error: %s", why);
	exit(1);
}

#ifdef KERBEROS
int
kerberos(char **host, char *bp, char *locuser, char *user)
{
	struct servent *sp;

again:
	if (use_kerberos) {
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(*host);
		rem =
		    doencrypt ?
			krcmd_mutual(host,
			    port, user, bp, 0, dest_realm, &cred, schedule) :
			krcmd(host, port, user, bp, 0, dest_realm);

		if (rem < 0) {
			use_kerberos = 0;
			if ((sp = getservbyname("shell", "tcp")) == NULL)
				errx(1, "unknown service shell/tcp");
			if (errno == ECONNREFUSED)
			    oldw("remote host doesn't support Kerberos");
			else if (errno == ENOENT)
			    oldw("can't provide Kerberos authentication data");
			port = sp->s_port;
			goto again;
		}
	} else {
		if (doencrypt)
			errx(1,
			   "the -x option requires Kerberos authentication");
		rem = rcmd(host, port, locuser, user, bp, 0);
	}
	return (rem);
}
#endif /* KERBEROS */

int
response(void)
{
	char ch, *cp, resp, rbuf[BUFSIZ];

	if (read(rem, &resp, sizeof(resp)) != sizeof(resp))
		lostconn(0);

	cp = rbuf;
	switch(resp) {
	case 0:				/* ok */
		return (0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:				/* error, followed by error msg */
	case 2:				/* fatal error, "" */
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				lostconn(0);
			*cp++ = ch;
		} while (cp < &rbuf[BUFSIZ] && ch != '\n');

		if (!iamremote)
			(void)write(STDERR_FILENO, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return (-1);
		exit(1);
	}
	/* NOTREACHED */
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-p] file1 file2\n"
	    "       %s [-pr] file ... directory\n",
	    __progname, __progname);
	exit(1);
}

#include <stdarg.h>

#ifdef KERBEROS
void
oldw(const char *fmt, ...)
{
	char realm[REALM_SZ];
	va_list ap;

	if (krb_get_lrealm(realm, 1) != KSUCCESS)
		return;
	va_start(ap, fmt);
	(void)fprintf(stderr, "rcp: ");
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, ", using standard rcp\n");
	va_end(ap);
}
#endif

void
run_err(const char *fmt, ...)
{
	static FILE *fp;
	va_list ap;

	++errs;
	if (fp == NULL && !(fp = fdopen(rem, "w")))
		return;
	(void)fprintf(fp, "%c", 0x01);
	(void)fprintf(fp, "rcp: ");
	va_start(ap, fmt);
	(void)vfprintf(fp, fmt, ap);
	va_end(ap);
	(void)fprintf(fp, "\n");
	(void)fflush(fp);

	if (!iamremote) {
		va_start(ap, fmt);
		vwarnx(fmt, ap);
		va_end(ap);
	}
}
