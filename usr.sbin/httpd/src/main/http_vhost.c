/* ====================================================================
 * Copyright (c) 1995-1999 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/*
 * http_vhost.c: functions pertaining to virtual host addresses
 *	(configuration and run-time)
 */

#define CORE_PRIVATE
#include "httpd.h"
#include "http_config.h"
#include "http_conf_globals.h"
#include "http_log.h"
#include "http_vhost.h"
#include "http_protocol.h"

/*
 * After all the definitions there's an explanation of how it's all put
 * together.
 */

/* meta-list of name-vhosts.  Each server_rec can be in possibly multiple
 * lists of name-vhosts.
 */
typedef struct name_chain name_chain;
struct name_chain {
    name_chain *next;
    server_addr_rec *sar;	/* the record causing it to be in
				 * this chain (needed for port comparisons) */
    server_rec *server;		/* the server to use on a match */
};

/* meta-list of ip addresses.  Each server_rec can be in possibly multiple
 * hash chains since it can have multiple ips.
 */
typedef struct ipaddr_chain ipaddr_chain;
struct ipaddr_chain {
    ipaddr_chain *next;
    server_addr_rec *sar;	/* the record causing it to be in
				 * this chain (need for both ip addr and port
				 * comparisons) */
    server_rec *server;		/* the server to use if this matches */
    name_chain *names;		/* if non-NULL then a list of name-vhosts
    				 * sharing this address */
};

/* This defines the size of the hash table used for hashing ip addresses
 * of virtual hosts.  It must be a power of two.
 */
#ifndef IPHASH_TABLE_SIZE
#define IPHASH_TABLE_SIZE 256
#endif

/* A (n) bucket hash table, each entry has a pointer to a server rec and
 * a pointer to the other entries in that bucket.  Each individual address,
 * even for virtualhosts with multiple addresses, has an entry in this hash
 * table.  There are extra buckets for _default_, and name-vhost entries.
 *
 * Note that after config time this is constant, so it is thread-safe.
 */
static ipaddr_chain *iphash_table[IPHASH_TABLE_SIZE];

/* dump out statistics about the hash function */
/* #define IPHASH_STATISTICS */

/* list of the _default_ servers */
static ipaddr_chain *default_list;

/* list of the NameVirtualHost addresses */
static server_addr_rec *name_vhost_list;
static server_addr_rec **name_vhost_list_tail;

/*
 * How it's used:
 *
 * The ip address determines which chain in iphash_table is interesting, then
 * a comparison is done down that chain to find the first ipaddr_chain whose
 * sar matches the address:port pair.
 *
 * If that ipaddr_chain has names == NULL then you're done, it's an ip-vhost.
 *
 * Otherwise it's a name-vhost list, and the default is the server in the
 * ipaddr_chain record.  We tuck away the ipaddr_chain record in the
 * conn_rec field vhost_lookup_data.  Later on after the headers we get a
 * second chance, and we use the name_chain to figure out what name-vhost
 * matches the headers.
 *
 * If there was no ip address match in the iphash_table then do a lookup
 * in the default_list.
 *
 * How it's put together ... well you should be able to figure that out
 * from how it's used.  Or something like that.
 */


/* called at the beginning of the config */
void ap_init_vhost_config(pool *p)
{
    memset(iphash_table, 0, sizeof(iphash_table));
    default_list = NULL;
    name_vhost_list = NULL;
    name_vhost_list_tail = &name_vhost_list;
}


/*
 * Parses a host of the form <address>[:port]
 * paddr is used to create a list in the order of input
 * **paddr is the ->next pointer of the last entry (or s->addrs)
 * *paddr is the variable used to keep track of **paddr between calls
 * port is the default port to assume
 */
