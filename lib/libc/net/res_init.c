/*	$OpenBSD: res_init.c,v 1.16 1998/03/16 05:07:01 millert Exp $	*/

/*
 * ++Copyright++ 1985, 1989, 1993
 * -
 * Copyright (c) 1985, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)res_init.c	8.1 (Berkeley) 6/7/93";
static char rcsid[] = "$From: res_init.c,v 8.7 1996/09/28 06:51:07 vixie Exp $";
#else
static char rcsid[] = "$OpenBSD: res_init.c,v 1.16 1998/03/16 05:07:01 millert Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <ctype.h>
#include <resolv.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------- info about "sortlist" --------------
 * Marc Majka		1994/04/16
 * Allan Nathanson	1994/10/29 (BIND 4.9.3.x)
 *
 * NetInfo resolver configuration directory support.
 *
 * Allow a NetInfo directory to be created in the hierarchy which
 * contains the same information as the resolver configuration file.
 *
 * - The local domain name is stored as the value of the "domain" property.
 * - The Internet address(es) of the name server(s) are stored as values
 *   of the "nameserver" property.
 * - The name server addresses are stored as values of the "nameserver"
 *   property.
 * - The search list for host-name lookup is stored as values of the
 *   "search" property.
 * - The sortlist comprised of IP address netmask pairs are stored as
 *   values of the "sortlist" property. The IP address and optional netmask
 *   should be seperated by a slash (/) or ampersand (&) character.
 * - Internal resolver variables can be set from the value of the "options"
 *   property.
 */

static void res_setoptions __P((char *, char *));

#ifdef RESOLVSORT
static const char sort_mask[] = "/&";
#define ISSORTMASK(ch) (strchr(sort_mask, ch) != NULL)
static u_int32_t net_mask __P((struct in_addr));
#endif

/*
 * Resolver state default settings.
 */

struct __res_state _res
# if defined(__BIND_RES_TEXT)
	= { RES_TIMEOUT, }	/* Motorola, et al. */
# endif
	;

/*
 * Set up default settings.  If the configuration file exist, the values
 * there will have precedence.  Otherwise, the server address is set to
 * INADDR_ANY and the default domain name comes from the gethostname().
 *
 * An interrim version of this code (BIND 4.9, pre-4.4BSD) used 127.0.0.1
 * rather than INADDR_ANY ("0.0.0.0") as the default name server address
 * since it was noted that INADDR_ANY actually meant ``the first interface
 * you "ifconfig"'d at boot time'' and if this was a SLIP or PPP interface,
 * it had to be "up" in order for you to reach your own name server.  It
 * was later decided that since the recommended practice is to always 
 * install local static routes through 127.0.0.1 for all your network
 * interfaces, that we could solve this problem without a code change.
 *
 * The configuration file should always be used, since it is the only way
 * to specify a default domain.  If you are running a server on your local
 * machine, you should say "nameserver 0.0.0.0" or "nameserver 127.0.0.1"
 * in the configuration file.
 *
 * Return 0 if completes successfully, -1 on error
 */
