/*	$OpenBSD: dhcrelay.c,v 1.45 2016/12/07 16:41:17 reyk Exp $ */

/*
 * Copyright (c) 2004 Henning Brauer <henning@cvs.openbsd.org>
 * Copyright (c) 1997, 1998, 1999 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <net/if.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"

void	 usage(void);
int	 rdaemon(int);
void	 relay(struct interface_info *, struct dhcp_packet *, int,
	    unsigned int, struct iaddr, struct hardware *);
char	*print_hw_addr(int, int, unsigned char *);
void	 got_response(struct protocol *);
int	 get_rdomain(char *);

ssize_t	 relay_agentinfo(struct interface_info *, struct dhcp_packet *,
	    size_t, struct in_addr *, struct in_addr *);

time_t cur_time;

int log_perror = 1;

u_int16_t server_port;
u_int16_t client_port;
int log_priority;
struct interface_info *interfaces = NULL;
int server_fd;
int oflag;

struct server_list {
	struct server_list *next;
	struct sockaddr_in to;
	int fd;
} *servers;

int
main(int argc, char *argv[])
{
	int			 ch, devnull = -1, daemonize, opt, rdomain;
	extern char		*__progname;
	struct server_list	*sp = NULL;
	struct passwd		*pw;
	struct sockaddr_in	 laddr;

	daemonize = 1;

	/* Initially, log errors to stderr as well as to syslogd. */
	openlog(__progname, LOG_NDELAY, DHCPD_LOG_FACILITY);
	setlogmask(LOG_UPTO(LOG_INFO));

	while ((ch = getopt(argc, argv, "adi:o")) != -1) {
		switch (ch) {
		case 'd':
			daemonize = 0;
			break;
		case 'i':
			if (interfaces != NULL)
				usage();

			interfaces = get_interface(optarg, got_one);
			break;
		case 'o':
			/* add the relay agent information option */
			oflag++;
			break;

		default:
			usage();
			/* not reached */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	while (argc > 0) {
		struct hostent		*he;
		struct in_addr		 ia, *iap = NULL;

		if (inet_aton(argv[0], &ia))
			iap = &ia;
		else {
			he = gethostbyname(argv[0]);
			if (!he)
				warning("%s: host unknown", argv[0]);
			else
				iap = ((struct in_addr *)he->h_addr_list[0]);
		}
		if (iap) {
			if ((sp = calloc(1, sizeof *sp)) == NULL)
				error("calloc");
			sp->next = servers;
			servers = sp;
			memcpy(&sp->to.sin_addr, iap, sizeof *iap);
		}
		argc--;
		argv++;
	}

	if (daemonize) {
		devnull = open(_PATH_DEVNULL, O_RDWR, 0);
		if (devnull == -1)
			error("open(%s): %m", _PATH_DEVNULL);
	}

	if (interfaces == NULL)
		error("no interface given");

	/* Default DHCP/BOOTP ports. */
	server_port = htons(SERVER_PORT);
	client_port = htons(CLIENT_PORT);

	/* We need at least one server. */
	if (!sp)
		usage();

	rdomain = get_rdomain(interfaces->name);

	/* Enable the relay agent option by default for enc0 */
	if (interfaces->hw_address.htype == HTYPE_IPSEC_TUNNEL)
		oflag++;

	bzero(&laddr, sizeof laddr);
	laddr.sin_len = sizeof laddr;
	laddr.sin_family = AF_INET;
	laddr.sin_port = server_port;
	laddr.sin_addr.s_addr = interfaces->primary_address.s_addr;
	/* Set up the server sockaddrs. */
	for (sp = servers; sp; sp = sp->next) {
		sp->to.sin_port = server_port;
		sp->to.sin_family = AF_INET;
		sp->to.sin_len = sizeof sp->to;
		sp->fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sp->fd == -1)
			error("socket: %m");
		opt = 1;
		if (setsockopt(sp->fd, SOL_SOCKET, SO_REUSEPORT,
		    &opt, sizeof(opt)) == -1)
			error("setsockopt: %m");
		if (setsockopt(sp->fd, SOL_SOCKET, SO_RTABLE, &rdomain,
		    sizeof(rdomain)) == -1)
			error("setsockopt: %m");
		if (bind(sp->fd, (struct sockaddr *)&laddr, sizeof laddr) == -1)
			error("bind: %m");
		if (connect(sp->fd, (struct sockaddr *)&sp->to,
		    sizeof sp->to) == -1)
			error("connect: %m");
		add_protocol("server", sp->fd, got_response, sp);
	}

	/* Socket used to forward packets to the DHCP client */
	if (interfaces->hw_address.htype == HTYPE_IPSEC_TUNNEL) {
		laddr.sin_addr.s_addr = INADDR_ANY;
		server_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (server_fd == -1)
			error("socket: %m");
		opt = 1;
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT,
		    &opt, sizeof(opt)) == -1)
			error("setsockopt: %m");
		if (setsockopt(server_fd, SOL_SOCKET, SO_RTABLE, &rdomain,
		    sizeof(rdomain)) == -1)
			error("setsockopt: %m");
		if (bind(server_fd, (struct sockaddr *)&laddr,
		    sizeof(laddr)) == -1)
			error("bind: %m");
	}

	tzset();

	time(&cur_time);
	bootp_packet_handler = relay;

	if ((pw = getpwnam("_dhcp")) == NULL)
		error("user \"_dhcp\" not found");
	if (chroot(_PATH_VAREMPTY) == -1)
		error("chroot: %m");
	if (chdir("/") == -1)
		error("chdir(\"/\"): %m");
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		error("can't drop privileges: %m");

	if (daemonize) {
		if (rdaemon(devnull) == -1)
			error("rdaemon: %m");
		log_perror = 0;
	}

	if (pledge("stdio route", NULL) == -1)
		error("pledge");

	dispatch();
	/* not reached */

	exit(0);
}

