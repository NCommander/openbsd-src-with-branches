/*	$OpenBSD: hoststated.h,v 1.31 2007/02/25 14:57:09 reyk Exp $	*/

/*
 * Copyright (c) 2006, 2007 Pierre-Yves Ritschard <pyr@spootnik.org>
 * Copyright (c) 2006, 2007 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#include <sys/tree.h>

#define CONF_FILE		"/etc/hoststated.conf"
#define HOSTSTATED_SOCKET	"/var/run/hoststated.sock"
#define PF_SOCKET		"/dev/pf"
#define HOSTSTATED_USER		"_hoststated"
#define HOSTSTATED_ANCHOR	"hoststated"
#define CHECK_TIMEOUT		200
#define CHECK_INTERVAL		10
#define EMPTY_TABLE		UINT_MAX
#define EMPTY_ID		UINT_MAX
#define TABLE_NAME_SIZE		32
#define	TAG_NAME_SIZE		64
#define SRV_NAME_SIZE		64
#define MAX_NAME_SIZE		64
#define SRV_MAX_VIRTS		16

#define RELAY_MAX_SESSIONS	1024
#define RELAY_TIMEOUT		600
#define RELAY_CACHESIZE		-1	/* use default size */
#define RELAY_NUMPROC		5
#define RELAY_MAXPROC		32
#define RELAY_MAXHOSTS		32
#define RELAY_STATINTERVAL	60
#define RELAY_BACKLOG		10

#define SMALL_READ_BUF_SIZE	1024
#define READ_BUF_SIZE		65535
#define ICMP_BUF_SIZE		64

/* buffer */
struct buf {
	TAILQ_ENTRY(buf)	 entry;
	u_char			*buf;
	size_t			 size;
	size_t			 max;
	size_t			 wpos;
	size_t			 rpos;
};

struct msgbuf {
	TAILQ_HEAD(, buf)	 bufs;
	u_int32_t		 queued;
	int			 fd;
};

#define IMSG_HEADER_SIZE	sizeof(struct imsg_hdr)
#define MAX_IMSGSIZE		8192

struct buf_read {
	u_char			 buf[READ_BUF_SIZE];
	u_char			*rptr;
	size_t			 wpos;
};

struct imsgbuf {
	TAILQ_HEAD(, imsg_fd)	 fds;
	struct buf_read		 r;
	struct msgbuf		 w;
	struct event		 ev;
	void			(*handler)(int, short, void *);
	int			 fd;
	pid_t			 pid;
	short			 events;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_OK,		/* answer to hoststatectl requests */
	IMSG_CTL_FAIL,
	IMSG_CTL_END,
	IMSG_CTL_SERVICE,
	IMSG_CTL_TABLE,
	IMSG_CTL_HOST,
	IMSG_CTL_RELAY,
	IMSG_CTL_TABLE_CHANGED,
	IMSG_CTL_PULL_RULESET,
	IMSG_CTL_PUSH_RULESET,
	IMSG_CTL_SHOW_SUM,	/* hoststatectl requests */
	IMSG_CTL_SERVICE_ENABLE,
	IMSG_CTL_SERVICE_DISABLE,
	IMSG_CTL_TABLE_ENABLE,
	IMSG_CTL_TABLE_DISABLE,
	IMSG_CTL_HOST_ENABLE,
	IMSG_CTL_HOST_DISABLE,
	IMSG_CTL_SHUTDOWN,
	IMSG_CTL_RELOAD,
	IMSG_CTL_NOTIFY,
	IMSG_CTL_STATISTICS,
	IMSG_SERVICE_ENABLE,	/* notifies from pfe to hce */
	IMSG_SERVICE_DISABLE,
	IMSG_TABLE_ENABLE,
	IMSG_TABLE_DISABLE,
	IMSG_HOST_ENABLE,
	IMSG_HOST_DISABLE,
	IMSG_HOST_STATUS,	/* notifies from hce to pfe */
	IMSG_SYNC,
	IMSG_NATLOOK,
	IMSG_DEMOTE,
	IMSG_STATISTICS
};

struct imsg_hdr {
	enum imsg_type	 type;
	u_int16_t	 len;
	u_int32_t	 peerid;
	pid_t		 pid;
};

