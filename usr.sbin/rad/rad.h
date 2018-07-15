/*	$OpenBSD: rad.h,v 1.9 2018/07/13 08:32:10 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
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

#define CONF_FILE		"/etc/rad.conf"
#define	RAD_SOCKET		"/var/run/rad.sock"
#define RAD_USER		"_rad"

#define OPT_VERBOSE	0x00000001
#define OPT_VERBOSE2	0x00000002
#define OPT_NOACTION	0x00000004


enum {
	PROC_MAIN,
	PROC_ENGINE,
	PROC_FRONTEND
} rad_process;

static const char * const log_procnames[] = {
	"main",
	"engine",
	"frontend",
};

struct imsgev {
	struct imsgbuf	 ibuf;
	void		(*handler)(int, short, void *);
	struct event	 ev;
	short		 events;
};

enum imsg_type {
	IMSG_NONE,
	IMSG_CTL_LOG_VERBOSE,
	IMSG_CTL_RELOAD,
	IMSG_RECONF_CONF,
	IMSG_RECONF_RA_IFACE,
	IMSG_RECONF_RA_AUTOPREFIX,
	IMSG_RECONF_RA_PREFIX,
	IMSG_RECONF_END,
	IMSG_ICMP6SOCK,
	IMSG_ROUTESOCK,
	IMSG_CONTROLFD,
	IMSG_STARTUP,
	IMSG_STARTUP_DONE,
	IMSG_RA_RS,
	IMSG_SEND_RA,
	IMSG_UPDATE_IF,
	IMSG_REMOVE_IF,
	IMSG_SHUTDOWN,
	IMSG_SOCKET_IPC
};

/* RFC 4861 Section 4.2 */
struct ra_options_conf {
	int		dfr;			/* is default router? */
	int		cur_hl;			/* current hop limit */
	int		m_flag;			/* managed address conf flag */
	int		o_flag;			/* other conf flag */
	int		router_lifetime;	/* default router lifetime */
	uint32_t	reachable_time;
	uint32_t	retrans_timer;
};

/* RFC 4861 Section 4.6.2 */
struct ra_prefix_conf {
	SIMPLEQ_ENTRY(ra_prefix_conf)	 entry;
	struct in6_addr			 prefix;	/* prefix */
	int				 prefixlen;	/* prefix length */
	uint32_t			 vltime;	/* valid lifetime */
	uint32_t			 pltime;	/* prefered lifetime */
	int				 lflag;		/* on-link flag*/
	int				 aflag;		/* autonom. addr flag */
};

struct ra_iface_conf {
	SIMPLEQ_ENTRY(ra_iface_conf)	 entry;
	struct ra_options_conf		 ra_options;
	struct ra_prefix_conf		*autoprefix;
	SIMPLEQ_HEAD(ra_prefix_conf_head,
	    ra_prefix_conf)		 ra_prefix_list;
	char				 name[IF_NAMESIZE];
};

struct rad_conf {
	struct ra_options_conf				 ra_options;
	SIMPLEQ_HEAD(ra_iface_conf_head, ra_iface_conf)	 ra_iface_list;
};

struct imsg_ra_rs {
	uint32_t		if_index;
	struct sockaddr_in6	from;
	ssize_t			len;
	uint8_t			packet[1500];
};

struct imsg_send_ra {
	uint32_t		if_index;
	struct sockaddr_in6	to;
};

extern uint32_t	 cmd_opts;

/* rad.c */
void	main_imsg_compose_frontend(int, pid_t, void *, uint16_t);
void	main_imsg_compose_frontend_fd(int, pid_t, int);

void	main_imsg_compose_engine(int, pid_t, void *, uint16_t);
void	merge_config(struct rad_conf *, struct rad_conf *);
void	imsg_event_add(struct imsgev *);
int	imsg_compose_event(struct imsgev *, uint16_t, uint32_t, pid_t,
	    int, void *, uint16_t);

struct rad_conf	*config_new_empty(void);
void		 config_clear(struct rad_conf *);

void	mask_prefix(struct in6_addr*, int len);
const char	*sin6_to_str(struct sockaddr_in6 *);
const char	*in6_to_str(struct in6_addr *);

/* printconf.c */
void	print_config(struct rad_conf *);

/* parse.y */
struct rad_conf	*parse_config(char *);
int			 cmdline_symset(char *);
