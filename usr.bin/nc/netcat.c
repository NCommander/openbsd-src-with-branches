/* $OpenBSD: netcat.c,v 1.21 2001/06/25 22:17:35 ericj Exp $ */
/*
 * Copyright (c) 2001 Eric Jackson <ericj@monkey.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * Re-written nc(1) for OpenBSD. Only code shared with previous version
 * was the code for telnet emulation. Original implementation by
 * *Hobbit* <hobbit@avian.org>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/telnet.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Command Line Options */
int	iflag;					/* Interval Flag */
int	kflag;					/* More than one connect */
int	lflag;					/* Bind to local port */
int	nflag;					/* Dont do name lookup */
char   *pflag;					/* Localport flag */
int	rflag;					/* Random ports flag */
char   *sflag;					/* Source Address */
int	tflag;					/* Telnet Emulation */
int	uflag;					/* UDP - Default to TCP */
int	vflag;					/* Verbosity */
int	zflag;					/* Port Scan Flag */

int timeout;
int family = AF_UNSPEC;
char *portlist[65535];

void atelnet __P((int, unsigned char *, unsigned int));
void build_ports __P((char *));
void help __P((void));
int local_listen __P((char *, char *, struct addrinfo));
void readwrite __P((int));
int remote_connect __P((char *, char *, struct addrinfo));
int udptest __P((int));
void usage __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, s, ret = 1;
	char *host, *uport;
	struct addrinfo hints;
	struct servent *sv = 0;
	socklen_t len;
	struct sockaddr *cliaddr;

	while ((ch = getopt(argc, argv, "46hi:klnp:rs:tuvw:z")) != -1) {
		switch (ch) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'h':
			help();
			break;
		case 'i':
			iflag = atoi(optarg);
			break;
		case 'k':
			kflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = optarg;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = optarg;
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'w':
			timeout = atoi(optarg);
			break;
		case 'z':
			zflag = 1;
			break;
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	/* Cruft to make sure options are clean, and used properly. */
	if (argv[0] && !argv[1]) {
		if  (!lflag)
			usage(1);
		uport = argv[0];
		host = NULL;
	} else if (argv[0] && argv[1]) {
		host = argv[0];
		uport = argv[1];
	} else
		usage(1);

	if (lflag && sflag)
		errx(1, "cannot use -s and -l");
	if (lflag && pflag)
		errx(1, "cannot use -p and -l");
	if (lflag && zflag)
		errx(1, "cannot use -p and -l");
	if (!lflag && kflag)
		errx(1, "must use -k with -l");

	/* Initialize addrinfo structure */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = family;
	hints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
	hints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
	if (nflag)
		hints.ai_flags |= AI_NUMERICHOST;

	if (lflag) {
		int connfd;
	
		if ((s = local_listen(host, uport, hints)) < 0)
			errx(1, NULL);

		ret = 0;
		/* Allow only one connection at a time, but stay alive */
		for (;;) {
			/*
			 * For UDP, we will use recvfrom() initially
			 * to wait for a caller, then use the regular
			 * functions to talk to the caller.
			 */
			if (uflag) {
				int ret;
				char buf[1024];
				char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
				struct sockaddr_storage z;

				len = sizeof(z);
				ret = recvfrom(s, buf, sizeof(buf), MSG_PEEK,
					(struct sockaddr *)&z, &len);
				if (ret < 0)
					errx(1, "%s", strerror(errno));

				ret = connect(s, (struct sockaddr *)&z,
					len);
				if (ret < 0)
					errx(1, "%s", strerror(errno));

				connfd = s;
			} else {
				connfd = accept(s, (struct sockaddr *)&cliaddr,
									&len);
			}

			readwrite(connfd);
			close(connfd);
			if (!kflag)
				break;
		}
	} else {
		int i = 0;

		/* construct the portlist[] array */
		build_ports(uport);

		/* Cycle through portlist, connecting to each port */
		for (i = 0; portlist[i] != NULL; i++) {
			
			if (s)
				close(s);
	
			if ((s = remote_connect(host, portlist[i], hints)) < 0)
				continue;

			ret = 0;
			if (vflag || zflag) {
				/* For UDP, make sure we are connected */
				if (uflag) {
					if ((udptest(s)) == -1) {
						ret = 1;
						continue;
					}
				}

				/* Don't lookup port if -n */
				if (nflag)
					sv = NULL;
				else {
					sv = getservbyport(
						ntohs(atoi(portlist[i])),
						uflag ? "udp" : "tcp");
				}
				
				printf("Connection to %s %s port [%s/%s] succeeded!\n",
					host, portlist[i], uflag ? "udp" : "tcp",
					sv ? sv->s_name : "*");
			}
			if (!zflag)
				readwrite(s);
		}
	}

	if (s)
		close(s);

	exit(ret);
}

