/*	$OpenBSD: filter.c,v 1.3 2002/06/04 19:15:54 deraadt Exp $	*/
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <err.h>

#include "intercept.h"
#include "systrace.h"

extern int connected;
extern char cwd[];

int
filter_match(struct intercept_tlq *tls, struct logic *logic)
{
	struct intercept_translate *tl;
	int off = 0;

	switch (logic->op) {
	case LOGIC_NOT:
		return (!filter_match(tls, logic->left));
	case LOGIC_OR:
		if (filter_match(tls, logic->left))
			return (1);
		return (filter_match(tls, logic->right));
	case LOGIC_AND:
		if (!filter_match(tls, logic->left))
			return (0);
		return (filter_match(tls, logic->right));
	default:
		break;
	}

	/* Now we just have a logic single */
	if (logic->type == NULL)
		goto match;

	TAILQ_FOREACH(tl, tls, next) {
		if (!tl->trans_valid)
			return (0);

		if (strcasecmp(tl->name, logic->type))
			continue;

		if (logic->typeoff == -1 || logic->typeoff == off)
			break;

		off++;
	}

	if (tl == NULL)
		return (0);

 match:
	return (logic->filter_match(tl, logic));
}

short
filter_evaluate(struct intercept_tlq *tls, struct filterq *fls, int *pflags)
{
	struct filter *filter;
	short action;

	TAILQ_FOREACH(filter, fls, next) {
		if (filter_match(tls, filter->logicroot)) {
			action = filter->match_action;
			if (action == ICPOLICY_NEVER)
				action = filter->match_error;
			*pflags = filter->match_flags;
			return (action);
		}
	}

	return (ICPOLICY_ASK);
}

void
logic_free(struct logic *logic)
{
	if (logic->left)
		logic_free(logic->left);
	if (logic->right)
		logic_free(logic->right);
	if (logic->type)
		free(logic->type);
	if (logic->filterdata)
		free(logic->filterdata);
	free(logic);
}

void
filter_free(struct filter *filter)
{
	if (filter->logicroot)
		logic_free(filter->logicroot);
	if (filter->rule)
		free(filter->rule);
	free(filter);
}

void
filter_review(struct filterq *fls)
{
	struct filter *filter;
	int i = 0;

	printf("Filter review:\n");

	TAILQ_FOREACH(filter, fls, next) {
		i++;
		printf("%d. %s\n", i, filter->rule);
	}
}

void
filter_policyrecord(struct policy *policy, struct filter *filter,
    char *emulation, char *name, char *rule)
{
	/* Record the filter in the policy */
	if (filter == NULL) {
		filter = calloc(1, sizeof(struct filter));
		if (filter == NULL)
			err(1, "%s:%d: calloc", __func__, __LINE__);
		if ((filter->rule = strdup(rule)) == NULL)
			err(1, "%s:%d: strdup", __func__, __LINE__);
	}

	strlcpy(filter->name, name, sizeof(filter->name));
	strlcpy(filter->emulation, emulation, sizeof(filter->emulation));

	TAILQ_INSERT_TAIL(&policy->filters, filter, policy_next);
	policy->nfilters++;

	policy->flags |= POLICY_CHANGED;
}

int
filter_parse(char *line, struct filter **pfilter)
{
	char *rule;

	if (parse_filter(line, pfilter) == -1)
		return (-1);

	if ((rule = strdup(line)) == NULL)
		err(1, "%s:%d: strdup", __func__, __LINE__);

	(*pfilter)->rule = rule;

	return (0);
}

/* Translate a simple action like "permit" or "deny[einval]" to numbers */

int
filter_parse_simple(char *rule, short *paction, short *pfuture)
{
	char buf[1024];
	int isfuture = 1;
	char *line, *p;

	strlcpy(buf, rule, sizeof(buf));
	line = buf;

	if (!strcmp("permit", line)) {
		*paction = *pfuture = ICPOLICY_PERMIT;
		return (0);
	} else if (!strcmp("permit-now", line)) {
		*paction = ICPOLICY_PERMIT;
		return (0);
	} else if (strncmp("deny", line, 4))
		return (-1);

	line +=4 ;
	if (!strncmp("-now", line, 4)) {
		line += 4;
		isfuture = 0;
	}

	*paction = ICPOLICY_NEVER;

	switch (line[0]) {
	case '\0':
		break;
	case '[':
		line++;
		p = strsep(&line, "]");
		if (line == NULL || *line != '\0')
			return (-1);

		*paction = systrace_error_translate(p);
		if (*paction == -1)
			return (-1);
		break;
	default:
		return (-1);
	}

	if (isfuture)
		*pfuture = *paction;

	return (NULL);
}

