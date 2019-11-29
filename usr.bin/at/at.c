/*	$OpenBSD: at.c,v 1.82 2019/06/28 13:35:00 deraadt Exp $	*/

/*
 *  at.c : Put file into atrun queue
 *  Copyright (C) 1993, 1994  Thomas Koenig
 *
 *  Atrun & Atq modifications
 *  Copyright (C) 1993  David Parsons
 *
 *  Traditional BSD behavior and other significant modifications
 *  Copyright (C) 2002-2003  Todd C. Miller
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <bitstring.h>                  /* for structs.h */
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "pathnames.h"
#include "macros.h"
#include "structs.h"
#include "funcs.h"
#include "globals.h"

#include "at.h"

#define ALARMC 10		/* Number of seconds to wait for timeout */
#define TIMESIZE 50		/* Size of buffer passed to strftime() */

/* Variables to remove from the job's environment. */
char *no_export[] =
{
	"TERM", "TERMCAP", "DISPLAY", "_", "SHELLOPTS", "BASH_VERSINFO",
	"EUID", "GROUPS", "PPID", "UID", "SSH_AUTH_SOCK", "SSH_AGENT_PID",
};

static int program = AT;	/* default program mode */
static char atfile[PATH_MAX];	/* path to the at spool file */
static char user_name[MAX_UNAME];/* invoking user name */
static int fcreated;		/* whether or not we created the file yet */
static char atqueue = 0;	/* which queue to examine for jobs (atq) */
static char vflag = 0;		/* show completed but unremoved jobs (atq) */
static char force = 0;		/* suppress errors (atrm) */
static char interactive = 0;	/* interactive mode (atrm) */
static int send_mail = 0;	/* whether we are sending mail */
static uid_t user_uid;		/* user's real uid */
static gid_t user_gid;		/* user's real gid */
static gid_t spool_gid;		/* gid for writing to at spool */

static void sigc(int);
static void writefile(const char *, time_t, char);
static void list_jobs(int, char **, int, int);
static time_t ttime(char *);
static __dead void fatal(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)));
static __dead void fatalx(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)));
static __dead void usage(void);
static int rmok(long long);
time_t parsetime(int, char **);

/*
 * Something fatal has happened, print error message and exit.
 */
static __dead void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);

	if (fcreated)
		unlink(atfile);

	exit(EXIT_FAILURE);
}

/*
 * Something fatal has happened, print error message and exit.
 */
static __dead void
fatalx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);

	if (fcreated)
		unlink(atfile);

	exit(EXIT_FAILURE);
}

/* ARGSUSED */
static void
sigc(int signo)
{
	/* If the user presses ^C, remove the spool file and exit. */
	if (fcreated)
		(void)unlink(atfile);

	_exit(EXIT_FAILURE);
}

static int
strtot(const char *nptr, char **endptr, time_t *tp)
{
	long long ll;

	errno = 0;
	ll = strtoll(nptr, endptr, 10);
	if (*endptr == nptr)
		return (-1);
	if (ll < 0 || (errno == ERANGE && ll == LLONG_MAX) || (time_t)ll != ll)
		return (-1);
	*tp = (time_t)ll;
	return (0);
}

static int
newjob(time_t runtimer, int queue)
{
	int fd, i;

	/*
	 * If we have a collision, try shifting the time by up to
	 * two minutes.  Perhaps it would be better to try different
	 * queues instead...
	 */
	for (i = 0; i < 120; i++) {
		snprintf(atfile, sizeof(atfile), "%s/%lld.%c", _PATH_AT_SPOOL,
		    (long long)runtimer, queue);
		fd = open(atfile, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR);
		if (fd >= 0)
			return (fd);
		runtimer++;
	}
	return (-1);
}

/*
 * This does most of the work if at or batch are invoked for
 * writing a job.
 */
