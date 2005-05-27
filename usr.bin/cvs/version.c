/*	$OpenBSD: version.c,v 1.13 2005/05/25 06:22:08 jfb Exp $	*/
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
#include <errno.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"

static int cvs_version_init     (struct cvs_cmd *, int, char **, int *);
static int cvs_version_pre_exec (struct cvsroot *);

struct cvs_cmd cvs_cmd_version = {
	CVS_OP_VERSION, CVS_REQ_VERSION, "version",
	{ "ve", "ver" },
	"Show current CVS version(s)",
	"",
	"",
	NULL,
	0,
	cvs_version_init,
	cvs_version_pre_exec,
	NULL,
	NULL,
	NULL,
	NULL,
	0
};


static int
cvs_version_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	*arg = 1;
	return (0);
}

static int
cvs_version_pre_exec(struct cvsroot *root)
{
	if ((root != NULL) && (root->cr_method != CVS_METHOD_LOCAL))
		printf("Client: ");
	cvs_printf("%s\n", CVS_VERSION);

	if ((root != NULL) && (root->cr_method != CVS_METHOD_LOCAL)) {
		cvs_printf("Server: %s\n", root->cr_version == NULL ?
		    "(unknown)" : root->cr_version);
	}

	return (0);
}
