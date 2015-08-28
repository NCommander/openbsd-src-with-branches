/*	$OpenBSD: policy.c,v 1.34 2013/11/21 15:54:46 deraadt Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>
#include <libgen.h>

#include "intercept.h"
#include "systrace.h"

static int psccompare(struct policy_syscall *, struct policy_syscall *);
static int policycompare(struct policy *, struct policy *);
static int polnrcompare(struct policy *, struct policy *);
static char *systrace_policyfilename(char *, const char *);
static char *systrace_policyline(char *line);
static int systrace_policyprocess(struct policy *,
    char *);
static int systrace_writepolicy(struct policy *);

int systrace_templatedir(void);

static int
psccompare(struct policy_syscall *a, struct policy_syscall *b)
{
	int diff;
	diff = strcmp(a->emulation, b->emulation);
	if (diff)
		return (diff);
	return (strcmp(a->name, b->name));
}

SPLAY_PROTOTYPE(syscalltree, policy_syscall, node, psccompare)
SPLAY_GENERATE(syscalltree, policy_syscall, node, psccompare)

static SPLAY_HEAD(policytree, policy) policyroot;
static SPLAY_HEAD(polnrtree, policy) polnrroot;

int
policycompare(struct policy *a, struct policy *b)
{
	return (strcmp(a->name, b->name));
}

int
polnrcompare(struct policy *a, struct policy *b)
{
	int diff = a->policynr - b->policynr;

	if (diff == 0)
		return (0);
	if (diff > 0 )
		return (1);
	return (-1);
}

SPLAY_PROTOTYPE(policytree, policy, node, policycompare)
SPLAY_GENERATE(policytree, policy, node, policycompare)

SPLAY_PROTOTYPE(polnrtree, policy, nrnode, polnrcompare)
SPLAY_GENERATE(polnrtree, policy, nrnode, polnrcompare)

extern int userpolicy;

static char policydir[PATH_MAX];

struct tmplqueue templates;

void
systrace_setupdir(char *path)
{
	char *home;
	struct stat sb;

	if (path == NULL) {
		home = getenv("HOME");

		if (home == NULL)
			errx(1, "No HOME environment set");

		if (strlcpy(policydir, home, sizeof(policydir)) >= sizeof(policydir))
			errx(1, "HOME too long");

		if (strlcat(policydir, "/.systrace", sizeof(policydir)) >= sizeof(policydir))
			errx(1, "HOME too long");
	} else if (strlcpy(policydir, path, sizeof(policydir)) >= sizeof(policydir))
		errx(1, "policy directory too long");
		

	if (stat(policydir, &sb) != -1) {
		if (!S_ISDIR(sb.st_mode))
			errx(1, "Not a directory: \"%s\"", policydir);
	} else if (mkdir(policydir, 0700) == -1)
		err(1, "mkdir(%s)", policydir);
}

int
systrace_initpolicy(char *file, char *path)
{
	SPLAY_INIT(&policyroot);
	SPLAY_INIT(&polnrroot);

	if (userpolicy) {
		systrace_setupdir(path);
		systrace_templatedir();
	}

	if (file != NULL)
		return (systrace_readpolicy(file) != NULL ? 0 : -1);

	return (0);
}

struct policy *
systrace_findpolicy(const char *name)
{
	struct policy tmp;

	tmp.name = name;

	return (SPLAY_FIND(policytree, &policyroot, &tmp));
}

struct policy *
systrace_findpolicy_wildcard(const char *name)
{
	struct policy tmp, *res;
	static char path[PATH_MAX], lookup[PATH_MAX];

	if (strlcpy(path, name, sizeof(path)) >= sizeof(path))
		errx(1, "%s: path name overflow", __func__);

	strlcpy(lookup, "*/", sizeof(lookup));
	strlcat(lookup, basename(path), sizeof(lookup));

	tmp.name = lookup;
	res = SPLAY_FIND(policytree, &policyroot, &tmp);
	if (res == NULL)
		return (NULL);

	/* we found the wildcarded policy; now remove it and bind it */
	SPLAY_REMOVE(policytree, &policyroot, res);

	free((char *)res->name);
	if ((res->name = strdup(name)) == NULL)
		err(1, "%s: strdup", __func__);

	SPLAY_INSERT(policytree, &policyroot, res);
	return (res);
}

