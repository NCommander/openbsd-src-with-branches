/*	$OpenBSD: sdl.c,v 1.4 2003/03/03 14:47:37 deraadt Exp $ */
/*
 * Copyright (c) 2003 Bob Beck.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * sdl.c - Implement spamd source lists
 *
 * This consists of everything we need to do to determine which lists
 * someone is on. Spamd gets the connecting address, and looks it up
 * against all lists to determine what deferral messages to feed back
 * to the connecting machine. - The redirection to spamd will happen
 * from pf in the kernel, first macth will rdr to us. Spamd (along with
 * setup) must keep track of *all* matches, so as to tell someone all the
 * lists that they are on.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl.h"

extern int debug;
struct sdlist *blacklists = NULL;
int blc = 0, blu = 0;

int
sdl_add(char *sdname, char *sdstring, char ** addrs, int addrc)
{
	int i, index = -1;
	char astring[40];
	unsigned int maskbits;
	struct sdaddr *m, *n;

	/*
	 * if a blacklist of same tag name is already there, replace it,
	 * otherwise append.
	 */

	for (i = 0; i < blu; i++)
		if (strcmp(blacklists[i].tag, sdname) == 0)
			index = i;
	if (index != -1) {
		if (debug > 0)
			printf("replacing list %s\n", blacklists[index].tag);
		free(blacklists[index].tag);
		blacklists[index].tag = NULL;
		free(blacklists[index].string);
		blacklists[index].string = NULL;
		free(blacklists[index].addrs);
		blacklists[index].addrs = NULL;
	} else {
		if (debug > 0)
			printf("adding list %s\n", sdname);
		index = blu;
	}
	if (index == blu && blu == blc) {
		struct sdlist *tmp;
		tmp = realloc (blacklists, (blc + 128) *
		    sizeof(struct sdlist));
		if (tmp == NULL)
			return (-1);
		blacklists = tmp;
		blc += 128;
	}
	blacklists[index].tag = strdup(sdname);
	blacklists[index].string = strdup(sdstring);
	blacklists[index].naddrs = addrc;

	/*
	 * Cycle through addrs, converting. We assume they are correctly
	 * formatted v4 and v6 addrs, if they don't all convert correcly, the
	 * add fails. Each address should be address/maskbits
	 */
	blacklists[index].addrs = malloc(addrc * sizeof(struct sdentry));
	if (blacklists[index].addrs == NULL)
		return (-1);

	for(i = 0; i < addrc; i++) {
		int j, k, af;

		n = &blacklists[index].addrs[i].sda;
		m = &blacklists[index].addrs[i].sdm;

		j = sscanf(addrs[i], "%39[^/]/%u", astring, &maskbits);
		if (j != 2)
			goto parse_error;
		if (maskbits > 128)
			goto parse_error;
		/*
		 * sanity check! we don't allow a 0 mask -
		 * don't blacklist the entire net.
		 */
		if (maskbits == 0)
			goto parse_error;
		if (strchr(astring, ':') != NULL)
			af = AF_INET6;
		else
			af = AF_INET;
		if (af == AF_INET && maskbits > 32)
			goto parse_error;
		j = inet_pton(af, astring, n);
		if (j != 1)
			goto parse_error;
		if (debug > 0)
			printf("added %s/%u\n", astring, maskbits);

		/* set mask, borrowed from pf */
		k = 0;
		for (j = 0; j < 4; j++)
			m->addr32[j] = 0;
		while (maskbits >= 32) {
			m->addr32[k++] = 0xffffffff;
			maskbits -= 32;
		}
		for (j = 31; j > 31 - maskbits; --j)
			m->addr32[k] |= (1 << j);
		if (maskbits)
			m->addr32[k] = htonl(m->addr32[k]);

		/* mask off address bits that won't ever be used */
		for (j = 0; j < 4; j++)
			n->addr32[j] = n->addr32[j] & m->addr32[j];
	}
	if (index == blu) {
		blu++;
		blacklists[blu].tag = NULL;
	}
	return (0);
 parse_error:
	if (debug > 0)
		printf("sdl_add: parse error, \"%s\"\n", astring);
	return (-1);
}


/*
 * Return 1 if the addresses a (with mask m) matches address b
 * otherwise return 0. It is assumed that address a has been
 * pre-masked out, we only need to mask b.
 */
int
match_addr(struct sdaddr *a, struct sdaddr *m, struct sdaddr *b,
    sa_family_t af)
{
	int	match = 0;

	switch (af) {
	case AF_INET:
		if ((a->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0]))
			match++;
		break;
	case AF_INET6:
		if (((a->addr32[0]) ==
		     (b->addr32[0] & m->addr32[0])) &&
		    ((a->addr32[1]) ==
		     (b->addr32[1] & m->addr32[1])) &&
		    ((a->addr32[2]) ==
		     (b->addr32[2] & m->addr32[2])) &&
		    ((a->addr32[3]) ==
		     (b->addr32[3] & m->addr32[3])))
			match++;
		break;
	}
	return (match);
}


/*
 * Given an address and address family
 * return list of pointers to matching nodes. or NULL if none.
 */
struct sdlist **
sdl_lookup(struct sdlist *head, int af, void * src)
{
	int i, matches = 0;
	struct sdlist *sdl;
	struct sdentry *sda;
	struct sdaddr *source = (struct sdaddr *) src;
	static int sdnewlen = 0;
	static struct sdlist **sdnew = NULL;

	if (head == NULL)
		return (NULL);
	else
		sdl = head;
	while (sdl->tag != NULL) {
		for (i = 0; i < sdl->naddrs; i++) {
			sda = sdl->addrs + i;
			if (match_addr(&sda->sda, &sda->sdm, source, af)) {
				if (matches == sdnewlen) {
					struct sdlist **tmp;

					tmp = realloc(sdnew,
					    (sdnewlen + 128) *
					     sizeof(struct sdlist *));
					if (tmp == NULL)
						/*
						 * XXX out of memory -
						 * return what we have
						 */
						return (sdnew);
					sdnew = tmp;
					sdnewlen += 128;
				}
				sdnew[matches]= sdl;
				matches++;
				sdnew[matches]=NULL;
				break;
			}
		}
		sdl++;
	}
	return (sdnew);
}
