/*	$OpenBSD: pppoe.h,v 1.1.1.1 2000/06/18 07:30:41 jason Exp $	*/

/*
 * Copyright (c) 2000 Network Security Technologies, Inc. http://www.netsec.net
 * All rights reserved.
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
 *	This product includes software developed by Network Security
 *	Technologies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* XXX */
#ifndef	ETHERTYPE_PPPOEDISC
#define	ETHERTYPE_PPPOEDISC	0x8863	/* PPP Over Ethernet Discovery Stage */
#endif
#ifndef	ETHERTYPE_PPPOE
#define	ETHERTYPE_PPPOE		0x8864  /* PPP Over Ethernet Session Stage */
#endif

#define	PPPOE_MAXSESSIONS	32
#define	PPPOE_BPF_BUFSIZ	65536

struct pppoe_header {
	u_int8_t vertype;	/* PPPoE version (low 4), type (high 4) */
	u_int8_t code;		/* PPPoE code (packet type) */
	u_int16_t sessionid;	/* PPPoE session id */
	u_int16_t len;		/* PPPoE payload length */
};
#define	PPPOE_MTU		(ETHERMTU - sizeof(struct pppoe_header))

#define	PPPOE_VER_S		0	/* Version shift */
#define	PPPOE_VER_M		0x0f	/* Version mask */
#define	PPPOE_TYPE_S		4	/* Type shift */
#define	PPPOE_TYPE_M		0xf0	/* Type mask */

#define	PPPOE_VER(vt)	(((vt) & PPPOE_VER_M) >> PPPOE_VER_S)
#define	PPPOE_TYPE(vt)	(((vt) & PPPOE_TYPE_M) >> PPPOE_TYPE_S)
#define	PPPOE_VERTYPE(v,t)					\
	((((v) << PPPOE_VER_S) & PPPOE_VER_M) |			\
	(((t) << PPPOE_TYPE_S) & PPPOE_TYPE_M))

#define	PPPOE_CODE_SESSION	0x00	/* Session */
#define	PPPOE_CODE_PADO		0x07	/* Active Discovery Offer */
#define	PPPOE_CODE_PADI		0x09	/* Active Discovery Initiation */
#define	PPPOE_CODE_PADR		0x19	/* Active Discovery Request */
#define	PPPOE_CODE_PADS		0x65	/* Active Discovery Session-Confirm */
#define	PPPOE_CODE_PADT		0xa7	/* Active Discovery Terminate */

struct pppoe_tag {
	u_int16_t type;		/* Tag Type */
	u_int16_t len;		/* Tag Length */
	u_int8_t *val;		/* Tag Value */
};

#define	PPPOE_TAG_END_OF_LIST		0x0000	/* End Of List */
#define	PPPOE_TAG_SERVICE_NAME		0x0101	/* Service Name */
#define	PPPOE_TAG_AC_NAME		0x0102	/* Access Concentrator Name */
#define	PPPOE_TAG_HOST_UNIQ		0x0103	/* Host Uniq */
#define	PPPOE_TAG_AC_COOKIE		0x0104	/* Access Concentratr Cookie */
#define	PPPOE_TAG_VENDOR_SPEC		0x0105	/* Vendor Specific */
#define	PPPOE_TAG_RELAY_SESSION		0x0110	/* Relay Session Id */
#define	PPPOE_TAG_SERVICE_NAME_ERROR	0x0201	/* Service Name Error */
#define	PPPOE_TAG_AC_SYSTEM_ERROR	0x0202	/* Acc. Concentrator Error */
#define	PPPOE_TAG_GENERIC_ERROR		0x0203	/* Generic Error */

extern int option_verbose;
extern u_char etherbroadcastaddr[];

void server_mode __P((int, char *, char *, struct ether_addr *));
int client_mode __P((int, char *, char *, struct ether_addr *));

struct tag_list {
	LIST_HEAD(, tag_node)		thelist;
};

struct tag_node {
	LIST_ENTRY(tag_node)		next;
	u_int16_t	type;
	u_int16_t	len;
	u_int8_t	*val;
	int		_ref;
};

void tag_init __P((struct tag_list *));
void tag_show __P((struct tag_list *));
void tag_destroy __P((struct tag_list *));
struct tag_node *tag_lookup __P((struct tag_list *, u_int16_t, int));
int tag_add __P((struct tag_list *, u_int16_t, u_int16_t, u_int8_t *));
int tag_pkt __P((struct tag_list *, u_long, u_int8_t *));
void tag_hton __P((struct tag_list *));
void tag_ntoh __P((struct tag_list *));

struct pppoe_session {
	LIST_ENTRY(pppoe_session)	s_next;
	struct ether_addr s_ea;		/* remote ethernet mac */
	u_int16_t s_id;			/* session id */
	int s_fd;			/* ttyfd */
	int s_first;
};

struct pppoe_session_master {
	LIST_HEAD(, pppoe_session)	sm_sessions;	/* session list */
	int 				sm_nsessions;	/* # of sessions */
};

extern struct pppoe_session_master session_master;

void session_init __P((void));
void session_destroy __P((struct pppoe_session *));
struct pppoe_session *session_new __P((struct ether_addr *));
struct pppoe_session *session_find_eaid __P((struct ether_addr *, u_int16_t));
struct pppoe_session *session_find_fd __P((int));

int runppp __P((int, char *));
int bpf_to_ppp __P((int, u_long, u_int8_t *));
int ppp_to_bpf __P((int, int, struct ether_addr *, struct ether_addr *,
    u_int16_t));
int send_padt __P((int, struct ether_addr *, struct ether_addr *, u_int16_t));
void recv_debug __P((int, struct ether_addr *,
    struct ether_header *, struct pppoe_header *, u_long, u_int8_t *));
void debug_packet __P((u_int8_t *, int));

u_int32_t cookie_bake __P((void));
