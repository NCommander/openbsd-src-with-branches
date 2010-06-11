/*	$OpenBSD: ldapd.h,v 1.2 2010/06/03 17:29:54 martinh Exp $ */

/*
 * Copyright (c) 2009, 2010 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LDAPD_H
#define _LDAPD_H

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>

#include "aldap.h"
#include "btree.h"

#define CONFFILE		 "/etc/ldapd.conf"
#define LDAPD_USER		 "_ldapd"
#define LDAPD_SOCKET		 "/var/run/ldapd.sock"
#define DATADIR			 "/var/db/ldap"
#define LDAP_PORT		 389
#define LDAPS_PORT		 636
#define LDAPD_SESSION_TIMEOUT	 30
#define MAX_LISTEN		 64

#define F_STARTTLS		 0x01
#define F_LDAPS			 0x02
#define F_SSL			(F_LDAPS|F_STARTTLS)

#define F_SECURE		 0x04

#define F_SCERT			 0x01

struct conn;

struct aci {
	SIMPLEQ_ENTRY(aci)	 entry;
#define ACI_DENY		 0
#define ACI_ALLOW		 1
	int			 type;
#define ACI_READ		 0x01
#define ACI_WRITE		 0x02
#define ACI_COMPARE		 0x04
#define ACI_CREATE		 0x08
#define ACI_BIND		 0x10
#define ACI_ALL			 0x1F
	int			 rights;
	enum scope		 scope;		/* base, onelevel or subtree */
	char			*attribute;
	char			*target;
	char			*subject;
	char			*filter;
};
SIMPLEQ_HEAD(acl, aci);

/* An LDAP request.
 */
struct request {
	TAILQ_ENTRY(request)	 next;
	unsigned long		 type;
	long long		 msgid;
	struct ber_element	*root;
	struct ber_element	*op;
	struct conn		*conn;
	int			 replayed;	/* true if replayed request */
};
TAILQ_HEAD(request_queue, request);

enum index_type {
	INDEX_NONE,
	INDEX_EQUAL	= 1,
	INDEX_APPROX	= 1,
	INDEX_SUBSTR,
	INDEX_PRESENCE
};

struct attr_index {
	TAILQ_ENTRY(attr_index)	 next;
	char			*attr;
	enum index_type		 type;
};
TAILQ_HEAD(attr_index_list, attr_index);

struct namespace {
	TAILQ_ENTRY(namespace)	 next;
	char			*suffix;
	char			*rootdn;
	char			*rootpw;
	char			*data_path;
	char			*indx_path;
	struct btree		*data_db;
	struct btree		*indx_db;
	struct btree_txn	*data_txn;
	struct btree_txn	*indx_txn;
	int			 sync;		/* 1 = fsync after commit */
	struct attr_index_list	 indices;
	unsigned int		 cache_size;
	unsigned int		 index_cache_size;
	int			 compacting;	/* true if being compacted */
	int			 indexing;	/* true if being indexed */
	struct request_queue	 request_queue;
	struct event		 ev_queue;
	unsigned int		 queued_requests;
	struct acl		 acl;
	int			 relax;		/* relax schema validation */
	int			 compression_level;	/* 0-9, 0 = disabled */
};

TAILQ_HEAD(namespace_list, namespace);

struct index
{
	TAILQ_ENTRY(index)	 next;
	char			*prefix;
};

/* A query plan.
 */
struct plan
{
	TAILQ_ENTRY(plan)	 next;
	TAILQ_HEAD(, plan)	 args;
	TAILQ_HEAD(, index)	 indices;
	int			 indexed;
};

/* For OR filters using multiple indices, matches are not unique. Remember
 * all DNs sent to the client to make them unique.
 */
struct uniqdn {
	RB_ENTRY(uniqdn)	 link;
	struct btval		 key;
};
RB_HEAD(dn_tree, uniqdn);
RB_PROTOTYPE(dn_tree, uniqdn, link, uniqdn_cmp);

