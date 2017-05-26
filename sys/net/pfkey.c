/*	$OpenBSD: pfkey.c,v 1.41 2017/05/16 12:24:01 mpi Exp $	*/

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed at the Information
 *	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Craig Metz. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/socketvar.h>
#include <net/radix.h>
#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>

#include <sys/protosw.h>
#include <sys/domain.h>
#include <net/raw_cb.h>

#define PFKEYV2_PROTOCOL 2

#define PFKEY_MSG_MAXSZ 4096

struct domain pfkeydomain;
struct sockaddr pfkey_addr = { 2, PF_KEY, };

int pfkey_usrreq(struct socket *, int , struct mbuf *, struct mbuf *,
    struct mbuf *, struct proc *);
int pfkey_output(struct mbuf *, struct socket *, struct sockaddr *,
    struct mbuf *);

void pfkey_init(void);

int
pfkey_sendup(struct socket *socket, struct mbuf *packet, int more)
{
	struct mbuf *packet2;

	NET_ASSERT_LOCKED();

	if (more) {
		if (!(packet2 = m_dup_pkt(packet, 0, M_DONTWAIT)))
			return (ENOMEM);
	} else
		packet2 = packet;

	if (!sbappendaddr(&socket->so_rcv, &pfkey_addr, packet2, NULL)) {
		m_freem(packet2);
		return (ENOBUFS);
	}

	sorwakeup(socket);
	return (0);
}

int
pfkey_output(struct mbuf *mbuf, struct socket *socket, struct sockaddr *dstaddr,
    struct mbuf *control)
{
	void *message;
	int error = 0;

#ifdef DIAGNOSTIC
	if (!mbuf || !(mbuf->m_flags & M_PKTHDR)) {
		error = EINVAL;
		goto ret;
	}
#endif /* DIAGNOSTIC */

	if (mbuf->m_pkthdr.len > PFKEY_MSG_MAXSZ) {
		error = EMSGSIZE;
		goto ret;
	}

	if (!(message = malloc((unsigned long) mbuf->m_pkthdr.len,
	    M_PFKEY, M_DONTWAIT))) {
		error = ENOMEM;
		goto ret;
	}

	m_copydata(mbuf, 0, mbuf->m_pkthdr.len, message);

	error = pfkeyv2_send(socket, message, mbuf->m_pkthdr.len);

ret:
	m_freem(mbuf);
	return (error);
}

int
pfkey_attach(struct socket *so, int proto)
{
	int rval;

	if ((so->so_state & SS_PRIV) == 0)
		return EACCES;

	if (!(so->so_pcb = malloc(sizeof(struct rawcb),
	    M_PCB, M_DONTWAIT | M_ZERO)))
		return (ENOMEM);

	rval = raw_attach(so, so->so_proto->pr_protocol);
	if (rval)
		goto ret;

	((struct rawcb *)so->so_pcb)->rcb_faddr = &pfkey_addr;
	soisconnected(so);

	so->so_options |= SO_USELOOPBACK;
	if ((rval = pfkeyv2_create(so)) != 0)
		goto ret;

	return (0);

ret:
	free(so->so_pcb, M_PCB, sizeof(struct rawcb));
	return (rval);
}

static int
pfkey_detach(struct socket *socket, struct proc *p)
{
	int rval, i;

	rval = pfkeyv2_release(socket);
	i = raw_usrreq(socket, PRU_DETACH, NULL, NULL, NULL, p);

	if (!rval)
		rval = i;

	return (rval);
}

int
pfkey_usrreq(struct socket *socket, int req, struct mbuf *mbuf,
    struct mbuf *nam, struct mbuf *control, struct proc *p)
{
	int rval;

	switch (req) {
	case PRU_DETACH:
		return (pfkey_detach(socket, p));

	default:
		rval = raw_usrreq(socket, req, mbuf, nam, control, p);
	}

	return (rval);
}

static struct protosw pfkeysw[] = {
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &pfkeydomain,
  .pr_protocol	= PFKEYV2_PROTOCOL,
  .pr_flags	= PR_ATOMIC | PR_ADDR,
  .pr_output	= pfkey_output,
  .pr_usrreq	= pfkey_usrreq,
  .pr_attach	= pfkey_attach,
  .pr_sysctl	= pfkeyv2_sysctl,
}
};

struct domain pfkeydomain = {
  .dom_family = PF_KEY,
  .dom_name = "PF_KEY",
  .dom_init = pfkey_init,
  .dom_protosw = pfkeysw,
  .dom_protoswNPROTOSW = &pfkeysw[nitems(pfkeysw)],
};

void
pfkey_init(void)
{
	rn_init(sizeof(struct sockaddr_encap));
}
