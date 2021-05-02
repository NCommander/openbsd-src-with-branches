/*	$OpenBSD: uipc_usrreq.c,v 1.143 2021/02/10 08:20:09 mvs Exp $	*/
/*	$NetBSD: uipc_usrreq.c,v 1.18 1996/02/09 19:00:50 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)uipc_usrreq.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/unpcb.h>
#include <sys/un.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mbuf.h>
#include <sys/task.h>
#include <sys/pledge.h>
#include <sys/pool.h>
#include <sys/rwlock.h>

/*
 * Locks used to protect global data and struct members:
 *      I       immutable after creation
 *      U       unp_lock
 */
struct rwlock unp_lock = RWLOCK_INITIALIZER("unplock");

/*
 * Stack of sets of files that were passed over a socket but were
 * not received and need to be closed.
 */
struct	unp_deferral {
	SLIST_ENTRY(unp_deferral)	ud_link;	/* [U] */
	int				ud_n;		/* [I] */
	/* followed by ud_n struct fdpass */
	struct fdpass			ud_fp[];	/* [I] */
};

void	uipc_setaddr(const struct unpcb *, struct mbuf *);
void	unp_discard(struct fdpass *, int);
void	unp_mark(struct fdpass *, int);
void	unp_scan(struct mbuf *, void (*)(struct fdpass *, int));
int	unp_nam2sun(struct mbuf *, struct sockaddr_un **, size_t *);

struct pool unpcb_pool;
struct task unp_gc_task = TASK_INITIALIZER(unp_gc, NULL);

/*
 * Unix communications domain.
 *
 * TODO:
 *	RDM
 *	rethink name space problems
 *	need a proper out-of-band
 */
const struct	sockaddr sun_noname = { sizeof(sun_noname), AF_UNIX };

/* [U] list of all UNIX domain sockets, for unp_gc() */
LIST_HEAD(unp_head, unpcb)	unp_head =
	LIST_HEAD_INITIALIZER(unp_head);
/* [U] list of sets of files that were sent over sockets that are now closed */
SLIST_HEAD(,unp_deferral)	unp_deferred =
	SLIST_HEAD_INITIALIZER(unp_deferred);

ino_t	unp_ino;	/* [U] prototype for fake inode numbers */
int	unp_rights;	/* [U] file descriptors in flight */
int	unp_defer;	/* [U] number of deferred fp to close by the GC task */
int	unp_gcing;	/* [U] GC task currently running */

void
unp_init(void)
{
	pool_init(&unpcb_pool, sizeof(struct unpcb), 0,
	    IPL_SOFTNET, 0, "unpcb", NULL);
}

void
uipc_setaddr(const struct unpcb *unp, struct mbuf *nam)
{
	if (unp != NULL && unp->unp_addr != NULL) {
		nam->m_len = unp->unp_addr->m_len;
		memcpy(mtod(nam, caddr_t), mtod(unp->unp_addr, caddr_t),
		    nam->m_len);
	} else {
		nam->m_len = sizeof(sun_noname);
		memcpy(mtod(nam, struct sockaddr *), &sun_noname,
		    nam->m_len);
	}
}