/* An LDAP search request.
 */
struct search {
	TAILQ_ENTRY(search)	 next;
	int			 init;		/* 1 if cursor initiated */
	struct conn		*conn;
	struct request		*req;
	struct namespace	*ns;
	struct btree_txn	*data_txn;
	struct btree_txn	*indx_txn;
	struct cursor		*cursor;
	unsigned int		 nscanned, nmatched, ndups;
	time_t			 started_at;
	long long		 szlim, tmlim;	/* size and time limits */
	int			 typesonly;	/* not implemented */
	long long		 scope;
	long long		 deref;		/* not implemented */
	char			*basedn;
	struct ber_element	*filter, *attrlist;
	struct plan		*plan;
	struct index		*cindx;		/* current index */
	struct dn_tree		 uniqdns;
};

struct listener {
	unsigned int		 flags;		/* F_STARTTLS or F_LDAPS */
	struct sockaddr_storage	 ss;
	int			 port;
	int			 fd;
	struct event		 ev;
	char			 ssl_cert_name[PATH_MAX];
	struct ssl		*ssl;
	void			*ssl_ctx;
	TAILQ_ENTRY(listener)	 entry;
};
TAILQ_HEAD(listenerlist, listener);

/* An LDAP client connection.
 */
struct conn
{
	TAILQ_ENTRY(conn)	 next;
	int			 fd;
	struct bufferevent	*bev;
	struct ber		 ber;
	int			 disconnect;
	struct request		*bind_req;	/* ongoing bind request */
	char			*binddn;
	TAILQ_HEAD(, search)	 searches;

	/* SSL support */
	struct event		 s_ev;
	struct timeval		 s_tv;
	struct listener		*s_l;
	void			*s_ssl;
	unsigned char		*s_buf;
	int			 s_buflen;
	unsigned int		 s_flags;
};
TAILQ_HEAD(conn_list, conn)	 conn_list;

enum usage {
	USAGE_USER_APP,
	USAGE_DIR_OP,		/* operational attribute */
	USAGE_DIST_OP,		/* operational attribute */
	USAGE_DSA_OP		/* operational attribute */
};

struct name {
	SLIST_ENTRY(name)	 next;
	const char		*name;
};
SLIST_HEAD(name_list, name);

struct attr_type {
	RB_ENTRY(attr_type)	 link;
	const char		*oid;
	struct name_list	*names;
	char			*desc;
	int			 obsolete;
	struct attr_type	*sup;
	char			*equality;
	char			*ordering;
	char			*substr;
	char			*syntax;
	int			 single;
	int			 collective;
	int			 immutable;	/* no-user-modification */
	enum usage		 usage;
};
RB_HEAD(attr_type_tree, attr_type);
RB_PROTOTYPE(attr_type_tree, attr_type, link, attr_oid_cmp);

struct attr_ptr {
	SLIST_ENTRY(attr_ptr)	 next;
	struct attr_type	*attr_type;
};
SLIST_HEAD(attr_list, attr_ptr);

enum object_kind {
	KIND_ABSTRACT,
	KIND_STRUCTURAL,
	KIND_AUXILIARY
};

struct object;
struct obj_ptr {
	SLIST_ENTRY(obj_ptr)	 next;
	struct object		*object;
};
SLIST_HEAD(obj_list, obj_ptr);

struct object {
	RB_ENTRY(object)	 link;
	const char		*oid;
	struct name_list	*names;
	char			*desc;
	int			 obsolete;
	struct obj_list		*sup;
	enum object_kind	 kind;
	struct attr_list	*must;
	struct attr_list	*may;
};
RB_HEAD(object_tree, object);
RB_PROTOTYPE(object_tree, object, link, obj_oid_cmp);

