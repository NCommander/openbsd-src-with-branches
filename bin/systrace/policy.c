/*	$OpenBSD$	*/
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
#include <sys/param.h>

#include <sys/stat.h>
#include <sys/tree.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>

#include "intercept.h"
#include "systrace.h"

static int
psccompare(struct policy_syscall *a, struct policy_syscall *b)
{
	int diff;
	diff = strcmp(a->emulation, b->emulation);
	if (diff)
		return (diff);
	return (strcmp(a->name, b->name));
}

SPLAY_PROTOTYPE(syscalltree, policy_syscall, node, psccompare);
SPLAY_GENERATE(syscalltree, policy_syscall, node, psccompare);

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

SPLAY_PROTOTYPE(policytree, policy, node, policycompare);
SPLAY_GENERATE(policytree, policy, node, policycompare);

SPLAY_PROTOTYPE(polnrtree, policy, nrnode, polnrcompare);
SPLAY_GENERATE(polnrtree, policy, nrnode, polnrcompare);

char policydir[MAXPATHLEN];

void
systrace_setupdir(void)
{
	char *home;
	struct stat sb;

	home = getenv("HOME");

	if (home == NULL)
		errx(1, "No HOME environment set");

	if (strlcpy(policydir, home, sizeof(policydir)) >= sizeof(policydir))
		errx(1, "HOME too long");

	if (strlcat(policydir, "/.systrace", sizeof(policydir)) >= sizeof(policydir))
		errx(1, "HOME too long");

	if (stat(policydir, &sb) != -1) {
		if (!(sb.st_mode & S_IFDIR))
			errx(1, "Not a directory: \"%s\"", policydir);
	} else if (mkdir(policydir, 0700) == -1)
		err(1, "mdkdir(%s)", policydir);
}

int
systrace_initpolicy(char *file)
{
	SPLAY_INIT(&policyroot);
	SPLAY_INIT(&polnrroot);

	systrace_setupdir();

	if (file != NULL)
		return (systrace_readpolicy(file));

	return (0);
}

struct policy *
systrace_findpolicy(char *name)
{
	struct policy tmp;

	tmp.name = name;

	return (SPLAY_FIND(policytree, &policyroot, &tmp));
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
		free(tmp);
		return (-1);
	}

	SPLAY_INSERT(polnrtree, &polnrroot, tmp);

	return (tmp->policynr);
}

struct policy *
systrace_newpolicy(char *emulation, char *name)
{
	struct policy *tmp;

	if ((tmp = systrace_findpolicy(name)) != NULL)
		return (tmp);

	tmp = calloc(1, sizeof(struct policy));
	if (tmp == NULL)
		return (NULL);

	tmp->policynr = -1;

	/* New policies requires intialization */
	if ((tmp->name = strdup(name)) == NULL)
		err(1, "%s:%d: strdup", __FUNCTION__, __LINE__);
	strlcpy(tmp->emulation, emulation, sizeof(tmp->emulation));

	SPLAY_INSERT(policytree, &policyroot, tmp);
	SPLAY_INIT(&tmp->pflqs);
	TAILQ_INIT(&tmp->filters);
	TAILQ_INIT(&tmp->prefilters);

	return (tmp);
}

struct filterq *
systrace_policyflq(struct policy *policy, char *emulation, char *name)
{
	struct policy_syscall tmp2, *tmp;

	strlcpy(tmp2.emulation, emulation, sizeof(tmp2.emulation));
	strlcpy(tmp2.name, name, sizeof(tmp2.name));

	tmp = SPLAY_FIND(syscalltree, &policy->pflqs, &tmp2);
	if (tmp != NULL)
		return (&tmp->flq);

	if ((tmp = calloc(1, sizeof(struct policy_syscall))) == NULL)
		err(1, "%s:%d: out of memory", __FUNCTION__, __LINE__);

	strlcpy(tmp->emulation, emulation, sizeof(tmp->emulation));
	strlcpy(tmp->name, name, sizeof(tmp->name));
	TAILQ_INIT(&tmp->flq);

	SPLAY_INSERT(syscalltree, &policy->pflqs, tmp);

	return (&tmp->flq);
}

