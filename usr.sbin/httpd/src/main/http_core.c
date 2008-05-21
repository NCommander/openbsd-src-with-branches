/* $OpenBSD: http_core.c,v 1.24 2008/05/15 06:05:43 mbalmer Exp $ */

/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
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
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

#define CORE_PRIVATE
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"	/* For index_of_response().  Grump. */
#include "http_request.h"
#include "http_conf_globals.h"
#include "http_vhost.h"
#include "http_main.h"		/* For the default_handler below... */
#include "http_log.h"
#include "rfc1413.h"
#include "util_md5.h"
#include "scoreboard.h"
#include "fnmatch.h"

#include <sys/mman.h>

/* mmap support for static files based on ideas from John Heidemann's
 * patch against 1.0.5.  See
 * <http://www.isi.edu/~johnh/SOFTWARE/APACHE/index.html>.
 */

/* Files have to be at least this big before they're mmap()d.  This is to deal
 * with systems where the expense of doing an mmap() and an munmap() outweighs
 * the benefit for small files.  It shouldn't be set lower than 1.
 */
#ifndef MMAP_THRESHOLD
#define MMAP_THRESHOLD		1
#endif
#ifndef MMAP_LIMIT
#define MMAP_LIMIT              (4*1024*1024)
#endif

/* Server core module... This module provides support for really basic
 * server operations, including options and commands which control the
 * operation of other modules.  Consider this the bureaucracy module.
 *
 * The core module also defines handlers, etc., do handle just enough
 * to allow a server with the core module ONLY to actually serve documents
 * (though it slaps DefaultType on all of 'em); this was useful in testing,
 * but may not be worth preserving.
 *
 * This file could almost be mod_core.c, except for the stuff which affects
 * the http_conf_globals.
 */

static void *
create_core_dir_config(pool *a, char *dir)
{
	core_dir_config *conf;

	conf = (core_dir_config *)ap_pcalloc(a, sizeof(core_dir_config));
	if (!dir || dir[strlen(dir) - 1] == '/')
		conf->d = dir;
	else if (strncmp(dir, "proxy:", 6) == 0)
		conf->d = ap_pstrdup(a, dir);
	else
		conf->d = ap_pstrcat(a, dir, "/", NULL);

	conf->d_is_fnmatch = conf->d ? (ap_is_fnmatch(conf->d) != 0) : 0;
	conf->d_components = conf->d ? ap_count_dirs(conf->d) : 0;

	conf->opts = dir ? OPT_UNSET : OPT_UNSET|OPT_ALL;
	conf->opts_add = conf->opts_remove = OPT_NONE;
	conf->override = dir ? OR_UNSET : OR_UNSET|OR_ALL;

	conf->content_md5 = 2;

	conf->use_canonical_name = USE_CANONICAL_NAME_UNSET;

	conf->hostname_lookups = HOSTNAME_LOOKUP_UNSET;
	conf->do_rfc1413 = DEFAULT_RFC1413 | 2; /* set bit 1 to indicate
						 * default
						 */
	conf->satisfy = SATISFY_NOSPEC;

	conf->limit_cpu = NULL;
	conf->limit_mem = NULL;
	conf->limit_nproc = NULL;
	conf->limit_nofile = NULL;

	conf->limit_req_body = 0;
	conf->sec = ap_make_array(a, 2, sizeof(void *));

	conf->server_signature = srv_sig_unset;

	conf->add_default_charset = ADD_DEFAULT_CHARSET_UNSET;
	conf->add_default_charset_name = DEFAULT_ADD_DEFAULT_CHARSET_NAME;

	/* Flag for use of inodes in ETags. */
	conf->etag_bits = ETAG_UNSET;
	conf->etag_add = ETAG_UNSET;
	conf->etag_remove = ETAG_UNSET;

	return (void *)conf;
}

static void *
merge_core_dir_configs(pool *a, void *basev, void *newv)
{
	core_dir_config *base = (core_dir_config *)basev;
	core_dir_config *new = (core_dir_config *)newv;
	core_dir_config *conf;
	int i;

	conf = (core_dir_config *)ap_palloc(a, sizeof(core_dir_config));
	memcpy((char *)conf, (const char *)base, sizeof(core_dir_config));
	if (base->response_code_strings) {
		conf->response_code_strings =
		ap_palloc(a, sizeof(*conf->response_code_strings)
		    * RESPONSE_CODES);
		memcpy(conf->response_code_strings, base->response_code_strings,
		    sizeof(*conf->response_code_strings) * RESPONSE_CODES);
	}

	conf->d = new->d;
	conf->d_is_fnmatch = new->d_is_fnmatch;
	conf->d_components = new->d_components;
	conf->r = new->r;

	if (new->opts & OPT_UNSET) {
		/* there was no explicit setting of new->opts, so we merge
		* preserve the invariant (opts_add & opts_remove) == 0
		*/
		conf->opts_add = (conf->opts_add & ~new->opts_remove) |
		    new->opts_add;
		conf->opts_remove = (conf->opts_remove & ~new->opts_add) |
		    new->opts_remove;
		conf->opts = (conf->opts & ~conf->opts_remove) | conf->opts_add;
		if ((base->opts & OPT_INCNOEXEC) && (new->opts & OPT_INCLUDES))
			conf->opts = (conf->opts & ~OPT_INCNOEXEC) |
			    OPT_INCLUDES;
	} else {
		/* otherwise we just copy, because an explicit opts setting
		* overrides all earlier +/- modifiers
		*/
		conf->opts = new->opts;
		conf->opts_add = new->opts_add;
		conf->opts_remove = new->opts_remove;
	}

	if (!(new->override & OR_UNSET))
		conf->override = new->override;
	if (new->ap_default_type)
		conf->ap_default_type = new->ap_default_type;
	if (new->ap_auth_type)
		conf->ap_auth_type = new->ap_auth_type;
	if (new->ap_auth_name)
		conf->ap_auth_name = new->ap_auth_name;
	if (new->ap_auth_nonce)
		conf->ap_auth_nonce = new->ap_auth_nonce;
	if (new->ap_requires)
		conf->ap_requires = new->ap_requires;

	if (new->response_code_strings) {
		if (conf->response_code_strings == NULL) {
			conf->response_code_strings = ap_palloc(a,
			    sizeof(*conf->response_code_strings) *
			    RESPONSE_CODES);
			memcpy(conf->response_code_strings,
			    new->response_code_strings,
			    sizeof(*conf->response_code_strings) *
			    RESPONSE_CODES);
		} else {
			for (i = 0; i < RESPONSE_CODES; ++i) {
				if (new->response_code_strings[i] != NULL)
					conf->response_code_strings[i]
					    = new->response_code_strings[i];
			}
		}
	}
	if (new->hostname_lookups != HOSTNAME_LOOKUP_UNSET)
		conf->hostname_lookups = new->hostname_lookups;
	if ((new->do_rfc1413 & 2) == 0)
		conf->do_rfc1413 = new->do_rfc1413;
	if ((new->content_md5 & 2) == 0)
		conf->content_md5 = new->content_md5;
	if (new->use_canonical_name != USE_CANONICAL_NAME_UNSET)
		conf->use_canonical_name = new->use_canonical_name;

	if (new->limit_cpu)
		conf->limit_cpu = new->limit_cpu;
	if (new->limit_mem)
		conf->limit_mem = new->limit_mem;
	if (new->limit_nproc)
		conf->limit_nproc = new->limit_nproc;
	if (new->limit_nofile)
		conf->limit_nofile = new->limit_nofile;

	if (new->limit_req_body)
		conf->limit_req_body = new->limit_req_body;

	conf->sec = ap_append_arrays(a, base->sec, new->sec);

	if (new->satisfy != SATISFY_NOSPEC)
		conf->satisfy = new->satisfy;

	if (new->server_signature != srv_sig_unset)
		conf->server_signature = new->server_signature;

	if (new->add_default_charset != ADD_DEFAULT_CHARSET_UNSET) {
		conf->add_default_charset = new->add_default_charset;
		if (new->add_default_charset_name)
			conf->add_default_charset_name =
			    new->add_default_charset_name;
	}

	/* Now merge the setting of the FileETag directive. */
	if (new->etag_bits == ETAG_UNSET) {
		conf->etag_add =
		    (conf->etag_add & (~ new->etag_remove)) | new->etag_add;
		conf->etag_remove =
		    (conf->opts_remove & (~ new->etag_add)) | new->etag_remove;
		conf->etag_bits =
		    (conf->etag_bits & (~ conf->etag_remove)) | conf->etag_add;
	} else {
		conf->etag_bits = new->etag_bits;
		conf->etag_add = new->etag_add;
		conf->etag_remove = new->etag_remove;
	}
	if (conf->etag_bits != ETAG_NONE)
		conf->etag_bits &= (~ ETAG_NONE);

	if (new->cgi_command_args != AP_FLAG_UNSET)
		conf->cgi_command_args = new->cgi_command_args;
	ap_server_strip_chroot(conf->d, 0);

	return (void*)conf;
}

static void *
create_core_server_config(pool *a, server_rec *s)
{
	core_server_config *conf;
	int is_virtual = s->is_virtual;

	conf = (core_server_config *)ap_pcalloc(a, sizeof(core_server_config));
#ifdef GPROF
	conf->gprof_dir = NULL;
#endif
	conf->access_name = is_virtual ? NULL : DEFAULT_ACCESS_FNAME;
	conf->ap_document_root = is_virtual ? NULL : DOCUMENT_LOCATION;
	conf->sec = ap_make_array(a, 40, sizeof(void *));
	conf->sec_url = ap_make_array(a, 40, sizeof(void *));

	/* recursion stopper */
	conf->redirect_limit = 0;
	conf->subreq_limit = 0;
	conf->recursion_limit_set = 0;

	return (void *)conf;
}

static void *
merge_core_server_configs(pool *p, void *basev, void *virtv)
{
	core_server_config *base = (core_server_config *)basev;
	core_server_config *virt = (core_server_config *)virtv;
	core_server_config *conf;

	conf = (core_server_config *)ap_pcalloc(p, sizeof(core_server_config));
	*conf = *virt;
	if (!conf->access_name)
		conf->access_name = base->access_name;
	if (!conf->ap_document_root)
		conf->ap_document_root = base->ap_document_root;

	conf->sec = ap_append_arrays(p, base->sec, virt->sec);
	conf->sec_url = ap_append_arrays(p, base->sec_url, virt->sec_url);

	conf->redirect_limit = virt->recursion_limit_set
	    ? virt->redirect_limit : base->redirect_limit;

	conf->subreq_limit = virt->recursion_limit_set
	    ? virt->subreq_limit : base->subreq_limit;

	return conf;
}

/* Add per-directory configuration entry (for <directory> section);
 * these are part of the core server config.
 */

CORE_EXPORT(void)
ap_add_per_dir_conf(server_rec *s, void *dir_config)
{
	core_server_config *sconf = ap_get_module_config(s->module_config,
	    &core_module);
	void **new_space = (void **)ap_push_array(sconf->sec);

	*new_space = dir_config;
}

CORE_EXPORT(void)
ap_add_per_url_conf(server_rec *s, void *url_config)
{
	core_server_config *sconf = ap_get_module_config(s->module_config,
	    &core_module);
	void **new_space = (void **)ap_push_array(sconf->sec_url);

	*new_space = url_config;
}

CORE_EXPORT(void)
ap_add_file_conf(core_dir_config *conf, void *url_config)
{
	void **new_space = (void **)ap_push_array(conf->sec);

	*new_space = url_config;
}

/* core_reorder_directories reorders the directory sections such that the
 * 1-component sections come first, then the 2-component, and so on, finally
 * followed by the "special" sections.  A section is "special" if it's a regex,
 * or if it doesn't start with / -- consider proxy: matching.  All movements
 * are in-order to preserve the ordering of the sections from the config files.
 * See directory_walk().
 */

#define IS_SPECIAL(entry_core)	\
    ((entry_core)->r != NULL || (entry_core)->d[0] != '/')

/* We need to do a stable sort, qsort isn't stable.  So to make it stable
 * we'll be maintaining the original index into the list, and using it
 * as the minor key during sorting.  The major key is the number of
 * components (where a "special" section has infinite components).
 */
struct reorder_sort_rec {
	void	*elt;
	int	 orig_index;
};

static int
reorder_sorter(const void *va, const void *vb)
{
	const struct reorder_sort_rec *a = va;
	const struct reorder_sort_rec *b = vb;
	core_dir_config *core_a;
	core_dir_config *core_b;

	core_a = (core_dir_config *)ap_get_module_config(a->elt, &core_module);
	core_b = (core_dir_config *)ap_get_module_config(b->elt, &core_module);
	if (IS_SPECIAL(core_a)) {
		if (!IS_SPECIAL(core_b))
			return 1;
	} else if (IS_SPECIAL(core_b))
		return -1;
	else {
		/* we know they're both not special */
		if (core_a->d_components < core_b->d_components)
			return -1;
		else if (core_a->d_components > core_b->d_components)
			return 1;
	}
	/* Either they're both special, or they're both not special and have the
	* same number of components.  In any event, we now have to compare
	* the minor key. */
	return a->orig_index - b->orig_index;
}