struct policy *
systrace_findpolnr(int nr)
{
	struct policy tmp;

	tmp.policynr = nr;

	return (SPLAY_FIND(polnrtree, &polnrroot, &tmp));
}

int
systrace_newpolicynr(int fd, struct policy *tmp)
{
	if (tmp->policynr != -1)
		return (-1);

	if ((tmp->policynr = intercept_newpolicy(fd)) == -1) {
		/* XXX - maybe free policy structure here */
		return (-1);
	}

	SPLAY_INSERT(polnrtree, &polnrroot, tmp);

	return (tmp->policynr);
}

struct policy *
systrace_newpolicy(const char *emulation, const char *name)
{
	int i;
	struct policy *tmp;

	if ((tmp = systrace_findpolicy(name)) != NULL)
		return (tmp);

	if ((tmp = systrace_findpolicy_wildcard(name)) != NULL)
		return (tmp);

	tmp = calloc(1, sizeof(struct policy));
	if (tmp == NULL)
		return (NULL);

	tmp->policynr = -1;

	/* New policies requires initialization */
	if ((tmp->name = strdup(name)) == NULL)
		err(1, "%s:%d: strdup", __func__, __LINE__);
	strlcpy(tmp->emulation, emulation, sizeof(tmp->emulation));

	SPLAY_INSERT(policytree, &policyroot, tmp);
	SPLAY_INIT(&tmp->pflqs);
	TAILQ_INIT(&tmp->filters);
	TAILQ_INIT(&tmp->prefilters);

	/* Set the default policy to ask */
	for (i = 0; i < INTERCEPT_MAXSYSCALLNR; i++)
		tmp->kerneltable[i] = ICPOLICY_ASK;

	return (tmp);
}

void
systrace_cleanpolicy(struct policy *policy)
{
	struct filter *filter;
	struct policy_syscall *pflq;
	int i;

	/* Set the default policy to ask */
	for (i = 0; i < INTERCEPT_MAXSYSCALLNR; i++)
		policy->kerneltable[i] = ICPOLICY_ASK;

	while ((filter = TAILQ_FIRST(&policy->prefilters)) != NULL) {
		TAILQ_REMOVE(&policy->prefilters, filter, policy_next);
		filter_free(filter);
	}

	while ((filter = TAILQ_FIRST(&policy->filters)) != NULL) {
		TAILQ_REMOVE(&policy->filters, filter, policy_next);
		filter_free(filter);
	}

	while ((pflq = SPLAY_ROOT(&policy->pflqs)) != NULL) {
		SPLAY_REMOVE(syscalltree, &policy->pflqs, pflq);

		while ((filter = TAILQ_FIRST(&pflq->flq)) != NULL) {
			TAILQ_REMOVE(&pflq->flq, filter, next);
			filter_free(filter);
		}

		free(pflq);
	}
}

void
systrace_freepolicy(struct policy *policy)
{
	if (policy->flags & POLICY_CHANGED) {
		if (systrace_writepolicy(policy) == -1)
			fprintf(stderr, "Failed to write policy for %s\n",
			    policy->name);
	}

	systrace_cleanpolicy(policy);

	SPLAY_REMOVE(policytree, &policyroot, policy);
	if (policy->policynr != -1)
		SPLAY_REMOVE(polnrtree, &polnrroot, policy);

	free((char *)policy->name);
	free(policy);
}