int
systrace_modifypolicy(int fd, int policynr, char *name, short action)
{
	struct policy *policy;
	int res;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		return (-1);

	res = intercept_modifypolicy(fd, policynr, policy->emulation,
		    name, action);

	return (res);
}

char *
systrace_policyfilename(char *dirname, char *name)
{
	static char file[MAXPATHLEN];
	char *p;
	int i, plen;

	if (strlen(name) + strlen(dirname) + 1 >= sizeof(file))
		return (NULL);

	strlcpy(file, dirname, sizeof(file));
	i = strlen(file);
	file[i++] = '/';
	plen = i;

	p = name;
	while (*p) {
		if (!isalnum(*p)) {
			if (i != plen)
				file[i++] = '_';
		} else
			file[i++] = *p;
		p++;
	}

	file[i] = '\0';

	return (file);
}

int
systrace_addpolicy(char *name)
{
	char *file;

	if ((file = systrace_policyfilename(policydir, name)) == NULL)
		return (-1);
	/* Check if the user policy file exists */
	if (access(file, R_OK) == -1) {
		file = systrace_policyfilename(POLICY_PATH, name);
		if (file == NULL)
			return (-1);
	}

	return (systrace_readpolicy(file));
}

int
systrace_readpolicy(char *filename)
{
	FILE *fp;
	struct policy *policy;
	char line[1024], *p;
	int linenumber = 0;
	char *name, *emulation, *rule;
	struct filter *filter, *parsed;
	short action, future;
	int res = -1;

	if ((fp = fopen(filename, "r")) == NULL)
		return (-1);

	policy = NULL;
	while (fgets(line, sizeof(line), fp)) {
		linenumber++;
		if ((p = strchr(line, '\n')) == NULL) {
			fprintf(stderr, "%s:%d: input line too long.\n",
			    filename, linenumber);
			goto out;
		}
		*p = '\0';
		
		p = line;
		strsep(&p, "#");

		p = line;
		p += strspn(p, " \t");
		if (strlen(p) == 0)
			continue;

		if (!strncasecmp(p, "Policy: ", 8)) {
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
			continue;
		}
		
		if (policy == NULL)
			goto error;

		if (!strncasecmp(p, "detached", 8)) {
			policy->flags |= POLICY_DETACHED;
			policy = NULL;
			continue;
		}

		emulation = strsep(&p, "-");
		if (p == NULL || *p == '\0')
			goto error;

		if (strcmp(emulation, policy->emulation))
			goto error;

		name = strsep(&p, ":");
		if (p == NULL || *p != ' ')
			goto error;
		p++;
		rule = p;

		if (filter_parse_simple(rule, &action, &future) == -1) {
			if (parse_filter(rule, &parsed) == -1)
				goto error;
			filter_free(parsed);
		}

		filter = calloc(1, sizeof(struct filter));
		if (filter == NULL)
			err(1, "%s:%d: calloc", __FUNCTION__, __LINE__);
		
		filter->rule = strdup(rule);
		strlcpy(filter->name, name, sizeof(filter->name));
		strlcpy(filter->emulation,emulation,sizeof(filter->emulation));

		TAILQ_INSERT_TAIL(&policy->prefilters, filter, policy_next);
	}
	res = 0;

 out:
	fclose(fp);
	return (res);

 error:
	fprintf(stderr, "%s:%d: systax error.\n",
	    filename, linenumber);
	goto out;
}

int
systrace_writepolicy(struct policy *policy)
{
	FILE *fp;
	int fd;
	char *p;
	char tmpname[MAXPATHLEN];
	char finalname[MAXPATHLEN];
	struct filter *filter;

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

	return (0);
}

int
systrace_dumppolicy(void)
{
	struct policy *policy;

	SPLAY_FOREACH(policy, policytree, &policyroot) {
		if (!(policy->flags & POLICY_CHANGED))
			continue;

		if (systrace_writepolicy(policy) == -1)
			fprintf(stderr, "Failed to write policy for %s\n",
			    policy->name);
	}

	return (0);
}
