/*	$OpenBSD: rcsprog.c,v 1.31 2005/10/18 01:22:14 joris Exp $	*/
/*
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
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

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "rcs.h"
#include "rcsprog.h"
#include "strtab.h"

#define RCS_CMD_MAXARG	128

const char rcs_version[] = "OpenCVS RCS version 3.6";
int verbose = 1;

int	rcs_optind;
char	*rcs_optarg;

struct rcs_prog {
	char	*prog_name;
	int	(*prog_hdlr)(int, char **);
	void	(*prog_usage)(void);
} programs[] = {
	{ "rcs",	rcs_main,	rcs_usage	},
	{ "ci",		checkin_main,	checkin_usage   },
	{ "co",		checkout_main,	checkout_usage  },
	{ "rcsclean",	rcsclean_main,	rcsclean_usage	},
	{ "rcsdiff",	rcsdiff_main,	rcsdiff_usage	},
	{ "rlog",	rlog_main,	rlog_usage	},
	{ "ident",	ident_main,	ident_usage	},
};

void
rcs_set_rev(const char *str, RCSNUM **rev)
{
	if (str == NULL)
		return;

	if (*rev != RCS_HEAD_REV)
		cvs_log(LP_WARN, "redefinition of revision number");

	if ((*rev = rcsnum_parse(str)) == NULL) {
		cvs_log(LP_ERR, "bad revision number '%s'", str);
		exit (1);
	}
}

int
rcs_init(char *envstr, char **argv, int argvlen)
{
	u_int i;
	int argc, error;
	char linebuf[256],  *lp, *cp;

	strlcpy(linebuf, envstr, sizeof(linebuf));
	memset(argv, 0, argvlen * sizeof(char *));

	error = argc = 0;
	for (lp = linebuf; lp != NULL;) {
		cp = strsep(&lp, " \t\b\f\n\r\t\v");;
		if (cp == NULL)
			break;
		else if (*cp == '\0')
			continue;

		if (argc == argvlen) {
			error++;
			break;
		}

		argv[argc] = strdup(cp);
		if (argv[argc] == NULL) {
			cvs_log(LP_ERRNO, "failed to copy argument");
			error++;
			break;
		}

		argc++;
	}

	if (error != 0) {
		for (i = 0; i < (u_int)argc; i++)
			free(argv[i]);
		argc = -1;
	}

	return (argc);
}

int
rcs_getopt(int argc, char **argv, const char *optstr)
{
	char *a;
	const char *c;
	static int i = 1;
	int opt, hasargument, ret;

	hasargument = 0;
	rcs_optarg = NULL;

	if (i >= argc)
		return (-1);

	a = argv[i++];
	if (*a++ != '-')
		return (-1);

	ret = 0;
	opt = *a;
	for (c = optstr; *c != '\0'; c++) {
		if (*c == opt) {
			a++;
			ret = opt;

			if (*(c + 1) == ':') {
				if (*(c + 2) == ':') {
					if (*a != '\0')
						hasargument = 1;
				} else {
					if (*a != '\0') {
						hasargument = 1;
					} else {
						ret = 1;
						break;
					}
				}
			}

			if (hasargument == 1)
				rcs_optarg = a;

			if (ret == opt)
				rcs_optind++;
			break;
		}
	}

	if (ret == 0)
		cvs_log(LP_ERR, "unknown option -%c", opt);
	else if (ret == 1)
		cvs_log(LP_ERR, "missing argument for option -%c", opt);

	return (ret);
}

int
rcs_statfile(char *fname, char *out, size_t len)
{
	int l;
	char *s;
	char filev[MAXPATHLEN], fpath[MAXPATHLEN];
	struct stat st;

	l = snprintf(filev, sizeof(filev), "%s%s", fname, RCS_FILE_EXT);
	if (l == -1 || l >= (int)sizeof(filev))
		return (-1);

	if ((stat(RCSDIR, &st) != -1) && (st.st_mode & S_IFDIR)) {
		l = snprintf(fpath, sizeof(fpath), "%s/%s", RCSDIR, filev);
		if (l == -1 || l >= (int)sizeof(fpath))
			return (-1);
	} else {
		strlcpy(fpath, filev, sizeof(fpath));
	}

	if (stat(fpath, &st) == -1) {
		if (strcmp(__progname, "rcsclean"))
			cvs_log(LP_ERRNO, "%s", fpath);
		return (-1);
	}

	strlcpy(out, fpath, len);
	if ((verbose == 1) && (strcmp(__progname, "rcsclean"))) {
		if (!strcmp(__progname, "co")) {
			printf("%s --> ", fpath);
			if ((s = strrchr(filev, ',')) != NULL) {
				*s = '\0';
				printf("%s\n", fname);
			}
		} else {
			printf("RCS file: %s\n", fpath);
		}
	}

	return (0);
}

int
main(int argc, char **argv)
{
	u_int i;
	char *rcsinit, *cmd_argv[RCS_CMD_MAXARG];
	int ret, cmd_argc;

	ret = -1;
	rcs_optind = 1;
	cvs_strtab_init();
	cvs_log_init(LD_STD, 0);

	cmd_argc = 0;
	cmd_argv[cmd_argc++] = argv[0];
	if ((rcsinit = getenv("RCSINIT")) != NULL) {
		ret = rcs_init(rcsinit, cmd_argv + 1,
		    RCS_CMD_MAXARG - 1);
		if (ret < 0) {
			cvs_log(LP_ERRNO, "failed to prepend RCSINIT options");
			exit (1);
		}

		cmd_argc += ret;
	}

	for (ret = 1; ret < argc; ret++)
		cmd_argv[cmd_argc++] = argv[ret];

	for (i = 0; i < (sizeof(programs)/sizeof(programs[0])); i++)
		if (strcmp(__progname, programs[i].prog_name) == 0) {
			usage = programs[i].prog_usage;
			ret = programs[i].prog_hdlr(cmd_argc, cmd_argv);
			break;
		}

	cvs_strtab_cleanup();

	exit(ret);
}


void
rcs_usage(void)
{
	fprintf(stderr,
	    "usage: rcs [-hiLMUV] [-a users] [-b [rev]] [-c string] [-e users]\n"
	    "           [-k opt] [-m rev:log] file ...\n");
}

/*
 * rcs_main()
 *
 * Handler for the `rcs' program.
 * Returns 0 on success, or >0 on error.
 */