int
uipc_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	struct unpcb *unp = sotounpcb(so);
	struct unpcb *unp2;
	struct socket *so2;
	int error = 0;

	if (req == PRU_CONTROL)
		return (EOPNOTSUPP);
	if (req != PRU_SEND && control && control->m_len) {
		error = EOPNOTSUPP;
		goto release;
	}
	if (unp == NULL) {
		error = EINVAL;
		goto release;
	}

	NET_ASSERT_UNLOCKED();

	switch (req) {

	case PRU_BIND:
		error = unp_bind(unp, nam, p);
		break;

	case PRU_LISTEN:
		if (unp->unp_vnode == NULL)
			error = EINVAL;
		break;

	case PRU_CONNECT:
		error = unp_connect(so, nam, p);
		break;

	case PRU_CONNECT2:
		error = unp_connect2(so, (struct socket *)nam);
		if (!error) {
			unp->unp_connid.uid = p->p_ucred->cr_uid;
			unp->unp_connid.gid = p->p_ucred->cr_gid;
			unp->unp_connid.pid = p->p_p->ps_pid;
			unp->unp_flags |= UNP_FEIDS;
			unp2 = sotounpcb((struct socket *)nam);
			unp2->unp_connid.uid = p->p_ucred->cr_uid;
			unp2->unp_connid.gid = p->p_ucred->cr_gid;
			unp2->unp_connid.pid = p->p_p->ps_pid;
			unp2->unp_flags |= UNP_FEIDS;
		}
		break;

	case PRU_DISCONNECT:
		unp_disconnect(unp);
		break;

	case PRU_ACCEPT:
		/*
		 * Pass back name of connected socket,
		 * if it was bound and we are still connected
		 * (our peer may have closed already!).
		 */
		uipc_setaddr(unp->unp_conn, nam);
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		unp_shutdown(unp);
		break;

	case PRU_RCVD:
		switch (so->so_type) {

		case SOCK_DGRAM:
			panic("uipc 1");
			/*NOTREACHED*/

		case SOCK_STREAM:
		case SOCK_SEQPACKET:
			if (unp->unp_conn == NULL)
				break;
			so2 = unp->unp_conn->unp_socket;
			/*
			 * Adjust backpressure on sender
			 * and wakeup any waiting to write.
			 */
			so2->so_snd.sb_mbcnt = so->so_rcv.sb_mbcnt;
			so2->so_snd.sb_cc = so->so_rcv.sb_cc;
			sowwakeup(so2);
			break;

		default:
			panic("uipc 2");
		}
		break;

	case PRU_SEND:
		if (control && (error = unp_internalize(control, p)))
			break;
		switch (so->so_type) {

		case SOCK_DGRAM: {
			const struct sockaddr *from;

			if (nam) {
				if (unp->unp_conn) {
					error = EISCONN;
					break;
				}
				error = unp_connect(so, nam, p);
				if (error)
					break;
			} else {
				if (unp->unp_conn == NULL) {
					error = ENOTCONN;
					break;
				}
			}
			so2 = unp->unp_conn->unp_socket;
			if (unp->unp_addr)
				from = mtod(unp->unp_addr, struct sockaddr *);
			else
				from = &sun_noname;
			if (sbappendaddr(so2, &so2->so_rcv, from, m, control)) {
				sorwakeup(so2);
				m = NULL;
				control = NULL;
			} else
				error = ENOBUFS;
			if (nam)
				unp_disconnect(unp);
			break;
		}

		case SOCK_STREAM:
		case SOCK_SEQPACKET:
			if (so->so_state & SS_CANTSENDMORE) {
				error = EPIPE;
				break;
			}
			if (unp->unp_conn == NULL) {
				error = ENOTCONN;
				break;
			}
			so2 = unp->unp_conn->unp_socket;
			/*
			 * Send to paired receive port, and then raise
			 * send buffer counts to maintain backpressure.
			 * Wake up readers.
			 */
			if (control) {
				if (sbappendcontrol(so2, &so2->so_rcv, m,
				    control)) {
					control = NULL;
				} else {
					error = ENOBUFS;
					break;
				}
			} else if (so->so_type == SOCK_SEQPACKET)
				sbappendrecord(so2, &so2->so_rcv, m);
			else
				sbappend(so2, &so2->so_rcv, m);
			so->so_snd.sb_mbcnt = so2->so_rcv.sb_mbcnt;
			so->so_snd.sb_cc = so2->so_rcv.sb_cc;
			if (so2->so_rcv.sb_cc > 0)
				sorwakeup(so2);
			m = NULL;
			break;

		default:
			panic("uipc 4");
		}
		/* we need to undo unp_internalize in case of errors */
		if (control && error)
			unp_dispose(control);
		break;

	case PRU_ABORT:
		unp_drop(unp, ECONNABORTED);
		break;

	case PRU_SENSE: {
		struct stat *sb = (struct stat *)m;

		sb->st_blksize = so->so_snd.sb_hiwat;
		sb->st_dev = NODEV;
		if (unp->unp_ino == 0)
			unp->unp_ino = unp_ino++;
		sb->st_atim.tv_sec =
		    sb->st_mtim.tv_sec =
		    sb->st_ctim.tv_sec = unp->unp_ctime.tv_sec;
		sb->st_atim.tv_nsec =
		    sb->st_mtim.tv_nsec =
		    sb->st_ctim.tv_nsec = unp->unp_ctime.tv_nsec;
		sb->st_ino = unp->unp_ino;
		break;
	}

	case PRU_RCVOOB:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		uipc_setaddr(unp, nam);
		break;

	case PRU_PEERADDR:
		uipc_setaddr(unp->unp_conn, nam);
		break;

	case PRU_SLOWTIMO:
		break;

	default:
		panic("uipc_usrreq");
	}