struct imsg {
	struct imsg_hdr	 hdr;
	void		*data;
};

typedef u_int32_t objid_t;

struct ctl_status {
	objid_t		 id;
	int		 up;
	int		 retry_cnt;
	u_long		 check_cnt;
};

struct ctl_id {
	objid_t		 id;
	char		 name[MAX_NAME_SIZE];
};

struct ctl_demote {
	char		 group[IFNAMSIZ];
	int		 level;
};

struct ctl_icmp_event {
	struct hoststated	*env;
	int			 s;
	int			 af;
	int			 last_up;
	struct event		 ev;
	struct timeval		 tv_start;
};

struct ctl_tcp_event {
	int			 s;
	char			*req;
	struct buf		*buf;
	struct host		*host;
	struct table		*table;
	struct timeval		 tv_start;
	struct event		 ev;
	int			(*validate_read)(struct ctl_tcp_event *);
	int			(*validate_close)(struct ctl_tcp_event *);
	SSL			*ssl;
	char			 rbuf[SMALL_READ_BUF_SIZE];
};

enum httpmethod {
	HTTP_METHOD_GET		= 0,
	HTTP_METHOD_HEAD	= 1,
	HTTP_METHOD_POST	= 2,
	HTTP_METHOD_PUT		= 3,
	HTTP_METHOD_DELETE	= 4,
	HTTP_METHOD_OPTIONS	= 5,
	HTTP_METHOD_TRACE	= 6,
	HTTP_METHOD_CONNECT	= 7
};

struct ctl_relay_event {
	int			 s;
	in_port_t		 port;
	struct sockaddr_storage	 ss;
	struct bufferevent	*bev;
	struct evbuffer		*output;
	struct ctl_relay_event	*dst;
	void			*con;
	SSL			*ssl;
	u_int8_t		*nodes;

	int			 marked;
	int			 line;
	size_t			 toread;
	enum httpmethod		 method;

	u_int8_t		*buf;
	int			 buflen;
	u_int8_t		 flags;
};

struct ctl_natlook {
	objid_t			 id;
	struct sockaddr_storage	 src;
	struct sockaddr_storage	 dst;
	struct sockaddr_storage	 rsrc;
	struct sockaddr_storage	 rdst;
	in_port_t		 rsport;
	in_port_t		 rdport;
	int			 in;
	int			 proc;
};

struct ctl_stats {
	objid_t			 id;
	int			 proc;
	u_int			 interval;
	u_long			 cnt;
	u_long			 tick;

	u_long			 avg;
	u_long			 last;

	u_long			 avg_hour;
	u_long			 last_hour;
	u_long			 avg_day;
	u_long			 last_day;
};

struct address {
	struct sockaddr_storage	 ss;
	in_port_t		 port;
	char			 ifname[IFNAMSIZ];
	TAILQ_ENTRY(address)	 entry;
};
TAILQ_HEAD(addresslist, address);

#define F_DISABLE		0x0001
#define F_BACKUP		0x0002
#define F_USED			0x0004
#define F_DOWN			0x0008
#define F_ADD			0x0010
#define F_DEL			0x0020
#define F_CHANGED		0x0040
#define F_STICKY		0x0080
#define F_CHECK_DONE		0x0100
#define F_ACTIVE_RULESET	0x0200
#define F_CHECK_SENT		0x0400
#define F_SSL			0x0800
#define F_NATLOOK		0x1000
#define F_DEMOTE		0x2000

struct host {
	u_int16_t		 flags;
	objid_t			 id;
	objid_t			 tableid;
	char			*tablename;
	char			 name[MAXHOSTNAMELEN];
	int			 up;
	int			 last_up;
	u_long			 check_cnt;
	u_long			 up_cnt;
	int			 retry_cnt;
	int			 retry;

	struct sockaddr_storage	 ss;
	struct ctl_tcp_event	 cte;
	TAILQ_ENTRY(host)	 entry;
};
TAILQ_HEAD(hostlist, host);

enum host_status {
	HOST_DOWN	= -1,
	HOST_UNKNOWN	= 0,
	HOST_UP		= 1
};
#define HOST_ISUP(x)	(x == HOST_UP)