struct oidname {
	RB_ENTRY(oidname)	 link;
	const char		*on_name;
#define	on_attr_type		 on_ptr.ou_attr_type
#define	on_object		 on_ptr.ou_object
	union	{
		struct attr_type	*ou_attr_type;
		struct object		*ou_object;
	} on_ptr;
};
RB_HEAD(oidname_tree, oidname);
RB_PROTOTYPE(oidname_tree, oidname, link, oidname_cmp);

struct ssl {
	SPLAY_ENTRY(ssl)	 ssl_nodes;
	char			 ssl_name[PATH_MAX];
	char			*ssl_cert;
	off_t			 ssl_cert_len;
	char			*ssl_key;
	off_t			 ssl_key_len;
	uint8_t			 flags;
};

struct ldapd_config
{
	struct namespace_list		 namespaces;
	struct attr_type_tree		 attr_types;
	struct oidname_tree		 attr_names;
	struct object_tree		 objects;
	struct oidname_tree		 object_names;
	struct listenerlist		 listeners;
	SPLAY_HEAD(ssltree, ssl)	*sc_ssl;
	struct acl			 acl;
};

struct ldapd_stats
{
	time_t			 started_at;	/* time of daemon startup */
	unsigned long long	 requests;	/* total number of requests */
	unsigned long long	 req_search;	/* search requests */
	unsigned long long	 req_bind;	/* bind requests */
	unsigned long long	 req_mod;	/* add/mod/del requests */
	unsigned long long	 timeouts;	/* search timeouts */
	unsigned long long	 unindexed;	/* unindexed searches */
	unsigned int		 conns;		/* active connections */
	unsigned int		 searches;	/* active searches */
};

struct auth_req
{
	int			 fd;
	long long		 msgid;
	char			 name[128];
	char			 password[128];
};

struct auth_res
{
	int			 ok;
	int			 fd;
	long long		 msgid;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,
	IMSG_CTL_FAIL,
	IMSG_CTL_END,
	IMSG_CTL_STATS,
	IMSG_CTL_NSSTATS,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_CTL_COMPACT,
	IMSG_CTL_COMPACT_STATUS,
	IMSG_CTL_INDEX,
	IMSG_CTL_INDEX_STATUS,

	IMSG_LDAPD_AUTH,
	IMSG_LDAPD_AUTH_RESULT,
};

struct ns_stat {
	char			 suffix[256];
	struct btree_stat	 data_stat;
	struct btree_stat	 indx_stat;
};

enum comp_state {
	COMPACT_DATA,
	COMPACT_INDX,
	COMPACT_DONE
};

struct compaction_status {
	char			 suffix[256];
	int			 db;
	int			 status;
};

struct indexer_status {
	char			 suffix[256];
	uint64_t		 entries;
	uint64_t		 ncomplete;
	int			 status;
};

struct imsgev {
	struct imsgbuf		 ibuf;
	void			(*handler)(int, short, void *);
	struct event		 ev;
	void			*data;
	short			 events;
};

struct ctl_conn;
typedef void (*ctl_close_func)(struct ctl_conn *);

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
#define CTL_CONN_LOCKED		 0x02		/* restricted mode */
	struct imsgev		 iev;

	ctl_close_func		 closecb;

	enum comp_state		 state;
	pid_t			 pid;		/* compaction process */
	struct namespace	*ns;		/* compacted or indexed */
	uint64_t		 ncomplete;	/* completed entries */
	int			 all;		/* 1: traverse all namespaces */
	struct cursor		*cursor;
	struct event		 ev;
};
TAILQ_HEAD(ctl_connlist, ctl_conn);
extern  struct ctl_connlist ctl_conns;


struct control_sock {
	const char		*cs_name;
	struct event		 cs_ev;
	int			 cs_fd;
	int			 cs_restricted;
};

/* ldapd.c */
extern struct ldapd_stats	 stats;
extern struct ldapd_config	*conf;