release:
	if (req != PRU_RCVD && req != PRU_RCVOOB && req != PRU_SENSE) {
		m_freem(control);
		m_freem(m);
	}
	return (error);
}

/*
 * Both send and receive buffers are allocated PIPSIZ bytes of buffering
 * for stream sockets, although the total for sender and receiver is
 * actually only PIPSIZ.
 * Datagram sockets really use the sendspace as the maximum datagram size,
 * and don't really want to reserve the sendspace.  Their recvspace should
 * be large enough for at least one max-size datagram plus address.
 */
#define	PIPSIZ	4096
u_long	unpst_sendspace = PIPSIZ;
u_long	unpst_recvspace = PIPSIZ;
u_long	unpdg_sendspace = 2*1024;	/* really max datagram size */
u_long	unpdg_recvspace = 4*1024;

int
uipc_attach(struct socket *so, int proto)
{
	struct unpcb *unp;
	int error;

	rw_assert_wrlock(&unp_lock);

	if (so->so_pcb)
		return EISCONN;
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		switch (so->so_type) {

		case SOCK_STREAM:
		case SOCK_SEQPACKET:
			error = soreserve(so, unpst_sendspace, unpst_recvspace);
			break;

		case SOCK_DGRAM:
			error = soreserve(so, unpdg_sendspace, unpdg_recvspace);
			break;

		default:
			panic("unp_attach");
		}
		if (error)
			return (error);
	}
	unp = pool_get(&unpcb_pool, PR_NOWAIT|PR_ZERO);
	if (unp == NULL)
		return (ENOBUFS);
	unp->unp_socket = so;
	so->so_pcb = unp;
	getnanotime(&unp->unp_ctime);
	LIST_INSERT_HEAD(&unp_head, unp, unp_link);
	return (0);
}

int
uipc_detach(struct socket *so)
{
	struct unpcb *unp = sotounpcb(so);

	if (unp == NULL)
		return (EINVAL);

	NET_ASSERT_UNLOCKED();

	unp_detach(unp);

	return (0);
}

void
unp_detach(struct unpcb *unp)
{
	struct socket *so = unp->unp_socket;
	struct vnode *vp = NULL;

	rw_assert_wrlock(&unp_lock);

	LIST_REMOVE(unp, unp_link);
	if (unp->unp_vnode) {
		/*
		 * `v_socket' is only read in unp_connect and
		 * unplock prevents concurrent access.
		 */

		unp->unp_vnode->v_socket = NULL;
		vp = unp->unp_vnode;
		unp->unp_vnode = NULL;
	}

	if (unp->unp_conn)
		unp_disconnect(unp);
	while (!SLIST_EMPTY(&unp->unp_refs))
		unp_drop(SLIST_FIRST(&unp->unp_refs), ECONNRESET);
	soisdisconnected(so);
	so->so_pcb = NULL;
	m_freem(unp->unp_addr);
	pool_put(&unpcb_pool, unp);
	if (unp_rights)
		task_add(systqmp, &unp_gc_task);

	if (vp != NULL) {
		/*
		 * Enforce `i_lock' -> `unplock' because fifo subsystem
		 * requires it. The socket can't be closed concurrently
		 * because the file descriptor reference is
		 * still hold.
		 */

		sounlock(so, SL_LOCKED);
		KERNEL_LOCK();
		vrele(vp);
		KERNEL_UNLOCK();
		solock(so);
	}
}

