/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	usage(void);
void		parent_shutdown(void);
void		parent_send_config(int, short, void *);
void		parent_dispatch_lka(int, short, void *);
void		parent_dispatch_mda(int, short, void *);
void		parent_dispatch_mfa(int, short, void *);
void		parent_dispatch_smtp(int, short, void *);
void		parent_sig_handler(int, short, void *);
int		parent_open_message_file(struct batch *);
int		parent_open_mailbox(struct batch *, struct path *);
int		parent_open_filename(struct batch *, struct path *);
int		parent_rename_mailfile(struct batch *);
int		parent_open_maildir(struct batch *, struct path *);
int		parent_maildir_init(struct passwd *, char *);
int		parent_external_mda(struct batch *, struct path *);
int		check_child(pid_t, const char *);
int		setup_spool(uid_t, gid_t);

pid_t	lka_pid = 0;
pid_t	mfa_pid = 0;
pid_t	queue_pid = 0;
pid_t	mda_pid = 0;
pid_t	mta_pid = 0;
pid_t	control_pid = 0;
pid_t	smtp_pid = 0;


int __b64_pton(char const *, unsigned char *, size_t);

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] "
	    "[-f file]\n", __progname);
	exit(1);
}

void
parent_shutdown(void)
{
	u_int		i;
	pid_t		pid;
	pid_t		pids[] = {
		lka_pid,
		mfa_pid,
		queue_pid,
		mda_pid,
		mta_pid,
		control_pid,
		smtp_pid
	};

	for (i = 0; i < sizeof(pids) / sizeof(pid); i++)
		if (pids[i])
			kill(pids[i], SIGTERM);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	log_info("terminating");
	exit(0);
}

void
parent_send_config(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct buf		*b;
	struct listener		*l;
	struct ssl		*s;

	log_debug("parent_send_config: configuring smtp");
	imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);

	SPLAY_FOREACH(s, ssltree, &env->sc_ssl) {
		b = imsg_create(env->sc_ibufs[PROC_SMTP], IMSG_CONF_SSL, 0, 0,
		    sizeof(*s) + s->ssl_cert_len + s->ssl_key_len);
		if (b == NULL)
			fatal("imsg_create");
		if (imsg_add(b, s, sizeof(*s)) == -1)
			fatal("imsg_add: ssl");
		if (imsg_add(b, s->ssl_cert, s->ssl_cert_len) == -1)
			fatal("imsg_add: ssl_cert");
		if (imsg_add(b, s->ssl_key, s->ssl_key_len) == -1)
			fatal("imsg_add: ssl_key");
		b->fd = -1;
		if (imsg_close(env->sc_ibufs[PROC_SMTP], b) == -1)
			fatal("imsg_close");
	}

	TAILQ_FOREACH(l, &env->sc_listeners, entry) {
		smtp_listener_setup(env, l);
		imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_CONF_LISTENER,
		    0, 0, l->fd, l, sizeof(*l));
	}
	imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

