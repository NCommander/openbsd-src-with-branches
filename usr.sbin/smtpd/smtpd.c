/*	$OpenBSD: smtpd.c,v 1.179 2012/10/17 16:39:49 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <event.h>
#include <imsg.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static void parent_imsg(struct imsgev *, struct imsg *);
static void usage(void);
static void parent_shutdown(void);
static void parent_send_config(int, short, void *);
static void parent_send_config_listeners(void);
static void parent_send_config_client_certs(void);
static void parent_send_config_ruleset(int);
static void parent_sig_handler(int, short, void *);
static void forkmda(struct imsgev *, uint32_t, struct deliver *);
static int parent_forward_open(char *);
static void fork_peers(void);
static struct child *child_add(pid_t, int, const char *);

static void	offline_scan(int, short, void *);
static int	offline_add(char *);
static void	offline_done(void);
static int	offline_enqueue(char *);

static void	purge_task(int, short, void *);
static void	log_imsg(int, int, struct imsg *);

enum child_type {
	CHILD_DAEMON,
	CHILD_MDA,
	CHILD_ENQUEUE_OFFLINE,
};

struct child {
	pid_t			 pid;
	enum child_type		 type;
	const char		*title;
	int			 mda_out;
	uint32_t		 mda_id;
	char			*path;
	char			*cause;
};

struct offline {
	TAILQ_ENTRY(offline)	 entry;
	char			*path;
};

#define OFFLINE_READMAX		20
#define OFFLINE_QUEUEMAX	5
static size_t			offline_running = 0;
TAILQ_HEAD(, offline)		offline_q;

static struct event		offline_ev;
static struct timeval		offline_timeout;

static pid_t			purge_pid;
static struct timeval		purge_timeout;
static struct event		purge_ev;

extern char	**environ;
void		(*imsg_callback)(struct imsgev *, struct imsg *);

struct smtpd	*env = NULL;

const char	*backend_queue = "fs";
const char	*backend_scheduler = "ramqueue";
const char	*backend_stat = "ram";

static int	 profiling;
static int	 profstat;

struct tree	 children;

static void
parent_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct forward_req	*fwreq;
	struct auth		*auth;
	struct auth_backend	*auth_backend;
	struct child		*c;
	size_t			 len;
	void			*i;
	int			 fd, n;

	if (iev->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_SEND_CONFIG:
			parent_send_config_listeners();
			return;

		case IMSG_PARENT_AUTHENTICATE:
			auth_backend = auth_backend_lookup(AUTH_BSD);
			auth = imsg->data;
			auth->success = auth_backend->authenticate(auth->user,
			    auth->pass);
			imsg_compose_event(iev, IMSG_PARENT_AUTHENTICATE, 0, 0,
			    -1, auth, sizeof *auth);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_FORWARD_OPEN:
			fwreq = imsg->data;
			fd = parent_forward_open(fwreq->as_user);
			fwreq->status = 0;
			if (fd == -2) {
				/* no ~/.forward, however it's optional. */
				fwreq->status = 1;
				fd = -1;
			} else if (fd != -1)
				fwreq->status = 1;
			imsg_compose_event(iev, IMSG_PARENT_FORWARD_OPEN, 0, 0,
			    fd, fwreq, sizeof *fwreq);
			return;
		}
	}

	if (iev->proc == PROC_MDA) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_FORK_MDA:
			forkmda(iev, imsg->hdr.peerid, imsg->data);
			return;

		case IMSG_PARENT_KILL_MDA:
			i = NULL;
			while ((n = tree_iter(&children, &i, NULL, (void**)&c)))
				if (c->type == CHILD_MDA &&
				    c->mda_id == imsg->hdr.peerid &&
				    c->cause == NULL)
					break;
			if (!n) {
				log_debug("smptd: kill request: proc not found");
				return;
			}
			len = imsg->hdr.len - sizeof imsg->hdr;
			if (len == 0)
				c->cause = xstrdup("no reason", "parent_imsg");
			else {
				c->cause = xmemdup(imsg->data, len,
				    "parent_imsg");
				c->cause[len - 1] = '\0';
			}
			log_debug("smptd: kill requested for %u: %s",
			    c->pid, c->cause);
			kill(c->pid, SIGTERM);
			return;
		}
	}

	if (iev->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);

			/* forward to other processes */
			imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_CTL_VERBOSE,
	    		    0, 0, -1, imsg->data, sizeof(int));
			imsg_compose_event(env->sc_ievs[PROC_MDA], IMSG_CTL_VERBOSE,
	    		    0, 0, -1, imsg->data, sizeof(int));
			imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_CTL_VERBOSE,
	    		    0, 0, -1, imsg->data, sizeof(int));
			imsg_compose_event(env->sc_ievs[PROC_MTA], IMSG_CTL_VERBOSE,
	    		    0, 0, -1, imsg->data, sizeof(int));
			imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_CTL_VERBOSE,
	    		    0, 0, -1, imsg->data, sizeof(int));
			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_CTL_VERBOSE,
	    		    0, 0, -1, imsg->data, sizeof(int));
			return;

		case IMSG_CTL_SHUTDOWN:
			parent_shutdown();
			return;
		}
	}

	errx(1, "parent_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] "
	    "[-f file] [-P system]\n", __progname);
	exit(1);
}