int
unp_bind(struct unpcb *unp, struct mbuf *nam, struct proc *p)
{
	struct sockaddr_un *soun;
	struct mbuf *nam2;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;
	size_t pathlen;

	if (unp->unp_flags & (UNP_BINDING | UNP_CONNECTING))
		return (EINVAL);
	if (unp->unp_vnode != NULL)
		return (EINVAL);
	if ((error = unp_nam2sun(nam, &soun, &pathlen)))
		return (error);

	nam2 = m_getclr(M_WAITOK, MT_SONAME);
	nam2->m_len = sizeof(struct sockaddr_un);
	memcpy(mtod(nam2, struct sockaddr_un *), soun,
	    offsetof(struct sockaddr_un, sun_path) + pathlen);
	/* No need to NUL terminate: m_getclr() returns zero'd mbufs. */

	soun = mtod(nam2, struct sockaddr_un *);

	/* Fixup sun_len to keep it in sync with m_len. */
	soun->sun_len = nam2->m_len;

	NDINIT(&nd, CREATE, NOFOLLOW | LOCKPARENT, UIO_SYSSPACE,
	    soun->sun_path, p);
	nd.ni_pledge = PLEDGE_UNIX;

	unp->unp_flags |= UNP_BINDING;

	/*
	 * Enforce `i_lock' -> `unplock' because fifo subsystem
	 * requires it. The socket can't be closed concurrently
	 * because the file descriptor reference is still held.
	 */

	sounlock(unp->unp_socket, SL_LOCKED);

	KERNEL_LOCK();
/* SHOULD BE ABLE TO ADOPT EXISTING AND wakeup() ALA FIFO's */
	error = namei(&nd);
	if (error != 0) {
		m_freem(nam2);
		solock(unp->unp_socket);
		goto out;
	}
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		m_freem(nam2);
		error = EADDRINUSE;
		solock(unp->unp_socket);
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VSOCK;
	vattr.va_mode = ACCESSPERMS &~ p->p_fd->fd_cmask;
	error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	vput(nd.ni_dvp);
	if (error) {
		m_freem(nam2);
		solock(unp->unp_socket);
		goto out;
	}
	solock(unp->unp_socket);
	unp->unp_addr = nam2;
	vp = nd.ni_vp;
	vp->v_socket = unp->unp_socket;
	unp->unp_vnode = vp;
	unp->unp_connid.uid = p->p_ucred->cr_uid;
	unp->unp_connid.gid = p->p_ucred->cr_gid;
	unp->unp_connid.pid = p->p_p->ps_pid;
	unp->unp_flags |= UNP_FEIDSBIND;
	VOP_UNLOCK(vp);
out:
	KERNEL_UNLOCK();
	unp->unp_flags &= ~UNP_BINDING;

	return (error);
}

int
unp_connect(struct socket *so, struct mbuf *nam, struct proc *p)
{
	struct sockaddr_un *soun;
	struct vnode *vp;
	struct socket *so2, *so3;
	struct unpcb *unp, *unp2, *unp3;
	struct nameidata nd;
	int error;

	unp = sotounpcb(so);
	if (unp->unp_flags & (UNP_BINDING | UNP_CONNECTING))
		return (EISCONN);
	if ((error = unp_nam2sun(nam, &soun, NULL)))
		return (error);

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, soun->sun_path, p);
	nd.ni_pledge = PLEDGE_UNIX;

	unp->unp_flags |= UNP_CONNECTING;

	/*
	 * Enforce `i_lock' -> `unplock' because fifo subsystem
	 * requires it. The socket can't be closed concurrently
	 * because the file descriptor reference is still held.
	 */

	sounlock(so, SL_LOCKED);

	KERNEL_LOCK();
	error = namei(&nd);
	if (error != 0)
		goto unlock;
	vp = nd.ni_vp;
	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto put;
	}
	if ((error = VOP_ACCESS(vp, VWRITE, p->p_ucred, p)) != 0)
		goto put;
	solock(so);
	so2 = vp->v_socket;
	if (so2 == NULL) {
		error = ECONNREFUSED;
		goto put_locked;
	}
	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		goto put_locked;
	}
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		if ((so2->so_options & SO_ACCEPTCONN) == 0 ||
		    (so3 = sonewconn(so2, 0)) == 0) {
			error = ECONNREFUSED;
			goto put_locked;
		}
		unp2 = sotounpcb(so2);
		unp3 = sotounpcb(so3);
		if (unp2->unp_addr)
			unp3->unp_addr =
			    m_copym(unp2->unp_addr, 0, M_COPYALL, M_NOWAIT);
		unp3->unp_connid.uid = p->p_ucred->cr_uid;
		unp3->unp_connid.gid = p->p_ucred->cr_gid;
		unp3->unp_connid.pid = p->p_p->ps_pid;
		unp3->unp_flags |= UNP_FEIDS;
		so2 = so3;
		if (unp2->unp_flags & UNP_FEIDSBIND) {
			unp->unp_connid = unp2->unp_connid;
			unp->unp_flags |= UNP_FEIDS;
		}
	}
	error = unp_connect2(so, so2);
put_locked:
	sounlock(so, SL_LOCKED);
put:
	vput(vp);
unlock:
	KERNEL_UNLOCK();
	solock(so);
	unp->unp_flags &= ~UNP_CONNECTING;

	return (error);
}