int
rcs_main(int argc, char **argv)
{
	int i, ch, flags, kflag, lkmode;
	char fpath[MAXPATHLEN];
	char *logstr, *logmsg;
	char *oldfile, *alist, *comment, *elist, *unp, *sp;
	mode_t fmode;
	RCSFILE *file;
	RCSNUM *logrev;

	kflag = lkmode = -1;
	fmode = 0;
	flags = RCS_RDWR;
	logstr = oldfile = alist = comment = elist = NULL;

	while ((ch = rcs_getopt(argc, argv, "A:a:b::c:e::hik:Lm:MqUV")) != -1) {
		switch (ch) {
		case 'A':
			oldfile = rcs_optarg;
			break;
		case 'a':
			alist = rcs_optarg;
			break;
		case 'c':
			comment = rcs_optarg;
			break;
		case 'e':
			elist = rcs_optarg;
			break;
		case 'h':
			(usage)();
			exit(0);
		case 'i':
			flags |= RCS_CREATE;
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid keyword substitution mode `%s'",
				    rcs_optarg);
				exit(1);
			}
			break;
		case 'L':
			if (lkmode == RCS_LOCK_LOOSE)
				cvs_log(LP_WARN, "-U overriden by -L");
			lkmode = RCS_LOCK_STRICT;
			break;
		case 'm':
			if ((logstr = strdup(rcs_optarg)) == NULL) {
				cvs_log(LP_ERRNO, "failed to copy logstring");
				exit(1);
			}
			break;
		case 'M':
			/* ignore for the moment */
			break;
		case 'q':
			verbose = 0;
			break;
		case 'U':
			if (lkmode == RCS_LOCK_STRICT)
				cvs_log(LP_WARN, "-L overriden by -U");
			lkmode = RCS_LOCK_LOOSE;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		file = rcs_open(fpath, flags, fmode);
		if (file == NULL)
			continue;

		if (logstr != NULL) {
			if ((logmsg = strchr(logstr, ':')) == NULL) {
				cvs_log(LP_ERR, "missing log message");
				rcs_close(file);
				continue;
			}

			*logmsg++ = '\0';
			if ((logrev = rcsnum_parse(logstr)) == NULL) {
				cvs_log(LP_ERR, "'%s' bad revision number", logstr);
				rcs_close(file);
				continue;
			}

			if (rcs_rev_setlog(file, logrev, logmsg) < 0) {
				cvs_log(LP_ERR,
				    "failed to set logmsg for '%s' to '%s'",
				    logstr, logmsg);
				rcs_close(file);
				rcsnum_free(logrev);
				continue;
			}

			rcsnum_free(logrev);
		}

		/* entries to add to the access list */
		if (alist != NULL) {
			unp = alist;
			do {
				sp = strchr(unp, ',');
				if (sp != NULL)
					*(sp++) = '\0';

				rcs_access_add(file, unp);

				unp = sp;
			} while (sp != NULL);
		}

		if (comment != NULL)
			rcs_comment_set(file, comment);

		if (kflag != -1)
			rcs_kwexp_set(file, kflag);

		if (lkmode != -1)
			rcs_lock_setmode(file, lkmode);

		rcs_close(file);

		if (verbose == 1)
			printf("done\n");
	}

	if (logstr != NULL)
		free(logstr);

	return (0);
}