static void
parent_shutdown(void)
{
	void		*iter;
	struct child	*child;
	pid_t		 pid;

	iter = NULL;
	while (tree_iter(&children, &iter, NULL, (void**)&child))
		if (child->type == CHILD_DAEMON)
			kill(child->pid, SIGTERM);

	do {
		pid = waitpid(WAIT_MYPGRP, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	log_warnx("parent terminating");
	exit(0);
}

static void
parent_send_config(int fd, short event, void *p)
{
	parent_send_config_listeners();
	parent_send_config_client_certs();
	parent_send_config_ruleset(PROC_MFA);
	parent_send_config_ruleset(PROC_LKA);
}

static void
parent_send_config_listeners(void)
{
	struct listener		*l;
	struct ssl		*s;
	struct iovec		 iov[5];
	int			 opt;

	log_debug("parent_send_config: configuring smtp");
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);

	SPLAY_FOREACH(s, ssltree, env->sc_ssl) {
		if (!(s->flags & F_SCERT))
			continue;

		iov[0].iov_base = s;
		iov[0].iov_len = sizeof(*s);
		iov[1].iov_base = s->ssl_cert;
		iov[1].iov_len = s->ssl_cert_len;
		iov[2].iov_base = s->ssl_key;
		iov[2].iov_len = s->ssl_key_len;
		iov[3].iov_base = s->ssl_dhparams;
		iov[3].iov_len = s->ssl_dhparams_len;
		iov[4].iov_base = s->ssl_ca;
		iov[4].iov_len = s->ssl_ca_len;

		imsg_composev(&env->sc_ievs[PROC_SMTP]->ibuf,
		    IMSG_CONF_SSL, 0, 0, -1, iov, nitems(iov));
		imsg_event_add(env->sc_ievs[PROC_SMTP]);
	}

	TAILQ_FOREACH(l, env->sc_listeners, entry) {
		if ((l->fd = socket(l->ss.ss_family, SOCK_STREAM, 0)) == -1)
			fatal("smtpd: socket");
		opt = 1;
		if (setsockopt(l->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			fatal("smtpd: setsockopt");
		if (bind(l->fd, (struct sockaddr *)&l->ss, l->ss.ss_len) == -1)
			fatal("smtpd: bind");
		imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_CONF_LISTENER,
		    0, 0, l->fd, l, sizeof(*l));
	}

	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

static void
parent_send_config_client_certs(void)
{
	struct ssl		*s;
	struct iovec		 iov[3];

	log_debug("parent_send_config_client_certs: configuring smtp");
	imsg_compose_event(env->sc_ievs[PROC_MTA], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);

	SPLAY_FOREACH(s, ssltree, env->sc_ssl) {
		if (!(s->flags & F_CCERT))
			continue;

		iov[0].iov_base = s;
		iov[0].iov_len = sizeof(*s);
		iov[1].iov_base = s->ssl_cert;
		iov[1].iov_len = s->ssl_cert_len;
		iov[2].iov_base = s->ssl_key;
		iov[2].iov_len = s->ssl_key_len;

		imsg_composev(&env->sc_ievs[PROC_MTA]->ibuf, IMSG_CONF_SSL,
		    0, 0, -1, iov, nitems(iov));
		imsg_event_add(env->sc_ievs[PROC_MTA]);
	}

	imsg_compose_event(env->sc_ievs[PROC_MTA], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

void
parent_send_config_ruleset(int proc)
{
	struct rule		*r;
	struct map		*m;
	struct mapel		*mapel;
	struct filter		*f;
	
	log_debug("parent_send_config_ruleset: reloading rules and maps");
	imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);

	if (proc == PROC_MFA) {
		TAILQ_FOREACH(f, env->sc_filters, f_entry) {
			imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_FILTER,
			    0, 0, -1, f, sizeof(*f));
		}
	}
	else {
		TAILQ_FOREACH(m, env->sc_maps, m_entry) {
			imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_MAP,
			    0, 0, -1, m, sizeof(*m));
			TAILQ_FOREACH(mapel, &m->m_contents, me_entry) {
			imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_MAP_CONTENT,
			    0, 0, -1, mapel, sizeof(*mapel));
			}
		}
	
		TAILQ_FOREACH(r, env->sc_rules, r_entry) {
			imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_RULE,
			    0, 0, -1, r, sizeof(*r));
			imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_RULE_SOURCE,
			    0, 0, -1, &r->r_sources->m_name, sizeof(r->r_sources->m_name));
		}
	}
	
	imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