struct table {
	objid_t			 id;
	objid_t			 serviceid;
	u_int16_t		 flags;
	int			 check;
	int			 up;
	int			 demoted;
	char			 demote_group[IFNAMSIZ];
	in_port_t		 port;
	int			 retcode;
	struct timeval		 timeout;
	char			 name[TABLE_NAME_SIZE];
	char			 path[MAXPATHLEN];
	char			*sendbuf;
	char			 exbuf[64];
	char			 digest[41]; /* length of sha1 digest * 2 */
	SSL_CTX			*ssl_ctx;
	struct hostlist		 hosts;
	TAILQ_ENTRY(table)	 entry;
};
TAILQ_HEAD(tablelist, table);

enum table_check {
	CHECK_NOCHECK		= 0,
	CHECK_ICMP		= 1,
	CHECK_TCP		= 2,
	CHECK_HTTP_CODE		= 3,
	CHECK_HTTP_DIGEST	= 4,
	CHECK_SEND_EXPECT	= 5
};

struct service {
	objid_t			 id;
	u_int16_t		 flags;
	in_port_t		 port;
	char			 name[SRV_NAME_SIZE];
	char			 tag[TAG_NAME_SIZE];
	struct addresslist	 virts;
	struct table		*table;
	struct table		*backup; /* use this if no host up */
	TAILQ_ENTRY(service)	 entry;
};
TAILQ_HEAD(servicelist, service);

struct session {
	objid_t				 id;
	struct ctl_relay_event		 in;
	struct ctl_relay_event		 out;
	u_int32_t			 outkey;
	struct event			 ev;
	struct timeval			 timeout;
	struct timeval			 tv_start;
	struct timeval			 tv_last;
	int				 done;
	int				 retry;
	struct evbuffer			*log;
	void				*relay;
	struct ctl_natlook		*cnl;
	TAILQ_ENTRY(session)		 entry;
};
TAILQ_HEAD(sessionlist, session);

enum nodeaction {
	NODE_ACTION_NONE	= 0,
	NODE_ACTION_APPEND	= 1,
	NODE_ACTION_CHANGE	= 2,
	NODE_ACTION_REMOVE	= 3,
	NODE_ACTION_EXPECT	= 4,
	NODE_ACTION_FILTER	= 5,
	NODE_ACTION_HASH	= 6,
	NODE_ACTION_LOG		= 7
};

enum nodetype {
	NODE_TYPE_HEADER	= 0,
	NODE_TYPE_URL		= 1,
	NODE_TYPE_COOKIE	= 2
};

#define PNFLAG_MACRO		0x01
#define PNFLAG_MARK		0x02
#define PNFLAG_LOG		0x04
#define PNFLAG_LOOKUP_URL	0x08
#define PNFLAG_LOOKUP_COOKIE	0x10

struct protonode {
	objid_t			 id;
	char			*key;
	enum nodeaction		 action;
	char			*value;
	u_int8_t		 flags;
	enum nodetype		 type;

	RB_ENTRY(protonode)	 nodes;
};
RB_HEAD(proto_tree, protonode);

enum prototype {
	RELAY_PROTO_TCP		= 0,
	RELAY_PROTO_HTTP	= 1
};

#define TCPFLAG_NODELAY		0x01
#define TCPFLAG_NNODELAY	0x02
#define TCPFLAG_SACK		0x04
#define TCPFLAG_NSACK		0x08
#define TCPFLAG_BUFSIZ		0x10
#define TCPFLAG_DEFAULT		0x00

#define SSLFLAG_SSLV2		0x01
#define SSLFLAG_SSLV3		0x02
#define SSLFLAG_TLSV1		0x04
#define SSLFLAG_VERSION		0x07
#define SSLFLAG_DEFAULT		(SSLFLAG_SSLV3|SSLFLAG_TLSV1)

#define SSLCIPHERS_DEFAULT	"HIGH:!ADH"

struct protocol {
	objid_t			 id;
	u_int16_t		 flags;
	u_int8_t		 tcpflags;
	int			 tcpbufsiz;
	int			 tcpbacklog;
	u_int8_t		 sslflags;
	char			*sslciphers;
	char			 name[MAX_NAME_SIZE];
	int			 cache;
	enum prototype		 type;
	int			 lateconnect;

