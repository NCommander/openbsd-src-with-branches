/*	$OpenBSD: hostapd.c,v 1.20 2005/10/07 21:52:40 reyk Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@vantronix.net>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "hostapd.h"
#include "iapp.h"

void	 hostapd_usage(void);
void	 hostapd_udp_init(struct hostapd_config *);
void	 hostapd_sig_handler(int);

struct hostapd_config hostapd_cfg;

extern char *__progname;
char printbuf[BUFSIZ];

void
hostapd_usage(void)
{
	fprintf(stderr, "usage: %s [-dv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(EXIT_FAILURE);
}

void
hostapd_log(u_int level, const char *fmt, ...)
{
	va_list ap;

	if (level > hostapd_cfg.c_verbose)
		return;

	va_start(ap, fmt);
	if (hostapd_cfg.c_debug) {
		vfprintf(stderr, fmt, ap);
		fflush(stderr);
	} else
		vsyslog(LOG_INFO, fmt, ap);
	va_end(ap);
}

void
hostapd_printf(const char *fmt, ...)
{
	char newfmt[BUFSIZ];
	va_list ap;
	size_t n;

	if (fmt == NULL) {
 flush:
		hostapd_log(HOSTAPD_LOG, "%s", printbuf);
		bzero(printbuf, sizeof(printbuf));
		return;
	}

	va_start(ap, fmt);
	bzero(newfmt, sizeof(newfmt));
	if ((n = strlcpy(newfmt, printbuf, sizeof(newfmt))) >= sizeof(newfmt))
		goto flush;
	if (strlcpy(newfmt + n, fmt, sizeof(newfmt) - n) >= sizeof(newfmt) - n)
		goto flush;
	if (vsnprintf(printbuf, sizeof(printbuf), newfmt, ap) == -1)
		goto flush;
	va_end(ap);

	if (fmt[0] == '\n')
		goto flush;
}

void
hostapd_fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (hostapd_cfg.c_debug) {
		vfprintf(stderr, fmt, ap);
		fflush(stderr);
	} else
		vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);

	hostapd_cleanup(&hostapd_cfg);
	exit(EXIT_FAILURE);
}

int
hostapd_check_file_secrecy(int fd, const char *fname)
{
	struct stat st;

	if (fstat(fd, &st)) {
		hostapd_log(HOSTAPD_LOG,
		    "cannot stat %s\n", fname);
		return (-1);
	}

	if (st.st_uid != 0 && st.st_uid != getuid()) {
		hostapd_log(HOSTAPD_LOG,
		    "%s: owner not root or current user\n", fname);
		return (-1);
	}

	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		hostapd_log(HOSTAPD_LOG,
		    "%s: group/world readable/writeable\n", fname);
		return (-1);
	}

	return (0);
}

int
hostapd_bpf_open(u_int flags)
{
	u_int i;
	int fd = -1;
	char *dev;
	struct bpf_version bpv;

	/*
	 * Try to open the next available BPF device
	 */
	for (i = 0; i < 255; i++) {
		if (asprintf(&dev, "/dev/bpf%u", i) == -1)
			hostapd_fatal("failed to allocate buffer\n");

		if ((fd = open(dev, flags)) != -1) {
			free(dev);
			break;
		}

		free(dev);
	}

	if (fd == -1)
		hostapd_fatal("unable to open BPF device\n");

	/*
	 * Get and validate the BPF version
	 */

	if (ioctl(fd, BIOCVERSION, &bpv) == -1)
		hostapd_fatal("failed to get BPF version: %s\n",
		    strerror(errno));

	if (bpv.bv_major != BPF_MAJOR_VERSION ||
	    bpv.bv_minor < BPF_MINOR_VERSION)
		hostapd_fatal("invalid BPF version\n");

	return (fd);
}