static void
parent_sig_handler(int sig, short event, void *p)
{
	struct child	*child;
	int		 die = 0, status, fail;
	pid_t		 pid;
	char		*cause;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		do {
			pid = waitpid(-1, &status, WNOHANG);
			if (pid <= 0)
				continue;

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					asprintf(&cause, "exited abnormally");
				} else
					asprintf(&cause, "exited okay");
			} else
				fatalx("smtpd: unexpected cause of SIGCHLD");

			if (pid == purge_pid)
				purge_pid = -1;

			child = tree_pop(&children, pid);
			if (child == NULL)
				goto skip;

			switch (child->type) {
			case CHILD_DAEMON:
				die = 1;
				if (fail)
					log_warnx("lost child: %s %s",
					    child->title, cause);
				break;

			case CHILD_MDA:
				if (WIFSIGNALED(status) &&
				    WTERMSIG(status) == SIGALRM) {
					free(cause);
					asprintf(&cause, "terminated; timeout");
				}
				else if (child->cause &&
				    WIFSIGNALED(status) &&
				    WTERMSIG(status) == SIGTERM) {
					free(cause);
					cause = child->cause;
					child->cause = NULL;
				}
				if (child->cause)
					free(child->cause);
				imsg_compose_event(env->sc_ievs[PROC_MDA],
				    IMSG_MDA_DONE, child->mda_id, 0,
				    child->mda_out, cause, strlen(cause) + 1);
				break;

			case CHILD_ENQUEUE_OFFLINE:
				if (fail)
					log_warnx("smtpd: couldn't enqueue offline "
					    "message %s; smtpctl %s", child->path, cause);
				else
					unlink(child->path);
				free(child->path);
				offline_done();
				break;

			default:
				fatalx("smtpd: unexpected child type");
			}
			free(child);
    skip:
			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die)
			parent_shutdown();
		break;
	default:
		fatalx("smtpd: unexpected signal");
	}
}