static void
writefile(const char *cwd, time_t runtimer, char queue)
{
	const char *ap;
	char *mailname, *shell;
	char timestr[TIMESIZE];
	struct passwd *pass_entry;
	struct tm runtime;
	int fd;
	FILE *fp;
	struct sigaction act;
	char **atenv;
	int ch;
	mode_t cmask;
	extern char **environ;

	/*
	 * Install the signal handler for SIGINT; terminate after removing the
	 * spool file if necessary
	 */
	bzero(&act, sizeof act);
	act.sa_handler = sigc;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	/*
	 * Create the file. The x bit is only going to be set after it has
	 * been completely written out, to make sure it is not executed in
	 * the meantime.  To make sure they do not get deleted, turn off
	 * their r bit.  Yes, this is a kluge.
	 */
	cmask = umask(S_IRUSR | S_IWUSR | S_IXUSR);
	if ((fd = newjob(runtimer, queue)) == -1)
		fatal("unable to create atjob file");

	/*
	 * We've successfully created the file; let's set the flag so it
	 * gets removed in case of an interrupt or error.
	 */
	fcreated = 1;

	if ((fp = fdopen(fd, "w")) == NULL)
		fatal("unable to reopen atjob file");

	/*
	 * Get the userid to mail to, first by trying getlogin(), which asks
	 * the kernel, then from $LOGNAME or $USER, finally from getpwuid().
	 */
	mailname = getlogin();
	if (mailname == NULL && (mailname = getenv("LOGNAME")) == NULL)
		mailname = getenv("USER");

	if ((mailname == NULL) || (mailname[0] == '\0') ||
	    (strlen(mailname) > MAX_UNAME) || (getpwnam(mailname) == NULL)) {
		mailname = user_name;
	}

	/*
	 * Get the shell to run the job under.  First check $SHELL, falling
	 * back to the user's shell in the password database or, failing
	 * that, /bin/sh.
	 */
	if ((shell = getenv("SHELL")) == NULL || *shell == '\0') {
		pass_entry = getpwuid(user_uid);
		if (pass_entry != NULL && *pass_entry->pw_shell != '\0')
			shell = pass_entry->pw_shell;
		else
			shell = _PATH_BSHELL;
	}

	(void)fprintf(fp, "#!/bin/sh\n# atrun uid=%lu gid=%lu\n# mail %*s %d\n",
	    (unsigned long)user_uid, (unsigned long)spool_gid,
	    MAX_UNAME, mailname, send_mail);

	/* Write out the umask at the time of invocation */
	(void)fprintf(fp, "umask %o\n", cmask);

	/*
	 * Write out the environment. Anything that may look like a special
	 * character to the shell is quoted, except for \n, which is done
	 * with a pair of "'s.  Don't export the no_export list (such as
	 * TERM or DISPLAY) because we don't want these.
	 */
	for (atenv = environ; *atenv != NULL; atenv++) {
		int export = 1;
		char *eqp;

		eqp = strchr(*atenv, '=');
		if (eqp == NULL)
			eqp = *atenv;
		else {
			int i;

			for (i = 0;i < sizeof(no_export) /
			    sizeof(no_export[0]); i++) {
				export = export
				    && (strncmp(*atenv, no_export[i],
					(size_t) (eqp - *atenv)) != 0);
			}
			eqp++;
		}

		if (export) {
			(void)fputs("export ", fp);
			(void)fwrite(*atenv, sizeof(char), eqp - *atenv, fp);
			for (ap = eqp; *ap != '\0'; ap++) {
				if (*ap == '\n')
					(void)fprintf(fp, "\"\n\"");
				else {
					if (!isalnum((unsigned char)*ap)) {
						switch (*ap) {
						case '%': case '/': case '{':
						case '[': case ']': case '=':
						case '}': case '@': case '+':
						case '#': case ',': case '.':
						case ':': case '-': case '_':
							break;
						default:
							(void)fputc('\\', fp);
							break;
						}
					}
					(void)fputc(*ap, fp);
				}
			}
			(void)fputc('\n', fp);
		}
	}
	/*
	 * Cd to the directory at the time and write out all the
	 * commands the user supplies from stdin.
	 */
	(void)fputs("cd ", fp);
	for (ap = cwd; *ap != '\0'; ap++) {
		if (*ap == '\n')
			fprintf(fp, "\"\n\"");
		else {
			if (*ap != '/' && !isalnum((unsigned char)*ap))
				(void)fputc('\\', fp);

			(void)fputc(*ap, fp);
		}
	}
	/*
	 * Test cd's exit status: die if the original directory has been
	 * removed, become unreadable or whatever.
	 */
	(void)fprintf(fp, " || {\n\t echo 'Execution directory inaccessible'"
	    " >&2\n\t exit 1\n}\n");

	if ((ch = getchar()) == EOF)
		fatalx("unexpected EOF");

	/* We want the job to run under the user's shell. */
	fprintf(fp, "%s << '_END_OF_AT_JOB'\n", shell);

	do {
		(void)fputc(ch, fp);
	} while ((ch = getchar()) != EOF);

	(void)fprintf(fp, "\n_END_OF_AT_JOB\n");
	(void)fflush(fp);
	if (ferror(fp))
		fatalx("write error");

	if (ferror(stdin))
		fatalx("read error");

	/*
	 * Set the x bit so that we're ready to start executing
	 */
	if (fchmod(fileno(fp), S_IRUSR | S_IWUSR | S_IXUSR) == -1)
		fatal("fchmod");

	(void)fclose(fp);

	/* Poke cron so it knows to reload the at spool. */
	poke_daemon(RELOAD_AT);

	runtime = *localtime(&runtimer);
	strftime(timestr, TIMESIZE, "%a %b %e %T %Y", &runtime);
	(void)fprintf(stderr, "commands will be executed using %s\n", shell);
	(void)fprintf(stderr, "job %s at %s\n", &atfile[sizeof(_PATH_AT_SPOOL)],
	    timestr);

	syslog(LOG_INFO, "(%s) CREATE (%s)", user_name,
	    &atfile[sizeof(_PATH_AT_SPOOL)]);
}