static const char *get_addresses(pool *p, char *w, server_addr_rec ***paddr,
			    unsigned port)
{
    struct hostent *hep;
    unsigned long my_addr;
    server_addr_rec *sar;
    char *t;
    int i, is_an_ip_addr;

    if (*w == 0)
	return NULL;

    t = strchr(w, ':');
    if (t) {
	if (strcmp(t + 1, "*") == 0) {
	    port = 0;
	}
	else if ((i = atoi(t + 1))) {
	    port = i;
	}
	else {
	    return ":port must be numeric";
	}
	*t = 0;
    }

    is_an_ip_addr = 0;
    if (strcmp(w, "*") == 0) {
	my_addr = htonl(INADDR_ANY);
	is_an_ip_addr = 1;
    }
    else if (strcasecmp(w, "_default_") == 0
	     || strcmp(w, "255.255.255.255") == 0) {
	my_addr = DEFAULT_VHOST_ADDR;
	is_an_ip_addr = 1;
    }
    else if ((my_addr = ap_inet_addr(w)) != INADDR_NONE) {
	is_an_ip_addr = 1;
    }
    if (is_an_ip_addr) {
	sar = ap_pcalloc(p, sizeof(server_addr_rec));
	**paddr = sar;
	*paddr = &sar->next;
	sar->host_addr.s_addr = my_addr;
	sar->host_port = port;
	sar->virthost = ap_pstrdup(p, w);
	if (t != NULL)
	    *t = ':';
	return NULL;
    }

    hep = gethostbyname(w);

    if ((!hep) || (hep->h_addrtype != AF_INET || !hep->h_addr_list[0])) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, NULL,
	    "Cannot resolve host name %s --- ignoring!", w);
	if (t != NULL)
	    *t = ':';
	return NULL;
    }

    for (i = 0; hep->h_addr_list[i]; ++i) {
	sar = ap_pcalloc(p, sizeof(server_addr_rec));
	**paddr = sar;
	*paddr = &sar->next;
	sar->host_addr = *(struct in_addr *) hep->h_addr_list[i];
	sar->host_port = port;
	sar->virthost = ap_pstrdup(p, w);
    }

    if (t != NULL)
	*t = ':';
    return NULL;
}


/* parse the <VirtualHost> addresses */
const char *ap_parse_vhost_addrs(pool *p, const char *hostname, server_rec *s)
{
    server_addr_rec **addrs;
    const char *err;

    /* start the list of addreses */
    addrs = &s->addrs;
    while (hostname[0]) {
	err = get_addresses(p, ap_getword_conf(p, &hostname), &addrs, s->port);
	if (err) {
	    *addrs = NULL;
	    return err;
	}
    }
    /* terminate the list */
    *addrs = NULL;
    if (s->addrs) {
	if (s->addrs->host_port) {
	    /* override the default port which is inherited from main_server */
	    s->port = s->addrs->host_port;
	}
    }
    return NULL;
}


const char *ap_set_name_virtual_host (cmd_parms *cmd, void *dummy, char *arg)
{
    /* use whatever port the main server has at this point */
    return get_addresses(cmd->pool, arg, &name_vhost_list_tail,
			    cmd->server->port);
}


/* hash table statistics, keep this in here for the beta period so
 * we can find out if the hash function is ok
 */
#ifdef IPHASH_STATISTICS
static int iphash_compare(const void *a, const void *b)
{
    return (*(const int *) b - *(const int *) a);
}


static void dump_iphash_statistics(server_rec *main_s)
{
    unsigned count[IPHASH_TABLE_SIZE];
    int i;
    ipaddr_chain *src;
    unsigned total;
    char buf[HUGE_STRING_LEN];
    char *p;

    total = 0;
    for (i = 0; i < IPHASH_TABLE_SIZE; ++i) {
	count[i] = 0;
	for (src = iphash_table[i]; src; src = src->next) {
	    ++count[i];
	    if (i < IPHASH_TABLE_SIZE) {
		/* don't count the slop buckets in the total */
		++total;
	    }
	}
    }
    qsort(count, IPHASH_TABLE_SIZE, sizeof(count[0]), iphash_compare);
    p = buf + ap_snprintf(buf, sizeof(buf),
		    "iphash: total hashed = %u, avg chain = %u, "
		    "chain lengths (count x len):",
		    total, total / IPHASH_TABLE_SIZE);
    total = 1;
    for (i = 1; i < IPHASH_TABLE_SIZE; ++i) {
	if (count[i - 1] != count[i]) {
	    p += ap_snprintf(p, sizeof(buf) - (p - buf), " %ux%u",
			     total, count[i - 1]);
	    total = 1;
	}
	else {
	    ++total;
	}
    }
    p += ap_snprintf(p, sizeof(buf) - (p - buf), " %ux%u",
		     total, count[IPHASH_TABLE_SIZE - 1]);
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, main_s, buf);
}
#endif


