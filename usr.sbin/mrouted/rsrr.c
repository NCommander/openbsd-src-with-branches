/*	$NetBSD: rsrr.c,v 1.2 1995/10/09 03:51:56 thorpej Exp $	*/

/*
 * Copyright (c) 1993 by the University of Southern California
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation in source and binary forms for non-commercial purposes
 * and without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both the copyright notice and
 * this permission notice appear in supporting documentation. and that
 * any documentation, advertising materials, and other materials related
 * to such distribution and use acknowledge that the software was
 * developed by the University of Southern California, Information
 * Sciences Institute.  The name of the University may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
 * the suitability of this software for any purpose.  THIS SOFTWARE IS
 * PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Other copyrights might apply to parts of this software and are so
 * noted when applicable.
 */

/* RSRR code written by Daniel Zappala, USC Information Sciences Institute,
 * April 1995.
 */

#ifdef RSRR

#include "defs.h"

/* Taken from prune.c */
/*
 * checks for scoped multicast addresses
 */
#define GET_SCOPE(gt) { \
	register int _i; \
	if (((gt)->gt_mcastgrp & 0xff000000) == 0xef000000) \
	    for (_i = 0; _i < numvifs; _i++) \
		if (scoped_addr(_i, (gt)->gt_mcastgrp)) \
		    VIFM_SET(_i, (gt)->gt_scope); \
	}

/*
 * Exported variables.
 */
int rsrr_socket;			/* interface to reservation protocol */

/* 
 * Global RSRR variables.
 */
char rsrr_recv_buf[RSRR_MAX_LEN];	/* RSRR receive buffer */
char rsrr_send_buf[RSRR_MAX_LEN];	/* RSRR send buffer */

/*
 * Procedure definitions needed internally.
 */
void rsrr_accept();
void rsrr_send();
void rsrr_accept_iq();
void rsrr_accept_rq();