int
res_init()
{
	register FILE *fp;
	register char *cp, **pp;
	register int n;
	char buf[BUFSIZ];
	int nserv = 0;    /* number of nameserver records read from file */
	int haveenv = 0;
	int havesearch = 0;
	size_t len;
#ifdef RESOLVSORT
	int nsort = 0;
	char *net;
#endif
#ifndef RFC1535
	int dots;
#endif

	/*
	 * These three fields used to be statically initialized.  This made
	 * it hard to use this code in a shared library.  It is necessary,
	 * now that we're doing dynamic initialization here, that we preserve
	 * the old semantics: if an application modifies one of these three
	 * fields of _res before res_init() is called, res_init() will not
	 * alter them.  Of course, if an application is setting them to
	 * _zero_ before calling res_init(), hoping to override what used
	 * to be the static default, we can't detect it and unexpected results
	 * will follow.  Zero for any of these fields would make no sense,
	 * so one can safely assume that the applications were already getting
	 * unexpected results.
	 *
	 * _res.options is tricky since some apps were known to diddle the bits
	 * before res_init() was first called. We can't replicate that semantic
	 * with dynamic initialization (they may have turned bits off that are
	 * set in RES_DEFAULT).  Our solution is to declare such applications
	 * "broken".  They could fool us by setting RES_INIT but none do (yet).
	 */
	if (!_res.retrans)
		_res.retrans = RES_TIMEOUT;
	if (!_res.retry)
		_res.retry = 4;
	if (!(_res.options & RES_INIT))
		_res.options = RES_DEFAULT;

#ifdef USELOOPBACK
	_res.nsaddr.sin_addr = inet_makeaddr(IN_LOOPBACKNET, 1);
#else
	_res.nsaddr.sin_addr.s_addr = INADDR_ANY;
#endif
	_res.nsaddr.sin_family = AF_INET;
	_res.nsaddr.sin_port = htons(NAMESERVER_PORT);
	_res.nscount = 1;
	_res.ndots = 1;
	_res.pfcode = 0;
	strncpy(_res.lookups, "f", sizeof _res.lookups);

	/* Allow user to override the local domain definition */
	if (issetugid() == 0 && (cp = getenv("LOCALDOMAIN")) != NULL) {
		(void)strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		_res.defdname[sizeof(_res.defdname) - 1] = '\0';
		haveenv++;

		/*
		 * Set search list to be blank-separated strings
		 * from rest of env value.  Permits users of LOCALDOMAIN
		 * to still have a search list, and anyone to set the
		 * one that they want to use as an individual (even more
		 * important now that the rfc1535 stuff restricts searches)
		 */
		cp = _res.defdname;
		pp = _res.dnsrch;
		*pp++ = cp;
		for (n = 0; *cp && pp < _res.dnsrch + MAXDNSRCH; cp++) {
			if (*cp == '\n')	/* silly backwards compat */
				break;
			else if (*cp == ' ' || *cp == '\t') {
				*cp = 0;
				n = 1;
			} else if (n) {
				*pp++ = cp;
				n = 0;
				havesearch = 1;
			}
		}
		/* null terminate last domain if there are excess */
		while (*cp != '\0' && *cp != ' ' && *cp != '\t' && *cp != '\n')
			cp++;
		*cp = '\0';
		*pp++ = 0;
	}

#define	MATCH(line, name) \
	(!strncmp(line, name, sizeof(name) - 1) && \
	(line[sizeof(name) - 1] == ' ' || \
	 line[sizeof(name) - 1] == '\t'))

	if ((fp = fopen(_PATH_RESCONF, "r")) != NULL) {
	    strncpy(_res.lookups, "bf", sizeof _res.lookups);

	    /* read the config file */
	    buf[0] = '\0';
	    while ((cp = fgetln(fp, &len)) != NULL) {
		/* skip lines that are too long or zero length */
		if (len >= sizeof(buf) || len == 0)
		    continue;
		(void)memcpy(buf, cp, len);
		buf[len] = '\0';
		/* skip comments */
		if ((cp = strpbrk(buf, ";#")) != NULL)
			*cp = '\0';
		if (buf[0] == '\0')
			continue;
		/* read default domain name */
		if (MATCH(buf, "domain")) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("domain") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		    _res.defdname[sizeof(_res.defdname) - 1] = '\0';
		    if ((cp = strpbrk(_res.defdname, " \t\n")) != NULL)
			    *cp = '\0';
		    havesearch = 0;
		    continue;
		}
		/* lookup types */
		if (MATCH(buf, "lookup")) {
		    char *sp = NULL;

		    bzero(_res.lookups, sizeof _res.lookups);
		    cp = buf + sizeof("lookup") - 1;
		    for (n = 0;; cp++) {
		    	    if (n == MAXDNSLUS)
				    break;
			    if ((*cp == '\0') || (*cp == '\n')) {
				    if (sp) {
					    if (*sp=='y' || *sp=='b' || *sp=='f')
						    _res.lookups[n++] = *sp;
					    sp = NULL;
				    }
				    break;
			    } else if ((*cp == ' ') || (*cp == '\t') || (*cp == ',')) {
				    if (sp) {
					    if (*sp=='y' || *sp=='b' || *sp=='f')
						    _res.lookups[n++] = *sp;
					    sp = NULL;
				    }
			    } else if (sp == NULL)
				    sp = cp;
		    }
		    continue;
		}
		/* set search list */
		if (MATCH(buf, "search")) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("search") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		    _res.defdname[sizeof(_res.defdname) - 1] = '\0';
		    if ((cp = strchr(_res.defdname, '\n')) != NULL)
			    *cp = '\0';
		    /*
		     * Set search list to be blank-separated strings
		     * on rest of line.
		     */
		    cp = _res.defdname;
		    pp = _res.dnsrch;
		    *pp++ = cp;
		    for (n = 0; *cp && pp < _res.dnsrch + MAXDNSRCH; cp++) {
			    if (*cp == ' ' || *cp == '\t') {
				    *cp = 0;
				    n = 1;
			    } else if (n) {
				    *pp++ = cp;
				    n = 0;
			    }
		    }
		    /* null terminate last domain if there are excess */
		    while (*cp != '\0' && *cp != ' ' && *cp != '\t')
			    cp++;
		    *cp = '\0';
		    *pp++ = 0;
		    havesearch = 1;
		    continue;
		}
		/* read nameservers to query */
		if (MATCH(buf, "nameserver") && nserv < MAXNS) {
		    struct in_addr a;

		    cp = buf + sizeof("nameserver") - 1;
		    while (*cp == ' ' || *cp == '\t')
			cp++;
		    if ((*cp != '\0') && (*cp != '\n') && inet_aton(cp, &a)) {
			_res.nsaddr_list[nserv].sin_addr = a;
			_res.nsaddr_list[nserv].sin_family = AF_INET;
			_res.nsaddr_list[nserv].sin_port =
				htons(NAMESERVER_PORT);
			nserv++;
		    }
		    continue;
		}
#ifdef RESOLVSORT
		if (MATCH(buf, "sortlist")) {
		    struct in_addr a;

		    cp = buf + sizeof("sortlist") - 1;
		    while (nsort < MAXRESOLVSORT) {
			while (*cp == ' ' || *cp == '\t')
			    cp++;
			if (*cp == '\0' || *cp == '\n' || *cp == ';')
			    break;
			net = cp;
			while (*cp && !ISSORTMASK(*cp) && *cp != ';' &&
			       isascii(*cp) && !isspace(*cp))
				cp++;
			n = *cp;
			*cp = 0;
			if (inet_aton(net, &a)) {
			    _res.sort_list[nsort].addr = a;
			    if (ISSORTMASK(n)) {
				*cp++ = n;
				net = cp;
				while (*cp && *cp != ';' &&
					isascii(*cp) && !isspace(*cp))
				    cp++;
				n = *cp;
				*cp = 0;
				if (inet_aton(net, &a)) {
				    _res.sort_list[nsort].mask = a.s_addr;
				} else {
				    _res.sort_list[nsort].mask = 
					net_mask(_res.sort_list[nsort].addr);
				}
			    } else {
				_res.sort_list[nsort].mask = 
				    net_mask(_res.sort_list[nsort].addr);
			    }
			    nsort++;
			}
			*cp = n;
		    }
		    continue;
		}
#endif
		if (MATCH(buf, "options")) {
		    res_setoptions(buf + sizeof("options") - 1, "conf");
		    continue;
		}
	    }
	    if (nserv > 1) 
		_res.nscount = nserv;
