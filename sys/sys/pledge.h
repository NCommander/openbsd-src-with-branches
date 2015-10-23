/*	$OpenBSD: pledge.h,v 1.9 2015/10/20 18:04:03 deraadt Exp $	*/

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 * Copyright (c) 2015 Theo de Raadt <deraadt@openbsd.org>
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

#ifndef _SYS_PLEDGE_H_
#define _SYS_PLEDGE_H_

#ifdef _KERNEL

#include <sys/cdefs.h>

#define PLEDGE_SELF	0x00000001	/* operate on own pid */
#define PLEDGE_RW		0x00000002	/* basic io operations */
#define PLEDGE_MALLOC	0x00000004	/* enough for malloc */
#define PLEDGE_DNS	0x00000008	/* DNS services */
#define PLEDGE_RPATH	0x00000010	/* allow open for read */
#define PLEDGE_WPATH	0x00000020	/* allow open for write */
#define PLEDGE_TMPPATH	0x00000040	/* for mk*temp() */
#define PLEDGE_INET	0x00000080	/* AF_INET/AF_INET6 sockets */
#define PLEDGE_UNIX	0x00000100	/* AF_UNIX sockets */
#define PLEDGE_ID	0x00000200	/* allow setuid, setgid, etc */
#define PLEDGE_IOCTL	0x00000400	/* Select ioctl */
#define PLEDGE_GETPW	0x00000800	/* YP enables if ypbind.lock */
#define PLEDGE_PROC	0x00001000	/* fork, waitpid, etc */
#define PLEDGE_CPATH	0x00002000	/* allow creat, mkdir, path creations */
#define PLEDGE_FATTR	0x00004000	/* allow explicit file st_* mods */
#define PLEDGE_PROTEXEC	0x00008000	/* allow use of PROT_EXEC */
#define PLEDGE_TTY	0x00010000	/* tty setting */
#define PLEDGE_SENDFD	0x00020000	/* AF_UNIX CMSG fd sending */
#define PLEDGE_RECVFD	0x00040000	/* AF_UNIX CMSG fd receiving */
#define PLEDGE_EXEC	0x00080000	/* execve, child is free of pledge */
#define PLEDGE_ROUTE	0x00100000	/* routing lookups */
#define PLEDGE_MCAST	0x00200000	/* multicast joins */
#define PLEDGE_FLOCK	0x00400000	/* file locking */
#define PLEDGE_PS	0x00800000	/* ps listings */
#define PLEDGE_VMINFO	0x01000000	/* vminfo listings */
#define PLEDGE_SETTIME	0x02000000	/* able to set/adj time/freq */

#define PLEDGE_ABORT	0x08000000	/* SIGABRT instead of SIGKILL */

/* Following flags are set by kernel, as it learns things.
 * Not user settable. Should be moved to a seperate variable */
#define PLEDGE_USERSET	0x0fffffff
#define PLEDGE_YP_ACTIVE	0x10000000	/* YP use detected and allowed */

int	pledge_check(struct proc *, int);
int	pledge_fail(struct proc *, int, int);
int	pledge_namei(struct proc *, char *);
void	pledge_aftersyscall(struct proc *, int, int);

struct mbuf;
int	pledge_sendfd_check(struct proc *p, struct file *);
int	pledge_recvfd_check(struct proc *p, struct file *);
int	pledge_sysctl_check(struct proc *p, int namelen, int *name, void *new);
int	pledge_chown_check(struct proc *p, uid_t, gid_t);
int	pledge_adjtime_check(struct proc *p, const void *v);
int	pledge_sendit_check(struct proc *p, const void *to);
int	pledge_socket_check(struct proc *p, int domain);
int	pledge_sockopt_check(struct proc *p, int level, int optname);
int	pledge_socket_check(struct proc *p, int dns);
int	pledge_ioctl_check(struct proc *p, long com, void *);
int	pledge_flock_check(struct proc *p);
int	pledge_swapctl_check(struct proc *p);

#define PLEDGE_MAXPATHS	8192

struct whitepaths {
	size_t	wl_size;
	int	wl_count;
	int	wl_ref;
	struct whitepath {
		char		*name;
		size_t		len;
	} wl_paths[0];
};
void	pledge_dropwpaths(struct process *);

#endif /* _KERNEL */

#endif /* _SYS_PLEDGE_H_ */