	int			 nodecount;
	struct proto_tree	 tree;
	TAILQ_ENTRY(protocol)	 entry;
};
TAILQ_HEAD(protolist, protocol);

struct relay {
	objid_t			 id;
	u_int16_t		 flags;
	int			 up;
	struct protocol		*proto;
	char			 name[MAXHOSTNAMELEN];
	int			 s;
	in_port_t		 port;
	struct sockaddr_storage	 ss;
	struct bufferevent	*bev;

	int			 dsts;
	in_port_t		 dstport;
	struct sockaddr_storage	 dstss;
	struct bufferevent	*dstbev;

	struct table		*dsttable;
	u_int32_t		 dstkey;
	struct host		*dsthost[RELAY_MAXHOSTS];
	int			 dstnhosts;
	int			 dstmode;
	int			 dstcheck;

	struct event		 ev;
	struct timeval		 timeout;
	SSL_CTX			*ctx;

	struct ctl_stats	 stats[RELAY_MAXPROC + 1];

	struct sessionlist	 sessions;
	TAILQ_ENTRY(relay)	 entry;
};
TAILQ_HEAD(relaylist, relay);

enum dstmode {
	RELAY_DSTMODE_LOADBALANCE	= 0,
	RELAY_DSTMODE_ROUNDROBIN	= 1,
	RELAY_DSTMODE_HASH		= 2
};
#define RELAY_DSTMODE_DEFAULT		RELAY_DSTMODE_LOADBALANCE

enum {
	PROC_MAIN,
	PROC_PFE,
	PROC_HCE,
	PROC_RELAY
} hoststated_process;

struct hoststated {
	u_int8_t		 opts;
	u_int16_t		 flags;
	struct pfdata		*pf;
	int			 tablecount;
	int			 servicecount;
	int			 protocount;
	int			 relaycount;
	struct timeval		 interval;
	struct timeval		 timeout;
	struct table		 empty_table;
	struct protocol		 proto_default;
	struct event		 ev;
	struct tablelist	 tables;
	struct servicelist	 services;
	struct protolist	 protos;
	struct relaylist	 relays;
	u_int16_t		 prefork_relay;
	char			 demote_group[IFNAMSIZ];
	u_int16_t		 id;

	struct event		 statev;
	struct timeval		 statinterval;

	int			 has_icmp;
	int			 has_icmp6;
	struct ctl_icmp_event	 icmp_send;
	struct ctl_icmp_event	 icmp_recv;
	struct ctl_icmp_event	 icmp6_send;
	struct ctl_icmp_event	 icmp6_recv;
};

#define HOSTSTATED_OPT_VERBOSE		0x01
#define HOSTSTATED_OPT_NOACTION		0x04
#define HOSTSTATED_OPT_LOGUPDATE	0x08
#define HOSTSTATED_OPT_LOGNOTIFY	0x10
#define HOSTSTATED_OPT_LOGALL		0x18

/* initially control.h */
struct {
	struct event	 ev;
	int		 fd;
} control_state;

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

struct ctl_conn {
	TAILQ_ENTRY(ctl_conn)	 entry;
	u_int8_t		 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct imsgbuf		 ibuf;

};
TAILQ_HEAD(ctl_connlist, ctl_conn);

/* control.c */
int	control_init(void);
int	control_listen(void);
void    control_accept(int, short, void *);
void    control_dispatch_imsg(int, short, void *);
void	control_imsg_forward(struct imsg *);
void    control_cleanup(void);

void    session_socket_blockmode(int, enum blockmodes);

extern  struct ctl_connlist ctl_conns;

/* parse.y */
int	parse_config(struct hoststated *, const char *, int);
int	cmdline_symset(char *);

/* log.c */
void	log_init(int);
void	log_warn(const char *, ...);
void	log_warnx(const char *, ...);
void	log_info(const char *, ...);
void	log_debug(const char *, ...);
void	fatal(const char *);
void	fatalx(const char *);
const char *host_status(enum host_status);
const char *table_check(enum table_check);
const char *print_availability(u_long, u_long);

