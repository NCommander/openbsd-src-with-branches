/*	$OpenBSD$	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      H�gskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "roken.h"
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#include "resolve.h"

RCSID("$KTH: resolve.c,v 1.6 1998/04/27 20:13:43 assar Exp $");

#if defined(HAVE_RES_SEARCH) && defined(HAVE_DN_EXPAND)

#define DECL(X) {#X, T_##X}

static struct stot{
    char *name;
    int type;
}stot[] = {
    DECL(A),
    DECL(NS),
    DECL(CNAME),
    DECL(PTR),
    DECL(MX),
    DECL(TXT),
    DECL(AFSDB),
    DECL(SRV),
    {NULL, 	0}
};

int _resolve_debug;

static int
string_to_type(const char *name)
{
    struct stot *p = stot;
    for(p = stot; p->name; p++)
	if(strcasecmp(name, p->name) == 0)
	    return p->type;
    return -1;
}


void
dns_free_data(struct dns_reply *r)
{
    struct resource_record *rr;
    if(r->q.domain)
	free(r->q.domain);
    for(rr = r->head; rr;){
	struct resource_record *tmp = rr;
	if(rr->domain)
	    free(rr->domain);
	if(rr->u.data)
	    free(rr->u.data);
	rr = rr->next;
	free(tmp);
    }
    free (r);
}

static struct dns_reply*
parse_reply(unsigned char *data, int len)
{
    unsigned char *p;
    char host[128];
    int status;
    
    struct dns_reply *r;
    struct resource_record **rr;
    
    r = (struct dns_reply*)malloc(sizeof(struct dns_reply));
    if (r == NULL)
	return NULL;
    memset(r, 0, sizeof(struct dns_reply));

    p = data;
    memcpy(&r->h, p, sizeof(HEADER));
    p += sizeof(HEADER);
    status = dn_expand(data, data + len, p, host, sizeof(host));
    if(status < 0){
	dns_free_data(r);
	return NULL;
    }
    r->q.domain = strdup(host);
    if(r->q.domain == NULL) {
	dns_free_data(r);
	return NULL;
    }
    p += status;
    r->q.type = (p[0] << 8 | p[1]);
    p += 2;
    r->q.class = (p[0] << 8 | p[1]);
    p += 2;
    rr = &r->head;
    while(p < data + len){
	int type, class, ttl, size;
	status = dn_expand(data, data + len, p, host, sizeof(host));
	if(status < 0){
	    dns_free_data(r);
	    return NULL;
	}
	p += status;
	type = (p[0] << 8) | p[1];
	p += 2;
	class = (p[0] << 8) | p[1];
	p += 2;
	ttl = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	p += 4;
	size = (p[0] << 8) | p[1];
	p += 2;
	*rr = (struct resource_record*)calloc(1, 
					      sizeof(struct resource_record));
	if(*rr == NULL) {
	    dns_free_data(r);
	    return NULL;
	}
	(*rr)->domain = strdup(host);
	if((*rr)->domain == NULL) {
	    dns_free_data(r);
	    return NULL;
	}
	(*rr)->type = type;
	(*rr)->class = class;
	(*rr)->ttl = ttl;
	(*rr)->size = size;
	switch(type){
	case T_NS:
	case T_CNAME:
	case T_PTR:
	    status = dn_expand(data, data + len, p, host, sizeof(host));
	    if(status < 0){
		dns_free_data(r);
		return NULL;
	    }
	    (*rr)->u.txt = strdup(host);
	    if((*rr)->u.txt == NULL) {
		dns_free_data(r);
		return NULL;
	    }
	    break;
	case T_MX:
	case T_AFSDB:{
	    status = dn_expand(data, data + len, p + 2, host, sizeof(host));
	    if(status < 0){
		dns_free_data(r);
		return NULL;
	    }
	    (*rr)->u.mx = (struct mx_record*)malloc(sizeof(struct mx_record) + 
						    strlen(host));
	    if((*rr)->u.mx == NULL) {
		dns_free_data(r);
		return NULL;
	    }
	    (*rr)->u.mx->preference = (p[0] << 8) | p[1];
	    strcpy((*rr)->u.mx->domain, host);
	    break;
	}
	case T_SRV:{
	    status = dn_expand(data, data + len, p + 6, host, sizeof(host));
	    if(status < 0){
		dns_free_data(r);
		return NULL;
	    }
	    (*rr)->u.srv = 
		(struct srv_record*)malloc(sizeof(struct srv_record) + 
					   strlen(host));
	    if((*rr)->u.srv == NULL) {
		dns_free_data(r);
		return NULL;
	    }
	    (*rr)->u.srv->priority = (p[0] << 8) | p[1];
	    (*rr)->u.srv->weight = (p[2] << 8) | p[3];
	    (*rr)->u.srv->port = (p[4] << 8) | p[5];
	    strcpy((*rr)->u.srv->target, host);
	    break;
	}
	case T_TXT:{
	    (*rr)->u.txt = (char*)malloc(size + 1);
	    if((*rr)->u.txt == NULL) {
		dns_free_data(r);
		return NULL;
	    }
	    strncpy((*rr)->u.txt, (char*)p + 1, *p);
	    (*rr)->u.txt[*p] = 0;
	    break;
	}
	    
	default:
	    (*rr)->u.data = (unsigned char*)malloc(size);
	    if((*rr)->u.data == NULL) {
		dns_free_data(r);
		return NULL;
	    }
	    memcpy((*rr)->u.data, p, size);
	}
	p += size;
	rr = &(*rr)->next;
    }
    *rr = NULL;
    return r;
}

struct dns_reply *
dns_lookup(const char *domain, const char *type_name)
{
    unsigned char reply[1024];
    int len;
    int type;
    struct dns_reply *r = NULL;
#ifdef HAVE__RES
    u_long old_options = 0;
#endif
    
    type = string_to_type(type_name);
    if (_resolve_debug) {
#ifdef HAVE__RES
        old_options = _res.options;
	_res.options |= RES_DEBUG;
#endif
	fprintf(stderr, "dns_lookup(%s, %s)\n", domain, type_name);
    }
    len = res_search(domain, C_IN, type, reply, sizeof(reply));
    if (_resolve_debug) {
#ifdef HAVE__RES
        _res.options = old_options;
#endif
	fprintf(stderr, "dns_lookup(%s, %s) --> %d\n", domain, type_name, len);
    }
    if (len >= 0)
	r = parse_reply(reply, len);
    return r;
}

#else /* NOT defined(HAVE_RES_SEARCH) && defined(HAVE_DN_EXPAND) */