struct filterq *
systrace_policyflq(struct policy *policy, const char *emulation,
    const char *name)
{
	struct policy_syscall tmp2, *tmp;

	strlcpy(tmp2.emulation, emulation, sizeof(tmp2.emulation));
	strlcpy(tmp2.name, name, sizeof(tmp2.name));

	tmp = SPLAY_FIND(syscalltree, &policy->pflqs, &tmp2);
	if (tmp != NULL)
		return (&tmp->flq);

	if ((tmp = calloc(1, sizeof(struct policy_syscall))) == NULL)
		err(1, "%s:%d: out of memory", __func__, __LINE__);

	strlcpy(tmp->emulation, emulation, sizeof(tmp->emulation));
	strlcpy(tmp->name, name, sizeof(tmp->name));
	TAILQ_INIT(&tmp->flq);

	SPLAY_INSERT(syscalltree, &policy->pflqs, tmp);

	return (&tmp->flq);
}

int
systrace_modifypolicy(int fd, int policynr, const char *name, short action)
{
	struct policy *policy;
	int res, nr;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		return (-1);

	res = intercept_modifypolicy(fd, policynr, policy->emulation,
	    name, action);

	/* Remember the kernel policy */
	if (res != -1 &&
	    (nr = intercept_getsyscallnumber(policy->emulation, name)) != -1)
		policy->kerneltable[nr] = action;

	return (res);
}

char *
systrace_policyfilename(char *dirname, const char *name)
{
	static char file[2*PATH_MAX];
	const char *p;
	int i, plen;

	if (strlen(name) + strlen(dirname) + 1 >= sizeof(file))
		return (NULL);

	strlcpy(file, dirname, sizeof(file));
	i = strlen(file);
	file[i++] = '/';
	plen = i;

	p = name;
	while (*p) {
		if (!isalnum((unsigned char)*p)) {
			if (i != plen)
				file[i++] = '_';
		} else
			file[i++] = *p;
		p++;
	}

	file[i] = '\0';

	return (file);
}

/*
 * Converts a executeable name into the corresponding filename
 * that contains the security policy.
 */

char *
systrace_getpolicyname(const char *name)
{
	char *file = NULL;

	if (userpolicy) {
		file = systrace_policyfilename(policydir, name);
		/* Check if the user policy file exists */
		if (file != NULL && access(file, R_OK) == -1)
			file = NULL;
	}

	/* Read global policy */
	if (file == NULL)
		file = systrace_policyfilename(POLICY_PATH, name);

	return (file);
}

int
systrace_addpolicy(const char *name)
{
	char *file;

	if ((file = systrace_getpolicyname(name)) == NULL)
		return (-1);

	return (systrace_readpolicy(file) != NULL ? 0 : -1);
}

/* 
 * Reads policy templates from the template directory.
 * These policies can be inserted during interactive policy
 * generation.
 */

int
systrace_templatedir(void)
{
	char filename[PATH_MAX];
	DIR *dir = NULL;
	struct stat sb;
	struct dirent *dp;
	struct template *template;
	int off;

	TAILQ_INIT(&templates);

	if (userpolicy) {
		if (strlcpy(filename, policydir, sizeof(filename)) >=
		    sizeof(filename))
			goto error;
		if (strlcat(filename, "/templates", sizeof(filename)) >=
		    sizeof(filename))
			goto error;

		/* Check if template directory exists */
		if (stat(filename, &sb) != -1 && S_ISDIR(sb.st_mode))
			dir = opendir(filename);
	}

	/* Read global policy */
	if (dir == NULL) {
		strlcpy(filename, POLICY_PATH, sizeof(filename));
		strlcat(filename, "/templates", sizeof(filename));
		if (stat(filename, &sb) != -1 && S_ISDIR(sb.st_mode))
			dir = opendir(filename);
		if (dir == NULL)
			return (-1);
	}

	if (strlcat(filename, "/", sizeof(filename)) >= sizeof(filename))
		goto error;
	off = strlen(filename);

	while ((dp = readdir(dir)) != NULL) {
		filename[off] = '\0';
		if (strlcat(filename, dp->d_name, sizeof(filename)) >=
		    sizeof(filename))
			goto error;

		if (stat(filename, &sb) == -1 || !S_ISREG(sb.st_mode))
			continue;

		template = systrace_readtemplate(filename, NULL, NULL);
		if (template == NULL)
			continue;

		TAILQ_INSERT_TAIL(&templates, template, next);
	}
	closedir(dir);

	return (0);

 error:
	errx(1, "%s: template name too long", __func__);

	/* NOTREACHED */
}