void			 fd_nonblock(int fd);
void			 imsg_event_add(struct imsgev *iev);
int			 imsg_compose_event(struct imsgev *iev, u_int16_t type,
			    u_int32_t peerid, pid_t pid, int fd, void *data,
			    u_int16_t datalen);
int			 imsg_event_handle(struct imsgev *iev, short event);

/* compact.c */
int			 run_compaction(struct ctl_conn *c,
				struct namespace *ns);
void			 check_compaction(pid_t pid, int status);

/* conn.c */
extern struct conn_list	 conn_list;
struct conn		*conn_by_fd(int fd);
void			 conn_read(struct bufferevent *bev, void *data);
void			 conn_write(struct bufferevent *bev, void *data);
void			 conn_err(struct bufferevent *bev, short w, void *data);
void			 conn_accept(int fd, short why, void *data);
void			 conn_close(struct conn *conn);
void			 conn_disconnect(struct conn *conn);
int			 request_dispatch(struct request *req);
void			 request_free(struct request *req);

/* ldape.c */
pid_t			 ldape(struct passwd *pw, char *csockpath,
				int pipe_parent2ldap[2]);
int			 ldap_abandon(struct request *req);
int			 ldap_unbind(struct request *req);
int			 ldap_extended(struct request *req);

void			 send_ldap_result(struct conn *conn, int msgid,
				unsigned long type, long long result_code);
int			 ldap_respond(struct request *req, int code);

/* namespace.c
 */
struct namespace	*namespace_new(const char *suffix);
int			 namespace_open(struct namespace *ns);
int			 namespace_reopen_data(struct namespace *ns);
int			 namespace_reopen_indx(struct namespace *ns);
struct namespace	*namespace_init(const char *suffix, const char *dir);
void			 namespace_close(struct namespace *ns);
void			 namespace_remove(struct namespace *ns);
struct ber_element	*namespace_get(struct namespace *ns, char *dn);
int			 namespace_exists(struct namespace *ns, char *dn);
int			 namespace_add(struct namespace *ns, char *dn,
				struct ber_element *root);
int			 namespace_update(struct namespace *ns, char *dn,
				struct ber_element *root);
int			 namespace_del(struct namespace *ns, char *dn);
struct namespace	*namespace_for_base(const char *basedn);
int			 namespace_has_index(struct namespace *ns,
				const char *attr, enum index_type type);
int			 namespace_begin(struct namespace *ns);
int			 namespace_commit(struct namespace *ns);
void			 namespace_abort(struct namespace *ns);
int			 namespace_queue_request(struct namespace *ns,
				struct request *req);
void			 namespace_queue_schedule(struct namespace *ns);
int			 namespace_should_queue(struct namespace *ns,
				struct request *req);
void			 namespace_cancel_conn(struct conn *conn);

int			 namespace_ber2db(struct namespace *ns,
				struct ber_element *root, struct btval *val);
struct ber_element	*namespace_db2ber(struct namespace *ns,
				struct btval *val);

/* attributes.c */
struct ber_element	*ldap_get_attribute(struct ber_element *root,
				const char *attr);
struct ber_element	*ldap_find_attribute(struct ber_element *entry,
				struct attr_type *at);
struct ber_element	*ldap_find_value(struct ber_element *elm,
				const char *value);
struct ber_element	*ldap_add_attribute(struct ber_element *root,
				const char *attr, struct ber_element *vals);
int			 ldap_set_values(struct ber_element *elm,
				struct ber_element *vals);
int			 ldap_merge_values(struct ber_element *elm,
				struct ber_element *vals);
int			 ldap_del_attribute(struct ber_element *entry,
				const char *attrdesc);
int			 ldap_del_values(struct ber_element *elm,
				struct ber_element *vals);
char			*ldap_strftime(time_t tm);
char			*ldap_now(void);