int
main(int argc, char *argv[])
{
	int		 c;
	int		 debug, verbose;
	int		 opts, flags;
	const char	*conffile = CONF_FILE;
	struct smtpd	 smtpd;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;
	struct event	 ev_sighup;
	struct timeval	 tv;
	struct peer	 peers[] = {
		{ PROC_CONTROL,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch },
		{ PROC_MDA,	imsg_dispatch },
		{ PROC_MFA,	imsg_dispatch },
		{ PROC_MTA,	imsg_dispatch },
		{ PROC_SMTP,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch }
	};

	env = &smtpd;

	flags = 0;
	opts = 0;
	debug = 0;
	verbose = 0;

	log_init(1);

	TAILQ_INIT(&offline_q);

	while ((c = getopt(argc, argv, "B:dD:nP:f:T:v")) != -1) {
		switch (c) {
		case 'B':
			if (strstr(optarg, "queue=") == optarg)
				backend_queue = strchr(optarg, '=') + 1;
			else if (strstr(optarg, "scheduler=") == optarg)
				backend_scheduler = strchr(optarg, '=') + 1;
			else if (strstr(optarg, "stat=") == optarg)
				backend_stat = strchr(optarg, '=') + 1;
			else
				log_warnx("invalid backend specifier %s", optarg);
			break;
		case 'd':
			debug = 2;
			verbose |= TRACE_VERBOSE;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'n':
			debug = 2;
			opts |= SMTPD_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'T':
			if (!strcmp(optarg, "imsg"))
				verbose |= TRACE_IMSG;
			else if (!strcmp(optarg, "io"))
				verbose |= TRACE_IO;
			else if (!strcmp(optarg, "smtp"))
				verbose |= TRACE_SMTP;
			else if (!strcmp(optarg, "mta"))
				verbose |= TRACE_MTA;
			else if (!strcmp(optarg, "bounce"))
				verbose |= TRACE_BOUNCE;
			else if (!strcmp(optarg, "scheduler"))
				verbose |= TRACE_SCHEDULER;
			else if (!strcmp(optarg, "stat"))
				verbose |= TRACE_STAT;
			else if (!strcmp(optarg, "profiling")) {
				verbose |= TRACE_PROFILING;
				profiling = 1;
			}
			else if (!strcmp(optarg, "profstat"))
				profstat = 1;
			else if (!strcmp(optarg, "all"))
				verbose |= ~TRACE_VERBOSE;
			else
				log_warnx("unknown trace flag \"%s\"", optarg);
			break;
		case 'P':
			if (!strcmp(optarg, "smtp"))
				flags |= SMTPD_SMTP_PAUSED;
			else if (!strcmp(optarg, "mta"))
				flags |= SMTPD_MTA_PAUSED;
			else if (!strcmp(optarg, "mda"))
				flags |= SMTPD_MDA_PAUSED;
			break;
		case 'v':
			verbose |=  TRACE_VERBOSE;
			break;
		default:
			usage();
		}
	}

	if (!(verbose & TRACE_VERBOSE))
		verbose = 0;

	argv += optind;
	argc -= optind;

	if (argc || *argv)
		usage();

	ssl_init();

	if (parse_config(&smtpd, conffile, opts))
		exit(1);

	if (strlcpy(env->sc_conffile, conffile, MAXPATHLEN) >= MAXPATHLEN)
		errx(1, "config file exceeds MAXPATHLEN");

	if (env->sc_opts & SMTPD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	env->sc_flags |= flags;

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	if ((env->sc_pw =  getpwnam(SMTPD_USER)) == NULL)
		errx(1, "unknown user %s", SMTPD_USER);

	if (ckdir(PATH_SPOOL, 0711, 0, 0, 1) == 0)
		errx(1, "error in spool directory setup");
	if (ckdir(PATH_SPOOL PATH_OFFLINE, 01777, 0, 0, 1) == 0)
		errx(1, "error in offline directory setup");
	if (ckdir(PATH_SPOOL PATH_PURGE, 0700, env->sc_pw->pw_uid, 0, 1) == 0)
		errx(1, "error in purge directory setup");
	if (ckdir(PATH_SPOOL PATH_TEMPORARY, 0700, env->sc_pw->pw_uid, 0, 1) == 0)
		errx(1, "error in purge directory setup");

	mvpurge(PATH_SPOOL PATH_INCOMING, PATH_SPOOL PATH_PURGE);

	if (ckdir(PATH_SPOOL PATH_INCOMING, 0700, env->sc_pw->pw_uid, 0, 1) == 0)
		errx(1, "error in incoming directory setup");

	env->sc_queue = queue_backend_lookup(backend_queue);
	if (env->sc_queue == NULL)
		errx(1, "could not find queue backend \"%s\"", backend_queue);

	if (!env->sc_queue->init(1))
		errx(1, "could not initialize queue backend");

	env->sc_stat = stat_backend_lookup(backend_stat);
	if (env->sc_stat == NULL)
		errx(1, "could not find stat backend \"%s\"", backend_stat);

	if (env->sc_queue_compress_algo) {
		env->sc_compress = 
			compress_backend_lookup(env->sc_queue_compress_algo);
		if (env->sc_compress == NULL)
			errx(1, "could not find queue compress backend \"%s\"",
			    env->sc_queue_compress_algo);
	}

	log_init(debug);
	log_verbose(verbose);

	if (!debug)
		if (daemon(0, 0) == -1)
			err(1, "failed to daemonize");

	log_debug("using \"%s\" queue backend", backend_queue);
	log_debug("using \"%s\" scheduler backend", backend_scheduler);
	log_debug("using \"%s\" stat backend", backend_stat);
	log_info("startup%s", (debug > 1)?" [debug mode]":"");

	if (env->sc_hostname[0] == '\0')
		errx(1, "machine does not have a hostname set");
	env->sc_uptime = time(NULL);

	fork_peers();

	smtpd_process = PROC_PARENT;
	setproctitle("%s", env->sc_title[smtpd_process]);

	imsg_callback = parent_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, parent_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, parent_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, parent_sig_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, parent_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	evtimer_set(&env->sc_ev, parent_send_config, NULL);
	bzero(&tv, sizeof(tv));
	evtimer_add(&env->sc_ev, &tv);

	/* defer offline scanning for a second */
	evtimer_set(&offline_ev, offline_scan, NULL);
	offline_timeout.tv_sec = 1;
	offline_timeout.tv_usec = 0;
	evtimer_add(&offline_ev, &offline_timeout);

	purge_pid = -1;
	evtimer_set(&purge_ev, purge_task, NULL);
	purge_timeout.tv_sec = 10;
	purge_timeout.tv_usec = 0;
	evtimer_add(&purge_ev, &purge_timeout);

	if (event_dispatch() < 0)
		fatal("smtpd: event_dispatch");

	return (0);
}

static void
fork_peers(void)
{
	tree_init(&children);

	/*
	 * Pick descriptor limit that will guarantee impossibility of fd
	 * starvation condition.  The logic:
	 *
	 * Treat hardlimit as 100%.
	 * Limit smtp to 50% (inbound connections)
	 * Limit mta to 50% (outbound connections)
	 * Limit mda to 50% (local deliveries)
	 * In all three above, compute max session limit by halving the fd
	 * limit (50% -> 25%), because each session costs two fds.
	 * Limit queue to 100% to cover the extreme case when tons of fds are
	 * opened for all four possible purposes (smtp, mta, mda, bounce)
	 */
	fdlimit(0.5);

	env->sc_instances[PROC_CONTROL] = 1;
	env->sc_instances[PROC_LKA] = 1;
	env->sc_instances[PROC_MDA] = 1;
	env->sc_instances[PROC_MFA] = 1;
	env->sc_instances[PROC_MTA] = 1;
	env->sc_instances[PROC_PARENT] = 1;
	env->sc_instances[PROC_QUEUE] = 1;
	env->sc_instances[PROC_SCHEDULER] = 1;
	env->sc_instances[PROC_SMTP] = 1;

	init_pipes();

	env->sc_title[PROC_CONTROL] = "control";
	env->sc_title[PROC_LKA] = "lookup";
	env->sc_title[PROC_MDA] = "delivery";
	env->sc_title[PROC_MFA] = "filter";
	env->sc_title[PROC_MTA] = "transfer";
	env->sc_title[PROC_PARENT] = "[priv]";
	env->sc_title[PROC_QUEUE] = "queue";
	env->sc_title[PROC_SCHEDULER] = "scheduler";
	env->sc_title[PROC_SMTP] = "smtp";

	child_add(control(), CHILD_DAEMON, env->sc_title[PROC_CONTROL]);
	child_add(lka(), CHILD_DAEMON, env->sc_title[PROC_LKA]);
	child_add(mda(), CHILD_DAEMON, env->sc_title[PROC_MDA]);
	child_add(mfa(), CHILD_DAEMON, env->sc_title[PROC_MFA]);
	child_add(mta(), CHILD_DAEMON, env->sc_title[PROC_MTA]);
	child_add(queue(), CHILD_DAEMON, env->sc_title[PROC_QUEUE]);
	child_add(scheduler(), CHILD_DAEMON, env->sc_title[PROC_SCHEDULER]);
	child_add(smtp(), CHILD_DAEMON, env->sc_title[PROC_SMTP]);
}

struct child *
child_add(pid_t pid, int type, const char *title)
{
	struct child	*child;

	if ((child = calloc(1, sizeof(*child))) == NULL)
		fatal("smtpd: child_add: calloc");

	child->pid = pid;
	child->type = type;
	child->title = title;

	tree_xset(&children, pid, child);

	return (child);
}

void
imsg_event_add(struct imsgev *iev)
{
	if (iev->handler == NULL) {
		imsg_flush(&iev->ibuf);
		return;
	}

	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev->data);
	event_add(&iev->ev, NULL);
}