void
relay(struct interface_info *ip, struct dhcp_packet *packet, int length,
    unsigned int from_port, struct iaddr from, struct hardware *hfrom)
{
	struct server_list	*sp;
	struct sockaddr_in	 to;
	struct hardware		 hto;

	if (packet->hlen > sizeof packet->chaddr) {
		note("Discarding packet with invalid hlen.");
		return;
	}

	/* If it's a bootreply, forward it to the client. */
	if (packet->op == BOOTREPLY) {
		bzero(&to, sizeof(to));
		if (!(packet->flags & htons(BOOTP_BROADCAST))) {
			to.sin_addr = packet->yiaddr;
			to.sin_port = client_port;
		} else {
			to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
			to.sin_port = client_port;
		}
		to.sin_family = AF_INET;
		to.sin_len = sizeof to;

		/*
		 * Set up the hardware destination address.  If it's a reply
		 * with the BROADCAST flag set, we should send an L2 broad-
		 * cast as well.
		 */
		if (!(packet->flags & htons(BOOTP_BROADCAST))) {
			hto.hlen = packet->hlen;
			if (hto.hlen > sizeof hto.haddr)
				hto.hlen = sizeof hto.haddr;
			memcpy(hto.haddr, packet->chaddr, hto.hlen);
			hto.htype = packet->htype;
		} else {
			bzero(&hto, sizeof(hto));
		}

		if ((length = relay_agentinfo(interfaces,
		    packet, length, NULL, &to.sin_addr)) == -1) {
			note("ignoring BOOTREPLY with invalid "
			    "relay agent information");
			return;
		}

		/*
		 * VMware PXE "ROMs" confuse the DHCP gateway address
		 * with the IP gateway address. This is a problem if your
		 * DHCP relay is running on something that's not your
		 * network gateway.
		 *
		 * It is purely informational from the relay to the client
		 * so we can safely clear it.
		 */
		packet->giaddr.s_addr = 0x0;

		if (send_packet(interfaces, packet, length,
		    interfaces->primary_address, &to, &hto) != -1)
			debug("forwarded BOOTREPLY for %s to %s",
			    print_hw_addr(packet->htype, packet->hlen,
			    packet->chaddr), inet_ntoa(to.sin_addr));
		return;
	}

	if (ip == NULL) {
		note("ignoring non BOOTREPLY from server");
		return;
	}

	/* If giaddr is set on a BOOTREQUEST, ignore it - it's already
	   been gatewayed. */
	if (packet->giaddr.s_addr) {
		note("ignoring BOOTREQUEST with giaddr of %s",
		    inet_ntoa(packet->giaddr));
		return;
	}

	/* Set the giaddr so the server can figure out what net it's
	   from and so that we can later forward the response to the
	   correct net. */
	packet->giaddr = ip->primary_address;

	if ((length = relay_agentinfo(ip, packet, length,
	    (struct in_addr *)from.iabuf, NULL)) == -1) {
		note("ignoring BOOTREQUEST with invalid "
		    "relay agent information");
		return;
	}

	/* Otherwise, it's a BOOTREQUEST, so forward it to all the
	   servers. */
	for (sp = servers; sp; sp = sp->next) {
		if (send(sp->fd, packet, length, 0) != -1) {
			debug("forwarded BOOTREQUEST for %s to %s",
			    print_hw_addr(packet->htype, packet->hlen,
			    packet->chaddr), inet_ntoa(sp->to.sin_addr));
		}
	}

}

void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-do] -i interface server1 [... serverN]\n",
	    __progname);
	exit(1);
}

int
rdaemon(int devnull)
{
	if (devnull == -1) {
		errno = EBADF;
		return (-1);
	}
	if (fcntl(devnull, F_GETFL) == -1)
		return (-1);

	switch (fork()) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		_exit(0);
	}

	if (setsid() == -1)
		return (-1);

	(void)dup2(devnull, STDIN_FILENO);
	(void)dup2(devnull, STDOUT_FILENO);
	(void)dup2(devnull, STDERR_FILENO);
	if (devnull > 2)
		(void)close(devnull);

	return (0);
}