int
unp_connect2(struct socket *so, struct socket *so2)
{
	struct unpcb *unp = sotounpcb(so);
	struct unpcb *unp2;

	rw_assert_wrlock(&unp_lock);

	if (so2->so_type != so->so_type)
		return (EPROTOTYPE);
	unp2 = sotounpcb(so2);
	unp->unp_conn = unp2;
	switch (so->so_type) {

	case SOCK_DGRAM:
		SLIST_INSERT_HEAD(&unp2->unp_refs, unp, unp_nextref);
		soisconnected(so);
		break;

	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		unp2->unp_conn = unp;
		soisconnected(so);
		soisconnected(so2);
		break;

	default:
		panic("unp_connect2");
	}
	return (0);
}

void
unp_disconnect(struct unpcb *unp)
{
	struct unpcb *unp2 = unp->unp_conn;

	if (unp2 == NULL)
		return;
	unp->unp_conn = NULL;
	switch (unp->unp_socket->so_type) {

	case SOCK_DGRAM:
		SLIST_REMOVE(&unp2->unp_refs, unp, unpcb, unp_nextref);
		unp->unp_socket->so_state &= ~SS_ISCONNECTED;
		break;

	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		unp->unp_socket->so_snd.sb_mbcnt = 0;
		unp->unp_socket->so_snd.sb_cc = 0;
		soisdisconnected(unp->unp_socket);
		unp2->unp_conn = NULL;
		unp2->unp_socket->so_snd.sb_mbcnt = 0;
		unp2->unp_socket->so_snd.sb_cc = 0;
		soisdisconnected(unp2->unp_socket);
		break;
	}
}

void
unp_shutdown(struct unpcb *unp)
{
	struct socket *so;

	switch (unp->unp_socket->so_type) {
	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		if (unp->unp_conn && (so = unp->unp_conn->unp_socket))
			socantrcvmore(so);
		break;
	default:
		break;
	}
}

void
unp_drop(struct unpcb *unp, int errno)
{
	struct socket *so = unp->unp_socket;

	rw_assert_wrlock(&unp_lock);

	so->so_error = errno;
	unp_disconnect(unp);
	if (so->so_head) {
		so->so_pcb = NULL;
		/*
		 * As long as `unp_lock' is taken before entering
		 * uipc_usrreq() releasing it here would lead to a
		 * double unlock.
		 */
		sofree(so, SL_NOUNLOCK);
		m_freem(unp->unp_addr);
		pool_put(&unpcb_pool, unp);
	}
}

#ifdef notdef
unp_drain(void)
{

}
#endif

extern	struct domain unixdomain;

static struct unpcb *
fptounp(struct file *fp)
{
	struct socket *so;

	if (fp->f_type != DTYPE_SOCKET)
		return (NULL);
	if ((so = fp->f_data) == NULL)
		return (NULL);
	if (so->so_proto->pr_domain != &unixdomain)
		return (NULL);
	return (sotounpcb(so));
}