CORE_EXPORT(void)
ap_core_reorder_directories(pool *p, server_rec *s)
{
	core_server_config *sconf;
	array_header *sec;
	struct reorder_sort_rec *sortbin;
	int nelts;
	void **elts;
	int i;
	pool *tmp;

	sconf = ap_get_module_config(s->module_config, &core_module);
	sec = sconf->sec;
	nelts = sec->nelts;
	elts = (void **)sec->elts;

	/* we have to allocate tmp space to do a stable sort */
	tmp = ap_make_sub_pool(p);
	sortbin = ap_palloc(tmp, sec->nelts * sizeof(*sortbin));
	for (i = 0; i < nelts; ++i) {
		sortbin[i].orig_index = i;
		sortbin[i].elt = elts[i];
	}

	qsort(sortbin, nelts, sizeof(*sortbin), reorder_sorter);

	/* and now copy back to the original array */
	for (i = 0; i < nelts; ++i)
		elts[i] = sortbin[i].elt;

	ap_destroy_pool(tmp);
}

/*****************************************************************
 *
 * There are some elements of the core config structures in which
 * other modules have a legitimate interest (this is ugly, but necessary
 * to preserve NCSA back-compatibility).  So, we have a bunch of accessors
 * here...
 */

API_EXPORT(int)
ap_allow_options(request_rec *r)
{
	core_dir_config *conf = 
	    (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module); 

	return conf->opts; 
} 

API_EXPORT(int)
ap_allow_overrides(request_rec *r) 
{ 
	core_dir_config *conf;
	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module); 

	return conf->override; 
} 

API_EXPORT(const char *)
ap_auth_type(request_rec *r)
{
	core_dir_config *conf;

	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module); 
	return conf->ap_auth_type;
}

API_EXPORT(const char *)
ap_auth_name(request_rec *r)
{
	core_dir_config *conf;

	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module); 
	return conf->ap_auth_name;
}

API_EXPORT(const char *)
ap_auth_nonce(request_rec *r)
{
	core_dir_config *conf;
	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module);
	if (conf->ap_auth_nonce)
		return conf->ap_auth_nonce;

	/* Ideally we'd want to mix in some per-directory style
	 * information; as we are likely to want to detect replay
	 * across those boundaries and some randomness. But that
	 * is harder due to the adhoc nature of .htaccess memory
	 * structures, restarts and forks.
	 *
	 * But then again - you should use AuthDigestRealmSeed in your config
	 * file if you care. So the adhoc value should do.
	 */
	return ap_psprintf(r->pool,"%pp%pp%pp%pp%pp",
	    (void *)&(r->connection->local_host),
	    (void *)ap_user_name,
	    (void *)ap_listeners,
	    (void *)ap_server_argv0,
	    (void *)ap_pid_fname);
}

API_EXPORT(const char *)
ap_default_type(request_rec *r)
{
	core_dir_config *conf;

	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module); 
	return conf->ap_default_type 
	    ? conf->ap_default_type : DEFAULT_CONTENT_TYPE;
}

API_EXPORT(const char *)
ap_document_root(request_rec *r) /* Don't use this! */
{
	core_server_config *conf;

	conf =
	    (core_server_config *)ap_get_module_config(r->server->module_config,
	    &core_module); 
	return conf->ap_document_root;
}

API_EXPORT(const array_header *)
ap_requires(request_rec *r)
{
	core_dir_config *conf;

	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module); 
	return conf->ap_requires;
}

API_EXPORT(int)
ap_satisfies(request_rec *r)
{
	core_dir_config *conf;

	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module);

	return conf->satisfy;
}

/* Should probably just get rid of this... the only code that cares is
 * part of the core anyway (and in fact, it isn't publicised to other
 * modules).
 */
API_EXPORT(char *)
ap_response_code_string(request_rec *r, int error_index)
{
	core_dir_config *conf;

	conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module); 

	if (conf->response_code_strings == NULL)
		return NULL;

	return conf->response_code_strings[error_index];
}


/* Code from Harald Hanche-Olsen <hanche@imf.unit.no> */
/* Note: the function returns its result in conn->double_reverse:
 *       +1: forward lookup of the previously reverse-looked-up
 *           hostname in conn->remote_host succeeded, and at
 *           least one of its IP addresses matches the client.
 *       -1: forward lookup of conn->remote_host failed, or
 *           none of the addresses found matches the client connection
 *           (possible DNS spoof in the reverse zone!)
 *       If do_double_reverse() returns -1, then it also invalidates
 *       conn->remote_host to prevent an invalid name from appearing
 *       in the log files. Conn->remote_host is set to "", because
 *       a setting of NULL would allow another reverse lookup,
 *       depending on the flags given to ap_get_remote_host().
 */
static ap_inline void
do_double_reverse(conn_rec *conn)
{
	struct addrinfo hints, *res, *res0;
	char hostbuf1[128], hostbuf2[128]; /* INET6_ADDRSTRLEN(=46) is enough */
	int ok = 0;

	if (conn->double_reverse)
		/* already done */
		return;

	if (conn->remote_host == NULL || conn->remote_host[0] == '\0') {
		/* single reverse failed, so don't bother */
		conn->double_reverse = -1;
		conn->remote_host = ""; /* prevent another lookup */
		return;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(conn->remote_host, NULL, &hints, &res0)) {
		conn->double_reverse = -1;
		return;
	}
	for (res = res0; res; res = res->ai_next) {
		if (res->ai_addr->sa_family != conn->remote_addr.ss_family ||
		    !(res->ai_family == AF_INET 
		    || res->ai_family == AF_INET6))
			continue;
#ifndef HAVE_SOCKADDR_LEN
		if (res->ai_addrlen !=
		    SA_LEN((struct sockaddr *)&conn->remote_addr))
#else
		if (res->ai_addr->sa_len != conn->remote_addr.ss_len)
#endif
			continue;
		if (getnameinfo(res->ai_addr, res->ai_addrlen,
		    hostbuf1, sizeof(hostbuf1), NULL, 0, NI_NUMERICHOST))
			continue;
		if (getnameinfo(((struct sockaddr *)&conn->remote_addr),
		    res->ai_addrlen, hostbuf2, sizeof(hostbuf2), NULL, 0,
		    NI_NUMERICHOST))
			continue;
		if (strcmp(hostbuf1, hostbuf2) == 0){
			ok = 1;
			break;
		}
	}
	conn->double_reverse = ok ? 1 : -1;
	freeaddrinfo(res0);
}

API_EXPORT(const char *)
ap_get_remote_host(conn_rec *conn, void *dir_config, int type)
{
	int hostname_lookups;
	int old_stat = SERVER_DEAD;	/* we shouldn't ever be in this state */
	char hostnamebuf[MAXHOSTNAMELEN];

	/* If we haven't checked the host name, and we want to */
	if (dir_config) {
		hostname_lookups =
		    ((core_dir_config *)ap_get_module_config(dir_config,
		    &core_module))->hostname_lookups;
		if (hostname_lookups == HOSTNAME_LOOKUP_UNSET)
			hostname_lookups = HOSTNAME_LOOKUP_OFF;

	} else
		/* the default */
		hostname_lookups = HOSTNAME_LOOKUP_OFF;

	if (type != REMOTE_NOLOOKUP && conn->remote_host == NULL
	    && (type == REMOTE_DOUBLE_REV
	    || hostname_lookups != HOSTNAME_LOOKUP_OFF)) {
		old_stat = ap_update_child_status(conn->child_num,
		    SERVER_BUSY_DNS, (request_rec*)NULL);
		if (!getnameinfo((struct sockaddr *)&conn->remote_addr,
		    conn->remote_addr.ss_len,
		    hostnamebuf, sizeof(hostnamebuf), NULL, 0, 0)) {
			conn->remote_host = ap_pstrdup(conn->pool,
			    (void *)hostnamebuf);
			ap_str_tolower(conn->remote_host);

			if (hostname_lookups == HOSTNAME_LOOKUP_DOUBLE)
				do_double_reverse(conn);
		}
		/* if failed, set it to the NULL string to indicate error */
		if (conn->remote_host == NULL)
			conn->remote_host = "";
	}
	if (type == REMOTE_DOUBLE_REV) {
		do_double_reverse(conn);
		if (conn->double_reverse == -1)
			return NULL;
	}
	if (old_stat != SERVER_DEAD)
		(void)ap_update_child_status(conn->child_num, old_stat,
		    (request_rec*)NULL);

	/*
	 * Return the desired information; either the remote DNS name, if found,
	 * or either NULL (if the hostname was requested) or the IP address
	 * (if any identifier was requested).
	 */
	if (conn->remote_host != NULL && conn->remote_host[0] != '\0')
		return conn->remote_host;
	else {
		if (type == REMOTE_HOST || type == REMOTE_DOUBLE_REV)
			return NULL;
		else
			return conn->remote_ip;
	}
}

API_EXPORT(const char *)
ap_get_remote_logname(request_rec *r)
{
	core_dir_config *dir_conf;

	if (r->connection->remote_logname != NULL)
		return r->connection->remote_logname;

	/* If we haven't checked the identity, and we want to */
	dir_conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module);

	if (dir_conf->do_rfc1413 & 1)
		return ap_rfc1413(r->connection, r->server);
	else
		return NULL;
}

/* There are two options regarding what the "name" of a server is.  The
 * "canonical" name as defined by ServerName and Port, or the "client's
 * name" as supplied by a possible Host: header or full URI.  We never
 * trust the port passed in the client's headers, we always use the
 * port of the actual socket.
 *
 * The DNS option to UseCanonicalName causes this routine to do a
 * reverse lookup on the local IP address of the connectiona and use
 * that for the ServerName. This makes its value more reliable while
 * at the same time allowing Demon's magic virtual hosting to work.
 * The assumption is that DNS lookups are sufficiently quick...
 * -- fanf 1998-10-03
 */
API_EXPORT(const char *)
ap_get_server_name(request_rec *r)
{
	conn_rec *conn = r->connection;
	core_dir_config *d;
	char hbuf[MAXHOSTNAMELEN];

	d = (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module);

	if (d->use_canonical_name == USE_CANONICAL_NAME_OFF)
		return r->hostname ? r->hostname : r->server->server_hostname;

	if (d->use_canonical_name == USE_CANONICAL_NAME_DNS) {
		if (conn->local_host == NULL) {
			int old_stat;
			old_stat = ap_update_child_status(conn->child_num,
			    SERVER_BUSY_DNS, r);
			if (getnameinfo((struct sockaddr *)&conn->local_addr,
			    conn->local_addr.ss_len,
			    hbuf, sizeof(hbuf), NULL, 0, 0) == 0)
				conn->local_host = ap_pstrdup(conn->pool, hbuf);
			else
				conn->local_host = ap_pstrdup(conn->pool,
				    r->server->server_hostname);
			ap_str_tolower(conn->local_host);
			(void)ap_update_child_status(conn->child_num, old_stat,
			    r);
		}
		return conn->local_host;
	}
	/* default */
	return r->server->server_hostname;
}

API_EXPORT(unsigned)
ap_get_server_port(const request_rec *r)
{
	unsigned port;
	core_dir_config *d =
	    (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module);

	port = r->server->port ? r->server->port : ap_default_port(r);

	if (d->use_canonical_name == USE_CANONICAL_NAME_OFF
	    || d->use_canonical_name == USE_CANONICAL_NAME_DNS) {
		return r->hostname
		    ? ntohs(((struct sockaddr_in *)
		    &r->connection->local_addr)->sin_port)
		    : port;
	}
	return r->hostname
	    ? ntohs(((struct sockaddr_in *)
	    &r->connection->local_addr)->sin_port)
	    : port;
}

API_EXPORT(char *)
ap_construct_url(pool *p, const char *uri, request_rec *r)
{
	unsigned port = ap_get_server_port(r);
	const char *host = ap_get_server_name(r);

	if (ap_is_default_port(port, r))
		return ap_pstrcat(p, ap_http_method(r), "://", host, uri, NULL);
	return ap_psprintf(p, "%s://%s:%u%s", ap_http_method(r), host, port,
	    uri);
}

API_EXPORT(unsigned long)
ap_get_limit_req_body(const request_rec *r)
{
	core_dir_config *d =
	    (core_dir_config *)ap_get_module_config(r->per_dir_config,
	    &core_module);

	return d->limit_req_body;
}


/*****************************************************************
 *
 * Commands... this module handles almost all of the NCSA httpd.conf
 * commands, but most of the old srm.conf is in the the modules.
 */

static const char end_directory_section[] = "</Directory>";
static const char end_directorymatch_section[] = "</DirectoryMatch>";
static const char end_location_section[] = "</Location>";
static const char end_locationmatch_section[] = "</LocationMatch>";
static const char end_files_section[] = "</Files>";
static const char end_filesmatch_section[] = "</FilesMatch>";
static const char end_virtualhost_section[] = "</VirtualHost>";
static const char end_ifmodule_section[] = "</IfModule>";
static const char end_ifdefine_section[] = "</IfDefine>";


API_EXPORT(const char *) ap_check_cmd_context(cmd_parms *cmd,
					      unsigned forbidden)
{
    const char *gt = (cmd->cmd->name[0] == '<'
		      && cmd->cmd->name[strlen(cmd->cmd->name)-1] != '>')
                         ? ">" : "";

    if ((forbidden & NOT_IN_VIRTUALHOST) && cmd->server->is_virtual) {
	return ap_pstrcat(cmd->pool, cmd->cmd->name, gt,
			  " cannot occur within <VirtualHost> section", NULL);
    }

    if ((forbidden & NOT_IN_LIMIT) && cmd->limited != -1) {
	return ap_pstrcat(cmd->pool, cmd->cmd->name, gt,
			  " cannot occur within <Limit> section", NULL);
    }

    if ((forbidden & NOT_IN_DIR_LOC_FILE) == NOT_IN_DIR_LOC_FILE
	&& cmd->path != NULL) {
	return ap_pstrcat(cmd->pool, cmd->cmd->name, gt,
			  " cannot occur within <Directory/Location/Files> "
			  "section", NULL);
    }
    
    if (((forbidden & NOT_IN_DIRECTORY)
	 && (cmd->end_token == end_directory_section
	     || cmd->end_token == end_directorymatch_section)) 
	|| ((forbidden & NOT_IN_LOCATION)
	    && (cmd->end_token == end_location_section
		|| cmd->end_token == end_locationmatch_section)) 
	|| ((forbidden & NOT_IN_FILES)
	    && (cmd->end_token == end_files_section
		|| cmd->end_token == end_filesmatch_section))) {
	return ap_pstrcat(cmd->pool, cmd->cmd->name, gt,
			  " cannot occur within <", cmd->end_token+2,
			  " section", NULL);
    }

    return NULL;
}

