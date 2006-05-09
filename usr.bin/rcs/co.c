/*	$OpenBSD: co.c,v 1.88 2006/05/05 01:29:59 ray Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include "rcsprog.h"

#define CO_OPTSTRING	"d:f::I::k:l::M::p::q::r::s:Tu::Vw::x::z::"

static void	checkout_err_nobranch(RCSFILE *, const char *, const char *,
    const char *, int);

int
checkout_main(int argc, char **argv)
{
	int fd, i, ch, flags, kflag, status, warg;
	RCSNUM *rev;
	RCSFILE *file;
	char fpath[MAXPATHLEN];
	char *author, *date, *rev_str, *username, *state;
	time_t rcs_mtime = -1;

	warg = flags = status = 0;
	kflag = RCS_KWEXP_ERR;
	rev = RCS_HEAD_REV;
	rev_str = NULL;
	state = NULL;
	author = NULL;
	date = NULL;

	while ((ch = rcs_getopt(argc, argv, CO_OPTSTRING)) != -1) {
		switch (ch) {
		case 'd':
			date = xstrdup(rcs_optarg);
			break;
		case 'f':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= FORCE;
			break;
		case 'I':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= INTERACTIVE;
			break;

		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				warnx("invalid RCS keyword substitution mode");
				(usage)();
				exit(1);
			}
			break;
		case 'l':
			if (flags & CO_UNLOCK) {
				warnx("warning: -u overridden by -l");
				flags &= ~CO_UNLOCK;
			}
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= CO_LOCK;
			break;
		case 'M':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= CO_REVDATE;
			break;
		case 'p':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= PIPEOUT;
			break;
		case 'q':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= QUIET;
			break;
		case 'r':
			rcs_setrevstr(&rev_str, rcs_optarg);
			break;
		case 's':
			state = xstrdup(rcs_optarg);
			flags |= CO_STATE;
			break;
		case 'T':
			flags |= PRESERVETIME;
			break;
		case 'u':
			rcs_setrevstr(&rev_str, rcs_optarg);
			if (flags & CO_LOCK) {
				warnx("warning: -l overridden by -u");
				flags &= ~CO_LOCK;
			}
			flags |= CO_UNLOCK;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'w':
			/* if no argument, assume current user */
			if (rcs_optarg == NULL) {
				if ((author = getlogin()) == NULL)
					err(1, "getlogin");
			} else {
				author = xstrdup(rcs_optarg);
				warg = 1;
			}
			flags |= CO_AUTHOR;
			break;
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		warnx("no input file");
		(usage)();
		exit (1);
	}

	if ((username = getlogin()) == NULL)
		err(1, "getlogin");

	for (i = 0; i < argc; i++) {
		fd = rcs_statfile(argv[i], fpath, sizeof(fpath), flags);
		if (fd < 0)
			continue;

		if (!(flags & QUIET))
			printf("%s  -->  %s\n", fpath,
			    (flags & PIPEOUT) ? "standard output" : argv[i]);

		if ((flags & CO_LOCK) && (kflag & RCS_KWEXP_VAL)) {
			warnx("%s: cannot combine -kv and -l", fpath);
			(void)close(fd);
			continue;
		}

		if ((file = rcs_open(fpath, fd,
		    RCS_RDWR|RCS_PARSE_FULLY)) == NULL)
			continue;

		if (flags & PRESERVETIME)
			rcs_mtime = rcs_get_mtime(file);

		rcs_kwexp_set(file, kflag);

		if (rev_str != NULL) {
			if ((rev = rcs_getrevnum(rev_str, file)) == NULL)
				errx(1, "invalid revision: %s", rev_str);
		} else {
			/* no revisions in RCS file, generate empty 0.0 */
			if (file->rf_ndelta == 0) {
				rev = rcsnum_parse("0.0");
				if (rev == NULL)
					errx(1, "failed to generate rev 0.0");
			} else {
				rev = rcsnum_alloc();
				rcsnum_cpy(file->rf_head, rev, 0);
			}
		}

		if ((status = checkout_rev(file, rev, argv[i], flags,
		    username, author, state, date)) < 0) {
			rcs_close(file);
			rcsnum_free(rev);
			continue;
		}

		if (!(flags & QUIET))
			printf("done\n");

		rcsnum_free(rev);

		rcs_write(file);
		if (flags & PRESERVETIME)
			rcs_set_mtime(file, rcs_mtime);
		rcs_close(file);
	}

	if (author != NULL && warg)
		xfree(author);

	if (date != NULL)
		xfree(date);

	if (state != NULL)
		xfree(state);

	return (status);
}

void
checkout_usage(void)
{
	fprintf(stderr,
	    "usage: co [-TV] [-ddate] [-f[rev]] [-I[rev]] [-kmode] [-l[rev]]\n"
	    "          [-M[rev]] [-p[rev]] [-q[rev]] [-r[rev]] [-sstate]\n"
	    "          [-u[rev]] [-w[user]] [-xsuffixes] [-ztz] file ...\n");
}