int
unp_externalize(struct mbuf *rights, socklen_t controllen, int flags)
{
	struct proc *p = curproc;		/* XXX */
	struct cmsghdr *cm = mtod(rights, struct cmsghdr *);
	struct filedesc *fdp = p->p_fd;
	int i, *fds = NULL;
	struct fdpass *rp;
	struct file *fp;
	int nfds, error = 0;

	rw_assert_wrlock(&unp_lock);

	/*
	 * This code only works because SCM_RIGHTS is the only supported
	 * control message type on unix sockets. Enforce this here.
	 */
	if (cm->cmsg_type != SCM_RIGHTS || cm->cmsg_level != SOL_SOCKET)
		return EINVAL;

	nfds = (cm->cmsg_len - CMSG_ALIGN(sizeof(*cm))) /
	    sizeof(struct fdpass);
	if (controllen < CMSG_ALIGN(sizeof(struct cmsghdr)))
		controllen = 0;
	else
		controllen -= CMSG_ALIGN(sizeof(struct cmsghdr));
	if (nfds > controllen / sizeof(int)) {
		error = EMSGSIZE;
		goto restart;
	}

	/* Make sure the recipient should be able to see the descriptors.. */
	rp = (struct fdpass *)CMSG_DATA(cm);

	/* fdp->fd_rdir requires KERNEL_LOCK() */
	KERNEL_LOCK();

	for (i = 0; i < nfds; i++) {
		fp = rp->fp;
		rp++;
		error = pledge_recvfd(p, fp);
		if (error)
			break;

		/*
		 * No to block devices.  If passing a directory,
		 * make sure that it is underneath the root.
		 */
		if (fdp->fd_rdir != NULL && fp->f_type == DTYPE_VNODE) {
			struct vnode *vp = (struct vnode *)fp->f_data;

			if (vp->v_type == VBLK ||
			    (vp->v_type == VDIR &&
			    !vn_isunder(vp, fdp->fd_rdir, p))) {
				error = EPERM;
				break;
			}
		}
	}

	KERNEL_UNLOCK();

	fds = mallocarray(nfds, sizeof(int), M_TEMP, M_WAITOK);

restart:
	fdplock(fdp);
	if (error != 0) {
		if (nfds > 0) {
			rp = ((struct fdpass *)CMSG_DATA(cm));
			unp_discard(rp, nfds);
		}
		goto out;
	}

	/*
	 * First loop -- allocate file descriptor table slots for the
	 * new descriptors.
	 */
	rp = ((struct fdpass *)CMSG_DATA(cm));
	for (i = 0; i < nfds; i++) {
		if ((error = fdalloc(p, 0, &fds[i])) != 0) {
			/*
			 * Back out what we've done so far.
			 */
			for (--i; i >= 0; i--)
				fdremove(fdp, fds[i]);

			if (error == ENOSPC) {
				fdexpand(p);
				error = 0;
			} else {
				/*
				 * This is the error that has historically
				 * been returned, and some callers may
				 * expect it.
				 */
				error = EMSGSIZE;
			}
			fdpunlock(fdp);
			goto restart;
		}

		/*
		 * Make the slot reference the descriptor so that
		 * fdalloc() works properly.. We finalize it all
		 * in the loop below.
		 */
		mtx_enter(&fdp->fd_fplock);
		KASSERT(fdp->fd_ofiles[fds[i]] == NULL);
		fdp->fd_ofiles[fds[i]] = rp->fp;
		mtx_leave(&fdp->fd_fplock);

		fdp->fd_ofileflags[fds[i]] = (rp->flags & UF_PLEDGED);
		if (flags & MSG_CMSG_CLOEXEC)
			fdp->fd_ofileflags[fds[i]] |= UF_EXCLOSE;

		rp++;
	}

	/*
	 * Now that adding them has succeeded, update all of the
	 * descriptor passing state.
	 */
	rp = (struct fdpass *)CMSG_DATA(cm);
	for (i = 0; i < nfds; i++) {
		struct unpcb *unp;

		fp = rp->fp;
		rp++;
		if ((unp = fptounp(fp)) != NULL)
			unp->unp_msgcount--;
		unp_rights--;
	}

	/*
	 * Copy temporary array to message and adjust length, in case of
	 * transition from large struct file pointers to ints.
	 */
	memcpy(CMSG_DATA(cm), fds, nfds * sizeof(int));
	cm->cmsg_len = CMSG_LEN(nfds * sizeof(int));
	rights->m_len = CMSG_LEN(nfds * sizeof(int));
 out:
	fdpunlock(fdp);
	if (fds != NULL)
		free(fds, M_TEMP, nfds * sizeof(int));
	return (error);
}