void
imsg_compose_event(struct imsgev *iev, uint16_t type, uint32_t peerid,
    pid_t pid, int fd, void *data, uint16_t datalen)
{
	if (imsg_compose(&iev->ibuf, type, peerid, pid, fd, data, datalen) == -1)
		err(1, "%s: imsg_compose(%s)",
		    proc_to_str(smtpd_process),
		    imsg_to_str(type));
	imsg_event_add(iev);
}


static void
purge_task(int fd, short ev, void *arg)
{
	DIR		*d;
	struct dirent	*de;
	int		 n;
	uid_t		 uid;
	gid_t		 gid;

	if (purge_pid == -1) {

		n = 0;
		if ((d = opendir(PATH_SPOOL PATH_PURGE))) {
			while ((de = readdir(d)) != NULL)
				n++;
			closedir(d);
		} else
			log_warn("purge_task: opendir");

		if (n > 2) {
			switch(purge_pid = fork()) {
			case -1:
				log_warn("purge_task: fork");
				break;
			case 0:
				if (chroot(PATH_SPOOL PATH_PURGE) == -1)
					fatal("smtpd: chroot");
				if (chdir("/") == -1)
					fatal("smtpd: chdir");
				uid = env->sc_pw->pw_uid;
				gid = env->sc_pw->pw_gid;
				if (setgroups(1, &gid) ||
				    setresgid(gid, gid, gid) ||
				    setresuid(uid, uid, uid))
					fatal("smtpd: cannot drop privileges");
				rmtree("/", 1);
				_exit(0);
				break;
			default:
				break;
			}
		}
	}

	evtimer_add(&purge_ev, &purge_timeout);
}

static void
forkmda(struct imsgev *iev, uint32_t id,
    struct deliver *deliver)
{
	char		 ebuf[128], sfn[32];
	struct user_backend	*ub;
	struct delivery_backend	*db;
	struct mta_user u;
	struct child	*child;
	pid_t		 pid;
	int		 n, allout, pipefd[2];

	log_debug("forkmda: to \"%s\" as %s", deliver->to, deliver->user);

	bzero(&u, sizeof (u));
	ub = user_backend_lookup(USER_PWD);
	errno = 0;
	if (! ub->getbyname(&u, deliver->user)) {
		n = snprintf(ebuf, sizeof ebuf, "getpwnam: %s",
		    errno ? strerror(errno) : "no such user");
		imsg_compose_event(iev, IMSG_MDA_DONE, id, 0, -1, ebuf, n + 1);
		return;
	}

	db = delivery_backend_lookup(deliver->mode);
	if (db == NULL)
		return;

	if (u.uid == 0 && ! db->allow_root) {
		n = snprintf(ebuf, sizeof ebuf, "not allowed to deliver to: %s",
		    deliver->user);
		imsg_compose_event(iev, IMSG_MDA_DONE, id, 0, -1, ebuf, n + 1);
		return;
	}

	/* lower privs early to allow fork fail due to ulimit */
	if (seteuid(u.uid) < 0)
		fatal("smtpd: forkmda: cannot lower privileges");

	if (pipe(pipefd) < 0) {
		n = snprintf(ebuf, sizeof ebuf, "pipe: %s", strerror(errno));
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		imsg_compose_event(iev, IMSG_MDA_DONE, id, 0, -1, ebuf, n + 1);
		return;
	}

	/* prepare file which captures stdout and stderr */
	strlcpy(sfn, "/tmp/smtpd.out.XXXXXXXXXXX", sizeof(sfn));
	allout = mkstemp(sfn);
	if (allout < 0) {
		n = snprintf(ebuf, sizeof ebuf, "mkstemp: %s", strerror(errno));
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		imsg_compose_event(iev, IMSG_MDA_DONE, id, 0, -1, ebuf, n + 1);
		close(pipefd[0]);
		close(pipefd[1]);
		return;
	}
	unlink(sfn);

