/*	$OpenBSD: hosts_access.c,v 1.5 2000/02/01 03:23:17 deraadt Exp $	*/

 /*
  * This module implements a simple access control language that is based on
  * host (or domain) names, NIS (host) netgroup names, IP addresses (or
  * network numbers) and daemon process names. When a match is found the
  * search is terminated, and depending on whether PROCESS_OPTIONS is defined,
  * a list of options is executed or an optional shell command is executed.
  * 
  * Host and user names are looked up on demand, provided that suitable endpoint
  * information is available as sockaddr_in structures or TLI netbufs. As a
  * side effect, the pattern matching process may change the contents of
  * request structure fields.
  * 
  * Diagnostics are reported through syslog(3).
  * 
  * Compile with -DNETGROUP if your library provides support for netgroups.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccsid[] = "@(#) hosts_access.c 1.21 97/02/12 02:13:22";
#else
static char rcsid[] = "$OpenBSD: hosts_access.c,v 1.5 2000/02/01 03:23:17 deraadt Exp $";
#endif
#endif

/* System libraries. */

#include <sys/types.h>
#include <sys/param.h>
#ifdef INET6
#include <sys/socket.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#ifdef NETGROUP
#include <netgroup.h>
#endif


#ifndef	INADDR_NONE
#define	INADDR_NONE	(-1)		/* XXX should be 0xffffffff */
#endif

/* Local stuff. */

#include "tcpd.h"

/* Error handling. */

extern jmp_buf tcpd_buf;

/* Delimiters for lists of daemons or clients. */

static char sep[] = ", \t\r\n";

/* Constants to be used in assignments only, not in comparisons... */

#define	YES		1
#define	NO		0

 /*
  * These variables are globally visible so that they can be redirected in
  * verification mode.
  */

char   *hosts_allow_table = HOSTS_ALLOW;
char   *hosts_deny_table = HOSTS_DENY;
int     hosts_access_verbose = 0;

 /*
  * In a long-running process, we are not at liberty to just go away.
  */

int     resident = (-1);		/* -1, 0: unknown; +1: yes */

/* Forward declarations. */

static int table_match();
static int list_match();
static int server_match();
static int client_match();
static int host_match();
static int string_match();
static int masked_match();
static int masked_match4();
#ifdef INET6
static int masked_match6();
#endif

/* Size of logical line buffer. */

#define	BUFLEN 2048

/* hosts_access - host access control facility */

int     hosts_access(request)
struct request_info *request;
{
    int     verdict;

    /*
     * If the (daemon, client) pair is matched by an entry in the file
     * /etc/hosts.allow, access is granted. Otherwise, if the (daemon,
     * client) pair is matched by an entry in the file /etc/hosts.deny,
     * access is denied. Otherwise, access is granted. A non-existent
     * access-control file is treated as an empty file.
     * 
     * After a rule has been matched, the optional language extensions may
     * decide to grant or refuse service anyway. Or, while a rule is being
     * processed, a serious error is found, and it seems better to play safe
     * and deny service. All this is done by jumping back into the
     * hosts_access() routine, bypassing the regular return from the
     * table_match() function calls below.
     */

    if (resident <= 0)
	resident++;
    verdict = setjmp(tcpd_buf);
    if (verdict != 0)
	return (verdict == AC_PERMIT);
    if (table_match(hosts_allow_table, request))
	return (YES);
    if (table_match(hosts_deny_table, request))
	return (NO);
    return (YES);
}

/* table_match - match table entries with (daemon, client) pair */

static int table_match(table, request)
char   *table;
struct request_info *request;
{
    FILE   *fp;
    char    sv_list[BUFLEN];		/* becomes list of daemons */
    char   *cl_list;			/* becomes list of clients */
    char   *sh_cmd;			/* becomes optional shell command */
    int     match = NO;
    struct tcpd_context saved_context;

    saved_context = tcpd_context;		/* stupid compilers */

    /*
     * Between the fopen() and fclose() calls, avoid jumps that may cause
     * file descriptor leaks.
     */