char *
print_hw_addr(int htype, int hlen, unsigned char *data)
{
	static char	 habuf[49];
	char		*s = habuf;
	int		 i, j, slen = sizeof(habuf);

	if (htype == 0 || hlen == 0) {
bad:
		strlcpy(habuf, "<null>", sizeof habuf);
		return habuf;
	}

	for (i = 0; i < hlen; i++) {
		j = snprintf(s, slen, "%02x", data[i]);
		if (j <= 0 || j >= slen)
			goto bad;
		j = strlen (s);
		s += j;
		slen -= (j + 1);
		*s++ = ':';
	}
	*--s = '\0';
	return habuf;
}

void
got_response(struct protocol *l)
{
	ssize_t result;
	struct iaddr ifrom;
	union {
		/*
		 * Packet input buffer.  Must be as large as largest
		 * possible MTU.
		 */
		unsigned char packbuf[4095];
		struct dhcp_packet packet;
	} u;
	struct server_list *sp = l->local;

	memset(&u, DHO_END, sizeof(u));
	if ((result = recv(l->fd, u.packbuf, sizeof(u), 0)) == -1 &&
	    errno != ECONNREFUSED) {
		/*
		 * Ignore ECONNREFUSED as too many dhcp servers send a bogus
		 * icmp unreach for every request.
		 */
		warning("recv failed for %s: %m",
		    inet_ntoa(sp->to.sin_addr));
		return;
	}
	if (result == -1 && errno == ECONNREFUSED)
		return;

	if (result == 0)
		return;

	if (result < BOOTP_MIN_LEN) {
		note("Discarding packet with invalid size.");
		return;
	}

	if (bootp_packet_handler) {
		ifrom.len = 4;
		memcpy(ifrom.iabuf, &sp->to.sin_addr, ifrom.len);

		(*bootp_packet_handler)(NULL, &u.packet, result,
		    sp->to.sin_port, ifrom, NULL);
	}
}

ssize_t
relay_agentinfo(struct interface_info *info, struct dhcp_packet *packet,
    size_t length, struct in_addr *from, struct in_addr *to)
{
	u_int8_t	*p;
	u_int		 i, j, railen;
	ssize_t		 optlen, maxlen, grow;

	if (!oflag)
		return (length);

	/* Buffer length vs. received packet length */
	maxlen = DHCP_MTU_MAX - DHCP_FIXED_LEN - DHCP_OPTIONS_COOKIE_LEN - 1;
	optlen = length - DHCP_FIXED_NON_UDP - DHCP_OPTIONS_COOKIE_LEN;
	if (maxlen < 1 || optlen < 1)
		return (length);

	if (memcmp(packet->options, DHCP_OPTIONS_COOKIE,
	    DHCP_OPTIONS_COOKIE_LEN) != 0)
		return (length);
	p = packet->options + DHCP_OPTIONS_COOKIE_LEN;

	for (i = 0; i < (u_int)optlen && *p != DHO_END;) {
		if (*p == DHO_PAD)
			j = 1;
		else
			j = p[1] + 2;

		if ((i + j) > (u_int)optlen) {
			warning("truncated dhcp options");
			break;
		}

		/* Revert any other relay agent information */
		if (*p == DHO_RELAY_AGENT_INFORMATION) {
			if (to != NULL) {
				/* Check the relay agent information */
				railen = 8 + sizeof(struct in_addr);
				if (j >= railen &&
				    p[1] == (railen - 2) &&
				    p[2] == RAI_CIRCUIT_ID &&
				    p[3] == 2 &&
				    p[4] == (u_int8_t)(info->index << 8) &&
				    p[5] == (info->index & 0xff) &&
				    p[6] == RAI_REMOTE_ID &&
				    p[7] == sizeof(*to))
					memcpy(to, p + 8, sizeof(*to));

				/* It should be the last option */
				memset(p, 0, j);
				*p = DHO_END;
			} else {
				/* Discard invalid option from a client */
				if (!packet->giaddr.s_addr)
					return (-1);
			}
			return (length);
		}

		p += j;
		i += j;

		if (from != NULL && (*p == DHO_END || (i >= optlen))) {
			j = 8 + sizeof(*from);
			if ((i + j) > (u_int)maxlen) {
				warning("skipping agent information");
				break;
			}

			/* Append the relay agent information if it fits */
			p[0] = DHO_RELAY_AGENT_INFORMATION;
			p[1] = j - 2;
			p[2] = RAI_CIRCUIT_ID;
			p[3] = 2;
			p[4] = info->index << 8;
			p[5] = info->index & 0xff;
			p[6] = RAI_REMOTE_ID;
			p[7] = sizeof(*from);
			memcpy(p + 8, from, sizeof(*from));

			/* Do we need to increase the packet length? */
			grow = j + 1 - (optlen - i);
			if (grow > 0)
				length += grow;
			p += j;

			*p = DHO_END;
			break;
		}
	}

	return (length);
}

int
get_rdomain(char *name)
{
	int rv = 0, s;
	struct  ifreq ifr;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		error("get_rdomain socket: %m");

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFRDOMAIN, (caddr_t)&ifr) != -1)
		rv = ifr.ifr_rdomainid;

	close(s);
	return rv;
}