static const char *set_access_name(cmd_parms *cmd, void *dummy, char *arg)
{
    void *sconf = cmd->server->module_config;
    core_server_config *conf = ap_get_module_config(sconf, &core_module);

    const char *err = ap_check_cmd_context(cmd,
					   NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    conf->access_name = ap_pstrdup(cmd->pool, arg);
    return NULL;
}

#ifdef GPROF
static const char *set_gprof_dir(cmd_parms *cmd, void *dummy, char *arg)
{
    void *sconf = cmd->server->module_config;
    core_server_config *conf = ap_get_module_config(sconf, &core_module);

    const char *err = ap_check_cmd_context(cmd,
					   NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    conf->gprof_dir = ap_pstrdup(cmd->pool, arg);
    return NULL;
}
#endif /*GPROF*/

static const char *set_add_default_charset(cmd_parms *cmd, 
	core_dir_config *d, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }
    if (!strcasecmp(arg, "Off")) {
       d->add_default_charset = ADD_DEFAULT_CHARSET_OFF;
    }
    else if (!strcasecmp(arg, "On")) {
       d->add_default_charset = ADD_DEFAULT_CHARSET_ON;
       d->add_default_charset_name = DEFAULT_ADD_DEFAULT_CHARSET_NAME;
    }
    else {
       d->add_default_charset = ADD_DEFAULT_CHARSET_ON;
       d->add_default_charset_name = arg;
    }
    return NULL;
}
static const char *set_accept_mutex(cmd_parms *cmd, void *dummy, char *arg)
{
	return ap_init_mutex_method(arg);
}

static const char *set_document_root(cmd_parms *cmd, void *dummy, char *arg)
{
    void *sconf = cmd->server->module_config;
    core_server_config *conf = ap_get_module_config(sconf, &core_module);
  
    const char *err = ap_check_cmd_context(cmd,
					   NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    arg = ap_os_canonical_filename(cmd->pool, arg);
    if (ap_configtestonly && ap_docrootcheck && !ap_is_directory(arg)) {
	if (cmd->server->is_virtual) {
	    fprintf(stderr, "Warning: DocumentRoot [%s] does not exist\n",
		    arg);
	}
	else {
	    return "DocumentRoot must be a directory";
	}
    }
    ap_server_strip_chroot(arg, 1);
    conf->ap_document_root = arg;
    return NULL;
}

API_EXPORT(void) ap_custom_response(request_rec *r, int status, char *string)
{
    core_dir_config *conf = 
	ap_get_module_config(r->per_dir_config, &core_module);
    int idx;

    ap_server_strip_chroot(conf->d, 0);
    if(conf->response_code_strings == NULL) {
        conf->response_code_strings = 
	    ap_pcalloc(r->pool,
		    sizeof(*conf->response_code_strings) * 
		    RESPONSE_CODES);
    }

    idx = ap_index_of_response(status);

    conf->response_code_strings[idx] = 
       ((ap_is_url(string) || (*string == '/')) && (*string != '"')) ? 
       ap_pstrdup(r->pool, string) : ap_pstrcat(r->pool, "\"", string, NULL);
}

static const char *set_error_document(cmd_parms *cmd, core_dir_config *conf,
				      char *line)
{
    int error_number, index_number, idx500;
    char *w;
                
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    /* 1st parameter should be a 3 digit number, which we recognize;
     * convert it into an array index
     */
  
    w = ap_getword_conf_nc(cmd->pool, &line);
    error_number = atoi(w);

    idx500 = ap_index_of_response(HTTP_INTERNAL_SERVER_ERROR);

    if (error_number == HTTP_INTERNAL_SERVER_ERROR) {
        index_number = idx500;
    }
    else if ((index_number = ap_index_of_response(error_number)) == idx500) {
        return ap_pstrcat(cmd->pool, "Unsupported HTTP response code ",
			  w, NULL);
    }

    /* The entry should be ignored if it is a full URL for a 401 error */

    if (error_number == 401 &&
	line[0] != '/' && line[0] != '"') { /* Ignore it... */
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, cmd->server,
		     "cannot use a full URL in a 401 ErrorDocument "
		     "directive --- ignoring!");
    }
    else { /* Store it... */
    	if (conf->response_code_strings == NULL) {
	    conf->response_code_strings =
		ap_pcalloc(cmd->pool,
			   sizeof(*conf->response_code_strings) * RESPONSE_CODES);
        }
        conf->response_code_strings[index_number] = ap_pstrdup(cmd->pool, line);
    }   

    return NULL;
}

/* access.conf commands...
 *
 * The *only* thing that can appear in access.conf at top level is a
 * <Directory> section.  NB we need to have a way to cut the srm_command_loop
 * invoked by dirsection (i.e., <Directory>) short when </Directory> is seen.
 * We do that by returning an error, which dirsection itself recognizes and
 * discards as harmless.  Cheesy, but it works.
 */

static const char *set_override(cmd_parms *cmd, core_dir_config *d,
				const char *l)
{
    char *w;
  
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    d->override = OR_NONE;
    while (l[0]) {
        w = ap_getword_conf(cmd->pool, &l);
	if (!strcasecmp(w, "Limit")) {
	    d->override |= OR_LIMIT;
	}
	else if (!strcasecmp(w, "Options")) {
	    d->override |= OR_OPTIONS;
	}
	else if (!strcasecmp(w, "FileInfo")) {
            d->override |= OR_FILEINFO;
	}
	else if (!strcasecmp(w, "AuthConfig")) {
	    d->override |= OR_AUTHCFG;
	}
	else if (!strcasecmp(w, "Indexes")) {
            d->override |= OR_INDEXES;
	}
	else if (!strcasecmp(w, "None")) {
	    d->override = OR_NONE;
	}
	else if (!strcasecmp(w, "All")) {
	    d->override = OR_ALL;
	}
	else {
	    return ap_pstrcat(cmd->pool, "Illegal override option ", w, NULL);
	}
	d->override &= ~OR_UNSET;
    }

    return NULL;
}

static const char *set_options(cmd_parms *cmd, core_dir_config *d,
			       const char *l)
{
    allow_options_t opt;
    int first = 1;
    char action;

    while (l[0]) {
        char *w = ap_getword_conf(cmd->pool, &l);
	action = '\0';

	if (*w == '+' || *w == '-') {
	    action = *(w++);
	}
	else if (first) {
  	    d->opts = OPT_NONE;
            first = 0;
        }
	    
	if (!strcasecmp(w, "Indexes")) {
	    opt = OPT_INDEXES;
	}
	else if (!strcasecmp(w, "Includes")) {
	    opt = OPT_INCLUDES;
	}
	else if (!strcasecmp(w, "IncludesNOEXEC")) {
	    opt = (OPT_INCLUDES | OPT_INCNOEXEC);
	}
	else if (!strcasecmp(w, "FollowSymLinks")) {
	    opt = OPT_SYM_LINKS;
	}
	else if (!strcasecmp(w, "SymLinksIfOwnerMatch")) {
	    opt = OPT_SYM_OWNER;
	}
	else if (!strcasecmp(w, "execCGI")) {
	    opt = OPT_EXECCGI;
	}
	else if (!strcasecmp(w, "MultiViews")) {
	    opt = OPT_MULTI;
	}
	else if (!strcasecmp(w, "RunScripts")) { /* AI backcompat. Yuck */
	    opt = OPT_MULTI|OPT_EXECCGI;
	}
	else if (!strcasecmp(w, "None")) {
	    opt = OPT_NONE;
	}
	else if (!strcasecmp(w, "All")) {
	    opt = OPT_ALL;
	}
	else {
	    return ap_pstrcat(cmd->pool, "Illegal option ", w, NULL);
	}

	/* we ensure the invariant (d->opts_add & d->opts_remove) == 0 */
	if (action == '-') {
	    d->opts_remove |= opt;
	    d->opts_add &= ~opt;
	    d->opts &= ~opt;
	}
	else if (action == '+') {
	    d->opts_add |= opt;
	    d->opts_remove &= ~opt;
	    d->opts |= opt;
	}
	else {
	    d->opts |= opt;
	}
    }

    return NULL;
}

static const char *satisfy(cmd_parms *cmd, core_dir_config *c, char *arg)
{
    if (!strcasecmp(arg, "all")) {
        c->satisfy = SATISFY_ALL;
    }
    else if (!strcasecmp(arg, "any")) {
        c->satisfy = SATISFY_ANY;
    }
    else {
        return "Satisfy either 'any' or 'all'.";
    }
    return NULL;
}

static const char *require(cmd_parms *cmd, core_dir_config *c, char *arg)
{
    require_line *r;
  
    if (!c->ap_requires) {
        c->ap_requires = ap_make_array(cmd->pool, 2, sizeof(require_line));
    }
    r = (require_line *)ap_push_array(c->ap_requires);
    r->requirement = ap_pstrdup(cmd->pool, arg);
    r->method_mask = cmd->limited;
    return NULL;
}

CORE_EXPORT_NONSTD(const char *) ap_limit_section(cmd_parms *cmd, void *dummy,
						  const char *arg)
{
    const char *limited_methods = ap_getword(cmd->pool, &arg, '>');
    void *tog = cmd->cmd->cmd_data;
    int limited = 0;
  
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    /* XXX: NB: Currently, we have no way of checking
     * whether <Limit> or <LimitExcept> sections are closed properly.
     * (If we would add a srm_command_loop() here we might...)
     */
    
    while (limited_methods[0]) {
        char *method = ap_getword_conf(cmd->pool, &limited_methods);
        int  methnum = ap_method_number_of(method);

        if (methnum == M_TRACE && !tog) {
            return "TRACE cannot be controlled by <Limit>";
        }
        else if (methnum == M_INVALID) {
            return ap_pstrcat(cmd->pool, "unknown method \"", method,
                              "\" in <Limit", tog ? "Except>" : ">", NULL);
        }
        else {
            limited |= (1 << methnum);
        }
    }

    /* Killing two features with one function,
     * if (tog == NULL) <Limit>, else <LimitExcept>
     */
    cmd->limited = tog ? ~limited : limited;
    return NULL;
}

static const char *endlimit_section(cmd_parms *cmd, void *dummy, void *dummy2)
{
    void *tog = cmd->cmd->cmd_data;

    if (cmd->limited == -1) {
        return tog ? "</LimitExcept> unexpected" : "</Limit> unexpected";
    }
    
    cmd->limited = -1;
    return NULL;
}

/*
 * When a section is not closed properly when end-of-file is reached,
 * then an error message should be printed:
 */
static const char *missing_endsection(cmd_parms *cmd, int nest)
{
    if (nest < 2) {
	return ap_psprintf(cmd->pool, "Missing %s directive at end-of-file",
			   cmd->end_token);
    }
    return ap_psprintf(cmd->pool, "%d missing %s directives at end-of-file",
		       nest, cmd->end_token);
}

/* We use this in <DirectoryMatch> and <FilesMatch>, to ensure that 
 * people don't get bitten by wrong-cased regex matches
 */

#define USE_ICASE 0

static const char *end_nested_section(cmd_parms *cmd, void *dummy)
{
    if (cmd->end_token == NULL) {
        return ap_pstrcat(cmd->pool, cmd->cmd->name,
			  " without matching <", cmd->cmd->name + 2, 
			  " section", NULL);
    }
    /*
     * This '!=' may look weird on a string comparison, but it's correct --
     * it's been set up so that checking for two pointers to the same datum
     * is valid here.  And faster.
     */
    if (cmd->cmd->name != cmd->end_token) {
	return ap_pstrcat(cmd->pool, "Expected ", cmd->end_token, " but saw ",
			  cmd->cmd->name, NULL);
    }
    return cmd->end_token;
}

/*
 * Report a missing-'>' syntax error.
 */
static char *unclosed_directive(cmd_parms *cmd)
{
    return ap_pstrcat(cmd->pool, cmd->cmd->name,
		      "> directive missing closing '>'", NULL);
}

static const char *dirsection(cmd_parms *cmd, void *dummy, const char *arg)
{
    const char *errmsg;
    char *endp = strrchr(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    core_dir_config *conf;
    void *new_dir_conf = ap_create_per_dir_config(cmd->pool);
    regex_t *r = NULL;
    const char *old_end_token;
    const command_rec *thiscmd = cmd->cmd;

    const char *err = ap_check_cmd_context(cmd,
					   NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    if (endp == NULL) {
	return unclosed_directive(cmd);
    }

    *endp = '\0';

    cmd->path = ap_getword_conf(cmd->pool, &arg);
    ap_server_strip_chroot(cmd->path, 1);
    cmd->override = OR_ALL|ACCESS_CONF;

    if (thiscmd->cmd_data) { /* <DirectoryMatch> */
	r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED|USE_ICASE);
    }
    else if (!strcmp(cmd->path, "~")) {
	cmd->path = ap_getword_conf(cmd->pool, &arg);
	ap_server_strip_chroot(cmd->path, 1);
	r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED|USE_ICASE);
    }
    else {
	/* Ensure that the pathname is canonical */
	cmd->path = ap_os_canonical_filename(cmd->pool, cmd->path);
    }

    old_end_token = cmd->end_token;
    cmd->end_token = thiscmd->cmd_data ? end_directorymatch_section : end_directory_section;
    errmsg = ap_srm_command_loop(cmd, new_dir_conf);
    if (errmsg == NULL) {
	errmsg = missing_endsection(cmd, 1);
    }
    cmd->end_token = old_end_token;
    if (errmsg != (thiscmd->cmd_data 
		       ? end_directorymatch_section 
		   : end_directory_section)) {
	return errmsg;
    }

    conf = (core_dir_config *)ap_get_module_config(new_dir_conf, &core_module);
    conf->r = r;

    ap_add_per_dir_conf(cmd->server, new_dir_conf);

    if (*arg != '\0') {
	return ap_pstrcat(cmd->pool, "Multiple ", thiscmd->name,
			  "> arguments not (yet) supported.", NULL);
    }

    cmd->path = old_path;
    cmd->override = old_overrides;

    return NULL;
}

static const char *urlsection(cmd_parms *cmd, void *dummy, const char *arg)
{
    const char *errmsg;
    char *endp = strrchr(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    core_dir_config *conf;
    regex_t *r = NULL;
    const char *old_end_token;
    const command_rec *thiscmd = cmd->cmd;

    void *new_url_conf = ap_create_per_dir_config(cmd->pool);

    const char *err = ap_check_cmd_context(cmd,
					   NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    if (endp == NULL) {
	return unclosed_directive(cmd);
    }

    *endp = '\0';

    cmd->path = ap_getword_conf(cmd->pool, &arg);
    ap_server_strip_chroot(cmd->path, 0);
    cmd->override = OR_ALL|ACCESS_CONF;

    if (thiscmd->cmd_data) { /* <LocationMatch> */
	r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED);
    }
    else if (!strcmp(cmd->path, "~")) {
	cmd->path = ap_getword_conf(cmd->pool, &arg);
	ap_server_strip_chroot(cmd->path, 0);
	r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED);
    }

    old_end_token = cmd->end_token;
    cmd->end_token = thiscmd->cmd_data ? end_locationmatch_section
                                       : end_location_section;
    errmsg = ap_srm_command_loop(cmd, new_url_conf);
    if (errmsg == NULL) {
	errmsg = missing_endsection(cmd, 1);
    }
    cmd->end_token = old_end_token;
    if (errmsg != (thiscmd->cmd_data 
		       ? end_locationmatch_section 
		       : end_location_section)) {
	return errmsg;
    }

    conf = (core_dir_config *)ap_get_module_config(new_url_conf, &core_module);
    conf->d = ap_pstrdup(cmd->pool, cmd->path);	/* No mangling, please */
    conf->d_is_fnmatch = ap_is_fnmatch(conf->d) != 0;
    conf->r = r;

    ap_add_per_url_conf(cmd->server, new_url_conf);
    
    if (*arg != '\0') {
	return ap_pstrcat(cmd->pool, "Multiple ", thiscmd->name,
			  "> arguments not (yet) supported.", NULL);
    }

    cmd->path = old_path;
    cmd->override = old_overrides;

    return NULL;
}

static const char *filesection(cmd_parms *cmd, core_dir_config *c,
			       const char *arg)
{
    const char *errmsg;
    char *endp = strrchr(arg, '>');
    int old_overrides = cmd->override;
    char *old_path = cmd->path;
    core_dir_config *conf;
    regex_t *r = NULL;
    const char *old_end_token;
    const command_rec *thiscmd = cmd->cmd;

    void *new_file_conf = ap_create_per_dir_config(cmd->pool);

    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT|NOT_IN_LOCATION);
    if (err != NULL) {
        return err;
    }

    if (endp == NULL) {
	return unclosed_directive(cmd);
    }

    *endp = '\0';

    cmd->path = ap_getword_conf(cmd->pool, &arg);
    ap_server_strip_chroot(cmd->path, 1);
    /* Only if not an .htaccess file */
    if (!old_path) {
	cmd->override = OR_ALL|ACCESS_CONF;
    }

    if (thiscmd->cmd_data) { /* <FilesMatch> */
        r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED|USE_ICASE);
    }
    else if (!strcmp(cmd->path, "~")) {
	cmd->path = ap_getword_conf(cmd->pool, &arg);
	ap_server_strip_chroot(cmd->path, 1);
	r = ap_pregcomp(cmd->pool, cmd->path, REG_EXTENDED|USE_ICASE);
    }
    else {
	/* Ensure that the pathname is canonical */
	cmd->path = ap_os_canonical_filename(cmd->pool, cmd->path);
    }

    old_end_token = cmd->end_token;
    cmd->end_token = thiscmd->cmd_data ? end_filesmatch_section : end_files_section;
    errmsg = ap_srm_command_loop(cmd, new_file_conf);
    if (errmsg == NULL) {
	errmsg = missing_endsection(cmd, 1);
    }
    cmd->end_token = old_end_token;
    if (errmsg != (thiscmd->cmd_data 
		       ? end_filesmatch_section 
		   : end_files_section)) {
	return errmsg;
    }

    conf = (core_dir_config *)ap_get_module_config(new_file_conf,
						   &core_module);
    conf->d = cmd->path;
    conf->d_is_fnmatch = ap_is_fnmatch(conf->d) != 0;
    conf->r = r;

    ap_add_file_conf(c, new_file_conf);

    if (*arg != '\0') {
	return ap_pstrcat(cmd->pool, "Multiple ", thiscmd->name,
			  "> arguments not (yet) supported.", NULL);
    }

    cmd->path = old_path;
    cmd->override = old_overrides;

    return NULL;
}