    if ((fp = fopen(table, "r")) != 0) {
	tcpd_context.file = table;
	tcpd_context.line = 0;
	while (match == NO && xgets(sv_list, sizeof(sv_list), fp) != 0) {
	    if (sv_list[strlen(sv_list) - 1] != '\n') {
		tcpd_warn("missing newline or line too long");
		continue;
	    }
	    if (sv_list[0] == '#' || sv_list[strspn(sv_list, " \t\r\n")] == 0)
		continue;
	    if ((cl_list = split_at(sv_list, ':')) == 0) {
		tcpd_warn("missing \":\" separator");
		continue;
	    }
	    sh_cmd = split_at(cl_list, ':');
	    match = list_match(sv_list, request, server_match)
		&& list_match(cl_list, request, client_match);
	}
	(void) fclose(fp);
    } else if (errno != ENOENT) {
	tcpd_warn("cannot open %s: %m", table);
    }
    if (match) {
	if (hosts_access_verbose > 1)
	    syslog(LOG_DEBUG, "matched:  %s line %d",
		   tcpd_context.file, tcpd_context.line);
	if (sh_cmd) {
#ifdef PROCESS_OPTIONS
	    process_options(sh_cmd, request);
#else
	    char    cmd[BUFSIZ];
	    shell_cmd(percent_x(cmd, sizeof(cmd), sh_cmd, request));
#endif
	}
    }
    tcpd_context = saved_context;
    return (match);
}

/* list_match - match a request against a list of patterns with exceptions */

static int list_match(list, request, match_fn)
char   *list;
struct request_info *request;
int   (*match_fn) ();
{
    char   *tok;
    int l;

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok(list, sep); tok != 0; tok = strtok((char *) 0, sep)) {
	if (STR_EQ(tok, "EXCEPT"))		/* EXCEPT: give up */
	    return (NO);
	l = strlen(tok);
	if (*tok == '[' && tok[l - 1] == ']') {
	    tok[l - 1] = '\0';
	    tok++;
	}
	if (match_fn(tok, request)) {		/* YES: look for exceptions */
	    while ((tok = strtok((char *) 0, sep)) && STR_NE(tok, "EXCEPT"))
		 /* VOID */ ;
	    return (tok == 0 || list_match((char *) 0, request, match_fn) == 0);
	}
    }
    return (NO);
}

/* server_match - match server information */

static int server_match(tok, request)
char   *tok;
struct request_info *request;
{
    char   *host;

    if ((host = split_at(tok + 1, '@')) == 0) {	/* plain daemon */
	return (string_match(tok, eval_daemon(request)));
    } else {					/* daemon@host */
	return (string_match(tok, eval_daemon(request))
		&& host_match(host, request->server));
    }
}

/* client_match - match client information */

static int client_match(tok, request)
char   *tok;
struct request_info *request;
{
    char   *host;

    if ((host = split_at(tok + 1, '@')) == 0) {	/* plain host */
	return (host_match(tok, request->client));
    } else {					/* user@host */
	return (host_match(host, request->client)
		&& string_match(tok, eval_user(request)));
    }
}

/* host_match - match host name and/or address against pattern */

static int host_match(tok, host)
char   *tok;
struct host_info *host;
{
    char   *mask;

    /*
     * This code looks a little hairy because we want to avoid unnecessary
     * hostname lookups.
     * 
     * The KNOWN pattern requires that both address AND name be known; some
     * patterns are specific to host names or to host addresses; all other
     * patterns are satisfied when either the address OR the name match.
     */

    if (tok[0] == '@') {			/* netgroup: look it up */
#ifdef  NETGROUP
	static char mydomain[MAXHOSTNAMELEN];
	if (mydomain[0] == '\0')
	    getdomainname(mydomain, sizeof(mydomain));
	return (innetgr(tok + 1, eval_hostname(host), (char *) 0, mydomain));
#else
	tcpd_warn("netgroup support is disabled");	/* not tcpd_jump() */
	return (NO);
#endif
    } else if (STR_EQ(tok, "KNOWN")) {		/* check address and name */
	char   *name = eval_hostname(host);
	return (STR_NE(eval_hostaddr(host), unknown) && HOSTNAME_KNOWN(name));
    } else if (STR_EQ(tok, "LOCAL")) {		/* local: no dots in name */
	char   *name = eval_hostname(host);
	return (strchr(name, '.') == 0 && HOSTNAME_KNOWN(name));
    } else if ((mask = split_at(tok, '/')) != 0) {	/* net/mask */
	return (masked_match(tok, mask, eval_hostaddr(host)));
    } else {					/* anything else */
	return (string_match(tok, eval_hostaddr(host))
	    || (NOT_INADDR(tok) && string_match(tok, eval_hostname(host))));
    }
}

/* string_match - match string against pattern */