struct template *
systrace_readtemplate(char *filename, struct policy *policy,
    struct template *template)
{
	FILE *fp;
	char line[_POSIX2_LINE_MAX], *p;
	char *emulation, *name, *description;
	int linenumber = 0;
	
	if ((fp = fopen(filename, "r")) == NULL)
		return (NULL);

	/* Set up pid with current information */
	while (fgets(line, sizeof(line), fp)) {
		linenumber++;

		if ((p = systrace_policyline(line)) == NULL) {
			fprintf(stderr, "%s:%d: input line too long.\n",
			    filename, linenumber);
			template = NULL;
			goto out;
		}

		if (strlen(p) == 0)
			continue;

		if (!strncasecmp(p, "Template: ", 10)) {
			p += 10;
			name = strsep(&p, ",");
			if (p == NULL)
				goto error;
			if (strncasecmp(p, " Emulation: ", 12))
				goto error;
			p += 12;
			emulation = strsep(&p, ", ");
			if (p == NULL)
				goto error;
			if (strncasecmp(p, " Description: ", 14))
				goto error;
			p += 14;
			description = p;

			if (template != NULL)
				continue;
			
			template = calloc(1, sizeof(struct template));
			if (template == NULL)
				err(1, "calloc");

			template->filename = strdup(filename);
			template->name = strdup(name);
			template->emulation = strdup(emulation);
			template->description = strdup(description);

			if (template->filename == NULL ||
			    template->name == NULL ||
			    template->emulation == NULL ||
			    template->description == NULL)
				err(1, "strdup");

			continue;
		}

		if (policy == NULL)
			goto out;

		if (systrace_policyprocess(policy, p) == -1)
			goto error;
	}

 out:
	fclose(fp);
	return (template);

 error:
	fprintf(stderr, "%s:%d: syntax error.\n", filename, linenumber);
	goto out;
}

/*
 * Removes trailing whitespace and comments from the input line
 */

static char *
systrace_policyline(char *line)
{
	char *p;
	int quoted = 0;

	if ((p = strchr(line, '\n')) == NULL)
		return (NULL);
	*p = '\0';

	/* Remove comments from the input line but ignore # that are part
	 * of the system call name or within quotes.
	 */
	for (p = line; *p; p++) {
		if (*p == '"')
			quoted = quoted ? 0 : 1;
		if (*p == '#') {
			if (quoted)
				continue;
			if (p != line && *(p-1) == '-')
				continue;
			*p = '\0';
			break;
		}
	}

	/* Remove trailing white space */
	p = line + strlen(line) - 1;
	while (p > line) {
		if (!isspace((unsigned char)*p))
			break;
		*p-- = '\0';
	}

	/* Ignore white space at start of line */
	p = line;
	p += strspn(p, " \t");

	return (p);
}

/*
 * Parse a single line from a policy and convert it into a policy filter.
 * Predicates are matched.
 */