/* XXX: NB: Currently, we have no way of checking
 * whether <IfModule> sections are closed properly.
 * Extra (redundant, unpaired) </IfModule> directives are
 * simply silently ignored.
 */
static const char *end_ifmod(cmd_parms *cmd, void *dummy)
{
    return NULL;
}

static const char *start_ifmod(cmd_parms *cmd, void *dummy, char *arg)
{
    char *endp = strrchr(arg, '>');
    char l[MAX_STRING_LEN];
    int not = (arg[0] == '!');
    module *found;
    int nest = 1;

    if (endp == NULL) {
	return unclosed_directive(cmd);
    }

    *endp = '\0';

    if (not) {
        arg++;
    }

    found = ap_find_linked_module(arg);

    if ((!not && found) || (not && !found)) {
        return NULL;
    }

    while (nest && !(ap_cfg_getline(l, MAX_STRING_LEN, cmd->config_file))) {
        if (!strncasecmp(l, "<IfModule", 9)) {
	    nest++;
	}
	if (!strcasecmp(l, "</IfModule>")) {
	  nest--;
	}
    }

    if (nest) {
	cmd->end_token = end_ifmodule_section;
	return missing_endsection(cmd, nest);
    }
    return NULL;
}

API_EXPORT(int) ap_exists_config_define(char *name)
{
    char **defines;
    int i;

    defines = (char **)ap_server_config_defines->elts;
    for (i = 0; i < ap_server_config_defines->nelts; i++) {
        if (strcmp(defines[i], name) == 0) {
            return 1;
	}
    }
    return 0;
}

static const char *end_ifdefine(cmd_parms *cmd, void *dummy) 
{
    return NULL;
}

static const char *start_ifdefine(cmd_parms *cmd, void *dummy, char *arg)
{
    char *endp;
    char l[MAX_STRING_LEN];
    int defined;
    int not = 0;
    int nest = 1;

    endp = strrchr(arg, '>');
    if (endp == NULL) {
	return unclosed_directive(cmd);
    }

    *endp = '\0';

    if (arg[0] == '!') {
        not = 1;
	arg++;
    }

    defined = ap_exists_config_define(arg);

    if ((!not && defined) || (not && !defined)) {
	return NULL;
    }

    while (nest && !(ap_cfg_getline(l, MAX_STRING_LEN, cmd->config_file))) {
        if (!strncasecmp(l, "<IfDefine", 9)) {
	    nest++;
	}
	if (!strcasecmp(l, "</IfDefine>")) {
	    nest--;
	}
    }
    if (nest) {
	cmd->end_token = end_ifdefine_section;
	return missing_endsection(cmd, nest);
    }
    return NULL;
}

/* httpd.conf commands... beginning with the <VirtualHost> business */

static const char *virtualhost_section(cmd_parms *cmd, void *dummy, char *arg)
{
    server_rec *main_server = cmd->server, *s;
    const char *errmsg;
    char *endp = strrchr(arg, '>');
    pool *p = cmd->pool, *ptemp = cmd->temp_pool;
    const char *old_end_token;

    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    if (endp == NULL) {
	return unclosed_directive(cmd);
    }

    *endp = '\0';
    
    /* FIXME: There's another feature waiting to happen here -- since you
	can now put multiple addresses/names on a single <VirtualHost>
	you might want to use it to group common definitions and then
	define other "subhosts" with their individual differences.  But
	personally I'd rather just do it with a macro preprocessor. -djg */
    if (main_server->is_virtual) {
	return "<VirtualHost> doesn't nest!";
    }
    
    errmsg = ap_init_virtual_host(p, arg, main_server, &s);
    if (errmsg) {
	return errmsg;
    }

    s->next = main_server->next;
    main_server->next = s;

    s->defn_name = cmd->config_file->name;
    s->defn_line_number = cmd->config_file->line_number;

    old_end_token = cmd->end_token;
    cmd->end_token = end_virtualhost_section;
    cmd->server = s;
    errmsg = ap_srm_command_loop(cmd, s->lookup_defaults);
    cmd->server = main_server;
    if (errmsg == NULL) {
	errmsg = missing_endsection(cmd, 1);
    }
    cmd->end_token = old_end_token;

    if (s->srm_confname) {
	ap_process_resource_config(s, s->srm_confname, p, ptemp);
    }

    if (s->access_confname) {
	ap_process_resource_config(s, s->access_confname, p, ptemp);
    }
    
    if (errmsg == end_virtualhost_section) {
	return NULL;
    }
    return errmsg;
}

static const char *set_server_alias(cmd_parms *cmd, void *dummy,
				    const char *arg)
{
    if (!cmd->server->names) {
	return "ServerAlias only used in <VirtualHost>";
    }
    while (*arg) {
	char **item, *name = ap_getword_conf(cmd->pool, &arg);
	if (ap_is_matchexp(name)) {
	    item = (char **)ap_push_array(cmd->server->wild_names);
	}
	else {
	    item = (char **)ap_push_array(cmd->server->names);
	}
	*item = name;
    }
    return NULL;
}

static const char *add_module_command(cmd_parms *cmd, void *dummy, char *arg)
{
    module *modp;
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    for (modp = top_module; modp; modp = modp->next) {
        if (modp->name != NULL && strcmp(modp->name, arg) == 0) {
            ap_log_error(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, cmd->server,
                          "module %s is already added, skipping", arg);
            return NULL;
        }
    }

    if (!ap_add_named_module(arg)) {
	return ap_pstrcat(cmd->pool, "Cannot add module via name '", arg, 
			  "': not in list of loaded modules", NULL);
    }
    return NULL;
}

static const char *clear_module_list_command(cmd_parms *cmd, void *dummy)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_clear_module_list();
    return NULL;
}