/* Sort by creation time. */
static int
byctime(const void *v1, const void *v2)
{
	const struct atjob *j1 = *(const struct atjob **)v1;
	const struct atjob *j2 = *(const struct atjob **)v2;

	return (j1->ctime < j2->ctime) ? -1 : (j1->ctime > j2->ctime);
}

/* Sort by job number (and thus execution time). */
static int
byjobno(const void *v1, const void *v2)
{
	const struct atjob *j1 = *(struct atjob **)v1;
	const struct atjob *j2 = *(struct atjob **)v2;

	if (j1->runtimer == j2->runtimer)
		return (j1->queue - j2->queue);
	return (j1->runtimer - j2->runtimer);
}

static void
print_job(struct atjob *job, int n, int shortformat)
{
	struct passwd *pw;
	struct tm runtime;
	char timestr[TIMESIZE];
	static char *ranks[] = {
		"th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
	};

	runtime = *localtime(&job->runtimer);
	if (shortformat) {
		strftime(timestr, TIMESIZE, "%a %b %e %T %Y", &runtime);
		(void)printf("%lld.%c\t%s\n", (long long)job->runtimer,
		    job->queue, timestr);
	} else {
		pw = getpwuid(job->uid);
		/* Rank hack shamelessly stolen from lpq */
		if (n / 10 == 1)
			printf("%3d%-5s", n,"th");
		else
			printf("%3d%-5s", n, ranks[n % 10]);
		strftime(timestr, TIMESIZE, "%b %e, %Y %R", &runtime);
		(void)printf("%-21.18s%-11.8s%10lld.%c   %c%s\n",
		    timestr, pw ? pw->pw_name : "???",
		    (long long)job->runtimer, job->queue, job->queue,
		    (S_IXUSR & job->mode) ? "" : " (done)");
	}
}

/*
 * List all of a user's jobs in the queue, by looping through
 * _PATH_AT_SPOOL, or all jobs if we are root.  If argc is > 0, argv
 * contains the list of users whose jobs shall be displayed. By
 * default, the list is sorted by execution date and queue.  If
 * csort is non-zero jobs will be sorted by creation/submission date.
 */