/* This hashing function is designed to get good distribution in the cases
 * where the server is handling entire "networks" of servers.  i.e. a
 * whack of /24s.  This is probably the most common configuration for
 * ISPs with large virtual servers.
 *
 * NOTE: This function is symmetric (i.e. collapses all 4 octets
 * into one), so machine byte order (big/little endianness) does not matter.
 *
 * Hash function provided by David Hankins.
 */
static ap_inline unsigned hash_inaddr(unsigned key)
{
    key ^= (key >> 16);
    return ((key >> 8) ^ key) % IPHASH_TABLE_SIZE;
}



static ipaddr_chain *new_ipaddr_chain(pool *p,
				    server_rec *s, server_addr_rec *sar)
{
    ipaddr_chain *new;

    new = ap_palloc(p, sizeof(*new));
    new->names = NULL;
    new->server = s;
    new->sar = sar;
    new->next = NULL;
    return new;
}


static name_chain *new_name_chain(pool *p, server_rec *s, server_addr_rec *sar)
{
    name_chain *new;

    new = ap_palloc(p, sizeof(*new));
    new->server = s;
    new->sar = sar;
    new->next = NULL;
    return new;
}


static ap_inline ipaddr_chain *find_ipaddr(struct in_addr *server_ip,
    unsigned port)
{
    unsigned bucket;
    ipaddr_chain *trav;
    unsigned addr;

    /* scan the hash table for an exact match first */
    addr = server_ip->s_addr;
    bucket = hash_inaddr(addr);
    for (trav = iphash_table[bucket]; trav; trav = trav->next) {
	server_addr_rec *sar = trav->sar;
	if ((sar->host_addr.s_addr == addr)
	    && (sar->host_port == 0 || sar->host_port == port
		|| port == 0)) {
	    return trav;
	}
    }
    return NULL;
}


static ipaddr_chain *find_default_server(unsigned port)
{
    server_addr_rec *sar;
    ipaddr_chain *trav;

    for (trav = default_list; trav; trav = trav->next) {
        sar = trav->sar;
        if (sar->host_port == 0 || sar->host_port == port) {
            /* match! */
	    return trav;
        }
    }
    return NULL;
}


static void dump_vhost_config(FILE *f)
{
    int i;
    ipaddr_chain *ic;
    name_chain *nc;
    char buf[MAX_STRING_LEN];

    fprintf(f, "VirtualHost configuration:\n");
    for (i = 0; i < IPHASH_TABLE_SIZE; ++i) {
	for (ic = iphash_table[i]; ic; ic = ic->next) {
	    if (ic->sar->host_port == 0) {
		ap_snprintf(buf, sizeof(buf), "%pA:*", &ic->sar->host_addr);
	    }
	    else {
		ap_snprintf(buf, sizeof(buf), "%pA:%u", &ic->sar->host_addr,
		    ic->sar->host_port);
	    }
	    if (ic->names == NULL) {
		fprintf(f, "%-22s %s (%s:%u)\n", buf,
		    ic->server->server_hostname, ic->server->defn_name,
		    ic->server->defn_line_number);
		continue;
	    }
	    fprintf(f, "%-22s is a NameVirtualHost\n"
	               "%22s default server %s (%s:%u)\n",
		       buf, "", ic->server->server_hostname,
		       ic->server->defn_name, ic->server->defn_line_number);
	    for (nc = ic->names; nc; nc = nc->next) {
		if (nc->sar->host_port) {
		    fprintf(f, "%22s port %u ", "", nc->sar->host_port);
		}
		else {
		    fprintf(f, "%22s port * ", "");
		}
		fprintf(f, "namevhost %s (%s:%u)\n",
			nc->server->server_hostname,
			nc->server->defn_name,
			nc->server->defn_line_number);
	    }
	}
    }
    if (default_list) {
	fprintf(f, "_default_ servers:\n");
	for (ic = default_list; ic; ic = ic->next) {
	    if (ic->sar->host_port == 0) {
		fprintf(f, "port * ");
	    }
	    else {
		fprintf(f, "port %u ", ic->sar->host_port);
	    }
	    fprintf(f, "server %s (%s:%u)\n",
		ic->server->server_hostname, ic->server->defn_name,
		ic->server->defn_line_number);
	}
    }
}

