/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	  must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)getservbyname.c	5.7 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id: getservbyname.c,v 1.3.4.1 1996/02/09 05:39:43 ghudson Exp $";
#endif /* LIBC_SCCS and not lint */

#include <netdb.h>
#include <string.h>
#include "serv_internal.h"

struct servent *getservbyname(const char *name, const char *proto)
{
	char *buf = _serv_buf();
	
	if (!buf)
		return NULL;
	return getservbyname_r(name, proto, (struct servent *) buf,
						   buf + sizeof(struct servent), SERV_BUFSIZE);
}

struct servent *getservbyname_r(const char *name, const char *proto,
								struct servent *result, char *buf, int bufsize)
{
	char **alias;

	pthread_mutex_lock(&serv_iterate_lock);
	setservent(0);
	while ((result = getservent_r(result, buf, bufsize)) != NULL) {
		/* Check the entry's name and aliases against the given name. */
		if (strcmp(result->s_name, name) != 0) {
			for (alias = result->s_aliases; *alias != NULL; alias++) {
				if (strcmp(*alias, name) == 0)
					break;
			}
			if (*alias == NULL)
				continue;
		}
		if (proto == NULL || strcmp(result->s_proto, proto) == 0)
			break;
	}
	pthread_mutex_unlock(&serv_iterate_lock);
	return result;
}

