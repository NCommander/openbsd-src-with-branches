/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fuse_private.h"
#include "debug.h"

static struct fuse_session *sigse;

int
fuse_loop(struct fuse *fuse)
{
	struct fusebuf fbuf;
	struct kevent ev;
	int error = 0;
	size_t len = 0;
	int ret;

	fuse->fc->kq = kqueue();
	if (fuse->fc->kq == -1)
		return (-1);

	EV_SET(&fuse->fc->event, fuse->fc->fd, EVFILT_READ, EV_ADD |
	    EV_ENABLE, 0, 0, 0);

	while (!fuse->fc->dead) {
		ret = kevent(fuse->fc->kq, &fuse->fc->event, 1, &ev, 1, NULL);
		if (ret == -1)
			DPERROR(__func__);
		else if (ret > 0) {
			error = read(fuse->fc->fd, &fbuf, sizeof(fbuf.fb_hdr));
			if (error != sizeof(fbuf.fb_hdr)) {
				DPRINTF("%s: bad hdr read\n", __func__);
				errno = EINVAL;
				return (-1);
			}

			if (fbuf.fb_len != 0) {
				error = read(fuse->fc->fd, (char *)&fbuf.F_dat,
				    fbuf.fb_len);

				if (error != (int)fbuf.fb_len) {
					errno = EINVAL;
					return (-1);
				}
			}

			ret = ifuse_exec_opcode(fuse, &fbuf);
			if (ret) {
				return (ret);
			}

			len = sizeof(fbuf.fb_hdr) + fbuf.fb_len;
			ret = write(fuse->fc->fd, &fbuf, len);

			if (ret != (int)len) {
				errno = EINVAL;
				return (-1);
			}
		}
	}

	return (0);
}

#define	DEVPATH	"/dev/"
#define	FUSEDEV	DEVPATH "fuse"

struct fuse_chan *
fuse_mount(const char *dir, unused struct fuse_args *args)
{
	struct fusefs_args fargs;
	struct fuse_chan *fc;
	struct stat st;
	char busnode[16];
	dev_t minor;
	int i;

	fc = calloc(1, sizeof(*fc));
	if (fc == NULL)
		return (NULL);

	fc->dir = strdup(dir);
	if (fc->dir == NULL)
		goto bad;

	for (i = 0; i < 8 ; i++) {
		minor = -1;
		snprintf(busnode, sizeof(busnode), FUSEDEV "%d", i);

		DPRINTF("trying %s\n", busnode);
		if ((fc->fd = open(busnode, O_RDWR)) < 0) {
			if (errno == EBUSY)
				DPRINTF("device %s already opened\n", busnode);
			else if (errno != ENOENT && errno != ENXIO)
				DPRINTF("could not open %s\n", busnode);
			continue;
		}

		if (fstat(fc->fd, &st) != 0)
			goto bad;

		minor = st.st_rdev;
		break;
	}

	if (minor == -1) {
		fprintf(stderr, "%s: Cannot find a suitable fuse device\n",
		    __func__);
		goto bad;
	}

	fargs.dev = minor;
	if (mount(MOUNT_FUSEFS, dir, 0, &fargs)) {
		if (errno == EOPNOTSUPP)
			fprintf(stderr,
			    "%s: %s: FS not supported by kernel\n", __func__,
			    dir);
		else
			perror("fuse_mount failure:");

		goto bad;
	}

	return (fc);
bad:
	if (fc->fd > 0)
		close(fc->fd);
	if (fc->dir != NULL)
		free(fc->dir);
	return (NULL);
}

void
fuse_unmount(const char *dir, unused struct fuse_chan *ch)
{
	int ret;

	if (ch->dead)
		return ;

	if ((ret = unmount(dir, MNT_UPDATE)) == -1)
		DPERROR(__func__);

	return ;
}

int
fuse_is_lib_option(unused const char *opt)
{
	DPRINTF("option %s\n", opt);
	return (0);
}

int
fuse_chan_fd(struct fuse_chan *ch)
{
	return (ch->fd);
}

struct fuse_session *
fuse_get_session(struct fuse *f)
{
	return (&f->se);
}

int
fuse_loop_mt(unused struct fuse *fuse)
{
	return (0);
}

struct fuse *
fuse_new(struct fuse_chan *fc, unused struct fuse_args *args,
    const struct fuse_operations *ops, unused size_t size,
    unused void *userdata)
{
	struct fuse *fuse;
	struct fuse_vnode *root;

	if ((fuse = calloc(1, sizeof(*fuse))) == NULL)
		return (NULL);

	/* copy fuse ops to their own structure */
	memcpy(&fuse->op, ops, sizeof(fuse->op));

	fuse->fc = fc;
	fuse->max_ino = FUSE_ROOT_INO;
	fuse->se.args = fuse;

	if ((root = alloc_vn(fuse, "/", FUSE_ROOT_INO, 0)) == NULL) {
		free(fuse);
		return (NULL);
	}

	tree_init(&fuse->vnode_tree);
	tree_init(&fuse->name_tree);
	if (!set_vn(fuse, root)) {
		free(fuse);
		return (NULL);
	}

	return (fuse);
}

int
fuse_daemonize(unused int foreground)
{
#ifdef DEBUG
	return (daemon(0,1));
#else
	return (daemon(0,0));
#endif
}

void
fuse_destroy(unused struct fuse *f)
{
	close(f->fc->fd);
	free(f->fc->dir);
	free(f->fc);
	free(f);
}

static void
ifuse_get_signal(unused int num)
{
	struct fuse *f;
	pid_t child;
	int status;

	if (sigse != NULL) {
		child = fork();

		if (child < 0)
			return ;

		f = sigse->args;
		if (child == 0) {
			fuse_unmount(f->fc->dir, f->fc);
			sigse = NULL;
			exit(0);
		}

		fuse_loop(f);
		wait(&status);
	}
}

void
fuse_remove_signal_handlers(unused struct fuse_session *se)
{
	sigse = NULL;
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
}

int
fuse_set_signal_handlers(unused struct fuse_session *se)
{
	sigse = se;
	signal(SIGHUP, ifuse_get_signal);
	signal(SIGINT, ifuse_get_signal);
	signal(SIGTERM, ifuse_get_signal);
	signal(SIGPIPE, SIG_IGN);
	return (0);
}

int
fuse_parse_cmdline(struct fuse_args *args, char **mp, int *mt, unused int *fg)
{
	int i;

#ifdef DEBUG
	ifuse_debug_init();
#endif

	for (i = args->argc - 1 ; i > 0 && *args->argv[i] == '-' ; --i);
	*mp = args->argv[i];
	*mt = 0;

	return (0);
}

int
fuse_main(int argc, char **argv, const struct fuse_operations *ops, void *data)
{
	struct fuse *fuse;
	struct fuse_chan *fc;
	struct fuse_args args;
	char *mountpoint;
	int mt, fg;

	args.argc = argc;
	args.argv = argv;
	fuse_parse_cmdline(&args, &mountpoint, &mt, &fg);

	fuse_daemonize(0);

	if ((fc = fuse_mount(mountpoint, NULL)) == NULL)
		return (-1);

	if ((fuse = fuse_new(fc, NULL, ops, sizeof(*(ops)), data)) == NULL) {
		free(fc);
		return (-1);
	}

	return (fuse_loop(fuse));
}
