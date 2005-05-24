/*	$OpenBSD: rde.h,v 1.15 2005/05/23 23:03:07 claudio Exp $ */

/*
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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

#ifndef _RDE_H_
#define _RDE_H_

#include <sys/types.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <event.h>
#include <limits.h>

struct vertex {
	RB_ENTRY(vertex)	 entry;
	TAILQ_ENTRY(vertex)	 cand;
	struct event		 ev;
	struct in_addr		 nexthop;
	struct vertex		*prev;
	struct rde_nbr		*nbr;
	struct lsa		*lsa;
	time_t			 changed;
	time_t			 stamp;
	u_int32_t		 cost;
	u_int32_t		 ls_id;
	u_int32_t		 adv_rtr;
	u_int8_t		 type;
	u_int8_t		 flooded;
};

struct rde_req_entry {
	TAILQ_ENTRY(rde_req_entry)	entry;
	u_int32_t			ls_id;
	u_int32_t			adv_rtr;
	u_int8_t			type;
};

/* just the info RDE needs */
struct rde_nbr {
	LIST_ENTRY(rde_nbr)		 entry, hash;
	struct in_addr			 id;
	struct in_addr			 area_id;
	TAILQ_HEAD(, rde_req_entry)	 req_list;
	struct area			*area;
	u_int32_t			 peerid;	/* unique ID in DB */
	int				 state;
	int				 self;
};

struct rde_asext {
	LIST_ENTRY(rde_asext)	 entry;
	struct kroute		 kr;
	int			 used;
};

struct rt_node {
	RB_ENTRY(rt_node)	 entry;
	struct in_addr		 prefix;
	struct in_addr		 nexthop;
	struct in_addr		 area;
	struct in_addr		 adv_rtr;
	u_int32_t		 cost;
	u_int32_t		 cost2;
	enum path_type		 p_type;
	enum dst_type		 d_type;
	u_int8_t		 flags;
	u_int8_t		 prefixlen;
	u_int8_t		 invalid;
};

/* rde.c */
pid_t		 rde(struct ospfd_conf *, int [2], int [2], int [2]);
int		 rde_imsg_compose_parent(int, pid_t, void *, u_int16_t);
int		 rde_imsg_compose_ospfe(int, u_int32_t, pid_t, void *,
		     u_int16_t);
u_int32_t	 rde_router_id(void);
void		 rde_send_change_kroute(struct rt_node *);
void		 rde_send_delete_kroute(struct rt_node *);
void		 rde_nbr_del(struct rde_nbr *);
int		 rde_nbr_loading(struct area *);
struct rde_nbr	*rde_nbr_self(struct area *);
void		 rde_summary_update(struct rt_node *, struct area *);

/* rde_lsdb.c */
void		 lsa_init(struct lsa_tree *);
int		 lsa_compare(struct vertex *, struct vertex *);
void		 vertex_free(struct vertex *);
int		 lsa_newer(struct lsa_hdr *, struct lsa_hdr *);
int		 lsa_check(struct rde_nbr *, struct lsa *, u_int16_t);
int		 lsa_self(struct rde_nbr *, struct lsa *, struct vertex *);
void		 lsa_add(struct rde_nbr *, struct lsa *);
void		 lsa_del(struct rde_nbr *, struct lsa_hdr *);
void		 lsa_age(struct vertex *);
struct vertex	*lsa_find(struct area *, u_int8_t, u_int32_t, u_int32_t);
struct vertex	*lsa_find_net(struct area *area, u_int32_t);
int		 lsa_num_links(struct vertex *);
void		 lsa_snap(struct area *, u_int32_t);
void		 lsa_dump(struct lsa_tree *, int, pid_t);
void		 lsa_merge(struct rde_nbr *, struct lsa *, struct vertex *);
void		 lsa_remove_invalid_sums(struct area *);

/* rde_spf.c */
void		 spf_calc(struct area *);
void		 spf_tree_clr(struct area *);

void		 cand_list_init(void);
void		 cand_list_add(struct vertex *);
struct vertex	*cand_list_pop(void);
int		 cand_list_present(struct vertex *);
void		 cand_list_clr(void);
int		 cand_list_empty(void);

void		 spf_timer(int, short, void *);
int		 start_spf_timer(struct ospfd_conf *);
int		 stop_spf_timer(struct ospfd_conf *);
int		 start_spf_holdtimer(struct ospfd_conf *);

void		 rt_init(void);
int		 rt_compare(struct rt_node *, struct rt_node *);
struct rt_node	*rt_find(in_addr_t, u_int8_t);
int		 rt_insert(struct rt_node *);
int		 rt_remove(struct rt_node *);
void		 rt_clear(void);
void		 rt_dump(struct in_addr, pid_t, u_int8_t);

struct lsa_rtr_link	*get_rtr_link(struct vertex *, int);
struct lsa_net_link	*get_net_link(struct vertex *, int);

RB_PROTOTYPE(lsa_tree, vertex, entry, lsa_compare)

#endif	/* _RDE_H_ */