/* buffer.c */
struct buf	*buf_open(size_t);
struct buf	*buf_dynamic(size_t, size_t);
int		 buf_add(struct buf *, void *, size_t);
void		*buf_reserve(struct buf *, size_t);
int		 buf_close(struct msgbuf *, struct buf *);
void		 buf_free(struct buf *);
void		 msgbuf_init(struct msgbuf *);
void		 msgbuf_clear(struct msgbuf *);
int		 msgbuf_write(struct msgbuf *);

/* imsg.c */
void	 imsg_init(struct imsgbuf *, int, void (*)(int, short, void *));
ssize_t	 imsg_read(struct imsgbuf *);
ssize_t	 imsg_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    void *, u_int16_t);
struct buf *imsg_create(struct imsgbuf *, enum imsg_type, u_int32_t, pid_t,
	    u_int16_t);
int	 imsg_add(struct buf *, void *, u_int16_t);
int	 imsg_close(struct imsgbuf *, struct buf *);
void	 imsg_free(struct imsg *);
void	 imsg_event_add(struct imsgbuf *); /* needs to be provided externally */

/* pfe.c */
pid_t	 pfe(struct hoststated *, int [2], int [2], int [2], int [2],
	    int [RELAY_MAXPROC][2]);
void	 show(struct ctl_conn *);
int	 enable_service(struct ctl_conn *, struct ctl_id *);
int	 enable_table(struct ctl_conn *, struct ctl_id *);
int	 enable_host(struct ctl_conn *, struct ctl_id *);
int	 disable_service(struct ctl_conn *, struct ctl_id *);
int	 disable_table(struct ctl_conn *, struct ctl_id *);
int	 disable_host(struct ctl_conn *, struct ctl_id *);

/* pfe_filter.c */
void	 init_filter(struct hoststated *);
void	 init_tables(struct hoststated *);
void	 flush_table(struct hoststated *, struct service *);
void	 sync_table(struct hoststated *, struct service *, struct table *);
void	 sync_ruleset(struct hoststated *, struct service *, int);
void	 flush_rulesets(struct hoststated *);
int	 natlook(struct hoststated *, struct ctl_natlook *);

/* hce.c */
pid_t	 hce(struct hoststated *, int [2], int [2], int [2], int [2],
	    int [RELAY_MAXPROC][2]);
void	 hce_notify_done(struct host *, const char *);

/* relay.c */
pid_t	 relay(struct hoststated *, int [2], int [2], int [2], int [2],
	    int [RELAY_MAXPROC][2]);
void	 relay_notify_done(struct host *, const char *);

RB_PROTOTYPE(proto_tree, protonode, nodes, relay_proto_cmp);

/* check_icmp.c */
void	 icmp_init(struct hoststated *);
void	 schedule_icmp(struct hoststated *, struct host *);
void	 check_icmp(struct hoststated *, struct timeval *);

/* check_tcp.c */
void	 check_tcp(struct ctl_tcp_event *);

/* ssl.c */
void	 ssl_init(struct hoststated *);
void	 ssl_transaction(struct ctl_tcp_event *);
SSL_CTX	*ssl_ctx_create(struct hoststated *);
void	 ssl_error(const char *, const char *);

/* hoststated.c */
struct host	*host_find(struct hoststated *, objid_t);
struct table	*table_find(struct hoststated *, objid_t);
struct service	*service_find(struct hoststated *, objid_t);
struct host	*host_findbyname(struct hoststated *, const char *);
struct table	*table_findbyname(struct hoststated *, const char *);
struct service	*service_findbyname(struct hoststated *, const char *);
void		 event_again(struct event *, int, short,
		    void (*)(int, short, void *),
		    struct timeval *, struct timeval *, void *);
struct relay	*relay_find(struct hoststated *, objid_t);
struct session	*session_find(struct hoststated *, objid_t);
struct relay	*relay_findbyname(struct hoststated *, const char *);
int		 expand_string(char *, size_t, const char *, const char *);

/* carp.c */
int	 carp_demote_init(char *, int);
void	 carp_demote_shutdown(void);
int	 carp_demote_get(char *);
int	 carp_demote_set(char *, int);
int	 carp_demote_reset(char *, int);