/* Initialize RSRR socket */
void
rsrr_init()
{
    int servlen;
    struct sockaddr_un serv_addr;

    if ((rsrr_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
	log(LOG_ERR, errno, "Can't create RSRR socket");

    unlink(RSRR_SERV_PATH);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, RSRR_SERV_PATH);
    servlen = sizeof(serv_addr.sun_family) + strlen(serv_addr.sun_path);
 
    if (bind(rsrr_socket, (struct sockaddr *) &serv_addr, servlen) < 0)
	log(LOG_ERR, errno, "Can't bind RSRR socket");

    if (register_input_handler(rsrr_socket,rsrr_read) < 0)
	log(LOG_WARNING, 0, "Couldn't register RSRR as an input handler");
}

/* Read a message from the RSRR socket */
void
rsrr_read()
{
    register int rsrr_recvlen;
    struct sockaddr_un client_addr;
    int client_length = sizeof(client_addr);
    register int omask;
    
    bzero((char *) &client_addr, sizeof(client_addr));
    rsrr_recvlen = recvfrom(rsrr_socket, rsrr_recv_buf, sizeof(rsrr_recv_buf),
			    0, &client_addr, &client_length);
    if (rsrr_recvlen < 0) {	
	if (errno != EINTR)
	    log(LOG_ERR, errno, "RSRR recvfrom");
	return;
    }
    /* Use of omask taken from main() */
    omask = sigblock(sigmask(SIGALRM));
    rsrr_accept(rsrr_recvlen,&client_addr,client_length);
    (void)sigsetmask(omask);
}

/* Accept a message from the reservation protocol and take
 * appropriate action.
 */
void
rsrr_accept(recvlen,client_addr,client_length)
    int recvlen;
    struct sockaddr_un *client_addr;
    int client_length;
{
    struct rsrr_header *rsrr;
    struct rsrr_rq *route_query;
    
    if (recvlen < RSRR_HEADER_LEN) {
	log(LOG_WARNING, 0,
	    "Received RSRR packet of %d bytes, which is less than min size",
	    recvlen);
	return;
    }
    
    rsrr = (struct rsrr_header *) rsrr_recv_buf;
    
    if (rsrr->version > RSRR_MAX_VERSION) {
	log(LOG_WARNING, 0,
	    "Received RSRR packet version %d, which I don't understand",
	    rsrr->version);
	return;
    }
    
    switch (rsrr->version) {
      case 1:
	switch (rsrr->type) {
	  case RSRR_INITIAL_QUERY:
	    /* Send Initial Reply to client */
	    log(LOG_INFO, 0, "Received Initial Query\n");
	    rsrr_accept_iq(client_addr,client_length);
	    break;
	  case RSRR_ROUTE_QUERY:
	    /* Check size */
	    if (recvlen < RSRR_RQ_LEN) {
		log(LOG_WARNING, 0,
		    "Received Route Query of %d bytes, which is too small",
		    recvlen);
		break;
	    }
	    /* Get the query */
	    route_query = (struct rsrr_rq *) (rsrr_recv_buf + RSRR_HEADER_LEN);
	    log(LOG_INFO, 0,
		"Received Route Query for src %s grp %s notification %d",
		inet_fmt(route_query->source_addr.s_addr, s1),
		inet_fmt(route_query->dest_addr.s_addr,s2),
		BIT_TST(rsrr->flags,RSRR_NOTIFICATION_BIT));
	    /* Send Route Reply to client */
	    rsrr_accept_rq(rsrr,route_query,client_addr,client_length);
	    break;
	  default:
	    log(LOG_WARNING, 0,
		"Received RSRR packet type %d, which I don't handle",
		rsrr->type);
	    break;
	}
	break;
	
      default:
	log(LOG_WARNING, 0,
	    "Received RSRR packet version %d, which I don't understand",
	    rsrr->version);
	break;
    }
}

/* Send an Initial Reply to the reservation protocol. */
void
rsrr_accept_iq(client_addr,client_length)
    struct sockaddr_un *client_addr;
    int client_length;    
{
    struct rsrr_header *rsrr;
    struct rsrr_vif *vif_list;
    struct uvif *v;
    int vifi, sendlen;
    
    /* Check for space.  There should be room for plenty of vifs,
     * but we should check anyway.
     */
    if (numvifs > RSRR_MAX_VIFS) {
	log(LOG_WARNING, 0,
	    "Can't send RSRR Route Reply because %d is too many vifs %d",
	    numvifs);
	return;
    }
    
    /* Set up message */
    rsrr = (struct rsrr_header *) rsrr_send_buf;
    rsrr->version = 1;
    rsrr->type = RSRR_INITIAL_REPLY;
    rsrr->flags = 0;
    rsrr->num = numvifs;
    
    vif_list = (struct rsrr_vif *) (rsrr_send_buf + RSRR_HEADER_LEN);
    
    /* Include the vif list. */
    for (vifi=0, v = uvifs; vifi < numvifs; vifi++, v++) {
	vif_list[vifi].id = vifi;
	vif_list[vifi].status = 0;
	if (v->uv_flags & VIFF_DISABLED)
	    BIT_SET(vif_list[vifi].status,RSRR_DISABLED_BIT);
	vif_list[vifi].threshold = v->uv_threshold;
	vif_list[vifi].local_addr.s_addr = v->uv_lcl_addr;
    }
    
    /* Get the size. */
    sendlen = RSRR_HEADER_LEN + numvifs*RSRR_VIF_LEN;
    
    /* Send it. */
    log(LOG_INFO, 0, "Send RSRR Initial Reply");
    rsrr_send(sendlen,client_addr,client_length);
}

/* Send a Route Reply to the reservation protocol. */
void
rsrr_accept_rq(rsrr_in,route_query,client_addr,client_length)
    struct rsrr_header *rsrr_in;
    struct rsrr_rq *route_query;    
    struct sockaddr_un *client_addr;
    int client_length;    
{
    struct rsrr_header *rsrr;
    struct rsrr_rr *route_reply;
    struct gtable *gt,local_g;
    struct rtentry *r;
    int sendlen,i;
    u_long mcastgrp;
    
    /* Set up message */
    rsrr = (struct rsrr_header *) rsrr_send_buf;
    rsrr->version = 1;
    rsrr->type = RSRR_ROUTE_REPLY;
    rsrr->flags = rsrr_in->flags;
    rsrr->num = 0;
    
    route_reply = (struct rsrr_rr *) (rsrr_send_buf + RSRR_HEADER_LEN);
    route_reply->dest_addr.s_addr = route_query->dest_addr.s_addr;
    route_reply->source_addr.s_addr = route_query->source_addr.s_addr;
    route_reply->query_id = route_query->query_id;
    
    /* Blank routing entry for error. */
    route_reply->in_vif = 0;
    route_reply->reserved = 0;
    route_reply->out_vif_bm = 0;
    
    /* Clear error bit. */
    BIT_CLR(rsrr->flags,RSRR_ERROR_BIT);
    /* Turn notification off.  We don't do it yet. */
    BIT_CLR(rsrr->flags,RSRR_NOTIFICATION_BIT);
    
    /* First check kernel. Code taken from add_table_entry() */
    if (find_src_grp(route_query->source_addr.s_addr, 0,
		     route_query->dest_addr.s_addr)) {
	gt = gtp ? gtp->gt_gnext : kernel_table;
	
	/* Include the routing entry. */
	route_reply->in_vif = gt->gt_route->rt_parent;
	route_reply->out_vif_bm = gt->gt_grpmems;
	
    } else {
	/* No kernel entry; use routing table. */
	r = determine_route(route_query->source_addr.s_addr);
	
	if (r != NULL) {
	    /* We need to mimic what will happen if a data packet
	     * is forwarded by multicast routing -- the kernel will
	     * make an upcall and mrouted will install a route in the kernel.
	     * Our outgoing vif bitmap should reflect what that table
	     * will look like.  Grab code from add_table_entry().
	     * This is gross, but it's probably better to be accurate.
	     */
	    
	    gt = &local_g;
	    mcastgrp = route_query->dest_addr.s_addr;
	    
	    gt->gt_mcastgrp    	= mcastgrp;
	    gt->gt_grpmems	= 0;
	    gt->gt_scope	= 0;
	    gt->gt_route        = r;
	    
	    /* obtain the multicast group membership list */
	    for (i = 0; i < numvifs; i++) {
		if (VIFM_ISSET(i, r->rt_children) && 
		    !(VIFM_ISSET(i, r->rt_leaves)))
		    VIFM_SET(i, gt->gt_grpmems);
		
		if (VIFM_ISSET(i, r->rt_leaves) && grplst_mem(i, mcastgrp))
		    VIFM_SET(i, gt->gt_grpmems);
	    }
	    
	    GET_SCOPE(gt);
	    gt->gt_grpmems &= ~gt->gt_scope;
	    
	    /* Include the routing entry. */
	    route_reply->in_vif = gt->gt_route->rt_parent;
	    route_reply->out_vif_bm = gt->gt_grpmems;

	} else {
	    /* Set error bit. */
	    BIT_SET(rsrr->flags,RSRR_ERROR_BIT);
	}
    }
    
    /* Get the size. */
    sendlen = RSRR_RR_LEN;
    
    log(LOG_INFO, 0, "Send RSRR Route Reply for src %s grp %s ",
	inet_fmt(route_reply->source_addr.s_addr,s1),
	inet_fmt(route_reply->dest_addr.s_addr,s2));
    log(LOG_INFO, 0, "in vif %d out vif %d\n",
	route_reply->in_vif,route_reply->out_vif_bm);
    
    /* Send it. */
    rsrr_send(sendlen,client_addr,client_length);
}

/* Send an RSRR message. */
void
rsrr_send(sendlen,client_addr,client_length)
    int sendlen;
    struct sockaddr_un *client_addr;
    int client_length;
{
    int error;
    
    /* Send it. */
    error = sendto(rsrr_socket, rsrr_send_buf, sendlen, 0,
		   *client_addr, client_length);
    
    /* Check for errors. */
    if (error < 0) {
	log(LOG_WARNING, errno, "Failed send on RSRR socket");
	return;
    }
    if (error != sendlen) {
	log(LOG_WARNING, 0,
	    "Sent only %d out of %d bytes on RSRR socket\n", error, sendlen);
	return;
    }
}

void
rsrr_clean()
{
    unlink(RSRR_SERV_PATH);
}

#endif RSRR