static int string_match(tok, string)
char   *tok;
char   *string;
{
    int     n;

    if (tok[0] == '.') {			/* suffix */
	n = strlen(string) - strlen(tok);
	return (n > 0 && STR_EQ(tok, string + n));
    } else if (STR_EQ(tok, "ALL")) {		/* all: match any */
	return (YES);
    } else if (STR_EQ(tok, "KNOWN")) {		/* not unknown */
	return (STR_NE(string, unknown));
    } else if (tok[(n = strlen(tok)) - 1] == '.') {	/* prefix */
	return (STRN_EQ(tok, string, n));
    } else {					/* exact match */
	return (STR_EQ(tok, string));
    }
}

/* masked_match - match address against netnumber/netmask */

static int masked_match(net_tok, mask_tok, string)
char   *net_tok;
char   *mask_tok;
char   *string;
{
#ifndef INET6
    return masked_match4(net_tok, mask_tok, string);
#else
    if (dot_quad_addr_new(net_tok, NULL)
     && dot_quad_addr_new(mask_tok, NULL)
     && dot_quad_addr_new(string, NULL)) {
	return masked_match4(net_tok, mask_tok, string);
    } else
	return masked_match6(net_tok, mask_tok, string);
#endif
}

static int masked_match4(net_tok, mask_tok, string)
char   *net_tok;
char   *mask_tok;
char   *string;
{
    in_addr_t net;
    in_addr_t mask;
    in_addr_t addr;

    /*
     * Disallow forms other than dotted quad: the treatment that inet_addr()
     * gives to forms with less than four components is inconsistent with the
     * access control language. John P. Rouillard <rouilj@cs.umb.edu>.
     */

    if (!dot_quad_addr_new(string, &addr))
	return (NO);
    if (!dot_quad_addr_new(net_tok, &net) ||
	!dot_quad_addr_new(mask_tok, &mask)) {
	tcpd_warn("bad net/mask expression: %s/%s", net_tok, mask_tok);
	return (NO);				/* not tcpd_jump() */
    }
    return ((addr & mask) == net);
}

#ifdef INET6
/* Ugly because it covers IPv4 mapped address.  I hate mapped addresses. */
static int masked_match6(net_tok, mask_tok, string)
char   *net_tok;
char   *mask_tok;
char   *string;
{
    struct in6_addr net;
    struct in6_addr mask;
    struct in6_addr addr;
    int masklen;
    int fail;
    int i;
    int maskoff;
    int netaf;
    const int sizoff64 = sizeof(struct in6_addr) - sizeof(struct in_addr);

    memset(&addr, 0, sizeof(addr));
    if (inet_pton(AF_INET6, string, &addr) == 1)
	; /* okay */
    else if (inet_pton(AF_INET, string, &addr.s6_addr[sizoff64]) == 1)
	addr.s6_addr[10] = addr.s6_addr[11] = 0xff;
    else
	return NO;

    memset(&net, 0, sizeof(net));
    if (inet_pton(AF_INET6, net_tok, &net) == 1) {
	netaf = AF_INET6;
	maskoff = 0;
    } else if (inet_pton(AF_INET, net_tok, &net.s6_addr[sizoff64]) == 1) {
	netaf = AF_INET;
	maskoff = sizoff64;
	net.s6_addr[10] = net.s6_addr[11] = 0xff;
    } else
	return NO;

    fail = 0;
    if (mask_tok[strspn(mask_tok, "0123456789")] == '\0') {
	masklen = atoi(mask_tok) + maskoff * 8;
	if (0 <= masklen && masklen <= 128) {
	    memset(&mask, 0, sizeof(mask));
	    memset(&mask, 0xff, masklen / 8);
	    if (masklen % 8) {
		((u_char *)&mask)[masklen / 8] =
			(0xff00 >> (masklen % 8)) & 0xff;
	    }
	} else
	    fail++;
    } else if (netaf == AF_INET6 && inet_pton(AF_INET6, mask_tok, &mask) == 1)
	; /* okay */
    else if (netaf == AF_INET
	  && inet_pton(AF_INET, mask_tok, &mask.s6_addr[12]) == 1) {
	memset(&mask, 0xff, sizoff64);
    } else
	fail++;
    if (fail) {
	tcpd_warn("bad net/mask expression: %s/%s", net_tok, mask_tok);
	return (NO);				/* not tcpd_jump() */
    }

    for (i = 0; i < sizeof(addr); i++)
	addr.s6_addr[i] &= mask.s6_addr[i];
    return (memcmp(&addr, &net, sizeof(addr)) == 0);
}
#endif
