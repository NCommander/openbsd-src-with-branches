/* $OpenBSD: doas.c,v 1.1 2015/07/16 20:44:21 tedu Exp $ */
/*
 * Copyright (c) 2015 Ted Unangst <tedu@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>

#include <limits.h>
#include <login_cap.h>
#include <bsd_auth.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>

#include "doas.h"

static void __dead
usage(void)
{
	fprintf(stderr, "usage: doas [-u user] command [args]\n");
	exit(1);
}

size_t
arraylen(const char **arr)
{
	size_t cnt = 0;
	while (*arr) {
		cnt++;
		arr++;
	}
	return cnt;
}

static int
parseuid(const char *s, uid_t *uid)
{
	struct passwd *pw;
	const char *errstr;

	if ((pw = getpwnam(s)) != NULL) {
		*uid = pw->pw_uid;
		return 0;
	}
	*uid = strtonum(s, 0, UID_MAX, &errstr);
	if (errstr)
		return -1;
	return 0;
}

static int
uidcheck(const char *s, uid_t desired)
{
	uid_t uid;

	if (parseuid(s, &uid) != 0)
		return -1;
	if (uid != desired)
		return -1;
	return 0;
}

static gid_t
strtogid(const char *s)
{
	struct group *gr;
	const char *errstr;
	gid_t gid;

	if ((gr = getgrnam(s)) != NULL)
		return gr->gr_gid;
	gid = strtonum(s, 0, GID_MAX, &errstr);
	if (errstr)
		return -1;
	return gid;
}

static int
match(uid_t uid, gid_t *groups, int ngroups, uid_t target, const char *cmd,
    struct rule *r)
{
	int i;

	if (r->ident[0] == ':') {
		gid_t rgid = strtogid(r->ident + 1);
		if (rgid == -1)
			return 0;
		for (i = 0; i < ngroups; i++) {
			if (rgid == groups[i])
				break;
		}
		if (i == ngroups)
			return 0;
	} else {
		if (uidcheck(r->ident, uid) != 0)
			return 0;
	}
	if (r->target && uidcheck(r->target, target) != 0)
		return 0;
	if (r->cmd && strcmp(r->cmd, cmd) != 0)
		return 0;
	return 1;
}

static int
permit(uid_t uid, gid_t *groups, int ngroups, struct rule **lastr,
    uid_t target, const char *cmd)
{
	int i;

	*lastr = NULL;
	for (i = 0; i < nrules; i++) {
		if (match(uid, groups, ngroups, target, cmd, rules[i]))
			*lastr = rules[i];
	}
	if (!*lastr)
		return 0;
	return (*lastr)->action == PERMIT;
}

static void
parseconfig(const char *filename)
{
	extern FILE *yyfp;
	extern int yyparse(void);

	yyfp = fopen(filename, "r");
	if (!yyfp) {
		fprintf(stderr, "doas is not enabled.\n");
		exit(1);
	}
	yyparse();
	fclose(yyfp);
}

static int
copyenvhelper(const char **oldenvp, const char **safeset, int nsafe, char **envp, int ei)
{
	int i;
	for (i = 0; i < nsafe; i++) {
		const char **oe = oldenvp;
		while (*oe) {
			size_t len = strlen(safeset[i]);
			if (strncmp(*oe, safeset[i], len) == 0 &&
			    (*oe)[len] == '=') {
				if (!(envp[ei++] = strdup(*oe)))
					err(1, "strdup");
				break;
			}
			oe++;
		}
	}
	return ei;
}

static char **
copyenv(const char **oldenvp, struct rule *rule)
{
	const char *safeset[] = {
		"DISPLAY", "HOME", "LOGNAME", "MAIL", "SHELL",
		"PATH", "TERM", "USER", "USERNAME",
		NULL,
	};
	int nsafe;
	int nextras = 0;
	char **envp;
	const char **extra;
	int ei;
	int i, j;
	
	if ((rule->options & KEEPENV) && !rule->envlist) {
		j = arraylen(oldenvp);
		envp = reallocarray(NULL, j + 1, sizeof(char *));
		for (i = 0; i < j; i++) {
			if (!(envp[i] = strdup(oldenvp[i])))
				err(1, "strdup");
		}
		envp[i] = NULL;
		return envp;
	}

	nsafe = arraylen(safeset);
	if ((extra = rule->envlist)) {
		nextras = arraylen(extra);
		for (i = 0; i < nsafe; i++) {
			for (j = 0; j < nextras; j++) {
				if (strcmp(extra[j], safeset[i]) == 0) {
					extra[j--] = extra[nextras--];
					extra[nextras] = NULL;
				}
			}
		}
	}

	envp = reallocarray(NULL, nsafe + nextras + 1, sizeof(char *));
	if (!envp)
		err(1, "can't allocate new environment");

	ei = 0;
	ei = copyenvhelper(oldenvp, safeset, nsafe, envp, ei);
	ei = copyenvhelper(oldenvp, rule->envlist, nextras, envp, ei);
	envp[ei] = NULL;

	return envp;
}

static void __dead
fail(void)
{
	const char *msgs[] = {
		"No lollygagging!",
		"Better luck next time.",
		"PEBKAC detected.",
		"That's what happens when you're lazy.",
		"It is clear that this has not been thought through.",
		"That's the most ridiculous thing I've heard in the last two or three minutes!",
		"No sane people allowed here.  Go home.",
		"I would explain, but I am too drunk.",
		"You're not allowed to have an opinion.",
		"Complaint forms are handled in another department.",
	};
	const char *m;

	m = msgs[arc4random_uniform(sizeof(msgs) / sizeof(msgs[0]))];
	fprintf(stderr, "%s\n", m);
	exit(1);
}

int
main(int argc, char **argv, char **envp)
{
	char cmdline[1024];
	char myname[32];
	uid_t uid, target = 0;
	gid_t groups[NGROUPS_MAX + 1];
	int ngroups;
	struct passwd *pw;
	struct rule *rule;
	const char *cmd;
	int i, ch;
	const char *safepath = "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin";

	parseconfig("/etc/doas.conf");

	while ((ch = getopt(argc, argv, "u:")) != -1) {
		switch (ch) {
		case 'u':
			if (parseuid(optarg, &target) != 0)
				errx(1, "unknown user");
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (!argc)
		usage();

	cmd = argv[0];
	strlcpy(cmdline, argv[0], sizeof(cmdline));
	for (i = 1; i < argc; i++) {
		strlcat(cmdline, " ", sizeof(cmdline));
		strlcat(cmdline, argv[i], sizeof(cmdline));
	}

	uid = getuid();
	pw = getpwuid(uid);
	if (!pw)
		err(1, "getpwuid failed");
	strlcpy(myname, pw->pw_name, sizeof(myname));
	ngroups = getgroups(NGROUPS_MAX, groups);
	if (ngroups == -1)
		err(1, "can't get groups");
	groups[ngroups++] = getgid();

	if (!permit(uid, groups, ngroups, &rule, target, cmd)) {
		syslog(LOG_AUTHPRIV | LOG_NOTICE, "failed command for %s: %s", myname, cmdline);
		fail();
	}

	if (!(rule->options & NOPASS)) {
		if (!auth_userokay(myname, NULL, NULL, NULL)) {
			syslog(LOG_AUTHPRIV | LOG_NOTICE, "failed password for %s", myname);
			fail();
		}
	}
	envp = copyenv((const char **)envp, rule);

	pw = getpwuid(target);
	if (!pw)
		errx(1, "no passwd entry for target");
	if (setusercontext(NULL, pw, target, LOGIN_SETGROUP |
	    LOGIN_SETPRIORITY | LOGIN_SETRESOURCES | LOGIN_SETUMASK |
	    LOGIN_SETUSER) != 0)
		errx(1, "failed to set user context for target");

	syslog(LOG_AUTHPRIV | LOG_INFO, "%s ran command as %s: %s", myname, pw->pw_name, cmdline);
	setenv("PATH", safepath, 1);
	execvpe(cmd, argv, envp);
	err(1, "%s", cmd);
}