/*
 * remote_connect()
 * Return's a socket connected to a remote host. Properly bind's to a local
 * port or source address if needed. Return's -1 on failure.
 */
int
remote_connect(host, port, hints)
	char *host, *port;
	struct addrinfo hints;
{
	struct addrinfo *res, *res0;
	int s, error;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		errx(1, "%s", gai_strerror(error));

	res0 = res;
	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
				res0->ai_protocol)) < 0)
			continue;

		/* Bind to a local port or source address if specified */
		if (sflag || pflag) {
			struct addrinfo ahints, *ares;

			if (!(sflag && pflag)) {
				if (!sflag)
					sflag = NULL;
				else
					pflag = NULL;
			}

			memset(&ahints, 0, sizeof(struct addrinfo));
			ahints.ai_family = res0->ai_family;
			ahints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
			ahints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
			if (getaddrinfo(sflag, pflag, &ahints, &ares))
				errx(1, "%s", gai_strerror(error));

			if (bind(s, (struct sockaddr *)ares->ai_addr,
							ares->ai_addrlen) < 0) {
				errx(1, "bind failed: %s", strerror(errno));
				freeaddrinfo(ares);
				continue;
			}
			freeaddrinfo(ares);
		}

		if (connect(s, res0->ai_addr, res0->ai_addrlen) == 0)
			break;
		
		close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	freeaddrinfo(res);

	return (s);
}

/*
 * local_listen()
 * Return's a socket listening on a local port, binds to specified source
 * address. Return's -1 on failure.
 */
int
local_listen(host, port, hints)
	char *host, *port;
	struct addrinfo hints;
{
	struct addrinfo *res, *res0;
	int s, ret, x = 1;
	int error;

	/* Allow nodename to be null */
	hints.ai_flags |= AI_PASSIVE;

	/*
	 * In the case of binding to a wildcard address
	 * default to binding to an ipv4 address.
	 */
	if (host == NULL && hints.ai_family == AF_UNSPEC)
		hints.ai_family = AF_INET;

	if ((error = getaddrinfo(host, port, &hints, &res)))
                errx(1, "%s", gai_strerror(error));

	res0 = res;
	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
				res0->ai_protocol)) == 0)
			continue;

		ret = setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &x, sizeof(x));
		if (ret == -1)
			errx(1, NULL);

		if (bind(s, (struct sockaddr *)res0->ai_addr,
						res0->ai_addrlen) == 0)
			break;

		close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	if (!uflag) {
		if (listen(s, 1) < 0)
			errx(1, "%s", strerror(errno));
	}

	freeaddrinfo(res);

	return (s);
}

/*
 * readwrite()
 * Loop that polls on the network file descriptor and stdin.
 */
void
readwrite(nfd)
	int nfd;
{
	struct pollfd *pfd;
	char buf[BUFSIZ];
	int wfd = fileno(stdin), n, ret;
	int lfd = fileno(stdout);

	pfd = malloc(2 * sizeof(struct pollfd));

	/* Setup Network FD */
	pfd[0].fd = nfd;
	pfd[0].events = POLLIN;

	/* Setup STDIN FD */
	pfd[1].fd = wfd;
	pfd[1].events = POLLIN;

	for (;;) {
		if (iflag)
			sleep(iflag);

		if (poll(pfd, 2, timeout) < 0) {
			close(nfd);
			close(wfd);
			free(pfd);
			errx(1, "Polling Error");
		}

		if (pfd[0].revents & POLLIN) {
			if ((n = read(nfd, buf, sizeof(buf))) <= 0) {
				return;
			} else {
				if (tflag)
					atelnet(nfd, buf, n);
				if ((ret = atomicio(write, lfd, buf, n)) != n)
					return;
			}
		}

		if (pfd[1].revents & POLLIN) {
			if ((n = read(wfd, buf, sizeof(buf))) <= 0) {
				return;
			} else
				if((ret = atomicio(write, nfd, buf, n)) != n)
					return;
		}
	}
}