static int
systrace_policyprocess(struct policy *policy, char *p)
{
	char line[_POSIX2_LINE_MAX];
	char *name, *emulation, *rule;
	struct filter *filter, *parsed;
	short action, future;
	int  resolved = 0, res, isvalid;

	/* Delay predicate evaluation if we are root */

	emulation = strsep(&p, "-");
	if (p == NULL || *p == '\0')
		return (-1);

	if (strcmp(emulation, policy->emulation))
		return (-1);

	name = strsep(&p, ":");
	if (p == NULL || *p != ' ')
		return (-1);

	isvalid = intercept_isvalidsystemcall(emulation, name);

	p++;
	rule = p;

	if ((p = strrchr(p, ',')) != NULL && !strncasecmp(p, ", if", 4)) {
		*p = '\0';
		res = filter_parse_simple(rule, &action, &future);
		*p = ',';
		if (res == 0) {
			/* Need to make a real policy out of it */
			snprintf(line, sizeof(line), "true then %s", rule);
			rule = line;
		}
	} else if (filter_parse_simple(rule, &action, &future) == 0)
		resolved = 1;

	/*
	 * For now, everything that does not seem to be a valid syscall
	 * does not get fast kernel policies even though the aliasing
	 * system supports it.
	 */
	if (resolved && !isvalid) {
		resolved = 0;
		snprintf(line, sizeof(line), "true then %s", rule);
		rule = line;
	}

	/* If the simple parser did not match, try real parser */
	if (!resolved) {
		if (parse_filter(rule, &parsed) == -1)
			return (-1);

		filter_free(parsed);
	}

	filter = calloc(1, sizeof(struct filter));
	if (filter == NULL)
		err(1, "%s:%d: calloc", __func__, __LINE__);

	filter->rule = strdup(rule);
	if (filter->rule == NULL)
		err(1, "%s:%d: strdup", __func__, __LINE__);

	strlcpy(filter->name, name, sizeof(filter->name));
	strlcpy(filter->emulation,  emulation, sizeof(filter->emulation));

	TAILQ_INSERT_TAIL(&policy->prefilters, filter, policy_next);

	return (0);
}

/*
 * Reads security policy from specified file.
 * If policy exists already, this function appends new statements from the
 * file to the existing policy.
 */

struct policy *
systrace_readpolicy(const char *filename)
{
	FILE *fp;
	struct policy *policy;
	char line[_POSIX2_LINE_MAX], *p;
	char *emulation, *name;
	int linenumber = 0;
	int res = -1;

	if ((fp = fopen(filename, "r")) == NULL)
		return (NULL);

	policy = NULL;
	while (fgets(line, sizeof(line), fp)) {
		linenumber++;

		if ((p = systrace_policyline(line)) == NULL) {
			fprintf(stderr, "%s:%d: input line too long.\n",
			    filename, linenumber);
			goto out;
		}

		if (strlen(p) == 0)
			continue;

		if (!strncasecmp(p, "Policy: ", 8)) {
			struct timeval now;

			p += 8;
			name = strsep(&p, ",");
			if (p == NULL)
				goto error;
			if (strncasecmp(p, " Emulation: ", 12))
				goto error;
			p += 12;
			emulation = p;

			policy = systrace_newpolicy(emulation, name);
			if (policy == NULL)
				goto error;

			/* Update access time */
			gettimeofday(&now, NULL);
			TIMEVAL_TO_TIMESPEC(&now, &policy->ts_last);
			continue;
		}

		if (policy == NULL)
			goto error;

		if (!strncasecmp(p, "detached", 8)) {
			policy->flags |= POLICY_DETACHED;
			policy = NULL;
			continue;
		}

		if (systrace_policyprocess(policy, p) == -1)
			goto error;
	}
	res = 0;

 out:
	fclose(fp);
	return (res == -1 ? NULL : policy);

 error:
	fprintf(stderr, "%s:%d: syntax error.\n", filename, linenumber);
	goto out;
}

/*
 * Appends new policy statements if the policy has been updated by
 * another process.  Assumes that policies are append-only.
 *
 * Returns:
 *	-1	if the policy could not be updated.
 *	 0	if the policy has been updated.
 */

