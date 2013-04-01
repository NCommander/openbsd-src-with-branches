/*	$OpenBSD: asr_private.h,v 1.14 2013/04/01 15:49:54 deraadt Exp $	*/
/*
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#include <stdio.h>

#ifndef ASRNODEBUG
#define DEBUG
#endif

#define QR_MASK		(0x1 << 15)
#define OPCODE_MASK	(0xf << 11)
#define AA_MASK		(0x1 << 10)
#define TC_MASK		(0x1 <<  9)
#define RD_MASK		(0x1 <<  8)
#define RA_MASK		(0x1 <<  7)
#define Z_MASK		(0x7 <<  4)
#define RCODE_MASK	(0xf)

#define OPCODE(v)	((v) & OPCODE_MASK)
#define RCODE(v)	((v) & RCODE_MASK)


struct pack {
	char		*buf;
	size_t		 len;
	size_t		 offset;
	const char	*err;
};

struct unpack {
	const char	*buf;
	size_t		 len;
	size_t		 offset;
	const char	*err;
};

struct header {
	uint16_t	id;
	uint16_t	flags;
	uint16_t	qdcount;
	uint16_t	ancount;
	uint16_t	nscount;
	uint16_t	arcount;
};

struct query {
	char		q_dname[MAXDNAME];
	uint16_t	q_type;
	uint16_t	q_class;
};

struct rr {
	char		rr_dname[MAXDNAME];
	uint16_t	rr_type;
	uint16_t	rr_class;
	uint32_t	rr_ttl;
	union {
		struct {
			char	cname[MAXDNAME];
		} cname;
		struct {
			uint16_t	preference;
			char		exchange[MAXDNAME];
		} mx;
		struct {
			char	nsname[MAXDNAME];
		} ns;
		struct {
			char	ptrname[MAXDNAME];
		} ptr;
		struct {
			char		mname[MAXDNAME];
			char		rname[MAXDNAME];
			uint32_t	serial;
			uint32_t	refresh;
			uint32_t	retry;
			uint32_t	expire;
			uint32_t	minimum;
		} soa;
		struct {
			struct in_addr	addr;
		} in_a;
		struct {
			struct in6_addr	addr6;
		} in_aaaa;
		struct {
			uint16_t	 rdlen;
			const void	*rdata;
		} other;
	} rr;
};


#define ASR_MAXNS	5
#define ASR_MAXDB	3
#define ASR_MAXDOM	10

enum async_type {
	ASR_SEND,
	ASR_SEARCH,
	ASR_GETRRSETBYNAME,
	ASR_GETHOSTBYNAME,
	ASR_GETHOSTBYADDR,
	ASR_GETNETBYNAME,
	ASR_GETNETBYADDR,
	ASR_GETADDRINFO,
	ASR_GETNAMEINFO,
};

enum asr_db_type {
	ASR_DB_FILE,
	ASR_DB_DNS,
	ASR_DB_YP,
};

struct asr_ctx {
	int		 ac_refcount;
	int		 ac_options;
	int		 ac_ndots;
	char		*ac_domain;
	int		 ac_domcount;
	char		*ac_dom[ASR_MAXDOM];
	int		 ac_dbcount;
	int		 ac_db[ASR_MAXDB];
	int		 ac_family[3];

	char		*ac_hostfile;

	int		 ac_nscount;
	int		 ac_nstimeout;
	int		 ac_nsretries;
	struct sockaddr *ac_ns[ASR_MAXNS];

};

struct asr {
	char		*a_path;
	time_t		 a_mtime;
	time_t		 a_rtime;
	struct asr_ctx	*a_ctx;
};


#define	ASYNC_DOM_FQDN		0x00000001
#define	ASYNC_DOM_NDOTS		0x00000002
#define	ASYNC_DOM_HOSTALIAS	0x00000004
#define	ASYNC_DOM_DOMAIN	0x00000008
#define ASYNC_DOM_ASIS		0x00000010

#define	ASYNC_NODATA		0x00000100
#define	ASYNC_AGAIN		0x00000200

#define	ASYNC_EXTIBUF		0x00001000
#define	ASYNC_EXTOBUF		0x00002000


struct async {
	int		(*as_run)(struct async *, struct async_res *);
	struct asr_ctx	*as_ctx;
	int		 as_type;
	int		 as_state;

	/* cond */
	int		 as_timeout;
	int		 as_fd;

	/* loop indices in ctx */
	int		 as_dom_step;
	int		 as_dom_idx;
	int		 as_dom_flags;
	int		 as_family_idx;
	int		 as_db_idx;
	int		 as_ns_idx;
	int		 as_ns_cycles;

	int		 as_count;

	union {
		struct {
			int		 flags;
			uint16_t	 reqid;
			int		 class;
			int		 type;
			char		*dname;		/* not fqdn! */
			int		 rcode;		/* response code */
			int		 ancount;	/* answer count */

			/* io buffers for query/response */
			unsigned char	*obuf;
			size_t		 obuflen;
			size_t		 obufsize;
			unsigned char	*ibuf;
			size_t		 ibuflen;
			size_t		 ibufsize;
			size_t		 datalen; /* for tcp io */
		} dns;

		struct {
			int		 flags;
			int		 class;
			int		 type;
			char		*name;
			struct async	*subq;
			int		 saved_h_errno;
			unsigned char	*ibuf;
			size_t		 ibuflen;
			size_t		 ibufsize;
		} search;

		struct {
			int		 flags;
			int		 class;
			int		 type;
			char		*name;
			struct async	*subq;
		} rrset;

		struct {
			char		*name;
			int		 family;
			struct async	*subq;
			char		 addr[16];
			int		 addrlen;
			int		 subq_h_errno;
		} hostnamadr;

		struct {
			char		*name;
			int		 family;
			struct async	*subq;
			in_addr_t	 addr;
		} netnamadr;

		struct {
			char		*hostname;
			char		*servname;
			int		 port_tcp;
			int		 port_udp;
			union {
				struct sockaddr		sa;
				struct sockaddr_in	sain;
				struct sockaddr_in6	sain6;
			}		 sa;

			struct addrinfo	 hints;
			char		*fqdn;
			struct addrinfo	*aifirst;
			struct addrinfo	*ailast;
			struct async	*subq;
			int		 flags;
		} ai;

		struct {
			char		*hostname;
			char		*servname;
			size_t		 hostnamelen;
			size_t		 servnamelen;
			union {
				struct sockaddr		sa;
				struct sockaddr_in	sain;
				struct sockaddr_in6	sain6;
			}		 sa;
			int		 flags;
			struct async	*subq;
		} ni;
#define MAXTOKEN 10
	} as;

};