/*
 * Checkout revision <rev> from RCSFILE <file>, writing it to the path <dst>
 * Currently recognised <flags> are CO_LOCK, CO_UNLOCK and CO_REVDATE.
 *
 * Looks up revision based upon <lockname>, <author>, <state> and <date>
 *
 * Returns 0 on success, -1 on failure.
 */
int
checkout_rev(RCSFILE *file, RCSNUM *frev, const char *dst, int flags,
    const char *lockname, const char *author, const char *state,
    const char *date)
{
	BUF *bp;
	u_int i;
	int fd, lcount;
	char buf[16];
	mode_t mode = 0444;
	struct stat st;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;
	char *content, msg[128], *fdate;
	time_t rcsdate, givendate;
	RCSNUM *rev;

	rcsdate = givendate = -1;
	if (date != NULL)
		givendate = rcs_date_parse(date);

	if (file->rf_ndelta == 0)
		printf("no revisions present; generating empty revision 0.0\n");

	/* XXX rcsnum_cmp()
	 * Check out the latest revision if <frev> is greater than HEAD
	 */
	if (file->rf_ndelta != 0) {
		for (i = 0; i < file->rf_head->rn_len; i++) {
			if (file->rf_head->rn_id[i] < frev->rn_id[i]) {
				frev = file->rf_head;
				break;
			}
		}
	}

	lcount = 0;
	TAILQ_FOREACH(lkp, &(file->rf_locks), rl_list) {
		if (!strcmp(lkp->rl_name, lockname))
			lcount++;
	}

	/*
	 * If the user didn't specify any revision, we cycle through
	 * revisions to lookup the first one that matches what he specified.
	 *
	 * If we cannot find one, we return an error.
	 */
	rdp = NULL;
	if (file->rf_ndelta != 0 && frev == file->rf_head) {
		if (lcount > 1) {
			warnx("multiple revisions locked by %s; "
			    "please specify one", lockname);
			return (-1);
		}

		TAILQ_FOREACH(rdp, &file->rf_delta, rd_list) {
			if (date != NULL) {
				fdate = asctime(&rdp->rd_date);
				rcsdate = rcs_date_parse(fdate);
				if (givendate <= rcsdate)
					continue;
			}

			if (author != NULL &&
			    strcmp(rdp->rd_author, author))
				continue;

			if (state != NULL &&
			    strcmp(rdp->rd_state, state))
				continue;

			frev = rdp->rd_num;
			break;
		}
	} else if (file->rf_ndelta != 0) {
		rdp = rcs_findrev(file, frev);
	}

	if (file->rf_ndelta != 0 && rdp == NULL) {
		checkout_err_nobranch(file, author, date, state, flags);
		return (-1);
	}

	if (file->rf_ndelta == 0)
		rev = frev;
	else
		rev = rdp->rd_num;

	rcsnum_tostr(rev, buf, sizeof(buf));

	if (file->rf_ndelta != 0 && rdp->rd_locker != NULL) {
		if (strcmp(lockname, rdp->rd_locker)) {
			if (strlcpy(msg, "Revision %s is already locked by %s; ",
			    sizeof(msg)) >= sizeof(msg))
				errx(1, "msg too long");

			if (flags & CO_UNLOCK) {
				if (strlcat(msg, "use co -r or rcs -u",
				    sizeof(msg)) >= sizeof(msg))
					errx(1, "msg too long");
			}

			warnx(msg, buf, rdp->rd_locker);
			return (-1);
		}
	}

	if (!(flags & QUIET) && !(flags & NEWFILE) &&
	    !(flags & CO_REVERT) && file->rf_ndelta != 0)
		printf("revision %s", buf);

	if (file->rf_ndelta != 0) {
		if ((bp = rcs_getrev(file, rev)) == NULL) {
			warnx("cannot find revision `%s'", buf);
			return (-1);
		}
	} else {
		bp = rcs_buf_alloc(1, 0);
	}

	/*
	 * Do keyword expansion if required.
	 */
	if (file->rf_ndelta != 0)
		bp = rcs_kwexp_buf(bp, file, rev);

	/*
	 * File inherits permissions from its ,v file
	 */
	if (file->fd != -1) {
		if (fstat(file->fd, &st) == -1)
			err(1, "%s", file->rf_path);
		mode = st.st_mode;
	}

	if (flags & CO_LOCK) {
		if (file->rf_ndelta != 0) {
			if (lockname != NULL &&
			    rcs_lock_add(file, lockname, rev) < 0) {
				if (rcs_errno != RCS_ERR_DUPENT)
					return (-1);
			}
		}

		/* Strip all write bits from mode */
		if (file->fd != -1) {
			mode = st.st_mode &
			    (S_IXUSR|S_IXGRP|S_IXOTH|S_IRUSR|S_IRGRP|S_IROTH);
		}

		mode |= S_IWUSR;

		if (file->rf_ndelta != 0) {
			if (!(flags & QUIET) && !(flags & NEWFILE) &&
			    !(flags & CO_REVERT))
				printf(" (locked)");
		}
	} else if (flags & CO_UNLOCK) {
		if (file->rf_ndelta != 0) {
			if (rcs_lock_remove(file, lockname, rev) < 0) {
				if (rcs_errno != RCS_ERR_NOENT)
					return (-1);
			}
		}

		/* Strip all write bits from mode */
		if (file->fd != -1) {
			mode = st.st_mode &
			    (S_IXUSR|S_IXGRP|S_IXOTH|S_IRUSR|S_IRGRP|S_IROTH);
		}

		if (file->rf_ndelta != 0) {
			if (!(flags & QUIET) && !(flags & NEWFILE) &&
			    !(flags & CO_REVERT))
				printf(" (unlocked)");
		}
	}

	if (file->rf_ndelta == 0 &&
	    ((flags & CO_LOCK) || (flags & CO_UNLOCK))) {
		warnx("no revisions, so nothing can be %s",
		    (flags & CO_LOCK) ? "locked" : "unlocked");
	} else if (file->rf_ndelta != 0) {
		/* XXX - Not a good way to detect if a newline is needed. */
		if (!(flags & QUIET) && !(flags & NEWFILE) &&
		    !(flags & CO_REVERT))
			printf("\n");
	}

	if (flags & CO_LOCK) {
		if (rcs_errno != RCS_ERR_DUPENT)
			lcount++;
		if (!(flags & QUIET) && lcount > 1 && !(flags & CO_REVERT))
			warnx("%s: warning: You now have %d locks.",
			    file->rf_path, lcount);
	}

	if (!(flags & PIPEOUT) && stat(dst, &st) != -1 && !(flags & FORCE)) {
		/*
		 * XXX - Not sure what is "right".  If we go according
		 * to GNU's behavior, an existing file with no writable
		 * bits is overwritten without prompting the user.
		 *
		 * This is dangerous, so we always prompt.
		 * Unfortunately this interferes with an unlocked
		 * checkout followed by a locked checkout, which should
		 * not prompt.  One (unimplemented) solution is to check
		 * if the existing file is the same as the checked out
		 * revision, and prompt if there are differences.
		 */
		if (st.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH))
			printf("writable ");
		printf("%s exists%s; ", dst,
		    (getuid() == st.st_uid) ? "" :
		    ", and you do not own it");
		printf("remove it? [ny](n): ");
		/* default is n */
		if (rcs_yesno() == -1) {
			if (!(flags & QUIET) && isatty(STDIN_FILENO))
				warnx("writable %s exists; "
				    "checkout aborted", dst);
			else
				warnx("checkout aborted");
			return (-1);
		}
	}

	if (flags & PIPEOUT) {
		rcs_buf_putc(bp, '\0');
		content = rcs_buf_release(bp);
		printf("%s", content);
		xfree(content);
	} else {
		(void)unlink(dst);

		if ((fd = open(dst, O_WRONLY|O_CREAT|O_TRUNC, mode)) < 0)
			err(1, "%s", dst);

		if (rcs_buf_write_fd(bp, fd) < 0) {
			warnx("failed to write revision to file");
			rcs_buf_free(bp);
			(void)close(fd);
			return (-1);
		}

		if (fchmod(fd, mode) == -1)
			warn("%s", dst);

		rcs_buf_free(bp);

		if (flags & CO_REVDATE) {
			struct timeval tv[2];
			memset(&tv, 0, sizeof(tv));
			tv[0].tv_sec = (long)rcs_rev_getdate(file, rev);
			tv[1].tv_sec = tv[0].tv_sec;
			if (futimes(fd, (const struct timeval *)&tv) < 0)
				warn("utimes");
		}

		(void)close(fd);
	}

	return (0);
}

/*
 * checkout_err_nobranch()
 *
 * XXX - should handle the dates too.
 */
static void
checkout_err_nobranch(RCSFILE *file, const char *author, const char *date,
    const char *state, int flags)
{
	if (!(flags & CO_AUTHOR))
		author = NULL;
	if (!(flags & CO_STATE))
		state = NULL;

	warnx("%s: No revision on branch has%s%s%s%s%s%s.",
	    file->rf_path,
	    date ? " a date before " : "",
	    date ? date : "",
	    author ? " and author " + (date ? 0:4 ) : "",
	    author ? author : "",
	    state  ? " and state " + (date || author ? 0:4) : "",
	    state  ? state : "");
}