/* compile the tables and such we need to do the run-time vhost lookups */
void ap_fini_vhost_config(pool *p, server_rec *main_s)
{
    server_addr_rec *sar;
    int has_default_vhost_addr;
    server_rec *s;
    int i;
    ipaddr_chain **iphash_table_tail[IPHASH_TABLE_SIZE];

    /* terminate the name_vhost list */
    *name_vhost_list_tail = NULL;

    /* Main host first */
    s = main_s;

    if (!s->server_hostname) {
	s->server_hostname = ap_get_local_host(p);
    }

    /* initialize the tails */
    for (i = 0; i < IPHASH_TABLE_SIZE; ++i) {
	iphash_table_tail[i] = &iphash_table[i];
    }

    /* The first things to go into the hash table are the NameVirtualHosts
     * Since name_vhost_list is in the same order that the directives
     * occured in the config file, we'll copy it in that order.
     */
    for (sar = name_vhost_list; sar; sar = sar->next) {
	unsigned bucket = hash_inaddr(sar->host_addr.s_addr);
	ipaddr_chain *new = new_ipaddr_chain(p, NULL, sar);

	*iphash_table_tail[bucket] = new;
	iphash_table_tail[bucket] = &new->next;

	/* Notice that what we've done is insert an ipaddr_chain with
	 * both server and names NULL.  Remember that.
	 */
    }

    /* The next things to go into the hash table are the virtual hosts
     * themselves.  They're listed off of main_s->next in the reverse
     * order they occured in the config file, so we insert them at
     * the iphash_table_tail but don't advance the tail.
     */

    for (s = main_s->next; s; s = s->next) {
	has_default_vhost_addr = 0;
	for (sar = s->addrs; sar; sar = sar->next) {
	    ipaddr_chain *ic;

	    if (sar->host_addr.s_addr == DEFAULT_VHOST_ADDR
		|| sar->host_addr.s_addr == INADDR_ANY) {
		/* add it to default bucket for each appropriate sar
		 * since we need to do a port test
		 */
		ipaddr_chain *other;

		other = find_default_server(sar->host_port);
		if (other && other->sar->host_port != 0) {
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, main_s,
			    "_default_ VirtualHost overlap on port %u,"
			    " the first has precedence", sar->host_port);
		}
		has_default_vhost_addr = 1;
		ic = new_ipaddr_chain(p, s, sar);
		ic->next = default_list;
		default_list = ic;
	    }
	    else {
		/* see if it matches something we've already got */
		ic = find_ipaddr(&sar->host_addr, sar->host_port);

		/* the first time we encounter a NameVirtualHost address
		 * ic->server will be NULL, on subsequent encounters
		 * ic->names will be non-NULL.
		 */
		if (ic && (ic->names || ic->server == NULL)) {
		    name_chain *nc = new_name_chain(p, s, sar);
		    nc->next = ic->names;
		    ic->names = nc;
		    ic->server = s;
		    if (sar->host_port != ic->sar->host_port) {
			/* one of the two is a * port, the other isn't */
			ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, main_s,
				"VirtualHost %s:%u -- mixing * "
				"ports and non-* ports with "
				"a NameVirtualHost address is not supported,"
				" proceeding with undefined results",
				sar->virthost, sar->host_port);
		    }
		}
		else if (ic) {
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, main_s,
			    "VirtualHost %s:%u overlaps with "
			    "VirtualHost %s:%u, the first has precedence, "
			    "perhaps you need a NameVirtualHost directive",
			    sar->virthost, sar->host_port,
			    ic->sar->virthost, ic->sar->host_port);
		    ic->sar = sar;
		    ic->server = s;
		}
		else {
		    unsigned bucket = hash_inaddr(sar->host_addr.s_addr);

		    ic = new_ipaddr_chain(p, s, sar);
		    ic->next = *iphash_table_tail[bucket];
		    *iphash_table_tail[bucket] = ic;
		}
	    }
	}

	/* Ok now we want to set up a server_hostname if the user was
	 * silly enough to forget one.
	 * XXX: This is silly we should just crash and burn.
	 */
	if (!s->server_hostname) {
	    if (has_default_vhost_addr) {
		s->server_hostname = main_s->server_hostname;
	    }
	    else if (!s->addrs) {
		/* what else can we do?  at this point this vhost has
		    no configured name, probably because they used
		    DNS in the VirtualHost statement.  It's disabled
		    anyhow by the host matching code.  -djg */
		s->server_hostname =
		    ap_pstrdup(p, "bogus_host_without_forward_dns");
	    }
	    else {
		struct hostent *h;

		if ((h = gethostbyaddr((char *) &(s->addrs->host_addr),
					sizeof(struct in_addr), AF_INET))) {
		    s->server_hostname = ap_pstrdup(p, (char *) h->h_name);
		}
		else {
		    /* again, what can we do?  They didn't specify a
		       ServerName, and their DNS isn't working. -djg */
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, main_s,
			    "Failed to resolve server name "
			    "for %s (check DNS) -- or specify an explicit "
			    "ServerName",
			    inet_ntoa(s->addrs->host_addr));
		    s->server_hostname =
			ap_pstrdup(p, "bogus_host_without_reverse_dns");
		}
	    }
	}
    }

    /* now go through and delete any NameVirtualHosts that didn't have any
     * hosts associated with them.  Lamers.
     */
    for (i = 0; i < IPHASH_TABLE_SIZE; ++i) {
	ipaddr_chain **pic = &iphash_table[i];

	while (*pic) {
	    ipaddr_chain *ic = *pic;

	    if (ic->server == NULL) {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, main_s,
			"NameVirtualHost %s:%u has no VirtualHosts",
			ic->sar->virthost, ic->sar->host_port);
		*pic = ic->next;
	    }
	    else if (ic->names == NULL) {
		/* if server != NULL and names == NULL then we're done
		 * looking at NameVirtualHosts
		 */
		break;
	    }
	    else {
		pic = &ic->next;
	    }
	}
    }