static void
list_jobs(int argc, char **argv, int count_only, int csort)
{
	struct passwd *pw;
	struct dirent *dirent;
	struct atjob **atjobs, **newatjobs, *job;
	struct stat stbuf;
	time_t runtimer;
	char **jobs;
	uid_t *uids;
	char queue, *ep;
	DIR *spool;
	int job_matches, jobs_len, uids_len;
	int dfd, i, shortformat;
	size_t numjobs, maxjobs;

	syslog(LOG_INFO, "(%s) LIST (%s)", user_name,
	    user_uid ? user_name : "ALL");

	/* Convert argv into a list of jobs and uids. */
	jobs = NULL;
	uids = NULL;
	jobs_len = uids_len = 0;

	if (argc) {
		if ((jobs = reallocarray(NULL, argc, sizeof(char *))) == NULL ||
		    (uids = reallocarray(NULL, argc, sizeof(uid_t))) == NULL)
			fatal(NULL);

		for (i = 0; i < argc; i++) {
			if (strtot(argv[i], &ep, &runtimer) == 0 &&
			    *ep == '.' && isalpha((unsigned char)*(ep + 1)) &&
			    *(ep + 2) == '\0')
				jobs[jobs_len++] = argv[i];
			else if ((pw = getpwnam(argv[i])) != NULL) {
				if (pw->pw_uid != user_uid && user_uid != 0)
					fatalx("only the superuser may "
					    "display other users' jobs");
				uids[uids_len++] = pw->pw_uid;
			} else
				fatalx("unknown user %s", argv[i]);
		}
	}

	shortformat = strcmp(__progname, "at") == 0;

	if ((dfd = open(_PATH_AT_SPOOL, O_RDONLY|O_DIRECTORY)) == -1 ||
	    (spool = fdopendir(dfd)) == NULL)
		fatal(_PATH_AT_SPOOL);

	if (fstat(dfd, &stbuf) != 0)
		fatal(_PATH_AT_SPOOL);

	/*
	 * The directory's link count should give us a good idea
	 * of how many files are in it.  Fudge things a little just
	 * in case someone adds a job or two.
	 */
	numjobs = 0;
	maxjobs = stbuf.st_nlink + 4;
	atjobs = reallocarray(NULL, maxjobs, sizeof(struct atjob *));
	if (atjobs == NULL)
		fatal(NULL);

	/* Loop over every file in the directory. */
	while ((dirent = readdir(spool)) != NULL) {
		if (fstatat(dfd, dirent->d_name, &stbuf, AT_SYMLINK_NOFOLLOW) != 0)
			fatal("%s", dirent->d_name);

		/*
		 * See it's a regular file and has its x bit turned on and
		 * is the user's
		 */
		if (!S_ISREG(stbuf.st_mode)
		    || ((stbuf.st_uid != user_uid) && !(user_uid == 0))
		    || !(S_IXUSR & stbuf.st_mode || vflag))
			continue;

		if (strtot(dirent->d_name, &ep, &runtimer) == -1)
			continue;
		if (*ep != '.' || !isalpha((unsigned char)*(ep + 1)) ||
		    *(ep + 2) != '\0')
			continue;
		queue = *(ep + 1);

		if (atqueue && (queue != atqueue))
			continue;

		/* Check against specified jobs and/or user(s). */
		job_matches = (argc == 0) ? 1 : 0;
		if (!job_matches) {
			for (i = 0; i < jobs_len; i++) {
				if (strcmp(dirent->d_name, jobs[i]) == 0) {
					job_matches = 1;
					break;
				}
			}
		}
		if (!job_matches) {
			for (i = 0; i < uids_len; i++) {
				if (uids[i] == stbuf.st_uid) {
					job_matches = 1;
					break;
				}
			}
		}
		if (!job_matches)
			continue;

		if (count_only) {
			numjobs++;
			continue;
		}

		job = malloc(sizeof(struct atjob));
		if (job == NULL)
			fatal(NULL);
		job->runtimer = runtimer;
		job->ctime = stbuf.st_ctime;
		job->uid = stbuf.st_uid;
		job->mode = stbuf.st_mode;
		job->queue = queue;
		if (numjobs == maxjobs) {
			size_t newjobs = maxjobs * 2;
			newatjobs = recallocarray(atjobs, maxjobs,
			    newjobs, sizeof(job));
			if (newatjobs == NULL)
				fatal(NULL);
			atjobs = newatjobs;
			maxjobs = newjobs;
		}
		atjobs[numjobs++] = job;
	}
	free(uids);
	closedir(spool);

	if (count_only || numjobs == 0) {
		if (numjobs == 0 && !shortformat)
			warnx("no files in queue");
		else if (count_only)
			printf("%zu\n", numjobs);
		free(atjobs);
		return;
	}

	/* Sort by job run time or by job creation time. */
	qsort(atjobs, numjobs, sizeof(struct atjob *),
	    csort ? byctime : byjobno);

	if (!shortformat)
		(void)puts(" Rank     Execution Date     Owner          "
		    "Job       Queue");

	for (i = 0; i < numjobs; i++) {
		print_job(atjobs[i], i + 1, shortformat);
		free(atjobs[i]);
	}
	free(atjobs);
}