struct dns_reply *
dns_lookup(const char *domain, const char *type_name)
{
    return NULL;
}

void
dns_free_data(struct dns_reply *r)
{
}

#endif

#ifdef TEST
static char *
type_to_string(int type)
{
    struct stot *p = stot;
    for(p = stot; p->name; p++)
	if(type == p->type)
	    return p->name;
    return NULL;
}

int 
main(int argc, char **argv)
{
    struct dns_reply *r;
    struct resource_record *rr;
    r = dns_lookup(argv[1], argv[2]);
    if(r == NULL){
	printf("No reply.\n");
	return 1;
    }
    for(rr = r->head; rr;rr=rr->next){
	printf("%s %s %d ", rr->domain, type_to_string(rr->type), rr->ttl);
	switch(rr->type){
	case T_NS:
	    printf("%s\n", (char*)rr->u.data);
	    break;
	case T_A:
	    printf("%d.%d.%d.%d\n", 
		   ((unsigned char*)rr->u.data)[0],
		   ((unsigned char*)rr->u.data)[1],
		   ((unsigned char*)rr->u.data)[2],
		   ((unsigned char*)rr->u.data)[3]);
	    break;
	case T_MX:
	case T_AFSDB:{
	    struct mx_record *mx = (struct mx_record*)rr->u.data;
	    printf("%d %s\n", mx->preference, mx->domain);
	    break;
	}
	case T_SRV:{
	    struct srv_record *srv = (struct srv_record*)rr->u.data;
	    printf("%d %d %d %s\n", srv->priority, srv->weight, 
		   srv->port, srv->target);
	    break;
	}
	default:
	    printf("\n");
	    break;
	}
    }
    
    return 0;
}
#endif