/* control.c */
void			 control_init(struct control_sock *);
struct ctl_conn		*control_connbypid(pid_t);
void			 control_listen(struct control_sock *);
void			 control_accept(int, short, void *);
void			 control_dispatch_imsg(int, short, void *);
void			 control_imsg_forward(struct imsg *);
void			 control_cleanup(struct control_sock *);
void			 control_end(struct ctl_conn *c);
void			 control_report_compaction(struct ctl_conn *c,
				int status);
void			 control_report_indexer( struct ctl_conn *c,
				int status);

/* filter.c */
int			 ldap_matches_filter(struct ber_element *root,
				struct ber_element *filter);

/* search.c */
int			 ldap_search(struct request *req);
void			 conn_search(struct search *search);
void			 search_close(struct search *search);
int			 is_child_of(struct btval *key, const char *base);

/* modify.c */
int			 ldap_add(struct request *req);
int			 ldap_delete(struct request *req);
int			 ldap_modify(struct request *req);

/* auth.c */
extern struct imsgev	*iev_ldapd;
int			 ldap_bind(struct request *req);
void			 ldap_bind_continue(struct conn *conn, int ok);
int			 authorized(struct conn *conn, struct namespace *ns,
				int rights, char *dn, int scope);

/* parse.y */
int			 parse_config(char *filename);
struct attr_type	*lookup_attribute_by_oid(const char *oid);
struct attr_type	*lookup_attribute_by_name(const char *name);
struct attr_type	*lookup_attribute(const char *oid_or_name);
struct object		*lookup_object_by_oid(const char *oid);
struct object		*lookup_object_by_name(const char *name);
struct object		*lookup_object(const char *oid_or_name);
int			 cmdline_symset(char *s);

/* log.c */
void			 log_init(int);
void			 log_verbose(int v);
void			 vlog(int, const char *, va_list);
void			 logit(int pri, const char *fmt, ...);
void			 log_warn(const char *, ...);
void			 log_warnx(const char *, ...);
void			 log_info(const char *, ...);
void			 log_debug(const char *, ...);
__dead void		 fatal(const char *);
__dead void		 fatalx(const char *);
const char		*print_host(struct sockaddr_storage *ss, char *buf,
				size_t len);

/* util.c */
int			 bsnprintf(char *str, size_t size,
				const char *format, ...);
int			 has_suffix(struct btval *key, const char *suffix);
int			 has_prefix(struct btval *key, const char *prefix);
void			 normalize_dn(char *dn);

/* index.c */
int			 index_namespace(struct namespace *ns);
int			 index_entry(struct namespace *ns, struct btval *dn,
				struct ber_element *elm);
int			 unindex_entry(struct namespace *ns, struct btval *dn,
				struct ber_element *elm);
int			 index_attribute(struct namespace *ns, char *attr,
				struct btval *dn, struct ber_element *a);
int			 unindex_attribute(struct namespace *ns, char *attr,
				struct btval *dn, struct ber_element *a);
int			 index_to_dn(struct namespace *ns, struct btval *indx,
				struct btval *dn);
int			 run_indexer(struct ctl_conn *c, struct namespace *ns);

/* ssl.c */
void	 ssl_init(void);
void	 ssl_transaction(struct conn *);

void	 ssl_session_init(struct conn *);
void	 ssl_session_destroy(struct conn *);
int	 ssl_load_certfile(struct ldapd_config *, const char *, u_int8_t);
void	 ssl_setup(struct ldapd_config *, struct listener *);
int	 ssl_cmp(struct ssl *, struct ssl *);
SPLAY_PROTOTYPE(ssltree, ssl, ssl_nodes, ssl_cmp);

/* ssl_privsep.c */
int	 ssl_ctx_use_private_key(void *, char *, off_t);
int	 ssl_ctx_use_certificate_chain(void *, char *, off_t);

/* validate.c */
int	validate_entry(const char *dn, struct ber_element *entry, int relax);

#endif /* _LDAPD_H */