int
unp_internalize(struct mbuf *control, struct proc *p)
{
	struct filedesc *fdp = p->p_fd;
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	struct fdpass *rp;
	struct file *fp;
	struct unpcb *unp;
	int i, error;
	int nfds, *ip, fd, neededspace;

	rw_assert_wrlock(&unp_lock);

	/*
	 * Check for two potential msg_controllen values because
	 * IETF stuck their nose in a place it does not belong.
	 */
	if (control->m_len < CMSG_LEN(0) || cm->cmsg_len < CMSG_LEN(0))
		return (EINVAL);
	if (cm->cmsg_type != SCM_RIGHTS || cm->cmsg_level != SOL_SOCKET ||
	    !(cm->cmsg_len == control->m_len ||
	    control->m_len == CMSG_ALIGN(cm->cmsg_len)))
		return (EINVAL);
	nfds = (cm->cmsg_len - CMSG_ALIGN(sizeof(*cm))) / sizeof (int);

	if (unp_rights + nfds > maxfiles / 10)
		return (EMFILE);

	/* Make sure we have room for the struct file pointers */
morespace:
	neededspace = CMSG_SPACE(nfds * sizeof(struct fdpass)) -
	    control->m_len;
	if (neededspace > m_trailingspace(control)) {
		char *tmp;
		/* if we already have a cluster, the message is just too big */
		if (control->m_flags & M_EXT)
			return (E2BIG);

		/* copy cmsg data temporarily out of the mbuf */
		tmp = malloc(control->m_len, M_TEMP, M_WAITOK);
		memcpy(tmp, mtod(control, caddr_t), control->m_len);

		/* allocate a cluster and try again */
		MCLGET(control, M_WAIT);
		if ((control->m_flags & M_EXT) == 0) {
			free(tmp, M_TEMP, control->m_len);
			return (ENOBUFS);       /* allocation failed */
		}

		/* copy the data back into the cluster */
		cm = mtod(control, struct cmsghdr *);
		memcpy(cm, tmp, control->m_len);
		free(tmp, M_TEMP, control->m_len);
		goto morespace;
	}

	/* adjust message & mbuf to note amount of space actually used. */
	cm->cmsg_len = CMSG_LEN(nfds * sizeof(struct fdpass));
	control->m_len = CMSG_SPACE(nfds * sizeof(struct fdpass));

	ip = ((int *)CMSG_DATA(cm)) + nfds - 1;
	rp = ((struct fdpass *)CMSG_DATA(cm)) + nfds - 1;
	fdplock(fdp);
	for (i = 0; i < nfds; i++) {
		memcpy(&fd, ip, sizeof fd);
		ip--;
		if ((fp = fd_getfile(fdp, fd)) == NULL) {
			error = EBADF;
			goto fail;
		}
		if (fp->f_count >= FDUP_MAX_COUNT) {
			error = EDEADLK;
			goto fail;
		}
		error = pledge_sendfd(p, fp);
		if (error)
			goto fail;

		/* kqueue descriptors cannot be copied */
		if (fp->f_type == DTYPE_KQUEUE) {
			error = EINVAL;
			goto fail;
		}
		rp->fp = fp;
		rp->flags = fdp->fd_ofileflags[fd] & UF_PLEDGED;
		rp--;
		if ((unp = fptounp(fp)) != NULL) {
			unp->unp_file = fp;
			unp->unp_msgcount++;
		}
		unp_rights++;
	}
	fdpunlock(fdp);
	return (0);
fail:
	fdpunlock(fdp);
	if (fp != NULL)
		FRELE(fp, p);
	/* Back out what we just did. */
	for ( ; i > 0; i--) {
		rp++;
		fp = rp->fp;
		if ((unp = fptounp(fp)) != NULL)
			unp->unp_msgcount--;
		FRELE(fp, p);
		unp_rights--;
	}

	return (error);
}

void
unp_gc(void *arg __unused)
{
	struct unp_deferral *defer;
	struct file *fp;
	struct socket *so;
	struct unpcb *unp;
	int nunref, i;

	rw_enter_write(&unp_lock);

	if (unp_gcing)
		goto unlock;
	unp_gcing = 1;

	/* close any fds on the deferred list */
	while ((defer = SLIST_FIRST(&unp_deferred)) != NULL) {
		SLIST_REMOVE_HEAD(&unp_deferred, ud_link);
		for (i = 0; i < defer->ud_n; i++) {
			fp = defer->ud_fp[i].fp;
			if (fp == NULL)
				continue;
			 /* closef() expects a refcount of 2 */
			FREF(fp);
			if ((unp = fptounp(fp)) != NULL)
				unp->unp_msgcount--;
			unp_rights--;
			rw_exit_write(&unp_lock);
			(void) closef(fp, NULL);
			rw_enter_write(&unp_lock);
		}
		free(defer, M_TEMP, sizeof(*defer) +
		    sizeof(struct fdpass) * defer->ud_n);
	}

	unp_defer = 0;
	LIST_FOREACH(unp, &unp_head, unp_link)
		unp->unp_flags &= ~(UNP_GCMARK | UNP_GCDEFER | UNP_GCDEAD);
	do {
		nunref = 0;
		LIST_FOREACH(unp, &unp_head, unp_link) {
			fp = unp->unp_file;
			if (unp->unp_flags & UNP_GCDEFER) {
				/*
				 * This socket is referenced by another
				 * socket which is known to be live,
				 * so it's certainly live.
				 */
				unp->unp_flags &= ~UNP_GCDEFER;
				unp_defer--;
			} else if (unp->unp_flags & UNP_GCMARK) {
				/* marked as live in previous pass */
				continue;
			} else if (fp == NULL) {
				/* not being passed, so can't be in loop */
			} else if (fp->f_count == 0) {
				/*
				 * Already being closed, let normal close
				 * path take its course
				 */
			} else {
				/*
				 * Unreferenced by other sockets so far,
				 * so if all the references (f_count) are
				 * from passing (unp_msgcount) then this
				 * socket is prospectively dead
				 */
				if (fp->f_count == unp->unp_msgcount) {
					nunref++;
					unp->unp_flags |= UNP_GCDEAD;
					continue;
				}
			}

			/*
			 * This is the first time we've seen this socket on
			 * the mark pass and known it has a live reference,
			 * so mark it, then scan its receive buffer for
			 * sockets and note them as deferred (== referenced,
			 * but not yet marked).
			 */
			unp->unp_flags |= UNP_GCMARK;

			so = unp->unp_socket;
			unp_scan(so->so_rcv.sb_mb, unp_mark);
		}
	} while (unp_defer);

	/*
	 * If there are any unreferenced sockets, then for each dispose
	 * of files in its receive buffer and then close it.
	 */
	if (nunref) {
		LIST_FOREACH(unp, &unp_head, unp_link) {
			if (unp->unp_flags & UNP_GCDEAD)
				unp_scan(unp->unp_socket->so_rcv.sb_mb,
				    unp_discard);
		}
	}
	unp_gcing = 0;
unlock:
	rw_exit_write(&unp_lock);
}

