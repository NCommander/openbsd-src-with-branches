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
 * mod_vhost_alias.c: support for dynamically configured mass virtual hosting
 * 
 * Copyright (c) 1998-1999 Demon Internet Ltd.
 *
 * This software was submitted by Demon Internet to the Apache Group
 * in May 1999. Future revisions and derivatives of this source code
 * must acknowledge Demon Internet as the original contributor of
 * this module. All other licensing and usage conditions are those
 * of the Apache Group.
 *
 * Originally written by Tony Finch <fanf@demon.net> <dot@dotat.at>.
 *
 * Implementation ideas were taken from mod_alias.c. The overall
 * concept is derived from the OVERRIDE_DOC_ROOT/OVERRIDE_CGIDIR
 * patch to Apache 1.3b3 and a similar feature in Demon's thttpd,
 * both written by James Grinter <jrg@blodwen.demon.co.uk>.
 */

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"


module MODULE_VAR_EXPORT vhost_alias_module;


/*
 * basic configuration things
 * we abbreviate "mod_vhost_alias" to "mva" for shorter names
 */

typedef enum {
    VHOST_ALIAS_UNSET, VHOST_ALIAS_NONE, VHOST_ALIAS_NAME, VHOST_ALIAS_IP
} mva_mode_e;

/*
 * Per-server module config record.
 */
typedef struct mva_sconf_t {
    char *doc_root;
    char *cgi_root;
    mva_mode_e doc_root_mode;
    mva_mode_e cgi_root_mode;
} mva_sconf_t;

static void *mva_create_server_config(pool *p, server_rec *s)
{
    mva_sconf_t *conf;

    conf = (mva_sconf_t *) ap_pcalloc(p, sizeof(mva_sconf_t));
    conf->doc_root = NULL;
    conf->cgi_root = NULL;
    conf->doc_root_mode = VHOST_ALIAS_UNSET;
    conf->cgi_root_mode = VHOST_ALIAS_UNSET;
    return conf;
}

static void *mva_merge_server_config(pool *p, void *parentv, void *childv)
{
    mva_sconf_t *parent = (mva_sconf_t *) parentv;
    mva_sconf_t *child = (mva_sconf_t *) childv;
    mva_sconf_t *conf;

    conf = (mva_sconf_t *) ap_pcalloc(p, sizeof(*conf));
    if (child->doc_root_mode == VHOST_ALIAS_UNSET) {
	conf->doc_root_mode = parent->doc_root_mode;
	conf->doc_root = parent->doc_root;
    }
    else {
	conf->doc_root_mode = child->doc_root_mode;
	conf->doc_root = child->doc_root;
    }
    if (child->cgi_root_mode == VHOST_ALIAS_UNSET) {
	conf->cgi_root_mode = parent->cgi_root_mode;
	conf->cgi_root = parent->cgi_root;
    }
    else {
	conf->cgi_root_mode = child->cgi_root_mode;
	conf->cgi_root = child->cgi_root;
    }
    return conf;
}


/*
 * These are just here to tell us what vhost_alias_set should do.
 * We don't put anything into them; we just use the cell addresses.
 */
static int vhost_alias_set_doc_root_ip,
    vhost_alias_set_cgi_root_ip,
    vhost_alias_set_doc_root_name,
    vhost_alias_set_cgi_root_name;