static int
rmok(long long job)
{
	int ch, junk;

	printf("%lld: remove it? ", job);
	ch = getchar();
	while ((junk = getchar()) != EOF && junk != '\n')
		;
	return (ch == 'y' || ch == 'Y');
}

/*
 * Loop through all jobs in _PATH_AT_SPOOL and display or delete ones
 * that match argv (may be job or username), or all if argc == 0.
 * Only the superuser may display/delete other people's jobs.
 */
static int
process_jobs(int argc, char **argv, int what)
{
	struct stat stbuf;
	struct dirent *dirent;
	struct passwd *pw;
	time_t runtimer;
	uid_t *uids;
	char **jobs, *ep;
	FILE *fp;
	DIR *spool;
	int job_matches, jobs_len, uids_len;
	int error, i, ch, changed, dfd;

	if ((dfd = open(_PATH_AT_SPOOL, O_RDONLY|O_DIRECTORY)) == -1 ||
	    (spool = fdopendir(dfd)) == NULL)
		fatal(_PATH_AT_SPOOL);

	/* Convert argv into a list of jobs and uids. */
	jobs = NULL;
	uids = NULL;
	jobs_len = uids_len = 0;
	if (argc > 0) {
		if ((jobs = reallocarray(NULL, argc, sizeof(char *))) == NULL ||
		    (uids = reallocarray(NULL, argc, sizeof(uid_t))) == NULL)
			fatal(NULL);

		for (i = 0; i < argc; i++) {
			if (strtot(argv[i], &ep, &runtimer) == 0 &&
			    *ep == '.' && isalpha((unsigned char)*(ep + 1)) &&
			    *(ep + 2) == '\0')
				jobs[jobs_len++] = argv[i];
			else if ((pw = getpwnam(argv[i])) != NULL) {
				if (user_uid != pw->pw_uid && user_uid != 0) {
					fatalx("only the superuser may %s "
					    "other users' jobs",
					    what == ATRM ? "remove" : "view");
				}
				uids[uids_len++] = pw->pw_uid;
			} else
				fatalx("unknown user %s", argv[i]);
		}
	}

	/* Loop over every file in the directory */
	changed = 0;
	while ((dirent = readdir(spool)) != NULL) {
		if (fstatat(dfd, dirent->d_name, &stbuf, AT_SYMLINK_NOFOLLOW) != 0)
			fatal("%s", dirent->d_name);

		if (stbuf.st_uid != user_uid && user_uid != 0)
			continue;

		if (strtot(dirent->d_name, &ep, &runtimer) == -1)
			continue;
		if (*ep != '.' || !isalpha((unsigned char)*(ep + 1)) ||
		    *(ep + 2) != '\0')
			continue;

		/* Check runtimer against argv; argc==0 means do all. */
		job_matches = (argc == 0) ? 1 : 0;
		if (!job_matches) {
			for (i = 0; i < jobs_len; i++) {
				if (jobs[i] != NULL &&
				    strcmp(dirent->d_name, jobs[i]) == 0) {
					jobs[i] = NULL;
					job_matches = 1;
					break;
				}
			}
		}
		if (!job_matches) {
			for (i = 0; i < uids_len; i++) {
				if (uids[i] == stbuf.st_uid) {
					job_matches = 1;
					break;
				}
			}
		}

		if (job_matches) {
			switch (what) {
			case ATRM:
				if (!interactive ||
				    (interactive && rmok(runtimer))) {
					if (unlinkat(dfd, dirent->d_name, 0) == 0) {
						syslog(LOG_INFO,
						    "(%s) DELETE (%s)",
						    user_name, dirent->d_name);
						changed = 1;
					} else if (!force)
						fatal("%s", dirent->d_name);
					if (!force && !interactive)
						warnx("%s removed",
						    dirent->d_name);
				}
				break;

			case CAT:
				i = openat(dfd, dirent->d_name,
				    O_RDONLY|O_NOFOLLOW);
				if (i == -1 || (fp = fdopen(i, "r")) == NULL)
					fatal("%s", dirent->d_name);
				syslog(LOG_INFO, "(%s) CAT (%s)",
				    user_name, dirent->d_name);

				while ((ch = getc(fp)) != EOF)
					putchar(ch);

				fclose(fp);
				break;

			default:
				fatalx("internal error");
				break;
			}
		}
	}
	closedir(spool);

	for (error = 0, i = 0; i < jobs_len; i++) {
		if (jobs[i] != NULL) {
			if (!force)
				warnx("%s: no such job", jobs[i]);
			error++;
		}
	}
	free(jobs);
	free(uids);

	/* If we modied the spool, poke cron so it knows to reload. */
	if (changed)
		poke_daemon(RELOAD_AT);

	return (error);
}

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