static const char *set_server_string_slot(cmd_parms *cmd, void *dummy,
					  char *arg)
{
    /* This one's pretty generic... */
  
    int offset = (int)(long)cmd->info;
    char *struct_ptr = (char *)cmd->server;
    
    const char *err = ap_check_cmd_context(cmd, 
					   NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    *(char **)(struct_ptr + offset) = arg;
    return NULL;
}

static const char *server_type(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    if (!strcasecmp(arg, "inetd")) {
        ap_standalone = 0;
    }
    else if (!strcasecmp(arg, "standalone")) {
        ap_standalone = 1;
    }
    else {
        return "ServerType must be either 'inetd' or 'standalone'";
    }

    return NULL;
}

static const char *server_port(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    int port;

    if (err != NULL) {
	return err;
    }
    port = atoi(arg);
    if (port <= 0 || port >= 65536) { /* 65536 == 1<<16 */
	return ap_pstrcat(cmd->temp_pool, "The port number \"", arg, 
			  "\" is outside the appropriate range "
			  "(i.e., 1..65535).", NULL);
    }
    cmd->server->port = port;
    return NULL;
}

static const char *set_signature_flag(cmd_parms *cmd, core_dir_config *d, 
				      char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    if (strcasecmp(arg, "On") == 0) {
	d->server_signature = srv_sig_on;
    }
    else if (strcasecmp(arg, "Off") == 0) {
        d->server_signature = srv_sig_off;
    }
    else if (strcasecmp(arg, "EMail") == 0) {
	d->server_signature = srv_sig_withmail;
    }
    else {
	return "ServerSignature: use one of: off | on | email";
    }
    return NULL;
}

static const char *set_send_buffer_size(cmd_parms *cmd, void *dummy, char *arg)
{
    int s = atoi(arg);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    if (s < 512 && s != 0) {
        return "SendBufferSize must be >= 512 bytes, or 0 for system default.";
    }
    cmd->server->send_buffer_size = s;
    return NULL;
}

static const char *set_user(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    /*
     * This is, again, tricky. on restarts, we cannot use uname2id.
     * keep the old settings for the main server.
     * barf out on user directives in <VirtualHost> sections.
     */ 

    if (!cmd->server->is_virtual) {
	if (!ap_server_is_chrooted()) {
	    ap_user_name = arg;
	    ap_user_id = ap_uname2id(arg);
	}
	cmd->server->server_uid = ap_user_id;
    }
    else {
        if (ap_suexec_enabled) {
	    if (ap_server_is_chrooted()) {
		fprintf(stderr, "cannot look up uids once chrooted. Thus, User "
		    "directives inside <VirtualHost> and restarts aren't "
		    "possible together. Please stop httpd and start a new "
		    "one\n");
		exit(1);
	    } else
		cmd->server->server_uid = ap_uname2id(arg);
	}
	else {
	    cmd->server->server_uid = ap_user_id;
	    fprintf(stderr,
		    "Warning: User directive in <VirtualHost> "
		    "requires SUEXEC wrapper.\n");
	}
    }
#if !defined (BIG_SECURITY_HOLE)
    if (cmd->server->server_uid == 0) {
	fprintf(stderr,
		"Error:\tApache has not been designed to serve pages while\n"
		"\trunning as root.  There are known race conditions that\n"
		"\twill allow any local user to read any file on the system.\n"
		"\tIf you still desire to serve pages as root then\n"
		"\tadd -DBIG_SECURITY_HOLE to the EXTRA_CFLAGS line in your\n"
		"\tsrc/Configuration file and rebuild the server.  It is\n"
		"\tstrongly suggested that you instead modify the User\n"
		"\tdirective in your httpd.conf file to list a non-root\n"
		"\tuser.\n");
	exit (1);
    }
#endif

    return NULL;
}

static const char *set_group(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    if (!cmd->server->is_virtual) {
	if (!ap_server_is_chrooted()) {
	    ap_group_id = ap_gname2id(arg);
	}
	cmd->server->server_gid = ap_group_id;
    }
    else {
        if (ap_suexec_enabled) {
	    if (ap_server_is_chrooted()) {
		fprintf(stderr, "cannot look up gids once chrooted. Thus, Group"
		    " directives inside <VirtualHost> and restarts aren't "
		    "possible together. Please stop httpd and start a new "
		    "one\n");
		exit(1);
	    } else
		cmd->server->server_gid = ap_gname2id(arg);
	}
	else {
	    cmd->server->server_gid = ap_group_id;
	    fprintf(stderr,
		    "Warning: Group directive in <VirtualHost> requires "
		    "SUEXEC wrapper.\n");
	}
    }

    return NULL;
}

static const char *set_server_root(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    arg = ap_os_canonical_filename(cmd->pool, arg);

    /*
     * This is a bit tricky. On startup we are not chrooted here.
     * On restarts (graceful or not) we are (unless we're in unsecure mode).
     * if we would strip off the chroot prefix, nothing (not even "/")
     * would last.
     * it's pointless to test wether ServerRoot is a directory if we are
     * already chrooted into that. 
     * Of course it's impossible to change ServerRoot without a full restart.
     * should we abort with an error if ap_server_root != arg?
     */

    if (!ap_server_is_chrooted()) {
	if (!ap_is_directory(arg)) {
	    return "ServerRoot must be a valid directory";
	}
	/* ServerRoot is never '/' terminated */
	while (strlen(ap_server_root) > 1 && ap_server_root[strlen(ap_server_root)-1] == '/')
	    ap_server_root[strlen(ap_server_root)-1] = '\0';
	ap_cpystrn(ap_server_root, arg, sizeof(ap_server_root));
    }
    return NULL;
}

static const char *set_timeout(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    cmd->server->timeout = atoi(arg);
    return NULL;
}

static const char *set_keep_alive_timeout(cmd_parms *cmd, void *dummy,
					  char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    cmd->server->keep_alive_timeout = atoi(arg);
    return NULL;
}

static const char *set_keep_alive(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    /* We've changed it to On/Off, but used to use numbers
     * so we accept anything but "Off" or "0" as "On"
     */
    if (!strcasecmp(arg, "off") || !strcmp(arg, "0")) {
	cmd->server->keep_alive = 0;
    }
    else {
	cmd->server->keep_alive = 1;
    }
    return NULL;
}

static const char *set_keep_alive_max(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    cmd->server->keep_alive_max = atoi(arg);
    return NULL;
}

static const char *set_pidfile(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    if (cmd->server->is_virtual) {
	return "PidFile directive not allowed in <VirtualHost>";
    }
    ap_pid_fname = arg;
    return NULL;
}

static const char *set_scoreboard(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_scoreboard_fname = arg;
    return NULL;
}

static const char *set_lockfile(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_lock_fname = arg;
    return NULL;
}

static const char *set_idcheck(cmd_parms *cmd, core_dir_config *d, int arg) 
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    d->do_rfc1413 = arg != 0;
    return NULL;
}

static const char *set_hostname_lookups(cmd_parms *cmd, core_dir_config *d,
					char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    if (!strcasecmp(arg, "on")) {
	d->hostname_lookups = HOSTNAME_LOOKUP_ON;
    }
    else if (!strcasecmp(arg, "off")) {
	d->hostname_lookups = HOSTNAME_LOOKUP_OFF;
    }
    else if (!strcasecmp(arg, "double")) {
	d->hostname_lookups = HOSTNAME_LOOKUP_DOUBLE;
    }
    else {
	return "parameter must be 'on', 'off', or 'double'";
    }
    return NULL;
}

static const char *set_serverpath(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    cmd->server->path = arg;
    cmd->server->pathlen = strlen(arg);
    return NULL;
}

static const char *set_content_md5(cmd_parms *cmd, core_dir_config *d, int arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    d->content_md5 = arg != 0;
    return NULL;
}

static const char *set_use_canonical_name(cmd_parms *cmd, core_dir_config *d, 
					  char *arg)
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
	return err;
    }

    if (strcasecmp(arg, "on") == 0) {
        d->use_canonical_name = USE_CANONICAL_NAME_ON;
    }
    else if (strcasecmp(arg, "off") == 0) {
        d->use_canonical_name = USE_CANONICAL_NAME_OFF;
    }
    else if (strcasecmp(arg, "dns") == 0) {
        d->use_canonical_name = USE_CANONICAL_NAME_DNS;
    }
    else {
        return "parameter must be 'on', 'off', or 'dns'";
    }
    return NULL;
}

static const char *set_daemons_to_start(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_daemons_to_start = atoi(arg);
    return NULL;
}

static const char *set_min_free_servers(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_daemons_min_free = atoi(arg);
    if (ap_daemons_min_free <= 0) {
       fprintf(stderr, "WARNING: detected MinSpareServers set to non-positive.\n");
       fprintf(stderr, "Resetting to 1 to avoid almost certain Apache failure.\n");
       fprintf(stderr, "Please read the documentation.\n");
       ap_daemons_min_free = 1;
    }
       
    return NULL;
}

static const char *set_max_free_servers(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_daemons_max_free = atoi(arg);
    return NULL;
}

static const char *set_server_limit (cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_daemons_limit = atoi(arg);
    if (ap_daemons_limit > HARD_SERVER_LIMIT) {
       fprintf(stderr, "WARNING: MaxClients of %d exceeds compile time limit "
           "of %d servers,\n", ap_daemons_limit, HARD_SERVER_LIMIT);
       fprintf(stderr, " lowering MaxClients to %d.  To increase, please "
           "see the\n", HARD_SERVER_LIMIT);
       fprintf(stderr, " HARD_SERVER_LIMIT define in src/include/httpd.h.\n");
       ap_daemons_limit = HARD_SERVER_LIMIT;
    } 
    else if (ap_daemons_limit < 1) {
	fprintf(stderr, "WARNING: Require MaxClients > 0, setting to 1\n");
	ap_daemons_limit = 1;
    }
    return NULL;
}

static const char *set_child_rl_cpu(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_max_cpu_per_child = atoi(arg);
    return NULL;
}

static const char *set_child_rl_data(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_max_data_per_child = atoi(arg);
    return NULL;
}

static const char *set_child_rl_nofile(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_max_nofile_per_child = atoi(arg);
    return NULL;
}

static const char *set_child_rl_rss(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_max_rss_per_child = atoi(arg);
    return NULL;
}

static const char *set_child_rl_stack(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_max_stack_per_child = atoi(arg);
    return NULL;
}

static const char *set_max_requests(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_max_requests_per_child = atoi(arg);
    return NULL;
}

static const char *set_threads(cmd_parms *cmd, void *dummy, char *arg) {
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_threads_per_child = atoi(arg);
    if (ap_threads_per_child > HARD_SERVER_LIMIT) {
        fprintf(stderr, "WARNING: ThreadsPerChild of %d exceeds compile time limit "
                "of %d threads,\n", ap_threads_per_child, HARD_SERVER_LIMIT);
        fprintf(stderr, " lowering ThreadsPerChild to %d.  To increase, please "
                "see the\n", HARD_SERVER_LIMIT);
        fprintf(stderr, " HARD_SERVER_LIMIT define in src/include/httpd.h.\n");
        ap_threads_per_child = HARD_SERVER_LIMIT;
    } 
    else if (ap_threads_per_child < 1) {
	fprintf(stderr, "WARNING: Require ThreadsPerChild > 0, setting to 1\n");
	ap_threads_per_child = 1;
    }

    return NULL;
}

static const char *set_excess_requests(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_excess_requests_per_child = atoi(arg);
    return NULL;
}


static void set_rlimit(cmd_parms *cmd, struct rlimit **plimit, const char *arg,
                       const char * arg2, int type)
{
    char *str;
    struct rlimit *limit;
    /* If your platform doesn't define rlim_t then typedef it in ap_config.h */
    rlim_t cur = 0;
    rlim_t max = 0;

    *plimit = (struct rlimit *)ap_pcalloc(cmd->pool, sizeof(**plimit));
    limit = *plimit;
    if ((getrlimit(type, limit)) != 0)	{
	*plimit = NULL;
	ap_log_error(APLOG_MARK, APLOG_ERR, cmd->server,
		     "%s: getrlimit failed", cmd->cmd->name);
	return;
    }

    if ((str = ap_getword_conf(cmd->pool, &arg))) {
	if (!strcasecmp(str, "max")) {
	    cur = limit->rlim_max;
	}
	else {
	    cur = atol(str);
	}
    }
    else {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
		     "Invalid parameters for %s", cmd->cmd->name);
	return;
    }
    
    if (arg2 && (str = ap_getword_conf(cmd->pool, &arg2))) {
	max = atol(str);
    }

    /* if we aren't running as root, cannot increase max */
    if (geteuid()) {
	limit->rlim_cur = cur;
	if (max) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, cmd->server,
			 "Must be uid 0 to raise maximum %s", cmd->cmd->name);
	}
    }
    else {
        if (cur) {
	    limit->rlim_cur = cur;
	}
        if (max) {
	    limit->rlim_max = max;
	}
    }
}

static const char *set_limit_cpu(cmd_parms *cmd, core_dir_config *conf, 
				 char *arg, char *arg2)
{
    set_rlimit(cmd, &conf->limit_cpu, arg, arg2, RLIMIT_CPU);
    return NULL;
}

static const char *set_limit_mem(cmd_parms *cmd, core_dir_config *conf, 
				 char *arg, char * arg2)
{
    set_rlimit(cmd, &conf->limit_mem, arg, arg2, RLIMIT_DATA);
    return NULL;
}

static const char *set_limit_nproc(cmd_parms *cmd, core_dir_config *conf,  
				   char *arg, char * arg2)
{
    set_rlimit(cmd, &conf->limit_nproc, arg, arg2, RLIMIT_NPROC);
    return NULL;
}

static const char *set_limit_nofile(cmd_parms *cmd, core_dir_config *conf,  
				   char *arg, char * arg2)
{
    set_rlimit(cmd, &conf->limit_nofile, arg, arg2, RLIMIT_NOFILE);
    return NULL;
}

static const char *set_bind_address(cmd_parms *cmd, void *dummy, char *arg) 
{
    struct addrinfo hints, *res;
    struct sockaddr *sa;
    size_t sa_len;
    int error;
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    if (strcmp(arg, "*") == 0)
      arg = NULL;

    sa = ap_get_virthost_addr(arg, NULL);
#ifdef HAVE_SOCKADDR_LEN
    sa_len = sa->sa_len;
#else
    sa_len = SA_LEN(sa);
#endif
    memcpy(&ap_bind_address, &sa, sa_len);
    return NULL;
}


/* Though the AcceptFilter functionality is not available across
 * all platforms - we still allow the config directive to appear
 * on all platforms and do intentionally not tie it to the compile
 * time flag SO_ACCEPTFILTER. This makes configuration files significantly
 * more portable; especially as an <IfModule http_core.c> or some
 * other construct is not possible.
 */
static const char *set_acceptfilter(cmd_parms *cmd, void *dummy, int flag)
{
    return NULL;
}