	pid = fork();
	if (pid < 0) {
		n = snprintf(ebuf, sizeof ebuf, "fork: %s", strerror(errno));
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		imsg_compose_event(iev, IMSG_MDA_DONE, id, 0, -1, ebuf, n + 1);
		close(pipefd[0]);
		close(pipefd[1]);
		close(allout);
		return;
	}

	/* parent passes the child fd over to mda */
	if (pid > 0) {
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		child = child_add(pid, CHILD_MDA, NULL);
		child->mda_out = allout;
		child->mda_id = id;
		close(pipefd[0]);
		imsg_compose_event(iev, IMSG_PARENT_FORK_MDA, id, 0, pipefd[1],
		    NULL, 0);
		return;
	}

#define error(m) { perror(m); _exit(1); }
	if (seteuid(0) < 0)
		error("forkmda: cannot restore privileges");
	if (chdir(u.directory) < 0 && chdir("/") < 0)
		error("chdir");
	if (dup2(pipefd[0], STDIN_FILENO) < 0 ||
	    dup2(allout, STDOUT_FILENO) < 0 ||
	    dup2(allout, STDERR_FILENO) < 0)
		error("forkmda: dup2");
	if (closefrom(STDERR_FILENO + 1) < 0)
		error("closefrom");
	if (setgroups(1, &u.gid) ||
	    setresgid(u.gid, u.gid, u.gid) ||
	    setresuid(u.uid, u.uid, u.uid))
		error("forkmda: cannot drop privileges");
	if (setsid() < 0)
		error("setsid");
	if (signal(SIGPIPE, SIG_DFL) == SIG_ERR ||
	    signal(SIGINT, SIG_DFL) == SIG_ERR ||
	    signal(SIGTERM, SIG_DFL) == SIG_ERR ||
	    signal(SIGCHLD, SIG_DFL) == SIG_ERR ||
	    signal(SIGHUP, SIG_DFL) == SIG_ERR)
		error("signal");

	/* avoid hangs by setting 5m timeout */
	alarm(300);

	db->open(deliver);

	error("forkmda: unknown mode");
}
#undef error

static void
offline_scan(int fd, short ev, void *arg)
{
	DIR		*dir = arg;
	struct dirent	*d;
	int		 n = 0;

	if (dir == NULL) {
		log_debug("smtpd: scanning offline queue...");
		if ((dir = opendir(PATH_SPOOL PATH_OFFLINE)) == NULL)
			errx(1, "smtpd: opendir");
	}

	while((d = readdir(dir)) != NULL) {
		if (d->d_type != DT_REG)
			continue;

		if (offline_add(d->d_name)) {
			log_warnx("smtpd: could not add offline message %s", d->d_name);
			continue;
		}

		if ((n++) == OFFLINE_READMAX) {
			evtimer_set(&offline_ev, offline_scan, dir);
			offline_timeout.tv_sec = 0;
			offline_timeout.tv_usec = 100000;
			evtimer_add(&offline_ev, &offline_timeout);
			return;
		}
	}

	log_debug("smtpd: offline scanning done");
	closedir(dir);
}

static int
offline_enqueue(char *name)
{
	char		 t[MAXPATHLEN], *path;
	struct stat	 sb;
	pid_t		 pid;
	struct child	*child;
	struct passwd	*pw;

	if (!bsnprintf(t, sizeof t, "%s/%s", PATH_SPOOL PATH_OFFLINE, name)) {
		log_warnx("smtpd: path name too long");
		return (-1);
	}

	if ((path = strdup(t)) == NULL) {
		log_warn("smtpd: strdup");
		return (-1);
	}

	log_debug("smtpd: enqueueing offline message %s", path);

	if ((pid = fork()) == -1) {
		log_warn("smtpd: fork");
		free(path);
		return (-1);
	}

	if (pid == 0) {
		char	*envp[2], *p, *tmp;
		FILE	*fp;
		size_t	 len;
		arglist	 args;

		bzero(&args, sizeof(args));

		if (lstat(path, &sb) == -1) {
			log_warn("smtpd: lstat: %s", path);
			_exit(1);
		}

		if (chflags(path, 0) == -1) {
			log_warn("smtpd: chflags: %s", path);
			_exit(1);
		}

		pw = getpwuid(sb.st_uid);
		if (pw == NULL) {
			log_warnx("smtpd: getpwuid for uid %d failed",
			    sb.st_uid);
			_exit(1);
		}
		

		if (! S_ISREG(sb.st_mode)) {
			log_warnx("smtpd: file %s (uid %d) not regular",
			    path, sb.st_uid);
			_exit(1);
		}

		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) ||
		    closefrom(STDERR_FILENO + 1) == -1)
			_exit(1);

		if ((fp = fopen(path, "r")) == NULL)
			_exit(1);

		if (chdir(pw->pw_dir) == -1 && chdir("/") == -1)
			_exit(1);

		if (setsid() == -1 ||
		    signal(SIGPIPE, SIG_DFL) == SIG_ERR ||
		    dup2(fileno(fp), STDIN_FILENO) == -1)
			_exit(1);

		if ((p = fgetln(fp, &len)) == NULL)
			_exit(1);

		if (p[len - 1] != '\n')
			_exit(1);
		p[len - 1] = '\0';

		addargs(&args, "%s", "sendmail");

		while ((tmp = strsep(&p, "|")) != NULL)
			addargs(&args, "%s", tmp);

		if (lseek(fileno(fp), len, SEEK_SET) == -1)
			_exit(1);

		envp[0] = "PATH=" _PATH_DEFPATH;
		envp[1] = (char *)NULL;
		environ = envp;

		execvp(PATH_SMTPCTL, args.list);
		_exit(1);
	}

	offline_running++;
	child = child_add(pid, CHILD_ENQUEUE_OFFLINE, NULL);
	child->path = path;

	return (0);
}

