/*	$OpenBSD$	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
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

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <paths.h>

#include "cvs.h"
#include "log.h"


extern char *cvs_rootstr;


/* keep these ordered with the defines */
const char *cvs_methods[] = {
	"",
	"local",
	"ssh",
	"pserver",
	"kserver",
	"gserver",
	"ext",
	"fork",
};

#define CVS_NBMETHODS  (sizeof(cvs_methods)/sizeof(cvs_methods[0]))
 


/*
 * cvsroot_parse()
 *
 * Parse a CVS root string (as found in CVS/Root files or the CVSROOT
 * environment variable) and store the fields in a dynamically
 * allocated cvs_root structure.  The format of the string is as follows:
 *	:method:path
 * Returns a pointer to the allocated information on success, or NULL
 * on failure.
 */

struct cvsroot*
cvsroot_parse(const char *str)
{
	u_int i;
	char *cp, *sp, *pp;
	struct cvsroot *root;

	root = (struct cvsroot *)malloc(sizeof(*root));
	if (root == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS root data");
		return (NULL);
	}
	memset(root, 0, sizeof(*root));

	root->cr_method = CVS_METHOD_NONE;

	root->cr_buf = strdup(str);
	if (root->cr_buf == NULL) {
		cvs_log(LP_ERRNO, "failed to copy CVS root");
		free(root);
		return (NULL);
	}

	sp = root->cr_buf;
	cp = root->cr_buf;

	if (*sp == ':') {
		sp++;
		cp = strchr(sp, ':');
		if (cp == NULL) {
			cvs_log(LP_ERR, "failed to parse CVSROOT: "
			    "unterminated method");
			free(root->cr_buf);
			free(root);
			return (NULL);
		}
		*(cp++) = '\0';

		for (i = 0; i < CVS_NBMETHODS; i++) {
			if (strcmp(sp, cvs_methods[i]) == 0) {
				root->cr_method = i;
				break;
			}
		}
	}

	/* find the start of the actual path */
	sp = strchr(cp, '/');
	if (sp == NULL) {
		cvs_log(LP_ERR, "no path specification in CVSROOT");
		free(root->cr_buf);
		free(root);
		return (NULL);
	}

	root->cr_dir = sp;
	if (sp == cp) {
		if (root->cr_method == CVS_METHOD_NONE)
			root->cr_method = CVS_METHOD_LOCAL;
		/* stop here, it's just a path */
		return (root);
	}

	if (*(sp - 1) != ':') {
		cvs_log(LP_ERR, "missing host/path delimiter in CVS root");
		free(root);
		return (NULL);
	}
	*(sp - 1) = '\0';

	/*
	 * looks like we have more than just a directory path, so
	 * attempt to split it into user and host parts
	 */
	sp = strchr(cp, '@');
	if (sp != NULL) {
		*(sp++) = '\0';

		/* password ? */
		pp = strchr(cp, ':');
		if (pp != NULL) {
			*(pp++) = '\0';
			root->cr_pass = pp;
		}

		root->cr_user = cp;
	}

	pp = strchr(sp, ':');
	if (pp != NULL) {
		*(pp++) = '\0';
		root->cr_port = (u_int)strtol(pp, &cp, 10);
		if (*cp != '\0' || root->cr_port > 65535) {
			cvs_log(LP_ERR,
			    "invalid port specification in CVSROOT");
			free(root);
			return (NULL);
		}

	}

	root->cr_host = sp;

	if (root->cr_method == CVS_METHOD_NONE) {
		/* no method found from start of CVSROOT, guess */
		if (root->cr_host != NULL)
			root->cr_method = CVS_METHOD_SERVER;
		else
			root->cr_method = CVS_METHOD_LOCAL;
	}

	return (root);
}


/*
 * cvsroot_free()
 *
 * Free a CVSROOT structure previously allocated and returned by
 * cvsroot_parse().
 */

void
cvsroot_free(struct cvsroot *root)
{
	free(root->cr_buf);
	free(root);
}


/*
 * cvsroot_get()
 *
 * Get the CVSROOT information for a specific directory <dir>.  The
 * value is taken from one of 3 possible sources (in order of precedence):
 *
 * 1) the `-d' command-line option
 * 2) the CVS/Root file found in checked-out trees
 * 3) the CVSROOT environment variable
 */

struct cvsroot*
cvsroot_get(const char *dir)
{
	size_t len;
	char rootpath[MAXPATHLEN], *rootstr, *line;
	FILE *fp;
	struct cvsroot *rp;

	if (cvs_rootstr != NULL)
		return cvsroot_parse(cvs_rootstr);

	snprintf(rootpath, sizeof(rootpath), "%s/" CVS_PATH_ROOTSPEC, dir);
	fp = fopen(rootpath, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			/* try env as a last resort */
			if ((rootstr = getenv("CVSROOT")) != NULL)
				return cvsroot_parse(rootstr);
			else
				return (NULL);
		}
		else {
			cvs_log(LP_ERRNO, "failed to open CVS/Root");
			return (NULL);
		}
	}

	line = fgetln(fp, &len);
	if (line == NULL) {
		cvs_log(LP_ERR, "failed to read CVSROOT line from CVS/Root");
		(void)fclose(fp);
	}

	/* line is not NUL-terminated, but we don't need to allocate an
	 * extra byte because we don't want the trailing newline.  It will
	 * get replaced by a \0.
	 */
	rootstr = (char *)malloc(len);
	if (rootstr == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVSROOT string");
		(void)fclose(fp);
		return (NULL);
	}
	memcpy(rootstr, line, len - 1);
	rootstr[len - 1] = '\0';
	rp = cvsroot_parse(rootstr);

	(void)fclose(fp);
	free(rootstr);

	return (rp);
}