int
systrace_updatepolicy(int fd, struct policy *policy)
{
	struct stat sb;
	struct timespec mtimespec;
	int i, policynr = policy->policynr;
	char *file;

	if ((file = systrace_getpolicyname(policy->name)) == NULL)
		return (-1);

	if (stat(file, &sb) == -1)
		return (-1);

	mtimespec = sb.st_mtimespec;

	/* Policy does not need updating */
	if (timespeccmp(&mtimespec, &policy->ts_last, <=))
		return (-1);

	/* Reset the existing policy */
	for (i = 0; i < INTERCEPT_MAXSYSCALLNR; i++) {
		if (policy->kerneltable[i] == ICPOLICY_ASK)
			continue;
		if (intercept_modifypolicy_nr(fd, policynr, i,
		    ICPOLICY_ASK) == -1)
			errx(1, "%s: failed to modify policy for %d",
			    __func__, i);
	}

	/* Now clean up all filter structures in this policy */
	systrace_cleanpolicy(policy);

	/* XXX - This does not deal with Detached and Automatic */
	if (systrace_readpolicy(file) == NULL)
		return (-1);

	/* Resets the changed flag */
	filter_prepolicy(fd, policy);

	return (0);
}

int
systrace_writepolicy(struct policy *policy)
{
	FILE *fp;
	int fd;
	char *p;
	char tmpname[2*PATH_MAX];
	char finalname[2*PATH_MAX];
	struct filter *filter;
	struct timeval now;

	if ((p = systrace_policyfilename(policydir, policy->name)) == NULL)
		return (-1);
	strlcpy(finalname, p, sizeof(finalname));
	if ((p = systrace_policyfilename(policydir, "tmpXXXXXXXX")) == NULL)
		return (-1);
	strlcpy(tmpname, p, sizeof(tmpname));
	if ((fd = mkstemp(tmpname)) == -1 ||
	    (fp = fdopen(fd, "w+")) == NULL) {
		if (fd != -1) {
			unlink(tmpname);
			close(fd);
		}
		return (-1);
	}


	fprintf(fp, "Policy: %s, Emulation: %s\n",
	    policy->name, policy->emulation);
	if (policy->flags & POLICY_DETACHED) {
		fprintf(fp, "detached\n");
	} else {
		TAILQ_FOREACH(filter, &policy->prefilters, policy_next) {
			fprintf(fp, "\t%s-%s: %s\n",
			    filter->emulation, filter->name, filter->rule);
		}
		TAILQ_FOREACH(filter, &policy->filters, policy_next) {
			fprintf(fp, "\t%s-%s: %s\n",
			    filter->emulation, filter->name, filter->rule);
		}
	}
	fprintf(fp, "\n");
	fclose(fp);

	if (rename(tmpname, finalname) == -1) {
		warn("rename(%s, %s)", tmpname, finalname);
		return (-1);
	}

	/* Update access time */
	gettimeofday(&now, NULL);
	TIMEVAL_TO_TIMESPEC(&now, &policy->ts_last);

	return (0);
}

int
systrace_updatepolicies(int fd)
{
	struct policy *policy;

	SPLAY_FOREACH(policy, policytree, &policyroot) {
		/* Check if the policy has been updated */
		systrace_updatepolicy(fd, policy);
	}

	return (0);
}

/*
 * Write policy to disk if it has been changed.  We need to
 * call systrace_updatepolicies() before this, so that we
 * don't clobber changes.
 */

int
systrace_dumppolicies(int fd)
{
	struct policy *policy;

	SPLAY_FOREACH(policy, policytree, &policyroot) {
		if (!(policy->flags & POLICY_CHANGED))
			continue;

		if (systrace_writepolicy(policy) == -1)
			fprintf(stderr, "Failed to write policy for %s\n",
			    policy->name);
		else
			policy->flags &= ~POLICY_CHANGED;
	}

	return (0);
}