static int
offline_add(char *path)
{
	struct offline	*q;

	if (offline_running < OFFLINE_QUEUEMAX)
		/* skip queue */
		return offline_enqueue(path);

	q = malloc(sizeof(*q) + strlen(path) + 1);
	if (q == NULL)
		return (-1);
	q->path = (char *)q + sizeof(*q);
	memmove(q->path, path, strlen(path) + 1);
	TAILQ_INSERT_TAIL(&offline_q, q, entry);

	return (0);
}

static void
offline_done(void)
{
	struct offline	*q;

	offline_running--;

	while(offline_running < OFFLINE_QUEUEMAX) {
		if ((q = TAILQ_FIRST(&offline_q)) == NULL)
			break; /* all done */
		TAILQ_REMOVE(&offline_q, q, entry);
		offline_enqueue(q->path);
		free(q);
	}
}

static int
parent_forward_open(char *username)
{
	struct user_backend *ub;
	struct mta_user u;
	char pathname[MAXPATHLEN];
	int fd;

	bzero(&u, sizeof (u));
	ub = user_backend_lookup(USER_PWD);
	if (! ub->getbyname(&u, username))
		return -1;

	if (! bsnprintf(pathname, sizeof (pathname), "%s/.forward", u.directory))
		fatal("smtpd: parent_forward_open: snprintf");

	fd = open(pathname, O_RDONLY);
	if (fd == -1) {
		if (errno == ENOENT)
			return -2;
		log_warn("smtpd: parent_forward_open: %s", pathname);
		return -1;
	}

	if (! secure_file(fd, pathname, u.directory, u.uid, 1)) {
		log_warnx("smtpd: %s: unsecure file", pathname);
		close(fd);
		return -1;
	}

	return fd;
}