static const char *set_listener(cmd_parms *cmd, void *dummy, char *h, char *p)
{
    listen_rec *new;
    char *host, *port, *endptr;
    struct addrinfo hints, *res;
    int error;
    
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    host = port = NULL;
    if (!p) {
      port = strrchr(h, ':');
      if (port != NULL) {
	if (port == h) {
	    return "Missing IP address";
	}
	else if (port[1] == '\0') {
	    return "Address must end in :<port-number>";
	}
	*(port++) = '\0';
	if (*h)
	    host = h;
      } else {
	host = NULL;
	port = h;
      }
    } else {
      host = h;
      port = p;
    }

    /* strip [] for ipv6 before calling getaddrinfo */
    if (host && host[0] == '[') {
      if (strlen(host) < 2 || host[strlen(host) - 1] != ']')
        return "Malformed IPv6 Address in :<host>";
      host[strlen(host) - 1] = 0;
      host++;
    }

    if (host && strcmp(host, "*") == 0)
      host = NULL;

    new = ap_pcalloc(cmd->pool, sizeof(listen_rec));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = host ? PF_UNSPEC : ap_default_family;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(host, port, &hints, &res);
    if (error || !res) {
      fprintf(stderr, "could not resolve ");
      if (host)
	fprintf(stderr, "host \"%s\" ", host);
      if (port)
        fprintf(stderr, "port \"%s\" ", port);
      fprintf(stderr, "--- %s\n", gai_strerror(error));
      exit(1);
    }
    if (res->ai_next) {
	if (host)
	  fprintf(stderr, "host \"%s\" ", host);
	if (port)
	  fprintf(stderr, "port \"%s\" ", port);
	fprintf(stderr, "resolved to multiple addresses, ambiguous.\n");
	exit(1);
    }

    memcpy(&new->local_addr, res->ai_addr, res->ai_addrlen);

    new->fd = -1;
    new->used = 0;
    new->next = ap_listeners;
    ap_listeners = new;
    return NULL;
}

static const char *set_listenbacklog(cmd_parms *cmd, void *dummy, char *arg) 
{
    int b;

    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    b = atoi(arg);
    if (b < 1) {
        return "ListenBacklog must be > 0";
    }
    ap_listenbacklog = b;
    return NULL;
}

static const char *set_coredumpdir (cmd_parms *cmd, void *dummy, char *arg) 
{
    struct stat finfo;
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    arg = ap_server_root_relative(cmd->pool, arg);
    if ((stat(arg, &finfo) == -1) || !S_ISDIR(finfo.st_mode)) {
	return ap_pstrcat(cmd->pool, "CoreDumpDirectory ", arg, 
			  " does not exist or is not a directory", NULL);
    }
    ap_cpystrn(ap_coredump_dir, arg, sizeof(ap_coredump_dir));
    return NULL;
}

static const char *include_config (cmd_parms *cmd, void *dummy, char *name)
{
    name = ap_server_root_relative(cmd->pool, name);
    
    ap_process_resource_config(cmd->server, name, cmd->pool, cmd->temp_pool);

    return NULL;
}

static const char *set_loglevel(cmd_parms *cmd, void *dummy, const char *arg) 
{
    char *str;
    
    const char *err = ap_check_cmd_context(cmd,
					   NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    if ((str = ap_getword_conf(cmd->pool, &arg))) {
        if (!strcasecmp(str, "emerg")) {
	    cmd->server->loglevel = APLOG_EMERG;
	}
	else if (!strcasecmp(str, "alert")) {
	    cmd->server->loglevel = APLOG_ALERT;
	}
	else if (!strcasecmp(str, "crit")) {
	    cmd->server->loglevel = APLOG_CRIT;
	}
	else if (!strcasecmp(str, "error")) {
	    cmd->server->loglevel = APLOG_ERR;
	}
	else if (!strcasecmp(str, "warn")) {
	    cmd->server->loglevel = APLOG_WARNING;
	}
	else if (!strcasecmp(str, "notice")) {
	    cmd->server->loglevel = APLOG_NOTICE;
	}
	else if (!strcasecmp(str, "info")) {
	    cmd->server->loglevel = APLOG_INFO;
	}
	else if (!strcasecmp(str, "debug")) {
	    cmd->server->loglevel = APLOG_DEBUG;
	}
	else {
            return "LogLevel requires level keyword: one of "
	           "emerg/alert/crit/error/warn/notice/info/debug";
	}
    }
    else {
        return "LogLevel requires level keyword";
    }

    return NULL;
}

API_EXPORT(const char *) ap_psignature(const char *prefix, request_rec *r)
{
    char sport[20];
    core_dir_config *conf;

    conf = (core_dir_config *)ap_get_module_config(r->per_dir_config,
						   &core_module);
    if ((conf->server_signature == srv_sig_off)
	    || (conf->server_signature == srv_sig_unset)) {
	return "";
    }

    ap_snprintf(sport, sizeof sport, "%u", (unsigned) ap_get_server_port(r));

    if (conf->server_signature == srv_sig_withmail) {
	return ap_pstrcat(r->pool, prefix, "<ADDRESS>" SERVER_BASEVERSION
			  " Server at <A HREF=\"mailto:",
			  r->server->server_admin, "\">",
			  ap_escape_html(r->pool, ap_get_server_name(r)),
                          "</A> Port ", sport,
			  "</ADDRESS>\n", NULL);
    }
    return ap_pstrcat(r->pool, prefix, "<ADDRESS>" SERVER_BASEVERSION
		      " Server at ",
                      ap_escape_html(r->pool, ap_get_server_name(r)),
                      " Port ", sport,
		      "</ADDRESS>\n", NULL);
}

/*
 * Load an authorisation realm into our location configuration, applying the
 * usual rules that apply to realms.
 */
static const char *set_authname(cmd_parms *cmd, void *mconfig, char *word1)
{
    core_dir_config *aconfig = (core_dir_config *)mconfig;

    aconfig->ap_auth_name = ap_escape_quotes(cmd->pool, word1);
    return NULL;
}

/*
 * Load an authorisation nonce into our location configuration, and
 * force it to be in the 0-9/A-Z realm.
 */
static const char *set_authnonce (cmd_parms *cmd, void *mconfig, char *word1)
{
    core_dir_config *aconfig = (core_dir_config *)mconfig;
    size_t i;

    aconfig->ap_auth_nonce = ap_escape_quotes(cmd->pool, word1);

    if (strlen(aconfig->ap_auth_nonce) > 510)
       return "AuthDigestRealmSeed length limited to 510 chars for browser compatibility";

    for(i=0;i<strlen(aconfig->ap_auth_nonce );i++)
       if (!ap_isalnum(aconfig->ap_auth_nonce [i]))
         return "AuthDigestRealmSeed limited to 0-9 and A-Z range for browser compatibility";

    return NULL;
}


static const char *set_protocol_req_check(cmd_parms *cmd,
                                              core_dir_config *d, int arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_protocol_req_check = arg != 0;
    return NULL;
}

static const char *set_change_shmem_uid(cmd_parms *cmd,
                                              core_dir_config *d, int arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_change_shmem_uid = arg != 0;
    return NULL;
}

/*
 * Handle a request to include the server's OS platform in the Server
 * response header field (the ServerTokens directive).  Unfortunately
 * this requires a new global in order to communicate the setting back to
 * http_main so it can insert the information in the right place in the
 * string.
 */
static const char *set_serv_tokens(cmd_parms *cmd, void *dummy, char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    if (!strcasecmp(arg, "OS")) {
        ap_server_tokens = SrvTk_OS;
    }
    else if (!strcasecmp(arg, "Min") || !strcasecmp(arg, "Minimal")) {
        ap_server_tokens = SrvTk_MIN;
    }
    else if (!strcasecmp(arg, "Full")) {
        ap_server_tokens = SrvTk_FULL;
    }
    else if (!strcasecmp(arg, "Prod") || !strcasecmp(arg, "ProductOnly")) {
        ap_server_tokens = SrvTk_PRODUCT_ONLY;
    }
    else {
	return ap_pstrcat(cmd->pool, "Unrecognised ServerTokens keyword: ",
			  arg, NULL);
    }
    return NULL;
}

static const char *set_limit_req_line(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    int lim;

    if (err != NULL) {
        return err;
    }
    lim = atoi(arg);
    if (lim < 0) {
        return ap_pstrcat(cmd->temp_pool, "LimitRequestLine \"", arg, 
                          "\" must be a non-negative integer", NULL);
    }
    if (lim > DEFAULT_LIMIT_REQUEST_LINE) {
        return ap_psprintf(cmd->temp_pool, "LimitRequestLine \"%s\" "
                           "must not exceed the precompiled maximum of %d",
                           arg, DEFAULT_LIMIT_REQUEST_LINE);
    }
    cmd->server->limit_req_line = lim;
    return NULL;
}

static const char *set_limit_req_fieldsize(cmd_parms *cmd, void *dummy,
                                           char *arg)
{
    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    int lim;

    if (err != NULL) {
        return err;
    }
    lim = atoi(arg);
    if (lim < 0) {
        return ap_pstrcat(cmd->temp_pool, "LimitRequestFieldsize \"", arg, 
                          "\" must be a non-negative integer (0 = no limit)",
                          NULL);
    }
    if (lim > DEFAULT_LIMIT_REQUEST_FIELDSIZE) {
        return ap_psprintf(cmd->temp_pool, "LimitRequestFieldsize \"%s\" "
                          "must not exceed the precompiled maximum of %d",
                           arg, DEFAULT_LIMIT_REQUEST_FIELDSIZE);
    }
    cmd->server->limit_req_fieldsize = lim;
    return NULL;
}

static const char *set_limit_req_fields(cmd_parms *cmd, void *dummy, char *arg)
{
    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    int lim;

    if (err != NULL) {
        return err;
    }
    lim = atoi(arg);
    if (lim < 0) {
        return ap_pstrcat(cmd->temp_pool, "LimitRequestFields \"", arg, 
                          "\" must be a non-negative integer (0 = no limit)",
                          NULL);
    }
    cmd->server->limit_req_fields = lim;
    return NULL;
}

static const char *set_limit_req_body(cmd_parms *cmd, core_dir_config *conf,
                                      char *arg) 
{
    const char *err = ap_check_cmd_context(cmd, NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

    /* WTF: If strtoul is not portable, then write a replacement.
     *      Instead we have an idiotic define in httpd.h that prevents
     *      it from being used even when it is available. Sheesh.
     */
    conf->limit_req_body = (unsigned long)ap_strtol(arg, (char **)NULL, 10);
    return NULL;
}


static const char *set_cgi_command_args(cmd_parms *cmd,
                                              void *mconfig,
                                              int arg)
{
    core_dir_config *cfg = (core_dir_config *)mconfig;
    cfg->cgi_command_args = arg ? AP_FLAG_ON : AP_FLAG_OFF;
    return NULL;
}

/*
 * Note what data should be used when forming file ETag values.
 * It would be nicer to do this as an ITERATE, but then we couldn't
 * remember the +/- state properly.
 */
static const char *set_etag_bits(cmd_parms *cmd, void *mconfig,
                                 const char *args_p)
{
    core_dir_config *cfg;
    etag_components_t bit;
    char action;
    char *token;
    const char *args;
    int valid;
    int first;
    int explicit;

    cfg = (core_dir_config *) mconfig;

    args = args_p;
    first = 1;
    explicit = 0;
    while (args[0] != '\0') {
        action = '*';
        bit = ETAG_UNSET;
        valid = 1;
        token = ap_getword_conf(cmd->pool, &args);
        if ((*token == '+') || (*token == '-')) {
            action = *token;
            token++;
        }
        else {
            /*
             * The occurrence of an absolute setting wipes
             * out any previous relative ones.  The first such
             * occurrence forgets any inherited ones, too.
             */
            if (first) {
                cfg->etag_bits = ETAG_UNSET;
                cfg->etag_add = ETAG_UNSET;
                cfg->etag_remove = ETAG_UNSET;
                first = 0;
            }
        }

        if (strcasecmp(token, "None") == 0) {
            if (action != '*') {
                valid = 0;
            }
            else {
                cfg->etag_bits = bit = ETAG_NONE;
                explicit = 1;
            }
        }
        else if (strcasecmp(token, "All") == 0) {
            if (action != '*') {
                valid = 0;
            }
            else {
                explicit = 1;
                cfg->etag_bits = bit = ETAG_ALL;
            }
        }
        else if (strcasecmp(token, "Size") == 0) {
            bit = ETAG_SIZE;
        }
        else if ((strcasecmp(token, "LMTime") == 0)
                 || (strcasecmp(token, "MTime") == 0)
                 || (strcasecmp(token, "LastModified") == 0)) {
            bit = ETAG_MTIME;
        }
        else if (strcasecmp(token, "INode") == 0) {
            bit = ETAG_INODE;
        }
        else {
            return ap_pstrcat(cmd->pool, "Unknown keyword '",
                              token, "' for ", cmd->cmd->name,
                              " directive", NULL);
        }

        if (! valid) {
            return ap_pstrcat(cmd->pool, cmd->cmd->name, " keyword '",
                              token, "' cannot be used with '+' or '-'",
                              NULL);
        }

        if (action == '+') {
            /*
             * Make sure it's in the 'add' list and absent from the
             * 'subtract' list.
             */
            cfg->etag_add |= bit;
            cfg->etag_remove &= (~ bit);
        }
        else if (action == '-') {
            cfg->etag_remove |= bit;
            cfg->etag_add &= (~ bit);
        }
        else {
            /*
             * Non-relative values wipe out any + or - values
             * accumulated so far.
             */
            cfg->etag_bits |= bit;
            cfg->etag_add = ETAG_UNSET;
            cfg->etag_remove = ETAG_UNSET;
            explicit = 1;
        }
    }

    /*
     * Any setting at all will clear the 'None' and 'Unset' bits.
     */

    if (cfg->etag_add != ETAG_UNSET) {
        cfg->etag_add &= (~ ETAG_UNSET);
    }
    if (cfg->etag_remove != ETAG_UNSET) {
        cfg->etag_remove &= (~ ETAG_UNSET);
    }
    if (explicit) {
        cfg->etag_bits &= (~ ETAG_UNSET);
        if ((cfg->etag_bits & ETAG_NONE) != ETAG_NONE) {
            cfg->etag_bits &= (~ ETAG_NONE);
        }
    }
    return NULL;
}

static const char *set_recursion_limit(cmd_parms *cmd, void *dummy,
                                       const char *arg1, const char *arg2)
{
    core_server_config *conf = ap_get_module_config(cmd->server->module_config,
                                                    &core_module);
    int limit = atoi(arg1);

    if (limit < 0) {
        return "The redirect recursion limit cannot be less than zero.";
    }
    if (limit && limit < 4) {
        ap_log_error(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, cmd->server,
                     "Limiting internal redirects to very low numbers may "
                     "cause normal requests to fail.");
    }

    conf->redirect_limit = limit;

    if (arg2) {
        limit = atoi(arg2);

        if (limit < 0) {
            return "The subrequest recursion limit cannot be less than zero.";
        }
        if (limit && limit < 4) {
            ap_log_error(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, cmd->server,
                         "Limiting the subrequest depth to a very low level may"
                         " cause normal requests to fail.");
        }
    }

    conf->subreq_limit = limit;
    conf->recursion_limit_set = 1;

    return NULL;
}

static void log_backtrace(const request_rec *r)
{
    const request_rec *top = r;

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, r,
                  "r->uri = %s", r->uri ? r->uri : "(unexpectedly NULL)");

    while (top && (top->prev || top->main)) {
        if (top->prev) {
            top = top->prev;
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, r,
                          "redirected from r->uri = %s",
                          top->uri ? top->uri : "(unexpectedly NULL)");
        }

        if (!top->prev && top->main) {
            top = top->main;
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, r,
                          "subrequested from r->uri = %s",
                          top->uri ? top->uri : "(unexpectedly NULL)");
        }
    }
}