#define AS_DB(p) ((p)->as_ctx->ac_db[(p)->as_db_idx - 1])
#define AS_FAMILY(p) ((p)->as_ctx->ac_family[(p)->as_family_idx])

enum asr_state {
	ASR_STATE_INIT,
	ASR_STATE_NEXT_DOMAIN,
	ASR_STATE_NEXT_DB,
	ASR_STATE_SAME_DB,
	ASR_STATE_NEXT_FAMILY,
	ASR_STATE_NEXT_NS,
	ASR_STATE_UDP_SEND,
	ASR_STATE_UDP_RECV,
	ASR_STATE_TCP_WRITE,
	ASR_STATE_TCP_READ,
	ASR_STATE_PACKET,
	ASR_STATE_SUBQUERY,
	ASR_STATE_NOT_FOUND,
	ASR_STATE_HALT,
};


/* asr_utils.c */
void	pack_init(struct pack *, char *, size_t);
int	pack_header(struct pack *, const struct header *);
int	pack_query(struct pack *, uint16_t, uint16_t, const char *);

void	unpack_init(struct unpack *, const char *, size_t);
int	unpack_header(struct unpack *, struct header *);
int	unpack_query(struct unpack *, struct query *);
int	unpack_rr(struct unpack *, struct rr *);
int	sockaddr_from_str(struct sockaddr *, int, const char *);
ssize_t dname_from_fqdn(const char *, char *, size_t);

/* asr.c */
struct asr_ctx *asr_use_resolver(struct asr *);
void asr_ctx_unref(struct asr_ctx *);
struct async *async_new(struct asr_ctx *, int);
void async_free(struct async *);
size_t asr_make_fqdn(const char *, const char *, char *, size_t);
size_t asr_domcat(const char *, const char *, char *, size_t);
char *asr_strdname(const char *, char *, size_t);
int asr_iter_db(struct async *);
int asr_iter_ns(struct async *);
int asr_iter_domain(struct async *, const char *, char *, size_t);
int asr_parse_namedb_line(FILE *, char **, int);

/* <*>_async.h */
struct async *res_query_async_ctx(const char *, int, int, unsigned char *, int,
    struct asr_ctx *);
struct async *res_search_async_ctx(const char *, int, int, unsigned char *, int,
    struct asr_ctx *);
struct async *gethostbyaddr_async_ctx(const void *, socklen_t, int,
    struct asr_ctx *);

#ifdef DEBUG

#define DPRINT(...)		do { if(asr_debug) {		\
		fprintf(asr_debug, __VA_ARGS__);		\
	} } while (0)
#define DPRINT_PACKET(n, p, s)	do { if(asr_debug) {		\
		fprintf(asr_debug, "----- %s -----\n", n);	\
		asr_dump_packet(asr_debug, (p), (s));		\
		fprintf(asr_debug, "--------------\n");		\
	} } while (0)

const char *asr_querystr(int);
const char *asr_statestr(int);
const char *asr_transitionstr(int);
const char *print_sockaddr(const struct sockaddr *, char *, size_t);
void asr_dump_config(FILE *, struct asr *);
void asr_dump_packet(FILE *, const void *, size_t);

extern FILE * asr_debug;

#else /* DEBUG */

#define DPRINT(...)
#define DPRINT_PACKET(...)

#endif /* DEBUG */

#define async_set_state(a, s) do {		\
	DPRINT("asr: [%s@%p] %s -> %s\n",	\
		asr_querystr((a)->as_type),	\
		as,				\
		asr_statestr((a)->as_state),	\
		asr_statestr((s)));		\
	(a)->as_state = (s); } while (0)