void
hostapd_udp_init(struct hostapd_config *cfg)
{
	struct ifreq ifr;
	struct sockaddr_in *addr, baddr;
	struct ip_mreq mreq;
	int brd = 1;

	bzero(&ifr, sizeof(ifr));

	/*
	 * Open a listening UDP socket
	 */

	if ((cfg->c_iapp_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		hostapd_fatal("unable to open udp socket\n");

	cfg->c_flags |= HOSTAPD_CFG_F_UDP;

	strlcpy(ifr.ifr_name, cfg->c_iapp_iface, sizeof(ifr.ifr_name));

	if (ioctl(cfg->c_iapp_udp, SIOCGIFADDR, &ifr) == -1)
		hostapd_fatal("UDP ioctl %s on \"%s\" failed: %s\n",
		    "SIOCGIFADDR", ifr.ifr_name, strerror(errno));

	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	cfg->c_iapp_addr.sin_family = AF_INET;
	cfg->c_iapp_addr.sin_addr.s_addr = addr->sin_addr.s_addr;
	cfg->c_iapp_addr.sin_port = htons(IAPP_PORT);

	if (ioctl(cfg->c_iapp_udp, SIOCGIFBRDADDR, &ifr) == -1)
		hostapd_fatal("UDP ioctl %s on \"%s\" failed: %s\n",
		    "SIOCGIFBRDADDR", ifr.ifr_name, strerror(errno));

	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	cfg->c_iapp_broadcast.sin_family = AF_INET;
	cfg->c_iapp_broadcast.sin_addr.s_addr = addr->sin_addr.s_addr;
	cfg->c_iapp_broadcast.sin_port = htons(IAPP_PORT);

	baddr.sin_family = AF_INET;
	baddr.sin_addr.s_addr = htonl(INADDR_ANY);
	baddr.sin_port = htons(IAPP_PORT);

	if (bind(cfg->c_iapp_udp, (struct sockaddr *)&baddr,
	    sizeof(baddr)) == -1)
		hostapd_fatal("failed to bind UDP socket: %s\n",
		    strerror(errno));

	/*
	 * The revised 802.11F standard requires IAPP messages to be
	 * sent via multicast to the group 224.0.1.178. Nevertheless,
	 * some implementations still use broadcasts for IAPP
	 * messages.
	 */
	if (cfg->c_flags & HOSTAPD_CFG_F_BRDCAST) {
		/*
		 * Enable broadcast
		 */

		hostapd_log(HOSTAPD_LOG_DEBUG, "using broadcast mode\n");

		if (setsockopt(cfg->c_iapp_udp, SOL_SOCKET, SO_BROADCAST,
		    &brd, sizeof(brd)) == -1)
			hostapd_fatal("failed to enable broadcast on socket\n");
	} else {
		/*
		 * Enable multicast
		 */

		hostapd_log(HOSTAPD_LOG_DEBUG, "using multicast mode\n");

		bzero(&mreq, sizeof(mreq));

		cfg->c_iapp_multicast.sin_family = AF_INET;
		cfg->c_iapp_multicast.sin_addr.s_addr =
		    inet_addr(IAPP_MCASTADDR);
		cfg->c_iapp_multicast.sin_port = htons(IAPP_PORT);

		mreq.imr_multiaddr.s_addr =
		    cfg->c_iapp_multicast.sin_addr.s_addr;
		mreq.imr_interface.s_addr =
		    cfg->c_iapp_addr.sin_addr.s_addr;

		if (setsockopt(cfg->c_iapp_udp, IPPROTO_IP,
		    IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
			hostapd_fatal("failed to add multicast membership to "
			    "%s: %s\n", IAPP_MCASTADDR, strerror(errno));
	}
}

void
hostapd_sig_handler(int sig)
{
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	switch (sig) {
	case SIGALRM:
	case SIGTERM:
	case SIGQUIT:
	case SIGINT:
		event_loopexit(&tv);
	}
}

void
hostapd_cleanup(struct hostapd_config *cfg)
{
	struct ip_mreq mreq;
	struct hostapd_table *table;
	struct hostapd_entry *entry;

	if (cfg->c_flags & HOSTAPD_CFG_F_PRIV &&
	    (cfg->c_flags & HOSTAPD_CFG_F_BRDCAST) == 0 &&
	    cfg->c_apme_n == 0) {
		/*
		 * Disable multicast and let the kernel unsubscribe
		 * from the multicast group.
		 */

		bzero(&mreq, sizeof(mreq));

		mreq.imr_multiaddr.s_addr =
		    inet_addr(IAPP_MCASTADDR);
		mreq.imr_interface.s_addr =
		    cfg->c_iapp_addr.sin_addr.s_addr;

		if (setsockopt(cfg->c_iapp_udp, IPPROTO_IP,
		    IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
			hostapd_log(HOSTAPD_LOG, "failed to remove multicast"
			    " membership to %s: %s\n",
			    IAPP_MCASTADDR, strerror(errno));
	}

	if ((cfg->c_flags & HOSTAPD_CFG_F_PRIV) == 0 &&
	    cfg->c_flags & HOSTAPD_CFG_F_APME) {
		/* Shutdown the Host AP protocol handler */
		hostapd_iapp_term(&hostapd_cfg);
	}

	/* Cleanup tables */
	while ((table = TAILQ_FIRST(&cfg->c_tables)) != NULL) {
		while ((entry = RB_MIN(hostapd_tree, &table->t_tree)) != NULL) {
			RB_REMOVE(hostapd_tree, &table->t_tree, entry);
			free(entry);
		}
		while ((entry = TAILQ_FIRST(&table->t_mask_head)) != NULL) {
			TAILQ_REMOVE(&table->t_mask_head, entry, e_entries);
			free(entry);
		}
		TAILQ_REMOVE(&cfg->c_tables, table, t_entries);
		free(table);
	}

	hostapd_log(HOSTAPD_LOG_VERBOSE, "bye!\n");
}

int
main(int argc, char *argv[])
{
	struct hostapd_config *cfg = &hostapd_cfg;
	char *iapp_iface = NULL, *hostap_iface = NULL, *config = NULL;
	u_int debug = 0;
	int ch;

	/* Set startup logging */
	cfg->c_debug = 1;

	/*
	 * Get and parse command line options
	 */
	while ((ch = getopt(argc, argv, "f:D:dv")) != -1) {
		switch (ch) {
		case 'f':
			config = optarg;
			break;
		case 'D':
			if (hostapd_parse_symset(optarg) < 0)
				hostapd_fatal("could not parse macro "
				    "definition %s\n", optarg);
			break;
		case 'd':
			debug++;
			break;
		case 'v':
			cfg->c_verbose++;
			break;
		default:
			hostapd_usage();
		}
	}

	if (config == NULL)
		strlcpy(cfg->c_config, HOSTAPD_CONFIG, sizeof(cfg->c_config));
	else
		strlcpy(cfg->c_config, config, sizeof(cfg->c_config));

	if (iapp_iface != NULL)
		strlcpy(cfg->c_iapp_iface, iapp_iface,
		    sizeof(cfg->c_iapp_iface));

	if (hostap_iface != NULL)
		strlcpy(cfg->c_apme_iface, hostap_iface,
		    sizeof(cfg->c_apme_iface));

	if (geteuid())
		hostapd_fatal("need root privileges\n");

	/* Parse the configuration file */
	if (hostapd_parse_file(cfg) != 0)
		hostapd_fatal("invalid configuration in %s\n", cfg->c_config);

	if ((cfg->c_flags & HOSTAPD_CFG_F_IAPP) == 0)
		hostapd_fatal("IAPP interface not specified\n");

	if ((cfg->c_flags & HOSTAPD_CFG_F_APME) == 0)
		strlcpy(cfg->c_apme_iface, "<none>", sizeof(cfg->c_apme_iface));

	if (cfg->c_apme_dlt == 0)
		cfg->c_apme_dlt = HOSTAPD_DLT;

	/*
	 * Setup the hostapd handlers
	 */
	hostapd_udp_init(cfg);
	hostapd_llc_init(cfg);

	/*
	 * Set runtime logging and detach as daemon
	 */
	if ((cfg->c_debug = debug) == 0) {
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);
		daemon(0, 0);
	}

	if (cfg->c_flags & HOSTAPD_CFG_F_APME)
		hostapd_apme_init(cfg);
	else
		hostapd_log(HOSTAPD_LOG, "%s/%s: running without a Host AP\n",
		    cfg->c_apme_iface, cfg->c_iapp_iface);

	/* Drop all privileges in an unprivileged child process */
	hostapd_priv_init(cfg);

	setproctitle("Host AP: %s, IAPP: %s",
	    cfg->c_apme_iface, cfg->c_iapp_iface);

	/*
	 * Unprivileged child process
	 */

	event_init();

	/*
	 * Set signal handlers
	 */
	signal(SIGALRM, hostapd_sig_handler);
	signal(SIGTERM, hostapd_sig_handler);
	signal(SIGQUIT, hostapd_sig_handler);
	signal(SIGINT, hostapd_sig_handler);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	/* Initialize the IAPP protocol handler */
	hostapd_iapp_init(cfg);

	/*
	 * Schedule the Host AP listener
	 */
	if (cfg->c_flags & HOSTAPD_CFG_F_APME) {
		event_set(&cfg->c_apme_ev, cfg->c_apme_raw,
		    EV_READ | EV_PERSIST, hostapd_apme_input, cfg);
		event_add(&cfg->c_apme_ev, NULL);
	}

	/*
	 * Schedule the IAPP listener
	 */
	event_set(&cfg->c_iapp_udp_ev, cfg->c_iapp_udp, EV_READ | EV_PERSIST,
	    hostapd_iapp_input, cfg);
	event_add(&cfg->c_iapp_udp_ev, NULL);

	hostapd_log(HOSTAPD_LOG, "%s/%s: starting hostapd with pid %u\n",
	    cfg->c_apme_iface, cfg->c_iapp_iface, getpid());

	/* Run event loop */
	event_dispatch();

	/* Executed after the event loop has been terminated */
	hostapd_cleanup(cfg);
	return (EXIT_SUCCESS);
}

void
hostapd_randval(u_int8_t *buf, const u_int len)
{
	u_int32_t data = 0;
	u_int i;

	for (i = 0; i < len; i++) {
		if ((i % sizeof(data)) == 0)
			data = arc4random();
		buf[i] = data & 0xff;
		data >>= 8;
	}
}

struct hostapd_table *
hostapd_table_add(struct hostapd_config *cfg, const char *name)
{
	struct hostapd_table *table;

	if (hostapd_table_lookup(cfg, name) != NULL)
		return (NULL);
	if ((table = (struct hostapd_table *)
	    calloc(1, sizeof(struct hostapd_table))) == NULL)
		return (NULL);

	strlcpy(table->t_name, name, sizeof(table->t_name));
	RB_INIT(&table->t_tree);
	TAILQ_INIT(&table->t_mask_head);
	TAILQ_INSERT_TAIL(&cfg->c_tables, table, t_entries);

	return (table);
}

struct hostapd_table *
hostapd_table_lookup(struct hostapd_config *cfg, const char *name)
{
	struct hostapd_table *table;

	TAILQ_FOREACH(table, &cfg->c_tables, t_entries) {
		if (strcmp(name, table->t_name) == 0)
			return (table);
	}

	return (NULL);
}

struct hostapd_entry *
hostapd_entry_add(struct hostapd_table *table, u_int8_t *lladdr)
{
	struct hostapd_entry *entry;

	if (hostapd_entry_lookup(table, lladdr) != NULL)
		return (NULL);

	if ((entry = (struct hostapd_entry *)
	    calloc(1, sizeof(struct hostapd_entry))) == NULL)
		return (NULL);

	bcopy(lladdr, entry->e_lladdr, IEEE80211_ADDR_LEN);
	RB_INSERT(hostapd_tree, &table->t_tree, entry);

	return (entry);
}

struct hostapd_entry *
hostapd_entry_lookup(struct hostapd_table *table, u_int8_t *lladdr)
{
	struct hostapd_entry *entry, key;

	bcopy(lladdr, key.e_lladdr, IEEE80211_ADDR_LEN);
	if ((entry = RB_FIND(hostapd_tree, &table->t_tree, &key)) != NULL)
		return (entry);

	/* Masked entries can't be handled by the red-black tree */
	TAILQ_FOREACH(entry, &table->t_mask_head, e_entries) {
		if (HOSTAPD_ENTRY_MASK_MATCH(entry, lladdr))
			return (entry);
	}

	return (NULL);
}

void
hostapd_entry_update(struct hostapd_table *table, struct hostapd_entry *entry)
{
	RB_REMOVE(hostapd_tree, &table->t_tree, entry);

	/* Apply mask to entry */
	if (entry->e_flags & HOSTAPD_ENTRY_F_MASK) {
		HOSTAPD_ENTRY_MASK_ADD(entry->e_lladdr, entry->e_mask);
		TAILQ_INSERT_TAIL(&table->t_mask_head, entry, e_entries);
	} else {
		RB_INSERT(hostapd_tree, &table->t_tree, entry);
	}
}

int
hostapd_entry_cmp(struct hostapd_entry *a, struct hostapd_entry *b)
{
	return (memcmp(a->e_lladdr, b->e_lladdr, IEEE80211_ADDR_LEN));
}

RB_GENERATE(hostapd_tree, hostapd_entry, e_nodes, hostapd_entry_cmp);