int
filter_prepolicy(int fd, struct policy *policy)
{
	int res;
	struct filter *filter, *parsed;
	struct filterq *fls;
	short action, future;

	/* Commit all matching pre-filters */
	for (filter = TAILQ_FIRST(&policy->prefilters);
	    filter; filter = TAILQ_FIRST(&policy->prefilters)) {
		future = ICPOLICY_ASK;

		TAILQ_REMOVE(&policy->prefilters, filter, policy_next);

		res = 0;
		parsed = NULL;
		/* Special rules that are not real filters */
		if (filter_parse_simple(filter->rule, &action, &future) == -1)
			res = filter_parse(filter->rule, &parsed);
		if (res == -1)
			errx(1, "%s:%d: can not parse \"%s\"",
			    __func__, __LINE__, filter->rule);

		if (future == ICPOLICY_ASK) {
			fls = systrace_policyflq(policy, policy->emulation,
			    filter->name);
			TAILQ_INSERT_TAIL(fls, parsed, next);
		} else {
			res = systrace_modifypolicy(fd, policy->policynr,
			    filter->name, future);
			if (res == -1)
				errx(1, "%s:%d: modify policy for \"%s\"",
				    __func__, __LINE__, filter->rule);
		}
		filter_policyrecord(policy, parsed, policy->emulation,
		    filter->name, filter->rule);

		filter_free(filter);
	}

	/* Existing policy applied undo changed flag */
	policy->flags &= ~POLICY_CHANGED;

	return (0);
}

short
filter_ask(struct intercept_tlq *tls, struct filterq *fls,
    int policynr, char *emulation, char *name,
    char *output, short *pfuture, int *pflags)
{
	char line[1024], *p;
	struct filter *filter;
	struct policy *policy;
	short action;
	int first = 0;

	*pfuture = ICPOLICY_ASK;
	*pflags = 0;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		errx(1, "%s:%d: no policy %d\n", __func__, __LINE__,
		    policynr);

	printf("%s\n", output);

	while (1) {
		filter = NULL;

		if (!connected)
			printf("Answer: ");
		else {
			/* Do not prompt the first time */
			if (first) {
				printf("WRONG\n");
			}
			first = 1;
		}

		fgets(line, sizeof(line), stdin);
		p = line;
		strsep(&p, "\n");

		/* Simple keywords */
		if (!strcasecmp(line, "detach")) {
			if (policy->nfilters) {
				policy->flags |= POLICY_UNSUPERVISED;
				action = ICPOLICY_NEVER;
			} else {
				policy->flags |= POLICY_DETACHED;
				policy->flags |= POLICY_CHANGED;
				action = ICPOLICY_PERMIT;
			}
			goto out;
		} else if (!strcasecmp(line, "kill")) {
			action = ICPOLICY_KILL;
			goto out;
		} else if (!strcasecmp(line, "review") && fls != NULL) {
			filter_review(fls);
			continue;
		}

		if (filter_parse_simple(line, &action, pfuture) != -1) {
			if (*pfuture == ICPOLICY_ASK)
				goto out;
			break;
		}

		if (fls == NULL) {
			printf("Syntax error.\n");
			continue;
		}

		if (filter_parse(line, &filter) == -1)
			continue;

		TAILQ_INSERT_TAIL(fls, filter, next);
		action = filter_evaluate(tls, fls, pflags);
		if (action == ICPOLICY_ASK) {
			TAILQ_REMOVE(fls, filter, next);
			printf("Filter unmatched. Freeing it\n");
			filter_free(filter);
			continue;
		}

		break;
	}

	filter_policyrecord(policy, filter, emulation, name, line);

 out:
	if (connected)
		printf("OKAY\n");
	return (action);

}

void
filter_replace(char *buf, size_t buflen, char *match, char *repl)
{
	while (strrpl(buf, buflen, match, repl) != NULL)
		;
}

char *
filter_expand(char *data)
{
	static char expand[2*MAXPATHLEN];
	char *what;

	if (data != NULL)
		strlcpy(expand, data, sizeof(expand));

	what = getenv("HOME");
	if (what != NULL)
		filter_replace(expand, sizeof(expand), "$HOME", what);
	what = getenv("USER");
	if (what != NULL)
		filter_replace(expand, sizeof(expand), "$USER", what);

	filter_replace(expand, sizeof(expand), "$CWD", cwd);

	return (expand);
}

int
filter_fnmatch(struct intercept_translate *tl, struct logic *logic)
{
	int res;
	char *line;

	if (tl->trans_size == 0)
		return (0);

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);
	res = fnmatch(logic->filterdata, line, FNM_PATHNAME | FNM_LEADING_DIR);

	return (res == 0);
}

int
filter_substrmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	return (strstr(line, logic->filterdata) != NULL);
}

int
filter_negsubstrmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	return (strstr(line, logic->filterdata) == NULL);
}

int
filter_stringmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (0);

	return (!strcasecmp(line, logic->filterdata));
}

int
filter_negstringmatch(struct intercept_translate *tl, struct logic *logic)
{
	char *line;

	if ((line = intercept_translate_print(tl)) == NULL)
		return (1);

	return (strcasecmp(line, logic->filterdata) != 0);
}

int
filter_true(struct intercept_translate *tl, struct logic *logic)
{
	return (1);
}