/*
 * check whether redirect limit is reached
 */
API_EXPORT(int) ap_is_recursion_limit_exceeded(const request_rec *r)
{
    core_server_config *conf = ap_get_module_config(r->server->module_config,
                                                    &core_module);
    const request_rec *top = r;
    int redirects = 0, subreqs = 0;
    int rlimit = conf->recursion_limit_set
                 ? conf->redirect_limit
                 : AP_DEFAULT_MAX_INTERNAL_REDIRECTS;
    int slimit = conf->recursion_limit_set
                 ? conf->subreq_limit
                 : AP_DEFAULT_MAX_SUBREQ_DEPTH;

    /* fast exit (unlimited) */
    if (!rlimit && !slimit) {
        return 0;
    }

    while (top->prev || top->main) {
        if (top->prev) {
            if (rlimit && ++redirects >= rlimit) {
                /* uuh, too much. */
                ap_log_rerror(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r,
                              "Request exceeded the limit of %d internal "
                              "redirects due to probable configuration error. "
                              "Use 'LimitInternalRecursion' to increase the "
                              "limit if necessary. Use 'LogLevel debug' to get "
                              "a backtrace.", rlimit);

                /* post backtrace */
                log_backtrace(r);

                /* return failure */
                return 1;
            }

            top = top->prev;
        }

        if (!top->prev && top->main) {
            if (slimit && ++subreqs >= slimit) {
                /* uuh, too much. */
                ap_log_rerror(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r,
                              "Request exceeded the limit of %d subrequest "
                              "nesting levels due to probable confguration "
                              "error. Use 'LimitInternalRecursion' to increase "
                              "the limit if necessary. Use 'LogLevel debug' to "
                              "get a backtrace.", slimit);

                /* post backtrace */
                log_backtrace(r);

                /* return failure */
                return 1;
            }

            top = top->main;
        }
    }

    /* recursion state: ok */
    return 0;
}

/* Note --- ErrorDocument will now work from .htaccess files.  
 * The AllowOverride of Fileinfo allows webmasters to turn it off
 */

static const command_rec core_cmds[] = {

/* Old access config file commands */

{ "<Directory", dirsection, NULL, RSRC_CONF, RAW_ARGS,
  "Container for directives affecting resources located in the specified "
  "directories" },
{ end_directory_section, end_nested_section, NULL, ACCESS_CONF, NO_ARGS,
  "Marks end of <Directory>" },
{ "<Location", urlsection, NULL, RSRC_CONF, RAW_ARGS,
  "Container for directives affecting resources accessed through the "
  "specified URL paths" },
{ end_location_section, end_nested_section, NULL, ACCESS_CONF, NO_ARGS,
  "Marks end of <Location>" },
{ "<VirtualHost", virtualhost_section, NULL, RSRC_CONF, RAW_ARGS,
  "Container to map directives to a particular virtual host, takes one or "
  "more host addresses" },
{ end_virtualhost_section, end_nested_section, NULL, RSRC_CONF, NO_ARGS,
  "Marks end of <VirtualHost>" },
{ "<Files", filesection, NULL, OR_ALL, RAW_ARGS, "Container for directives "
  "affecting files matching specified patterns" },
{ end_files_section, end_nested_section, NULL, OR_ALL, NO_ARGS,
  "Marks end of <Files>" },
{ "<Limit", ap_limit_section, NULL, OR_ALL, RAW_ARGS, "Container for "
  "authentication directives when accessed using specified HTTP methods" },
{ "</Limit>", endlimit_section, NULL, OR_ALL, NO_ARGS,
  "Marks end of <Limit>" },
{ "<LimitExcept", ap_limit_section, (void*)1, OR_ALL, RAW_ARGS,
  "Container for authentication directives to be applied when any HTTP "
  "method other than those specified is used to access the resource" },
{ "</LimitExcept>", endlimit_section, (void*)1, OR_ALL, NO_ARGS,
  "Marks end of <LimitExcept>" },
{ "<IfModule", start_ifmod, NULL, OR_ALL, TAKE1,
  "Container for directives based on existance of specified modules" },
{ end_ifmodule_section, end_ifmod, NULL, OR_ALL, NO_ARGS,
  "Marks end of <IfModule>" },
{ "<IfDefine", start_ifdefine, NULL, OR_ALL, TAKE1,
  "Container for directives based on existance of command line defines" },
{ end_ifdefine_section, end_ifdefine, NULL, OR_ALL, NO_ARGS,
  "Marks end of <IfDefine>" },
{ "<DirectoryMatch", dirsection, (void*)1, RSRC_CONF, RAW_ARGS,
  "Container for directives affecting resources located in the "
  "specified directories" },
{ end_directorymatch_section, end_nested_section, NULL, ACCESS_CONF, NO_ARGS,
  "Marks end of <DirectoryMatch>" },
{ "<LocationMatch", urlsection, (void*)1, RSRC_CONF, RAW_ARGS,
  "Container for directives affecting resources accessed through the "
  "specified URL paths" },
{ end_locationmatch_section, end_nested_section, NULL, ACCESS_CONF, NO_ARGS,
  "Marks end of <LocationMatch>" },
{ "<FilesMatch", filesection, (void*)1, OR_ALL, RAW_ARGS,
  "Container for directives affecting files matching specified patterns" },
{ end_filesmatch_section, end_nested_section, NULL, OR_ALL, NO_ARGS,
  "Marks end of <FilesMatch>" },
{ "AuthType", ap_set_string_slot,
  (void*)XtOffsetOf(core_dir_config, ap_auth_type), OR_AUTHCFG, TAKE1,
  "An HTTP authorization type (e.g., \"Basic\")" },
{ "AuthName", set_authname, NULL, OR_AUTHCFG, TAKE1,
  "The authentication realm (e.g. \"Members Only\")" },
{ "AuthDigestRealmSeed", set_authnonce, NULL, OR_AUTHCFG, TAKE1,
  "An authentication token which should be different for each logical realm. "\
  "A random value or the servers IP may be a good choise.\n" },
{ "Require", require, NULL, OR_AUTHCFG, RAW_ARGS,
  "Selects which authenticated users or groups may access a protected space" },
{ "Satisfy", satisfy, NULL, OR_AUTHCFG, TAKE1,
  "access policy if both allow and require used ('all' or 'any')" },    
#ifdef GPROF
{ "GprofDir", set_gprof_dir, NULL, RSRC_CONF, TAKE1,
  "Directory to plop gmon.out files" },
#endif
{ "AddDefaultCharset", set_add_default_charset, NULL, OR_FILEINFO, 
  TAKE1, "The name of the default charset to add to any Content-Type without one or 'Off' to disable" },

/* Old resource config file commands */
  
{ "AccessFileName", set_access_name, NULL, RSRC_CONF, RAW_ARGS,
  "Name(s) of per-directory config files (default: .htaccess)" },
{ "DocumentRoot", set_document_root, NULL, RSRC_CONF, TAKE1,
  "Root directory of the document tree"  },
{ "ErrorDocument", set_error_document, NULL, OR_FILEINFO, RAW_ARGS,
  "Change responses for HTTP errors" },
{ "AllowOverride", set_override, NULL, ACCESS_CONF, RAW_ARGS,
  "Controls what groups of directives can be configured by per-directory "
  "config files" },
{ "Options", set_options, NULL, OR_OPTIONS, RAW_ARGS,
  "Set a number of attributes for a given directory" },
{ "DefaultType", ap_set_string_slot,
  (void*)XtOffsetOf (core_dir_config, ap_default_type),
  OR_FILEINFO, TAKE1, "the default MIME type for untypable files" },

/* Old server config file commands */

{ "ServerType", server_type, NULL, RSRC_CONF, TAKE1,
  "'inetd' or 'standalone'"},
{ "Port", server_port, NULL, RSRC_CONF, TAKE1, "A TCP port number"},
{ "HostnameLookups", set_hostname_lookups, NULL, ACCESS_CONF|RSRC_CONF, TAKE1,
  "\"on\" to enable, \"off\" to disable reverse DNS lookups, or \"double\" to "
  "enable double-reverse DNS lookups" },
{ "User", set_user, NULL, RSRC_CONF, TAKE1,
  "Effective user id for this server"},
{ "Group", set_group, NULL, RSRC_CONF, TAKE1,
  "Effective group id for this server"},
{ "ServerAdmin", set_server_string_slot,
  (void *)XtOffsetOf (server_rec, server_admin), RSRC_CONF, TAKE1,
  "The email address of the server administrator" },
{ "ServerName", set_server_string_slot,
  (void *)XtOffsetOf (server_rec, server_hostname), RSRC_CONF, TAKE1,
  "The hostname of the server" },
{ "ServerSignature", set_signature_flag, NULL, OR_ALL, TAKE1,
  "En-/disable server signature (on|off|email)" },
{ "ServerRoot", set_server_root, NULL, RSRC_CONF, TAKE1,
  "Common directory of server-related files (logs, confs, etc.)" },
{ "ErrorLog", set_server_string_slot,
  (void *)XtOffsetOf (server_rec, error_fname), RSRC_CONF, TAKE1,
  "The filename of the error log" },
{ "PidFile", set_pidfile, NULL, RSRC_CONF, TAKE1,
    "A file for logging the server process ID"},
{ "ScoreBoardFile", set_scoreboard, NULL, RSRC_CONF, TAKE1,
    "A file for Apache to maintain runtime process management information"},
{ "LockFile", set_lockfile, NULL, RSRC_CONF, TAKE1,
    "The lockfile used when Apache needs to lock the accept() call"},
{ "AccessConfig", set_server_string_slot,
  (void *)XtOffsetOf (server_rec, access_confname), RSRC_CONF, TAKE1,
  "The filename of the access config file" },
{ "ResourceConfig", set_server_string_slot,
  (void *)XtOffsetOf (server_rec, srm_confname), RSRC_CONF, TAKE1,
  "The filename of the resource config file" },
{ "ServerAlias", set_server_alias, NULL, RSRC_CONF, RAW_ARGS,
  "A name or names alternately used to access the server" },
{ "ServerPath", set_serverpath, NULL, RSRC_CONF, TAKE1,
  "The pathname the server can be reached at" },
{ "Timeout", set_timeout, NULL, RSRC_CONF, TAKE1, "Timeout duration (sec)" },
{ "KeepAliveTimeout", set_keep_alive_timeout, NULL, RSRC_CONF, TAKE1,
  "Keep-Alive timeout duration (sec)"},
{ "MaxKeepAliveRequests", set_keep_alive_max, NULL, RSRC_CONF, TAKE1,
  "Maximum number of Keep-Alive requests per connection, or 0 for infinite" },
{ "KeepAlive", set_keep_alive, NULL, RSRC_CONF, TAKE1,
  "Whether persistent connections should be On or Off" },
{ "IdentityCheck", set_idcheck, NULL, RSRC_CONF|ACCESS_CONF, FLAG,
  "Enable identd (RFC 1413) user lookups - SLOW" },
{ "ContentDigest", set_content_md5, NULL, OR_OPTIONS,
  FLAG, "whether or not to send a Content-MD5 header with each request" },
{ "UseCanonicalName", set_use_canonical_name, NULL,
  RSRC_CONF|ACCESS_CONF, TAKE1,
  "How to work out the ServerName : Port when constructing URLs" },
{ "StartServers", set_daemons_to_start, NULL, RSRC_CONF, TAKE1,
  "Number of child processes launched at server startup" },
{ "MinSpareServers", set_min_free_servers, NULL, RSRC_CONF, TAKE1,
  "Minimum number of idle children, to handle request spikes" },
{ "MaxSpareServers", set_max_free_servers, NULL, RSRC_CONF, TAKE1,
  "Maximum number of idle children" },
{ "MaxServers", set_max_free_servers, NULL, RSRC_CONF, TAKE1,
  "Deprecated equivalent to MaxSpareServers" },
{ "ServersSafetyLimit", set_server_limit, NULL, RSRC_CONF, TAKE1,
  "Deprecated equivalent to MaxClients" },
{ "MaxClients", set_server_limit, NULL, RSRC_CONF, TAKE1,
  "Maximum number of children alive at the same time" },
{ "MaxRequestsPerChild", set_max_requests, NULL, RSRC_CONF, TAKE1,
  "Maximum number of requests a particular child serves before dying." },
{ "MaxCPUPerChild", set_child_rl_cpu, NULL, RSRC_CONF, TAKE1,
  "Maximum amount of CPU time a child can use (rlimit)." },
{ "MaxDATAPerChild", set_child_rl_data, NULL, RSRC_CONF, TAKE1,
  "Maximum size of the data segment for a child process (rlimit)." },
{ "MaxNOFILEPerChild", set_child_rl_nofile, NULL, RSRC_CONF, TAKE1,
  "Maximum number of open file descriptors a child can have (rlimit)." },
{ "MaxRSSPerChild", set_child_rl_rss, NULL, RSRC_CONF, TAKE1,
  "Maximum amount of physical memory a child can use (rlimit)." },
{ "MaxSTACKPerChild", set_child_rl_stack, NULL, RSRC_CONF, TAKE1,
  "Maximum amount of stack space a child can use (rlimit)." },
{ "RLimitCPU",
  set_limit_cpu, (void*)XtOffsetOf(core_dir_config, limit_cpu),
  OR_ALL, TAKE12, "Soft/hard limits for max CPU usage in seconds" },
{ "RLimitMEM",
  set_limit_mem, (void*)XtOffsetOf(core_dir_config, limit_mem),
  OR_ALL, TAKE12, "Soft/hard limits for max memory usage per process" },
{ "RLimitNPROC",
  set_limit_nproc, (void*)XtOffsetOf(core_dir_config, limit_nproc),
   OR_ALL, TAKE12, "soft/hard limits for max number of processes per uid" },
{ "RLimitNOFILE",
  set_limit_nofile, (void*)XtOffsetOf(core_dir_config, limit_nofile),
   OR_ALL, TAKE12, "soft/hard limits for max number of files per process" },
{ "BindAddress", set_bind_address, NULL, RSRC_CONF, TAKE1,
  "'*', a numeric IP address, or the name of a host with a unique IP address"},
{ "Listen", set_listener, NULL, RSRC_CONF, TAKE12,
  "A port number or a numeric IP address and a port number"},
{ "SendBufferSize", set_send_buffer_size, NULL, RSRC_CONF, TAKE1,
  "Send buffer size in bytes"},
{ "AddModule", add_module_command, NULL, RSRC_CONF, ITERATE,
  "The name of a module" },
{ "ClearModuleList", clear_module_list_command, NULL, RSRC_CONF, NO_ARGS, 
  NULL },
{ "ThreadsPerChild", set_threads, NULL, RSRC_CONF, TAKE1,
  "Number of threads a child creates" },
{ "ExcessRequestsPerChild", set_excess_requests, NULL, RSRC_CONF, TAKE1,
  "Maximum number of requests a particular child serves after it is ready "
  "to die." },
{ "ListenBacklog", set_listenbacklog, NULL, RSRC_CONF, TAKE1,
  "Maximum length of the queue of pending connections, as used by listen(2)" },
{ "AcceptFilter", set_acceptfilter, NULL, RSRC_CONF, FLAG,
  "Switch AcceptFiltering on/off (default is "
	"on"
	")."
	"This feature is currently not compiled in; so this directive "
	"is ignored."
   },
{ "CoreDumpDirectory", set_coredumpdir, NULL, RSRC_CONF, TAKE1,
  "The location of the directory Apache changes to before dumping core" },
{ "Include", include_config, NULL, (RSRC_CONF | ACCESS_CONF), TAKE1,
  "Name of the config file to be included" },
{ "LogLevel", set_loglevel, NULL, RSRC_CONF, TAKE1,
  "Level of verbosity in error logging" },
{ "NameVirtualHost", ap_set_name_virtual_host, NULL, RSRC_CONF, TAKE12,
  "A numeric IP address:port, or the name of a host" },
{ "CGICommandArgs", set_cgi_command_args, NULL, OR_OPTIONS, FLAG,
  "Allow or Disallow CGI requests to pass args on the command line" },
{ "ServerTokens", set_serv_tokens, NULL, RSRC_CONF, TAKE1,
  "Tokens displayed in the Server: header - Min[imal], OS, Prod[uctOnly], Full" },
{ "LimitRequestLine", set_limit_req_line, NULL, RSRC_CONF, TAKE1,
  "Limit on maximum size of an HTTP request line"},
{ "LimitRequestFieldsize", set_limit_req_fieldsize, NULL, RSRC_CONF, TAKE1,
  "Limit on maximum size of an HTTP request header field"},
{ "LimitRequestFields", set_limit_req_fields, NULL, RSRC_CONF, TAKE1,
  "Limit (0 = unlimited) on max number of header fields in a request message"},
{ "LimitRequestBody", set_limit_req_body,
  (void*)XtOffsetOf(core_dir_config, limit_req_body),
  OR_ALL, TAKE1,
  "Limit (in bytes) on maximum size of request message body" },
{ "ProtocolReqCheck", set_protocol_req_check, NULL, RSRC_CONF, FLAG,
  "Enable strict checking of Protocol type in requests" },
{ "ShmemUIDisUser", set_change_shmem_uid, NULL, RSRC_CONF, FLAG,
  "Enable the setting of SysV shared memory scoreboard uid/gid to User/Group" },
{ "AcceptMutex", set_accept_mutex, NULL, RSRC_CONF, TAKE1,
  "Serialized Accept Mutex; the methods " 
    "'sysvsem' "
    "'flock' "
    "are compiled in"
},

{ "FileETag", set_etag_bits, NULL, OR_FILEINFO, RAW_ARGS,
  "Specify components used to construct a file's ETag"},

{ "LimitInternalRecursion", set_recursion_limit, NULL, RSRC_CONF, TAKE12,
  "maximum recursion depth of internal redirects and subrequests"},

{ NULL }
};