#ifdef IPHASH_STATISTICS
    dump_iphash_statistics(main_s);
#endif
    if (ap_dump_settings) {
	dump_vhost_config(stderr);
    }
}


/*****************************************************************************
 * run-time vhost matching functions
 */

/* Remove :port and optionally a single trailing . from the hostname, this
 * canonicalizes it somewhat.
 */
static void fix_hostname(request_rec *r)
{
    const char *hostname = r->hostname;
    char *host = ap_getword(r->pool, &hostname, ':');	/* get rid of port */
    size_t l;

    /* trim a trailing . */
    l = strlen(host);
    if (l > 0 && host[l-1] == '.') {
        host[l-1] = '\0';
    }

    r->hostname = host;
}


/* return 1 if host matches ServerName or ServerAliases */
static int matches_aliases(server_rec *s, const char *host)
{
    int i;
    array_header *names;

    /* match ServerName */
    if (!strcasecmp(host, s->server_hostname)) {
	return 1;
    }

    /* search all the aliases from ServerAlias directive */
    names = s->names;
    if (names) {
	char **name = (char **) names->elts;
	for (i = 0; i < names->nelts; ++i) {
	    if(!name[i]) continue;
	    if (!strcasecmp(host, name[i]))
		return 1;
	}
    }
    names = s->wild_names;
    if (names) {
	char **name = (char **) names->elts;
	for (i = 0; i < names->nelts; ++i) {
	    if(!name[i]) continue;
	    if (!ap_strcasecmp_match(host, name[i]))
		return 1;
	}
    }
    return 0;
}


/* Suppose a request came in on the same socket as this r, and included
 * a header "Host: host:port", would it map to r->server?  It's more
 * than just that though.  When we do the normal matches for each request
 * we don't even bother considering Host: etc on non-namevirtualhosts,
 * we just call it a match.  But here we require the host:port to match
 * the ServerName and/or ServerAliases.
 */
API_EXPORT(int) ap_matches_request_vhost(request_rec *r, const char *host,
    unsigned port)
{
    server_rec *s;
    server_addr_rec *sar;

    s = r->server;

    /* search all the <VirtualHost> values */
    /* XXX: If this is a NameVirtualHost then we may not be doing the Right Thing
     * consider: 
     *
     *     NameVirtualHost 10.1.1.1
     *     <VirtualHost 10.1.1.1>
     *     ServerName v1
     *     </VirtualHost>
     *     <VirtualHost 10.1.1.1>
     *     ServerName v2
     *     </VirtualHost>
     *
     * Suppose r->server is v2, and we're asked to match "10.1.1.1".  We'll say
     * "yup it's v2", when really it isn't... if a request came in for 10.1.1.1
     * it would really go to v1.
     */
    for (sar = s->addrs; sar; sar = sar->next) {
	if ((sar->host_port == 0 || port == sar->host_port)
	    && !strcasecmp(host, sar->virthost)) {
	    return 1;
	}
    }

    /* the Port has to match now, because the rest don't have ports associated
     * with them. */
    if (port != s->port) {
	return 0;
    }

    return matches_aliases(s, host);
}


