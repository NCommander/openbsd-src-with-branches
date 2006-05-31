/*
 * Copyright (c) 2006 Chris Kuethe <ckuethe@openbsd.org>
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpd.h"

extern struct passwd *pw;
extern int pfpipe[2];
extern char *abandoned_tab;
extern char *changedmac_tab;

__dead void
pftable_handler()
{
	struct pf_cmd cmd;
	struct pollfd pfd[1];
	int l, r, fd, nfds;

	if ((fd = open(_PATH_DEV_PF, O_RDWR|O_NOFOLLOW, 0660)) == -1)
		error("can't open pf device: %m");
	if (chroot(_PATH_VAREMPTY) == -1)
		error("chroot %s: %m", _PATH_VAREMPTY);
	if (chdir("/") == -1)
		error("chdir(\"/\"): %m");
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		error("can't drop privileges: %m");

	setproctitle("pf table handler");
	l = sizeof(struct pf_cmd);

	for(;;){
		pfd[0].fd = fd;
		pfd[0].events = POLLIN;
		if ((nfds = poll(pfd, 1, -1)) == -1)
			if (errno != EINTR)
				error("poll: %m");

		if (nfds > 0 && (pfd[0].revents & POLLIN)){
			bzero(&cmd, l);
			r = atomicio(read, pfpipe[0], &cmd, l);

			if (r != l)
				error("pf pipe error: %m");

			switch (cmd.type){
			case 'A':
				pf_change_table(fd, 1, cmd.ip, abandoned_tab);
				pf_kill_state(fd, cmd.ip);
				break;
			case 'C':
				pf_change_table(fd, 0, cmd.ip, abandoned_tab);
				pf_change_table(fd, 0, cmd.ip, changedmac_tab);
				break;
			case 'L':
				pf_change_table(fd, 0, cmd.ip, abandoned_tab);
				break;
			default:
				break;
			}
		}
	}
	/* not reached */
	exit(1);
}

/* inspired by ("stolen") from usr.sbin/authpf/authpf.c */
void
pf_change_table(int fd, int op, struct in_addr ip, char *table)
{
	struct pfioc_table	io;
	struct pfr_addr		addr;

	bzero(&io, sizeof(io));
	strlcpy(io.pfrio_table.pfrt_name, table,
	    sizeof(io.pfrio_table.pfrt_name));
	io.pfrio_buffer = &addr;
	io.pfrio_esize = sizeof(addr);
	io.pfrio_size = 1;

	bzero(&addr, sizeof(addr));
	bcopy(&ip, &addr.pfra_ip4addr, 4);
	addr.pfra_af = AF_INET;
	addr.pfra_net = 32;

	if (ioctl(fd, op ? DIOCRADDADDRS : DIOCRDELADDRS, &io) &&
	    errno != ESRCH) {
		warning( "DIOCR%sADDRS on table %s: %s",
		    op ? "ADD" : "DEL", table, strerror(errno));
	}
}

void
pf_kill_state(int fd, struct in_addr ip)
{
	struct pfioc_state_kill	psk;
	struct pf_addr target;

	bzero(&psk, sizeof(psk));
	bzero(&target, sizeof(target));

	bcopy(&ip.s_addr, &target.v4, 4);
	psk.psk_af = AF_INET;

	/* Kill all states from target */
	bcopy(&target, &psk.psk_src.addr.v.a.addr,
	    sizeof(psk.psk_src.addr.v.a.addr));
	memset(&psk.psk_src.addr.v.a.mask, 0xff,
	    sizeof(psk.psk_src.addr.v.a.mask));
	if (ioctl(fd, DIOCKILLSTATES, &psk)){
		warning("DIOCKILLSTATES failed (%s)", strerror(errno));
	}

	/* Kill all states to target */
	bzero(&psk.psk_src, sizeof(psk.psk_src));
	bcopy(&target, &psk.psk_dst.addr.v.a.addr,
	    sizeof(psk.psk_dst.addr.v.a.addr));
	memset(&psk.psk_dst.addr.v.a.mask, 0xff,
	    sizeof(psk.psk_dst.addr.v.a.mask));
	if (ioctl(fd, DIOCKILLSTATES, &psk)){
		warning("DIOCKILLSTATES failed (%s)", strerror(errno));
	}
}

/* inspired by ("stolen") from usr.bin/ssh/atomicio.c */
size_t
atomicio(ssize_t (*f) (int, void *, size_t), int fd, void *_s, size_t n)
{
	char *s = _s;
	size_t pos = 0;
	ssize_t res;

	while (n > pos) {
		res = (f) (fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return 0;
		case 0:
			errno = EPIPE;
			return pos;
		default:
			pos += (size_t)res;
		}
	}
	return (pos);
}
