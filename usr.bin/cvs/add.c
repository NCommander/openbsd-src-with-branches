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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"



extern struct cvsroot *cvs_root;



/*
 * cvs_add()
 *
 * Handler for the `cvs add' command.
 * Returns 0 on success, or one of the known system exit codes on failure.
 */

int
cvs_add(int argc, char **argv)
{
	int ch, i, ret;
	char *kflag, *msg;

	kflag = NULL;

	while ((ch = getopt(argc, argv, "k:m:")) != -1) {
		switch (ch) {
		case 'k':
			kflag = optarg;
			break;
		case 'm':
			msg = optarg;
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		return (EX_USAGE);
	}

	for (i = 0; i < argc; i++) {
		/*
		 * XXX figure out if we should send `Modified' or `Is-Modified'
		 * The cvs documentation specifies that Modified should be used
		 * in this case, but GNU CVS sends the latter.
		 */
		ret = cvs_client_sendreq(CVS_REQ_ISMODIFIED, argv[i], 0);
		if (ret < 0)
			return (EX_DATAERR);
	}

	for (i = 0; i < argc; i++) {
		ret = cvs_client_sendreq(CVS_REQ_ARGUMENT, argv[i], 0);
	}

	ret = cvs_client_sendreq(CVS_REQ_ADD, NULL, 0);

	return (0);
}