/*****************************************************************
 *
 * Core handlers for various phases of server operation...
 */

static int core_translate(request_rec *r)
{
    void *sconf = r->server->module_config;
    core_server_config *conf = ap_get_module_config(sconf, &core_module);
  
    if (r->proxyreq != NOT_PROXY) {
        return HTTP_FORBIDDEN;
    }
    if ((r->uri[0] != '/') && strcmp(r->uri, "*")) {
	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
		     "Invalid URI in request %s", r->the_request);
	return BAD_REQUEST;
    }
    
    if (r->server->path 
	&& !strncmp(r->uri, r->server->path, r->server->pathlen)
	&& (r->server->path[r->server->pathlen - 1] == '/'
	    || r->uri[r->server->pathlen] == '/'
	    || r->uri[r->server->pathlen] == '\0')) {
        r->filename = ap_pstrcat(r->pool, conf->ap_document_root,
				 (r->uri + r->server->pathlen), NULL);
    }
    else {
	/*
         * Make sure that we do not mess up the translation by adding two
         * /'s in a row.  This happens under windows when the document
         * root ends with a /
         */
        if ((conf->ap_document_root[strlen(conf->ap_document_root)-1] == '/')
	    && (*(r->uri) == '/')) {
	    r->filename = ap_pstrcat(r->pool, conf->ap_document_root, r->uri+1,
				     NULL);
	}
	else {
	    r->filename = ap_pstrcat(r->pool, conf->ap_document_root, r->uri,
				     NULL);
	}
    }

    return OK;
}

static int do_nothing(request_rec *r) { return OK; }

struct mmap_rec {
    void *mm;
    size_t length;
};

static void mmap_cleanup(void *mmv)
{
    struct mmap_rec *mmd = mmv;

    if (munmap(mmd->mm, mmd->length) == -1) {
        ap_log_error(APLOG_MARK, APLOG_ERR, NULL,
                     "Failed to munmap memory of length %ld at 0x%lx",
                     (long) mmd->length, (long) mmd->mm);
    }
}

/*
 * Default handler for MIME types without other handlers.  Only GET
 * and OPTIONS at this point... anyone who wants to write a generic
 * handler for PUT or POST is free to do so, but it seems unwise to provide
 * any defaults yet... So, for now, we assume that this will always be
 * the last handler called and return 405 or 501.
 */

static int default_handler(request_rec *r)
{
    core_dir_config *d =
      (core_dir_config *)ap_get_module_config(r->per_dir_config, &core_module);
    int rangestatus, errstatus;
    FILE *f;
    caddr_t mm;

    /* This handler has no use for a request body (yet), but we still
     * need to read and discard it if the client sent one.
     */
    if ((errstatus = ap_discard_request_body(r)) != OK) {
        return errstatus;
    }

    r->allowed |= (1 << M_GET) | (1 << M_OPTIONS);

    if (r->method_number == M_INVALID) {
	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
		    "Invalid method in request %s",
		    ap_escape_logitem(r->pool, r->the_request));
	return NOT_IMPLEMENTED;
    }
    if (r->method_number == M_OPTIONS) {
        return ap_send_http_options(r);
    }
    if (r->method_number == M_PUT) {
        return METHOD_NOT_ALLOWED;
    }

    if (r->finfo.st_mode == 0 || (r->path_info && *r->path_info)) {
	ap_log_rerror(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r,
		      "File does not exist: %s",r->path_info ?
		      ap_pstrcat(r->pool, r->filename, r->path_info, NULL)
		      : r->filename);
	return HTTP_NOT_FOUND;
    }
    if (r->method_number != M_GET) {
        return METHOD_NOT_ALLOWED;
    }
	
    f = ap_pfopen(r->pool, r->filename, "r");

    if (f == NULL) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
		     "file permissions deny server access: %s", r->filename);
        return FORBIDDEN;
    }
	
    ap_update_mtime(r, r->finfo.st_mtime);
    ap_set_last_modified(r);
    ap_set_etag(r);
    ap_table_setn(r->headers_out, "Accept-Ranges", "bytes");
    if (((errstatus = ap_meets_conditions(r)) != OK)
	|| (errstatus = ap_set_content_length(r, r->finfo.st_size))) {
        return errstatus;
    }

    ap_block_alarms();
    if ((r->finfo.st_size >= MMAP_THRESHOLD)
	&& (r->finfo.st_size < MMAP_LIMIT)
	&& (!r->header_only || (d->content_md5 & 1))) {
	/* we need to protect ourselves in case we die while we've got the
 	 * file mmapped */
	mm = mmap(NULL, r->finfo.st_size, PROT_READ, MAP_PRIVATE,
		  fileno(f), 0);
	if (mm == (caddr_t)-1) {
	    ap_log_rerror(APLOG_MARK, APLOG_CRIT, r,
			 "default_handler: mmap failed: %s", r->filename);
	}
    }
    else {
	mm = (caddr_t)-1;
    }

    if (mm == (caddr_t)-1) {
	ap_unblock_alarms();

	if (d->content_md5 & 1) {
	    ap_table_setn(r->headers_out, "Content-MD5",
			  ap_md5digest(r->pool, f));
	}

	rangestatus = ap_set_byterange(r);

	ap_send_http_header(r);
	
	if (!r->header_only) {
	    if (!rangestatus) {
		ap_send_fd(f, r);
	    }
	    else {
		long offset, length;
		while (ap_each_byterange(r, &offset, &length)) {
		    /*
		     * Non zero returns are more portable than checking
		     * for a return of -1.
		     */
		    if (fseek(f, offset, SEEK_SET)) {
			ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			      "Failed to fseek for byterange (%ld, %ld): %s",
			      offset, length, r->filename);
		    }
		    else {
			ap_send_fd_length(f, r, length);
		    }
		}
	    }
	}

    }
    else {
	struct mmap_rec *mmd;

	mmd = ap_palloc(r->pool, sizeof(*mmd));
	mmd->mm = mm;
	mmd->length = r->finfo.st_size;
	ap_register_cleanup(r->pool, (void *)mmd, mmap_cleanup, mmap_cleanup);
	ap_unblock_alarms();

	if (d->content_md5 & 1) {
	    AP_MD5_CTX context;
	    
	    ap_MD5Init(&context);
	    ap_MD5Update(&context, (void *)mm, (unsigned int)r->finfo.st_size);
	    ap_table_setn(r->headers_out, "Content-MD5",
			  ap_md5contextTo64(r->pool, &context));
	}

	rangestatus = ap_set_byterange(r);
	ap_send_http_header(r);
	
	if (!r->header_only) {
	    if (!rangestatus) {
		ap_send_mmap(mm, r, 0, r->finfo.st_size);
	    }
	    else {
		long offset, length;
		while (ap_each_byterange(r, &offset, &length)) {
		    ap_send_mmap(mm, r, offset, length);
		}
	    }
	}
    }

    ap_pfclose(r->pool, f);
    return OK;
}

static const handler_rec core_handlers[] = {
{ "*/*", default_handler },
{ "default-handler", default_handler },
{ NULL, NULL }
};

API_VAR_EXPORT module core_module = {
    STANDARD_MODULE_STUFF,
    NULL,			/* initializer */
    create_core_dir_config,	/* create per-directory config structure */
    merge_core_dir_configs,	/* merge per-directory config structures */
    create_core_server_config,	/* create per-server config structure */
    merge_core_server_configs,	/* merge per-server config structures */
    core_cmds,			/* command table */
    core_handlers,		/* handlers */
    core_translate,		/* translate_handler */
    NULL,			/* check_user_id */
    NULL,			/* check auth */
    do_nothing,			/* check access */
    do_nothing,			/* type_checker */
    NULL,			/* pre-run fixups */
    NULL,			/* logger */
    NULL,			/* header parser */
    NULL,			/* child_init */
    NULL,			/* child_exit */
    NULL			/* post_read_request */
};