void
parent_dispatch_lka(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_LKA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("parent_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_dispatch_mfa(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MFA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("parent_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_dispatch_mda(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MDA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_MAILBOX_OPEN: {
			struct batch *batchp;
			struct path *path;
			u_int8_t i;
			int desc;
			struct action_handler {
				enum action_type action;
				int (*handler)(struct batch *, struct path *);
			} action_hdl_table[] = {
				{ A_MBOX,	parent_open_mailbox },
				{ A_MAILDIR,	parent_open_maildir },
				{ A_EXT,	parent_external_mda },
				{ A_FILENAME,	parent_open_filename }
			};

			batchp = imsg.data;
			path = &batchp->message.recipient;
			if (batchp->type & T_DAEMON_BATCH) {
				path = &batchp->message.sender;
			}

			for (i = 0; i < sizeof(action_hdl_table) / sizeof(struct action_handler); ++i) {
				if (action_hdl_table[i].action == path->rule.r_action) {
					desc = action_hdl_table[i].handler(batchp, path);
					imsg_compose(ibuf, IMSG_MDA_MAILBOX_FILE, 0, 0,
					    desc, batchp, sizeof(struct batch));
					break;
				}
			}
			if (i == sizeof(action_hdl_table) / sizeof(struct action_handler))
				errx(1, "%s: unknown action.", __func__);

			break;
		}
		case IMSG_PARENT_MESSAGE_OPEN: {
			struct batch *batchp;
			int desc;

			batchp = imsg.data;
			desc = parent_open_message_file(batchp);
			imsg_compose(ibuf, IMSG_MDA_MESSAGE_FILE, 0, 0,
			    desc, batchp, sizeof(struct batch));

			break;
		}
		case IMSG_PARENT_MAILBOX_RENAME: {
			struct batch *batchp;

			batchp = imsg.data;
			parent_rename_mailfile(batchp);

			break;
		}
		default:
			log_debug("parent_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_dispatch_smtp(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_SMTP];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
			/* XXX - NOT ADVERTISED YET */
		case IMSG_PARENT_AUTHENTICATE: {
			struct session_auth_req *req;
			struct session_auth_reply reply;
			u_int8_t buffer[1024];
			char *pw_name;
			char *pw_passwd;
			struct passwd *pw;

			req = (struct session_auth_req *)imsg.data;

			reply.session_id = req->session_id;
			reply.value = 0;

			if (__b64_pton(req->buffer, buffer, 1024) >= 0) {
				pw_name = buffer+1;
				pw_passwd = pw_name+strlen(pw_name)+1;
				pw = getpwnam(pw_name);
				if (pw != NULL)
					if (strcmp(pw->pw_passwd, crypt(pw_passwd,
						    pw->pw_passwd)) == 0)
						reply.value = 1;
			}
			imsg_compose(ibuf, IMSG_PARENT_AUTHENTICATE, 0, 0,
			    -1, &reply, sizeof(reply));

			break;
		}
		default:
			log_debug("parent_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_sig_handler(int sig, short event, void *p)
{
	int					 i;
	int					 die = 0;
	pid_t					 pid;
	struct mdaproc				*mdaproc;
	struct mdaproc				 lookup;
	struct smtpd				*env = p;
	struct { pid_t p; const char *s; }	 procs[] = {
		{ lka_pid,	"lookup agent" },
		{ mfa_pid,	"mail filter agent" },
		{ queue_pid,	"mail queue" },
		{ mda_pid,	"mail delivery agent" },
		{ mta_pid,	"mail transfer agent" },
		{ control_pid,	"control process" },
		{ smtp_pid,	"smtp server" },
		{ 0,		NULL },
	};

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		for (i = 0; procs[i].s != NULL; i++)
			if (check_child(procs[i].p, procs[i].s)) {
				procs[i].p = 0;
				die = 1;
			}
		if (die)
			parent_shutdown();

		do {
			int status;

			pid = waitpid(-1, &status, WNOHANG);
			if (pid > 0) {
				lookup.pid = pid;
				mdaproc = SPLAY_FIND(mdaproctree, &env->mdaproc_queue, &lookup);
				if (mdaproc == NULL)
					errx(1, "received SIGCHLD but no known child for that pid (#%d)", pid);

				if (WIFEXITED(status) && !WIFSIGNALED(status)) {
					switch (WEXITSTATUS(status)) {
					case EX_OK:
						log_debug("DEBUG: external mda reported success");
						break;
					case EX_TEMPFAIL:
						log_debug("DEBUG: external mda reported temporary failure");
						break;
					default:
						log_debug("DEBUG: external mda reported permanent failure");
					}
				}
				else {
					log_debug("DEBUG: external mda process has terminated in a baaaad way");
				}

				free(mdaproc);
			}
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		/**/		
		break;
	default:
		fatalx("unexpected signal");
	}
}

int
main(int argc, char *argv[])
{
	int		 c;
	int		 debug;
	int		 opts;
	const char	*conffile = CONF_FILE;
	struct smtpd	 env;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;
	struct event	 ev_sighup;
	struct timeval	 tv;
	struct peer	 peers[] = {
		{ PROC_LKA,	parent_dispatch_lka },
		{ PROC_MDA,	parent_dispatch_mda },
		{ PROC_MFA,	parent_dispatch_mfa },
		{ PROC_SMTP,	parent_dispatch_smtp },
	};

	opts = 0;
	debug = 0;

	log_init(1);

	while ((c = getopt(argc, argv, "dD:nf:v")) != -1) {
		switch (c) {
		case 'd':
			debug = 2;
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
		case 'v':
			opts |= SMTPD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	if (parse_config(&env, conffile, opts))
		exit(1);

	if (env.sc_opts & SMTPD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	if ((env.sc_pw =  getpwnam(SMTPD_USER)) == NULL)
		errx(1, "unknown user %s", SMTPD_USER);
	endpwent();

	if (!setup_spool(env.sc_pw->pw_uid, 0))
		errx(1, "invalid directory permissions");

	log_init(debug);

	if (!debug) {
		if (daemon(1, 0) == -1)
			err(1, "failed to daemonize");
	}

	log_info("startup%s", (debug > 1)?" [debug mode]":"");

	init_peers(&env);

	/* start subprocesses */
	lka_pid = lka(&env);
	mfa_pid = mfa(&env);
	queue_pid = queue(&env);
	mda_pid = mda(&env);
	mta_pid = mta(&env);
	smtp_pid = smtp(&env);
	control_pid = control(&env);

	setproctitle("parent");
	SPLAY_INIT(&env.mdaproc_queue);

	event_init();

	signal_set(&ev_sigint, SIGINT, parent_sig_handler, &env);
	signal_set(&ev_sigterm, SIGTERM, parent_sig_handler, &env);
	signal_set(&ev_sigchld, SIGCHLD, parent_sig_handler, &env);
	signal_set(&ev_sighup, SIGHUP, parent_sig_handler, &env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	config_peers(&env, peers, 4);

	evtimer_set(&env.sc_ev, parent_send_config, &env);
	bzero(&tv, sizeof(tv));
	evtimer_add(&env.sc_ev, &tv);

	event_dispatch();

	return (0);
}


int
check_child(pid_t pid, const char *pname)
{
	int	status;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("check_child: lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			log_warnx("check_child: lost child: %s terminated; "
			    "signal %d", pname, WTERMSIG(status));
			return (1);
		}
	}

	return (0);
}

int
setup_spool(uid_t uid, gid_t gid)
{
	unsigned int	 n;
	char		*paths[] = { PATH_MESSAGES, PATH_LOCAL, PATH_RELAY,
				     PATH_DAEMON, PATH_ENVELOPES };
	char		 pathname[MAXPATHLEN];
	struct stat	 sb;
	int		 ret;

	if (snprintf(pathname, MAXPATHLEN, "%s", PATH_SPOOL) >= MAXPATHLEN)
		fatal("snprintf");

	if (stat(pathname, &sb) == -1) {
		if (errno != ENOENT) {
			warn("stat: %s", pathname);
			return 0;
		}

		if (mkdir(pathname, 0711) == -1) {
			warn("mkdir: %s", pathname);
			return 0;
		}

		if (chown(pathname, 0, 0) == -1) {
			warn("chown: %s", pathname);
			return 0;
		}

		if (stat(pathname, &sb) == -1)
			err(1, "stat: %s", pathname);
	}

	/* check if it's a directory */
	if (!S_ISDIR(sb.st_mode)) {
		warnx("%s is not a directory", pathname);
		return 0;
	}

	/* check that it is owned by uid/gid */
	if (sb.st_uid != 0 || sb.st_gid != 0) {
		warnx("%s must be owned by root:wheel", pathname);
		return 0;
	}

	/* check permission */
	if ((sb.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR)) != (S_IRUSR|S_IWUSR|S_IXUSR) ||
	    (sb.st_mode & (S_IRGRP|S_IWGRP|S_IXGRP)) != S_IXGRP ||
	    (sb.st_mode & (S_IROTH|S_IWOTH|S_IXOTH)) != S_IXOTH) {
		warnx("%s must be rwx--x--x (0711)", pathname);
		return 0;
	}

	ret = 1;
	for (n = 0; n < sizeof(paths)/sizeof(paths[0]); n++) {
		if (snprintf(pathname, MAXPATHLEN, "%s%s", PATH_SPOOL,
			paths[n]) >= MAXPATHLEN)
			fatal("snprintf");

		if (stat(pathname, &sb) == -1) {
			if (errno != ENOENT) {
				warn("stat: %s", pathname);
				ret = 0;
				continue;
			}

			if (mkdir(pathname, 0700) == -1) {
				ret = 0;
				warn("mkdir: %s", pathname);
			}

			if (chown(pathname, uid, gid) == -1) {
				ret = 0;
				warn("chown: %s", pathname);
			}

			if (stat(pathname, &sb) == -1)
				err(1, "stat: %s", pathname);
		}

		/* check if it's a directory */
		if (!S_ISDIR(sb.st_mode)) {
			ret = 0;
			warnx("%s is not a directory", pathname);
		}

		/* check that it is owned by uid/gid */
		if (sb.st_uid != uid) {
			ret = 0;
			warnx("%s is not owned by uid %d", pathname, uid);
		}
		if (sb.st_gid != gid) {
			ret = 0;
			warnx("%s is not owned by gid %d", pathname, gid);
		}

		/* check permission */
		if ((sb.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR)) != (S_IRUSR|S_IWUSR|S_IXUSR) ||
		    (sb.st_mode & (S_IRGRP|S_IWGRP|S_IXGRP)) ||
		    (sb.st_mode & (S_IROTH|S_IWOTH|S_IXOTH))) {
			ret = 0;
			warnx("%s must be rwx------ (0700)", pathname);
		}
	}
	return ret;
}

void
imsg_event_add(struct imsgbuf *ibuf)
{
	if (ibuf->handler == NULL) {
		imsg_flush(ibuf);
		return;
	}

	ibuf->events = EV_READ;
	if (ibuf->w.queued)
		ibuf->events |= EV_WRITE;

	event_del(&ibuf->ev);
	event_set(&ibuf->ev, ibuf->fd, ibuf->events, ibuf->handler, ibuf->data);
	event_add(&ibuf->ev, NULL);
}

int
parent_open_message_file(struct batch *batchp)
{
	int fd;
	char pathname[MAXPATHLEN];

	if (snprintf(pathname, MAXPATHLEN, "%s%s/%s",
	    PATH_SPOOL, PATH_MESSAGES, batchp->message_id)
	    >= MAXPATHLEN) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	fd = open(pathname, O_RDONLY);
	return fd;
}

int
parent_open_mailbox(struct batch *batchp, struct path *path)
{
	int fd;
	struct passwd *pw;
	char pathname[MAXPATHLEN];

	pw = getpwnam(path->pw_name);
	if (pw == NULL) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	(void)snprintf(pathname, MAXPATHLEN, "%s", path->rule.r_value.path);

	fd = open(pathname, O_CREAT|O_APPEND|O_RDWR|O_EXLOCK|O_SYNC|O_NONBLOCK, 0600);
	if (fd == -1) {
		/* XXX - this needs to be discussed ... */
		switch (errno) {
		case ENOTDIR:
		case ENOENT:
		case EACCES:
		case ELOOP:
		case EROFS:
		case EDQUOT:
		case EINTR:
		case EIO:
		case EMFILE:
		case ENFILE:
		case ENOSPC:
		case EWOULDBLOCK:
			batchp->message.status |= S_MESSAGE_TEMPFAILURE;
			break;
		default:
			batchp->message.status |= S_MESSAGE_PERMFAILURE;
		}
		return -1;
	}

	fchown(fd, pw->pw_uid, 0);

	return fd;
}


int
parent_open_maildir(struct batch *batchp, struct path *path)
{
	int fd;
	struct passwd *pw;
	char pathname[MAXPATHLEN];

	pw = getpwnam(path->pw_name);
	if (pw == NULL) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	snprintf(pathname, MAXPATHLEN, "%s", path->rule.r_value.path);
	log_debug("PATH: %s", pathname);
	if (! parent_maildir_init(pw, pathname)) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	if (snprintf(pathname, MAXPATHLEN, "%s/tmp/%s",
		pathname, batchp->message.message_uid) >= MAXPATHLEN) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	fd = open(pathname, O_CREAT|O_RDWR|O_TRUNC|O_EXLOCK|O_SYNC, 0600);
	if (fd == -1) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	fchown(fd, pw->pw_uid, pw->pw_gid);

	return fd;
}

int
parent_maildir_init(struct passwd *pw, char *root)
{
	u_int8_t i;
	char pathname[MAXPATHLEN];
	char *subdir[] = { "/", "/tmp", "/cur", "/new" };

	for (i = 0; i < sizeof (subdir) / sizeof (char *); ++i) {
		if (snprintf(pathname, MAXPATHLEN, "%s%s", root, subdir[i])
		    >= MAXPATHLEN)
			return 0;
		if (mkdir(pathname, 0700) == -1)
			if (errno != EEXIST)
				return 0;
		chown(pathname, pw->pw_uid, pw->pw_gid);
	}

	return 1;
}

int
parent_rename_mailfile(struct batch *batchp)
{
	struct passwd *pw;
	char srcpath[MAXPATHLEN];
	char dstpath[MAXPATHLEN];
	struct path *path;

	if (batchp->type & T_DAEMON_BATCH) {
		path = &batchp->message.sender;
	}
	else {
		path = &batchp->message.recipient;
	}

	pw = getpwnam(path->pw_name);
	if (pw == NULL) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return 0;
	}

	(void)snprintf(srcpath, MAXPATHLEN, "%s/tmp/%s",
	    path->rule.r_value.path, batchp->message.message_uid);
	(void)snprintf(dstpath, MAXPATHLEN, "%s/new/%s",
	    path->rule.r_value.path, batchp->message.message_uid);

	if (rename(srcpath, dstpath) == -1) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return 0;
	}

	return 1;
}

int
parent_external_mda(struct batch *batchp, struct path *path)
{
	struct passwd *pw;
	pid_t pid;
	int pipefd[2];
	struct mdaproc *mdaproc;
	char *pw_name;

	pw_name = path->pw_name;
	if (pw_name[0] == '\0')
		pw_name = SMTPD_USER;

	log_debug("executing filter as user: %s", pw_name);
	pw = getpwnam(pw_name);
	if (pw == NULL) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) == -1) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	pid = fork();
	if (pid == -1) {
		close(pipefd[0]);
		close(pipefd[1]);
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	if (pid == 0) {
		setproctitle("external MDA");

		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			fatal("mta: cannot drop privileges");

		close(pipefd[0]);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		dup2(pipefd[1], 0);

		execlp(_PATH_BSHELL, "sh", "-c", path->rule.r_value.path, (void *)NULL);
		exit(1);
	}

	mdaproc = calloc(1, sizeof (struct mdaproc));
	if (mdaproc == NULL)
		err(1, "calloc");
	mdaproc->pid = pid;

	SPLAY_INSERT(mdaproctree, &batchp->env->mdaproc_queue, mdaproc);

	close(pipefd[1]);
	return pipefd[0];
}

int
parent_open_filename(struct batch *batchp, struct path *path)
{
	int fd;
	char pathname[MAXPATHLEN];

	(void)snprintf(pathname, MAXPATHLEN, "%s", path->u.filename);
	fd = open(pathname, O_CREAT|O_APPEND|O_RDWR|O_EXLOCK|O_SYNC|O_NONBLOCK, 0600);
	if (fd == -1) {
		/* XXX - this needs to be discussed ... */
		switch (errno) {
		case ENOTDIR:
		case ENOENT:
		case EACCES:
		case ELOOP:
		case EROFS:
		case EDQUOT:
		case EINTR:
		case EIO:
		case EMFILE:
		case ENFILE:
		case ENOSPC:
		case EWOULDBLOCK:
			batchp->message.status |= S_MESSAGE_TEMPFAILURE;
			break;
		default:
			batchp->message.status |= S_MESSAGE_PERMFAILURE;
		}
		return -1;
	}

	return fd;
}

int
mdaproc_cmp(struct mdaproc *s1, struct mdaproc *s2)
{
	if (s1->pid < s2->pid)
		return (-1);

	if (s1->pid > s2->pid)
		return (1);

	return (0);
}

SPLAY_GENERATE(mdaproctree, mdaproc, mdaproc_nodes, mdaproc_cmp);
