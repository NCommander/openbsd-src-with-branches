/*	$OpenBSD: getrealm.c,v 1.10 1998/02/25 15:51:15 art Exp $	*/
/*	$KTH: getrealm.c,v 1.26 1997/10/08 22:51:13 joda Exp $		*/

/*
 * This source code is no longer held under any constraint of USA
 * `cryptographic laws' since it was exported legally.  The cryptographic
 * functions were removed from the code and a "Bones" distribution was
 * made.  A Commodity Jurisdiction Request #012-94 was filed with the
 * USA State Department, who handed it to the Commerce department.  The
 * code was determined to fall under General License GTDA under ECCN 5D96G,
 * and hence exportable.  The cryptographic interfaces were re-added by Eric
 * Young, and then KTH proceeded to maintain the code in the free world.
 *
 */

/* 
 *  Copyright (C) 1989 by the Massachusetts Institute of Technology
 *
 *  Export of this software from the United States of America is assumed
 *  to require a specific license from the United States Government.
 *  It is the responsibility of any person or organization contemplating
 *  export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#include "krb_locl.h"

#define MATCH_SUBDOMAINS        0

/*
 * krb_realmofhost.
 * Given a fully-qualified domain-style primary host name,
 * return the name of the Kerberos realm for the host.
 * If the hostname contains no discernable domain, or an error occurs,
 * return the local realm name, as supplied by get_krbrlm().
 * If the hostname contains a domain, but no translation is found,
 * the hostname's domain is converted to upper-case and returned.
 *
 * The format of each line of the translation file is:
 * domain_name kerberos_realm
 * -or-
 * host_name kerberos_realm
 *
 * domain_name should be of the form .XXX.YYY (e.g. .LCS.MIT.EDU)
 * host names should be in the usual form (e.g. FOO.BAR.BAZ)
 */

/* To automagically find the correct realm of a host (without
 * krb.realms) add a text record for your domain with the name of your
 * realm, like this:
 *
 * krb4-realm	IN	TXT	FOO.SE
 *
 * The search is recursive, so you can also add entries for specific
 * hosts. To find the realm of host a.b.c, it first tries
 * krb4-realm.a.b.c, then krb4-realm.b.c and so on.
 */

static int
dns_find_realm(char *hostname, char *realm)
{
    char domain[MAXHOSTNAMELEN + sizeof("krb4-realm..")];
    char *p;
    int level = 0;
    struct dns_reply *r;
    
    p = hostname;

    while(1){
	snprintf(domain, sizeof(domain), "krb4-realm.%s.", p);
	p = strchr(p, '.');
	if(p == NULL)
	    break;
	p++;
	r = dns_lookup(domain, "TXT");
	if(r){
	    struct resource_record *rr = r->head;
	    while(rr){
		if(rr->type == T_TXT){
		    strncpy(realm, rr->u.txt, REALM_SZ);
		    realm[REALM_SZ - 1] = 0;
		    dns_free_data(r);
		    return level;
		}
		rr = rr->next;
	    }
	    dns_free_data(r);
	}
	level++;
    }
    return -1;
}


static FILE *
open_krb_realms(void)
{
    int i;
    char file[MAXPATHLEN];
    FILE *res;

    for(i = 0; krb_get_krbrealms(i, file, sizeof(file)) == 0; i++)
	if ((res = fopen(file, "r")) != NULL)
	    return res;

    return NULL;
}

static int
file_find_realm(const char *phost, const char *domain, 
		char *ret_realm, size_t ret_realm_sz)
{
    FILE *trans_file;
    char buf[1024];
    int ret = -1;
    
    if ((trans_file = open_krb_realms()) == NULL)
	return -1;

    while (fgets(buf, sizeof(buf), trans_file) != NULL) {
	char *save = NULL;
	char *tok;
	char *tmp_host;
	char *tmp_realm;

        tok = strtok_r(buf, " \t\r\n", &save);
        if(tok == NULL)
            continue;
	tmp_host = tok;
	tok = strtok_r(NULL, " \t\r\n", &save);
	if(tok == NULL)
	    continue;
	tmp_realm = tok;
	if (strcasecmp(tmp_host, phost) == 0) {
	    /* exact match of hostname, so return the realm */
	    strncpy(ret_realm, tmp_realm, ret_realm_sz);
	    ret_realm[ret_realm_sz - 1] = '\0';
	    ret = 0;
	    break;
	}
	if ((tmp_host[0] == '.') && domain) { 
	    const char *cp = domain;
	    do {
		if(strcasecmp(tmp_host, domain) == 0){
		    /* domain match, save for later */ 
		    strncpy(ret_realm, tmp_realm, ret_realm_sz);
		    ret_realm[ret_realm_sz - 1] = '\0';
		    ret = 0;
		    break;
		}
		cp = strchr(cp + 1, '.');
	    } while(MATCH_SUBDOMAINS && cp);
	}
    }
    fclose(trans_file);
    return ret;
}

char *
krb_realmofhost(const char *host)
{
    static char ret_realm[REALM_SZ];
    char *domain;
    char phost[MAXHOSTNAMELEN];
	
    krb_name_to_name(host, phost, sizeof(phost));
	
    domain = strchr(phost, '.');

    if(file_find_realm(phost, domain, ret_realm, sizeof(ret_realm)) == 0)
	return ret_realm;

    if(dns_find_realm(phost, ret_realm) >= 0)
	return ret_realm;
  
    if (domain) {
	char *cp;
	  
	strncpy(ret_realm, &domain[1], REALM_SZ);
	ret_realm[REALM_SZ - 1] = 0;
	/* Upper-case realm */
	for (cp = ret_realm; *cp; cp++)
	    *cp = toupper(*cp);
    } else {
	strncpy(ret_realm, krb_get_default_realm(), REALM_SZ); /* Wild guess */
    }
    return ret_realm;
}