void
unp_dispose(struct mbuf *m)
{

	if (m)
		unp_scan(m, unp_discard);
}

void
unp_scan(struct mbuf *m0, void (*op)(struct fdpass *, int))
{
	struct mbuf *m;
	struct fdpass *rp;
	struct cmsghdr *cm;
	int qfds;

	while (m0) {
		for (m = m0; m; m = m->m_next) {
			if (m->m_type == MT_CONTROL &&
			    m->m_len >= sizeof(*cm)) {
				cm = mtod(m, struct cmsghdr *);
				if (cm->cmsg_level != SOL_SOCKET ||
				    cm->cmsg_type != SCM_RIGHTS)
					continue;
				qfds = (cm->cmsg_len - CMSG_ALIGN(sizeof *cm))
				    / sizeof(struct fdpass);
				if (qfds > 0) {
					rp = (struct fdpass *)CMSG_DATA(cm);
					op(rp, qfds);
				}
				break;		/* XXX, but saves time */
			}
		}
		m0 = m0->m_nextpkt;
	}
}

void
unp_mark(struct fdpass *rp, int nfds)
{
	struct unpcb *unp;
	int i;

	rw_assert_wrlock(&unp_lock);

	for (i = 0; i < nfds; i++) {
		if (rp[i].fp == NULL)
			continue;

		unp = fptounp(rp[i].fp);
		if (unp == NULL)
			continue;

		if (unp->unp_flags & (UNP_GCMARK|UNP_GCDEFER))
			continue;

		unp_defer++;
		unp->unp_flags |= UNP_GCDEFER;
		unp->unp_flags &= ~UNP_GCDEAD;
	}
}

void
unp_discard(struct fdpass *rp, int nfds)
{
	struct unp_deferral *defer;

	rw_assert_wrlock(&unp_lock);

	/* copy the file pointers to a deferral structure */
	defer = malloc(sizeof(*defer) + sizeof(*rp) * nfds, M_TEMP, M_WAITOK);
	defer->ud_n = nfds;
	memcpy(&defer->ud_fp[0], rp, sizeof(*rp) * nfds);
	memset(rp, 0, sizeof(*rp) * nfds);
	SLIST_INSERT_HEAD(&unp_deferred, defer, ud_link);

	task_add(systqmp, &unp_gc_task);
}

int
unp_nam2sun(struct mbuf *nam, struct sockaddr_un **sun, size_t *pathlen)
{
	struct sockaddr *sa = mtod(nam, struct sockaddr *);
	size_t size, len;

	if (nam->m_len < offsetof(struct sockaddr, sa_data))
		return EINVAL;
	if (sa->sa_family != AF_UNIX)
		return EAFNOSUPPORT;
	if (sa->sa_len != nam->m_len)
		return EINVAL;
	if (sa->sa_len > sizeof(struct sockaddr_un))
		return EINVAL;
	*sun = (struct sockaddr_un *)sa;

	/* ensure that sun_path is NUL terminated and fits */
	size = (*sun)->sun_len - offsetof(struct sockaddr_un, sun_path);
	len = strnlen((*sun)->sun_path, size);
	if (len == sizeof((*sun)->sun_path))
		return EINVAL;
	if (len == size) {
		if (m_trailingspace(nam) == 0)
			return EINVAL;
		nam->m_len++;
		(*sun)->sun_len++;
		(*sun)->sun_path[len] = '\0';
	}
	if (pathlen != NULL)
		*pathlen = len;

	return 0;
}