/*
 * Adapted from date(1)
 */
static time_t
ttime(char *arg)
{
	time_t now, then;
	struct tm *lt;
	int yearset;
	char *dot, *p;

	if (time(&now) == (time_t)-1 || (lt = localtime(&now)) == NULL)
		fatal("unable to get current time");

	/* Valid date format is [[CC]YY]MMDDhhmm[.SS] */
	for (p = arg, dot = NULL; *p != '\0'; p++) {
		if (*p == '.' && dot == NULL)
			dot = p;
		else if (!isdigit((unsigned char)*p))
			goto terr;
	}
	if (dot == NULL)
		lt->tm_sec = 0;
	else {
		*dot++ = '\0';
		if (strlen(dot) != 2)
			goto terr;
		lt->tm_sec = ATOI2(dot);
		if (lt->tm_sec > 61)	/* could be leap second */
			goto terr;
	}

	yearset = 0;
	switch(strlen(arg)) {
	case 12:			/* CCYYMMDDhhmm */
		lt->tm_year = ATOI2(arg) * 100;
		lt->tm_year -= 1900;	/* Convert to Unix time */
		yearset = 1;
		/* FALLTHROUGH */
	case 10:			/* YYMMDDhhmm */
		if (yearset) {
			yearset = ATOI2(arg);
			lt->tm_year += yearset;
		} else {
			yearset = ATOI2(arg);
			/* POSIX logic: [00,68]=>20xx, [69,99]=>19xx */
			lt->tm_year = yearset;
			if (yearset < 69)
				lt->tm_year += 100;
		}
		/* FALLTHROUGH */
	case 8:				/* MMDDhhmm */
		lt->tm_mon = ATOI2(arg);
		if (lt->tm_mon > 12 || lt->tm_mon == 0)
			goto terr;
		--lt->tm_mon;		/* Convert from 01-12 to 00-11 */
		lt->tm_mday = ATOI2(arg);
		if (lt->tm_mday > 31 || lt->tm_mday == 0)
			goto terr;
		lt->tm_hour = ATOI2(arg);
		if (lt->tm_hour > 23)
			goto terr;
		lt->tm_min = ATOI2(arg);
		if (lt->tm_min > 59)
			goto terr;
		break;
	default:
		goto terr;
	}

	lt->tm_isdst = -1;		/* mktime will deduce DST. */
	then = mktime(lt);
	if (then == (time_t)-1) {
    terr:
		fatalx("illegal time specification: [[CC]YY]MMDDhhmm[.SS]");
	}
	if (then < now)
		fatalx("cannot schedule jobs in the past");
	return (then);
}