static void check_hostalias(request_rec *r)
{
    /*
     * Even if the request has a Host: header containing a port we ignore
     * that port.  We always use the physical port of the socket.  There
     * are a few reasons for this:
     *
     * - the default of 80 or 443 for SSL is easier to handle this way
     * - there is less of a possibility of a security problem
     * - it simplifies the data structure
     * - the client may have no idea that a proxy somewhere along the way
     *   translated the request to another ip:port
     * - except for the addresses from the VirtualHost line, none of the other
     *   names we'll match have ports associated with them
     */
    const char *host = r->hostname;
    unsigned port = ntohs(r->connection->local_addr.sin_port);
    server_rec *s;
    server_rec *last_s;
    name_chain *src;

    last_s = NULL;

    /* Recall that the name_chain is a list of server_addr_recs, some of
     * whose ports may not match.  Also each server may appear more than
     * once in the chain -- specifically, it will appear once for each
     * address from its VirtualHost line which matched.  We only want to
     * do the full ServerName/ServerAlias comparisons once for each
     * server, fortunately we know that all the VirtualHost addresses for
     * a single server are adjacent to each other.
     */

    for (src = r->connection->vhost_lookup_data; src; src = src->next) {
        server_addr_rec *sar;

	/* We only consider addresses on the name_chain which have a matching
	 * port
	 */
	sar = src->sar;
	if (sar->host_port != 0 && port != sar->host_port) {
	    continue;
	}

        s = src->server;

	/* does it match the virthost from the sar? */
	if (!strcasecmp(host, sar->virthost)) {
	    goto found;
	}

	if (s == last_s) {
	    /* we've already done ServerName and ServerAlias checks for this
	     * vhost
	     */
	    continue;
	}
	last_s = s;

	if (matches_aliases(s, host)) {
	    goto found;
	}
    }
    return;

found:
    /* s is the first matching server, we're done */
    r->server = r->connection->server = s;
}


static void check_serverpath(request_rec *r)
{
    server_rec *s;
    server_rec *last_s;
    name_chain *src;
    unsigned port = ntohs(r->connection->local_addr.sin_port);

    /*
     * This is in conjunction with the ServerPath code in http_core, so we
     * get the right host attached to a non- Host-sending request.
     *
     * See the comment in check_hostalias about how each vhost can be
     * listed multiple times.
     */

    last_s = NULL;
    for (src = r->connection->vhost_lookup_data; src; src = src->next) {
	/* We only consider addresses on the name_chain which have a matching
	 * port
	 */
	if (src->sar->host_port != 0 && port != src->sar->host_port) {
	    continue;
	}

        s = src->server;
	if (s == last_s) {
	    continue;
	}
	last_s = s;

        if (s->path && !strncmp(r->uri, s->path, s->pathlen) &&
            (s->path[s->pathlen - 1] == '/' ||
             r->uri[s->pathlen] == '/' ||
             r->uri[s->pathlen] == '\0')) {
            r->server = r->connection->server = s;
	    return;
	}
    }
}


void ap_update_vhost_from_headers(request_rec *r)
{
    /* must set this for HTTP/1.1 support */
    if (r->hostname || (r->hostname = ap_table_get(r->headers_in, "Host"))) {
	fix_hostname(r);
    }
    /* check if we tucked away a name_chain */
    if (r->connection->vhost_lookup_data) {
        if (r->hostname)
            check_hostalias(r);
        else
            check_serverpath(r);
    }
}


/* Called for a new connection which has a known local_addr.  Note that the
 * new connection is assumed to have conn->server == main server.
 */
void ap_update_vhost_given_ip(conn_rec *conn)
{
    ipaddr_chain *trav;
    unsigned port = ntohs(conn->local_addr.sin_port);

    /* scan the hash table for an exact match first */
    trav = find_ipaddr(&conn->local_addr.sin_addr, port);
    if (trav) {
	/* save the name_chain for later in case this is a name-vhost */
	conn->vhost_lookup_data = trav->names;
	conn->server = trav->server;
	return;
    }

    /* There's certainly no name-vhosts with this address, they would have
     * been matched above.
     */
    conn->vhost_lookup_data = NULL;

    /* maybe there's a default server matching this port */
    trav = find_default_server(port);
    if (trav) {
	conn->server = trav->server;
    }

    /* otherwise we're stuck with just the main server */
}