static const char *vhost_alias_set(cmd_parms *cmd, void *dummy, char *map)
{
    mva_sconf_t *conf;
    mva_mode_e mode, *pmode;
    char **pmap;
    char *p;
  
    conf = (mva_sconf_t *) ap_get_module_config(cmd->server->module_config,
						&vhost_alias_module);
    /* there ought to be a better way of doing this */
    if (&vhost_alias_set_doc_root_ip == cmd->info) {
	mode = VHOST_ALIAS_IP;
	pmap = &conf->doc_root;
	pmode = &conf->doc_root_mode;
    }
    else if (&vhost_alias_set_cgi_root_ip == cmd->info) {
	mode = VHOST_ALIAS_IP;
	pmap = &conf->cgi_root;
	pmode = &conf->cgi_root_mode;
    }
    else if (&vhost_alias_set_doc_root_name == cmd->info) {
	mode = VHOST_ALIAS_NAME;
	pmap = &conf->doc_root;
	pmode = &conf->doc_root_mode;
    }
    else if (&vhost_alias_set_cgi_root_name == cmd->info) {
	mode = VHOST_ALIAS_NAME;
	pmap = &conf->cgi_root;
	pmode = &conf->cgi_root_mode;
    }
    else {
	return "INTERNAL ERROR: unknown command info";
    }

    if (*map != '/') {
	if (strcasecmp(map, "none")) {
	    return "format string must start with '/' or be 'none'";
	}
	*pmap = NULL;
	*pmode = VHOST_ALIAS_NONE;
	return NULL;
    }

    /* sanity check */
    p = map;
    while (*p != '\0') {
	if (*p++ != '%') {
	    continue;
	}
	/* we just found a '%' */
	if (*p == 'p' || *p == '%') {
	    ++p;
	    continue;
	}
	/* optional dash */
	if (*p == '-') {
	    ++p;
	}
	/* digit N */
	if (ap_isdigit(*p)) {
	    ++p;
	}
	else {
	    return "syntax error in format string";
	}
	/* optional plus */
	if (*p == '+') {
	    ++p;
	}
	/* do we end here? */
	if (*p != '.') {
	    continue;
	}
	++p;
	/* optional dash */
	if (*p == '-') {
	    ++p;
	}
	/* digit M */
	if (ap_isdigit(*p)) {
	    ++p;
	}
	else {
	    return "syntax error in format string";
	}
	/* optional plus */
	if (*p == '+') {
	    ++p;
	}
    }
    *pmap = map;
    *pmode = mode;
    return NULL;
}

static const command_rec mva_commands[] =
{
    {"VirtualScriptAlias", vhost_alias_set, &vhost_alias_set_cgi_root_name,
     RSRC_CONF, TAKE1, "how to create a ScriptAlias based on the host"},
    {"VirtualDocumentRoot", vhost_alias_set, &vhost_alias_set_doc_root_name,
     RSRC_CONF, TAKE1, "how to create the DocumentRoot based on the host"},
    {"VirtualScriptAliasIP", vhost_alias_set, &vhost_alias_set_cgi_root_ip,
     RSRC_CONF, TAKE1, "how to create a ScriptAlias based on the host"},
    {"VirtualDocumentRootIP", vhost_alias_set, &vhost_alias_set_doc_root_ip,
     RSRC_CONF, TAKE1, "how to create the DocumentRoot based on the host"},
    { NULL }
};


/*
 * This really wants to be a nested function
 * but C is too feeble to support them.
 */
static ap_inline void vhost_alias_checkspace(request_rec *r, char *buf,
					     char **pdest, int size)
{
    /* XXX: what if size > HUGE_STRING_LEN? */
    if (*pdest + size > buf + HUGE_STRING_LEN) {
	**pdest = '\0';
	if (r->filename) {
	    r->filename = ap_pstrcat(r->pool, r->filename, buf, NULL);
	}
	else {
	    r->filename = ap_pstrdup(r->pool, buf);
	}
	*pdest = buf;
    }
}