void
imsg_dispatch(int fd, short event, void *p)
{
	struct imsgev		*iev = p;
	struct imsg		 imsg;
	ssize_t			 n;
	struct timespec		 t0, t1, dt;

	if (event & EV_READ) {
		if ((n = imsg_read(&iev->ibuf)) == -1)
			err(1, "%s: imsg_read", proc_to_str(smtpd_process));
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&iev->ibuf.w) == -1)
			err(1, "%s: msgbuf_write", proc_to_str(smtpd_process));
	}

	for (;;) {
		if ((n = imsg_get(&iev->ibuf, &imsg)) == -1)
			err(1, "%s: imsg_get", proc_to_str(smtpd_process));
		if (n == 0)
			break;

		log_imsg(smtpd_process, iev->proc, &imsg);

		if (profiling || profstat)
			clock_gettime(CLOCK_MONOTONIC, &t0);

		imsg_callback(iev, &imsg);

		if (profiling || profstat) {
			clock_gettime(CLOCK_MONOTONIC, &t1);
			timespecsub(&t1, &t0, &dt);

			log_trace(TRACE_PROFILING, "PROFILE %s %s %s %li.%06li",
			    proc_to_str(smtpd_process),
			    proc_to_str(iev->proc),
			    imsg_to_str(imsg.hdr.type),
			    dt.tv_sec * 1000000 + dt.tv_nsec / 1000000,
			    dt.tv_nsec % 1000000);

			if (profstat) {
				char	key[STAT_KEY_SIZE];

				/* can't profstat control process yet */
				if (smtpd_process == PROC_CONTROL)
					return;

				if (! bsnprintf(key, sizeof key,
					"profiling.imsg.%s.%s.%s",
					imsg_to_str(imsg.hdr.type),
					proc_to_str(iev->proc),
					proc_to_str(smtpd_process)))
					return;
				stat_set(key, stat_timespec(&dt));
			}
		}

		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

static void
log_imsg(int to, int from, struct imsg *imsg)
{
	if (imsg->fd != -1)
		log_trace(TRACE_IMSG, "imsg: %s <- %s: %s (len=%zu, fd=%i)",
		    proc_to_str(to),
		    proc_to_str(from),
		    imsg_to_str(imsg->hdr.type),
		    imsg->hdr.len - IMSG_HEADER_SIZE,
		    imsg->fd);
	else
		log_trace(TRACE_IMSG, "imsg: %s <- %s: %s (len=%zu)",
		    proc_to_str(to),
		    proc_to_str(from),
		    imsg_to_str(imsg->hdr.type),
		    imsg->hdr.len - IMSG_HEADER_SIZE);
}

#define CASE(x) case x : return #x

const char *
proc_to_str(int proc)
{
	switch (proc) {
	CASE(PROC_PARENT);
	CASE(PROC_SMTP);
	CASE(PROC_MFA);
	CASE(PROC_LKA);
	CASE(PROC_QUEUE);
	CASE(PROC_MDA);
	CASE(PROC_MTA);
	CASE(PROC_CONTROL);
	CASE(PROC_SCHEDULER);
	default:
		return "PROC_???";
	}
}

const char *
imsg_to_str(int type)
{
	static char	 buf[32];

	switch(type) {
	CASE(IMSG_NONE);
	CASE(IMSG_CTL_OK);
	CASE(IMSG_CTL_FAIL);
	CASE(IMSG_CTL_SHUTDOWN);
	CASE(IMSG_CTL_VERBOSE);
	CASE(IMSG_CONF_START);
	CASE(IMSG_CONF_SSL);
	CASE(IMSG_CONF_LISTENER);
	CASE(IMSG_CONF_MAP);
	CASE(IMSG_CONF_MAP_CONTENT);
	CASE(IMSG_CONF_RULE);
	CASE(IMSG_CONF_RULE_SOURCE);
	CASE(IMSG_CONF_FILTER);
	CASE(IMSG_CONF_END);

	CASE(IMSG_LKA_UPDATE_MAP);
	CASE(IMSG_LKA_MAIL);
	CASE(IMSG_LKA_RCPT);
	CASE(IMSG_LKA_SECRET);
	CASE(IMSG_LKA_RULEMATCH);
	CASE(IMSG_MDA_SESS_NEW);
	CASE(IMSG_MDA_DONE);

	CASE(IMSG_MFA_CONNECT);
	CASE(IMSG_MFA_HELO);
	CASE(IMSG_MFA_MAIL);
	CASE(IMSG_MFA_RCPT);
	CASE(IMSG_MFA_DATALINE);
	CASE(IMSG_MFA_QUIT);
	CASE(IMSG_MFA_CLOSE);
	CASE(IMSG_MFA_RSET);

	CASE(IMSG_QUEUE_CREATE_MESSAGE);
	CASE(IMSG_QUEUE_SUBMIT_ENVELOPE);
	CASE(IMSG_QUEUE_COMMIT_ENVELOPES);
	CASE(IMSG_QUEUE_REMOVE_MESSAGE);
	CASE(IMSG_QUEUE_COMMIT_MESSAGE);
	CASE(IMSG_QUEUE_TEMPFAIL);
	CASE(IMSG_QUEUE_PAUSE_MDA);
	CASE(IMSG_QUEUE_PAUSE_MTA);
	CASE(IMSG_QUEUE_RESUME_MDA);
	CASE(IMSG_QUEUE_RESUME_MTA);

	CASE(IMSG_QUEUE_DELIVERY_OK);
	CASE(IMSG_QUEUE_DELIVERY_TEMPFAIL);
	CASE(IMSG_QUEUE_DELIVERY_PERMFAIL);
	CASE(IMSG_QUEUE_DELIVERY_LOOP);

	CASE(IMSG_QUEUE_MESSAGE_FD);
	CASE(IMSG_QUEUE_MESSAGE_FILE);
	CASE(IMSG_QUEUE_REMOVE);
	CASE(IMSG_QUEUE_EXPIRE);

	CASE(IMSG_SCHEDULER_REMOVE);
	CASE(IMSG_SCHEDULER_SCHEDULE);

	CASE(IMSG_BATCH_CREATE);
	CASE(IMSG_BATCH_APPEND);
	CASE(IMSG_BATCH_CLOSE);

	CASE(IMSG_PARENT_FORWARD_OPEN);
	CASE(IMSG_PARENT_FORK_MDA);
	CASE(IMSG_PARENT_KILL_MDA);

	CASE(IMSG_PARENT_AUTHENTICATE);
	CASE(IMSG_PARENT_SEND_CONFIG);

	CASE(IMSG_SMTP_ENQUEUE);
	CASE(IMSG_SMTP_PAUSE);
	CASE(IMSG_SMTP_RESUME);

	CASE(IMSG_DNS_HOST);
	CASE(IMSG_DNS_HOST_END);
	CASE(IMSG_DNS_MX);
	CASE(IMSG_DNS_PTR);

	CASE(IMSG_STAT_INCREMENT);
	CASE(IMSG_STAT_DECREMENT);
	CASE(IMSG_STAT_SET);

	CASE(IMSG_STATS);
  	CASE(IMSG_STATS_GET);
	default:
		snprintf(buf, sizeof(buf), "IMSG_??? (%d)", type);

		return buf;
	}
}