#ifdef RESOLVSORT
	    _res.nsort = nsort;
#endif
	    (void) fclose(fp);
	}
	if (_res.defdname[0] == 0 &&
	    gethostname(buf, sizeof(_res.defdname) - 1) == 0 &&
	    (cp = strchr(buf, '.')) != NULL)
	{
		strncpy(_res.defdname, cp + 1,
		        sizeof(_res.defdname) - 1);
		_res.defdname[sizeof(_res.defdname) - 1] = '\0';
	}

	/* find components of local domain that might be searched */
	if (havesearch == 0) {
		pp = _res.dnsrch;
		*pp++ = _res.defdname;
		*pp = NULL;

#ifndef RFC1535
		dots = 0;
		for (cp = _res.defdname; *cp; cp++)
			dots += (*cp == '.');

		cp = _res.defdname;
		while (pp < _res.dnsrch + MAXDFLSRCH) {
			if (dots < LOCALDOMAINPARTS)
				break;
			cp = strchr(cp, '.') + 1;    /* we know there is one */
			*pp++ = cp;
			dots--;
		}
		*pp = NULL;
#ifdef DEBUG
		if (_res.options & RES_DEBUG) {
			printf(";; res_init()... default dnsrch list:\n");
			for (pp = _res.dnsrch; *pp; pp++)
				printf(";;\t%s\n", *pp);
			printf(";;\t..END..\n");
		}
#endif /* DEBUG */
#endif /* !RFC1535 */
	}

	if (issetugid())
		_res.options |= RES_NOALIASES;
	else if ((cp = getenv("RES_OPTIONS")) != NULL)
		res_setoptions(cp, "env");
	_res.options |= RES_INIT;
	return (0);
}

/* ARGSUSED */
static void
res_setoptions(options, source)
	char *options, *source;
{
	char *cp = options;
	char *endp;
	long l;

#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf(";; res_setoptions(\"%s\", \"%s\")...\n",
		       options, source);
#endif
	while (*cp) {
		/* skip leading and inner runs of spaces */
		while (*cp == ' ' || *cp == '\t')
			cp++;
		/* search for and process individual options */
		if (!strncmp(cp, "ndots:", sizeof("ndots:") - 1)) {
			char *p = cp + sizeof("ndots:") - 1;
			l = strtol(p, &endp, 10);
			if (l >= 0 && endp != p &&
			    (*endp = '\0' || issapce(*endp))) {
				if (l <= RES_MAXNDOTS)
					_res.ndots = l;
				else
					_res.ndots = RES_MAXNDOTS;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf(";;\tndots=%d\n", _res.ndots);
#endif
			}
		} else if (!strncmp(cp, "debug", sizeof("debug") - 1)) {
#ifdef DEBUG
			if (!(_res.options & RES_DEBUG)) {
				printf(";; res_setoptions(\"%s\", \"%s\")..\n",
				       options, source);
				_res.options |= RES_DEBUG;
			}
			printf(";;\tdebug\n");
#endif
		} else if (!strncmp(cp, "inet6", sizeof("inet6") - 1)) {
			_res.options |= RES_USE_INET6;
		} else {
			/* XXX - print a warning here? */
		}
		/* skip to next run of spaces */
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
	}
}

#ifdef RESOLVSORT
/* XXX - should really support CIDR which means explicit masks always. */
static u_int32_t
net_mask(in)		/* XXX - should really use system's version of this */
	struct in_addr in;
{
	register u_int32_t i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return (htonl(IN_CLASSA_NET));
	else if (IN_CLASSB(i))
		return (htonl(IN_CLASSB_NET));
	return (htonl(IN_CLASSC_NET));
}
#endif