static void vhost_alias_interpolate(request_rec *r, const char *name,
				    const char *map, const char *uri)
{
    /* 0..9 9..0 */
    enum { MAXDOTS = 19 };
    const char *dots[MAXDOTS+1];
    int ndots;

    char buf[HUGE_STRING_LEN];
    char *dest, last;

    int N, M, Np, Mp, Nd, Md;
    const char *start, *end;

    const char *p;

    ndots = 0;
    dots[ndots++] = name-1; /* slightly naughty */
    for (p = name; *p; ++p){
	if (*p == '.' && ndots < MAXDOTS) {
	    dots[ndots++] = p;
	}
    }
    dots[ndots] = p;

    r->filename = NULL;
  
    dest = buf;
    last = '\0';
    while (*map) {
	if (*map != '%') {
	    /* normal characters */
	    vhost_alias_checkspace(r, buf, &dest, 1);
	    last = *dest++ = *map++;
	    continue;
	}
	/* we are in a format specifier */
	++map;
	/* can't be a slash */
	last = '\0';
	/* %% -> % */
	if (*map == '%') {
	    ++map;
	    vhost_alias_checkspace(r, buf, &dest, 1);
	    *dest++ = '%';
	    continue;
	}
	/* port number */
	if (*map == 'p') {
	    ++map;
	    /* no. of decimal digits in a short plus one */
	    vhost_alias_checkspace(r, buf, &dest, 7);
	    dest += ap_snprintf(dest, 7, "%d", ap_get_server_port(r));
	    continue;
	}
	/* deal with %-N+.-M+ -- syntax is already checked */
	N = M = 0;   /* value */
	Np = Mp = 0; /* is there a plus? */
	Nd = Md = 0; /* is there a dash? */
	if (*map == '-') ++map, Nd = 1;
	N = *map++ - '0';
	if (*map == '+') ++map, Np = 1;
	if (*map == '.') {
	    ++map;
	    if (*map == '-') {
		++map, Md = 1;
	    }
	    M = *map++ - '0';
	    if (*map == '+') {
		++map, Mp = 1;
	    }
	}
	/* note that N and M are one-based indices, not zero-based */
	start = dots[0]+1; /* ptr to the first character */
	end = dots[ndots]; /* ptr to the character after the last one */
	if (N != 0) {
	    if (N > ndots) {
		start = "_";
		end = start+1;
	    }
	    else if (!Nd) {
		start = dots[N-1]+1;
		if (!Np) {
		    end = dots[N];
		}
	    }
	    else {
		if (!Np) {
		    start = dots[ndots-N]+1;
		}
		end = dots[ndots-N+1];
	    }
	}
	if (M != 0) {
	    if (M > end - start) {
		start = "_";
		end = start+1;
	    }
	    else if (!Md) {
		start = start+M-1;
		if (!Mp) {
		    end = start+1;
		}
	    }
	    else {
		if (!Mp) {
		    start = end-M;
		}
		end = end-M+1;
	    }
	}
	vhost_alias_checkspace(r, buf, &dest, end - start);
	for (p = start; p < end; ++p) {
	    *dest++ = ap_tolower(*p);
	}
    }
    *dest = '\0';
    /* no double slashes */
    if (last == '/') {
	++uri;
    }
    if (r->filename) {
	r->filename = ap_pstrcat(r->pool, r->filename, buf, uri, NULL);
    }
    else {
	r->filename = ap_pstrcat(r->pool, buf, uri, NULL);
    }
}

static int mva_translate(request_rec *r)
{
    mva_sconf_t *conf;
    const char *name, *map, *uri;
    mva_mode_e mode;
    int cgi;
  
    conf = (mva_sconf_t *) ap_get_module_config(r->server->module_config,
					      &vhost_alias_module);
    if (!strncmp(r->uri, "/cgi-bin/", 9)) {
	mode = conf->cgi_root_mode;
	map = conf->cgi_root;
	uri = r->uri + 8;
	/*
	 * can't force cgi immediately because we might not handle this
	 * call if the mode is wrong
	 */
	cgi = 1;
    }
    else if (r->uri[0] == '/') {
	mode = conf->doc_root_mode;
	map = conf->doc_root;
	uri = r->uri;
	cgi = 0;
    }
    else {
	return DECLINED;
    }
  
    if (mode == VHOST_ALIAS_NAME) {
	name = ap_get_server_name(r);
    }
    else if (mode == VHOST_ALIAS_IP) {
	name = r->connection->local_ip;
    }
    else {
	return DECLINED;
    }

    vhost_alias_interpolate(r, name, map, uri);

    if (cgi) {
	/* see is_scriptaliased() in mod_cgi */
	r->handler = "cgi-script";
	ap_table_setn(r->notes, "alias-forced-type", r->handler);
    }

    return OK;
}


module MODULE_VAR_EXPORT vhost_alias_module =
{
    STANDARD_MODULE_STUFF,
    NULL,			/* initializer */
    NULL,			/* dir config creater */
    NULL,			/* dir merger --- default is to override */
    mva_create_server_config,	/* server config */
    mva_merge_server_config,	/* merge server configs */
    mva_commands,		/* command table */
    NULL,			/* handlers */
    mva_translate,		/* filename translation */
    NULL,			/* check_user_id */
    NULL,			/* check auth */
    NULL,			/* check access */
    NULL,			/* type_checker */
    NULL,			/* fixups */
    NULL,			/* logger */
    NULL,			/* header parser */
    NULL,			/* child_init */
    NULL,			/* child_exit */
    NULL			/* post read-request */
};