/*
 * Answer anything that looks like telnet negotiation with don't/won't.
 * This doesn't modify any data buffers, update the global output count,
 * or show up in a hexdump -- it just shits into the outgoing stream.
 * Idea and codebase from Mudge@l0pht.com.
 */
void
atelnet(nfd, buf, size)
	int nfd;
	unsigned char *buf;
	unsigned int size;
{
	static unsigned char obuf[4];
	int x, ret;
	unsigned char y;
	unsigned char *p;

	y = 0;
	p = buf;
	x = size;
	while (x > 0) {
		if (*p != IAC)
			goto notiac;
		obuf[0] = IAC;
		p++; x--;
		if ((*p == WILL) || (*p == WONT))
			y = DONT;
		if ((*p == DO) || (*p == DONT))
			y = WONT;
		if (y) {
			obuf[1] = y;
			p++; x--;
			obuf[2] = *p;
			if ((ret = atomicio(write , nfd, obuf, 3)) != 3)
				warnx("Write Error!");
			y = 0;
		}
notiac:
		p++; x--;
	}
}

/*
 * build_ports()
 * Build an array or ports in portlist[], listing each port
 * that we should try to connect too.
 */
void
build_ports(p)
	char *p;
{
	char *n;
	int hi, lo, cp;
	int x = 0;

	if ((n = strchr(p, '-')) != NULL) {
		if (lflag)
			errx(1, "Cannot use -l with multiple ports!");

		*n = '\0';
		n++;

		/* Make sure the ports are in order: lowest->highest */
		hi = atoi(n);
		lo = atoi(p);

		if (lo > hi) {
			cp = hi;
			hi = lo;
			lo = cp;
		}

		/* Load ports sequentially */
		for (cp = lo; cp <= hi; cp++) {
			portlist[x] = malloc(sizeof(65535));
			sprintf(portlist[x], "%d", cp);
			x++;
		}

		/* Randomly swap ports */
		if (rflag) {
			int y;
			char *c;

			for (x = 0; x <= (hi - lo); x++) {
				y = (arc4random() & 0xFFFF) % (hi - lo);
				c = portlist[x];
				portlist[x] = portlist[y];
				portlist[y] = c;
			}
		}
	} else {
		portlist[0] = malloc(sizeof(65535));
		portlist[0] = p;
	}
}

/*
 * udptest()
 * Do a few writes to see if the UDP port is there.
 * XXX - Better way of doing this? Doesn't work for IPv6
 * Also fails after around 100 ports checked.
 */
int
udptest(s)
        int s;
{
	int i, rv, ret;

	for (i=0; i <= 3; i++) {
		if ((rv = write(s, "X", 1)) == 1)
			ret = 1;
		else
			ret = -1;
	}
	return (ret);
}

void
help()
{
	usage(0);
	fprintf(stderr, "\tCommand Summary:\n\
	\t-4		Use IPv4\n\
	\t-6		Use IPv6\n\
	\t-h		This help text\n\
	\t-i secs\t	Delay interval for lines sent, ports scanned\n\
	\t-k		Keep inbound sockets open for multiple connects\n\
	\t-l		Listen mode, for inbound connects\n\
	\t-n		Suppress name/port resolutions\n\
	\t-p		Specify local port for remote connects\n\
	\t-r		Randomize remote ports\n\
	\t-s addr\t	Local source address\n\
	\t-t		Answer TELNET negotiation\n\
	\t-u		UDP mode\n\
	\t-v		Verbose\n\
	\t-w secs\t	Timeout for connects and final net reads\n\
	\t-z		Zero-I/O mode [used for scanning]\n\
	Port numbers can be individual or ranges: lo-hi [inclusive]\n");
	exit(1);
}

void
usage(ret)
	int ret;
{
	fprintf(stderr, "usage: nc [-46hklnrtuvz] [-i interval] [-p source port]\n");
	fprintf(stderr, "\t  [-s ip address] [-w timeout] [hostname] [port[s...]]\n");
	if (ret)
		exit(1);
}