static __dead void
usage(void)
{
	/* Print usage and exit.  */
	switch (program) {
	case AT:
	case CAT:
		(void)fprintf(stderr,
		    "usage: at [-bm] [-f file] [-l [job ...]] [-q queue] "
		    "-t time_arg | timespec\n"
		    "       at -c | -r job ...\n");
		break;
	case ATQ:
		(void)fprintf(stderr,
		    "usage: atq [-cnv] [-q queue] [name ...]\n");
		break;
	case ATRM:
		(void)fprintf(stderr,
		    "usage: atrm [-afi] [[job] [name] ...]\n");
		break;
	case BATCH:
		(void)fprintf(stderr,
		    "usage: batch [-m] [-f file] [-q queue] [timespec]\n");
		break;
	}
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	time_t timer = -1;
	char *atinput = NULL;			/* where to get input from */
	char queue = DEFAULT_AT_QUEUE;
	char queue_set = 0;
	char *options = "q:f:t:bcdlmrv";	/* default options for at */
	char cwd[PATH_MAX];
	struct passwd *pw;
	int ch;
	int aflag = 0;
	int cflag = 0;
	int nflag = 0;

	if (pledge("stdio rpath wpath cpath fattr getpw unix id", NULL) == -1)
		fatal("pledge");

	openlog(__progname, LOG_PID, LOG_CRON);

	if (argc < 1)
		usage();

	user_uid = getuid();
	user_gid = getgid();
	spool_gid = getegid();

	/* find out what this program is supposed to do */
	if (strcmp(__progname, "atq") == 0) {
		program = ATQ;
		options = "cnvq:";
	} else if (strcmp(__progname, "atrm") == 0) {
		program = ATRM;
		options = "afi";
	} else if (strcmp(__progname, "batch") == 0) {
		program = BATCH;
		options = "f:q:mv";
	}

	/* process whatever options we can process */
	while ((ch = getopt(argc, argv, options)) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;

		case 'i':
			interactive = 1;
			force = 0;
			break;

		case 'v':	/* show completed but unremoved jobs */
			/*
			 * This option is only useful when we are invoked
			 * as atq but we accept (and ignore) this flag in
			 * the other programs for backwards compatibility.
			 */
			vflag = 1;
			break;

		case 'm':	/* send mail when job is complete */
			send_mail = 1;
			break;

		case 'f':
			if (program == ATRM) {
				force = 1;
				interactive = 0;
			} else
				atinput = optarg;
			break;

		case 'q':	/* specify queue */
			if (strlen(optarg) > 1)
				usage();

			atqueue = queue = *optarg;
			if (!(islower((unsigned char)queue) ||
			    isupper((unsigned char)queue)))
				usage();

			queue_set = 1;
			break;

		case 'd':		/* for backwards compatibility */
		case 'r':
			program = ATRM;
			options = "";
			break;

		case 't':
			timer = ttime(optarg);
			break;

		case 'l':
			program = ATQ;
			options = "cnvq:";
			break;

		case 'b':
			program = BATCH;
			options = "f:q:mv";
			break;

		case 'c':
			if (program == ATQ) {
				cflag = 1;
			} else {
				program = CAT;
				options = "";
			}
			break;

		case 'n':
			nflag = 1;
			break;

		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	switch (program) {
	case AT:
	case BATCH:
		if (atinput != NULL) {
			if (setegid(user_gid) != 0)
				fatal("setegid(user_gid)");
			if (freopen(atinput, "r", stdin) == NULL)
				fatal("%s", atinput);
			if (setegid(spool_gid) != 0)
				fatal("setegid(spool_gid)");
		}

		if (pledge("stdio rpath wpath cpath fattr getpw unix", NULL)
		    == -1)
			fatal("pledge");
		break;

	case ATQ:
	case CAT:
		if (pledge("stdio rpath getpw", NULL) == -1)
			fatal("pledge");
		break;

	case ATRM:
		if (pledge("stdio rpath cpath getpw unix", NULL) == -1)
			fatal("pledge");
		break;

	default:
		fatalx("internal error");
		break;
	}

	if ((pw = getpwuid(user_uid)) == NULL)
	    fatalx("unknown uid %u", user_uid);
	if (strlcpy(user_name, pw->pw_name, sizeof(user_name)) >= sizeof(user_name))
	    fatalx("username too long");

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		fatal("unable to get current working directory");

	if (!allowed(pw->pw_name, _PATH_AT_ALLOW, _PATH_AT_DENY)) {
		syslog(LOG_WARNING, "(%s) AUTH (at command not allowed)",
		    pw->pw_name);
		fatalx("you do not have permission to use at.");
	}

	/* select our program */
	switch (program) {
	case ATQ:
		list_jobs(argc, argv, nflag, cflag);
		break;

	case ATRM:
	case CAT:
		if ((aflag && argc) || (!aflag && !argc))
			usage();
		return process_jobs(argc, argv, program);
		break;

	case AT:
		/* Time may have been specified via the -t flag. */
		if (timer == -1) {
			if (argc == 0)
				usage();
			else if ((timer = parsetime(argc, argv)) == -1)
				return EXIT_FAILURE;
		}
		writefile(cwd, timer, queue);
		break;

	case BATCH:
		if (queue_set)
			queue = toupper((unsigned char)queue);
		else
			queue = DEFAULT_BATCH_QUEUE;

		if (argc == 0)
			timer = time(NULL);
		else if ((timer = parsetime(argc, argv)) == -1)
			return EXIT_FAILURE;

		writefile(cwd, timer, queue);
		break;

	default:
		fatalx("internal error");
		break;
	}
	return EXIT_SUCCESS;
}
