/*	$OpenBSD: aucat.c,v 1.60 2009/04/27 18:09:34 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <varargs.h>

#include "conf.h"
#include "aparams.h"
#include "aproc.h"
#include "abuf.h"
#include "wav.h"
#include "listen.h"
#include "dev.h"
#include "midi.h"
#include "opt.h"
#include "miofile.h"

#define MODE_PLAY	1
#define MODE_REC	2

#define PROG_AUCAT	"aucat"
#define PROG_MIDICAT	"midicat"

int debug_level = 0;
volatile int quit_flag = 0;

/*
 * SIGINT handler, it raises the quit flag. If the flag is already set,
 * that means that the last SIGINT was not handled, because the process
 * is blocked somewhere, so exit
 */
void
sigint(int s)
{
	if (quit_flag)
		_exit(1);
	quit_flag = 1;
}

/*
 * increase debug level on SIGUSR1
 */
void
sigusr1(int s)
{
	if (debug_level < 4)
		debug_level++;
}

/*
 * decrease debug level on SIGUSR2
 */
void
sigusr2(int s)
{
	if (debug_level > 0)
		debug_level--;
}

void
set_debug_level(char *envname)
{
	char *dbgenv;
	const char *errstr;

	dbgenv = getenv(envname);
	if (dbgenv) {
		debug_level = strtonum(dbgenv, 0, 4, &errstr);
		if (errstr)
			errx(1, "%s is %s: %s", envname, errstr, dbgenv);
	}
}

void
opt_ch(struct aparams *par)
{
	if (sscanf(optarg, "%u:%u", &par->cmin, &par->cmax) != 2 ||
	    par->cmax < par->cmin || par->cmax > NCHAN_MAX - 1)
		errx(1, "%s: bad channel range", optarg);
}

void
opt_rate(struct aparams *par)
{
	if (sscanf(optarg, "%u", &par->rate) != 1 ||
	    par->rate < RATE_MIN || par->rate > RATE_MAX)
		errx(1, "%s: bad sample rate", optarg);
}

void
opt_vol(unsigned *vol)
{
	if (sscanf(optarg, "%u", vol) != 1 ||
	    *vol > MIDI_MAXCTL)
		errx(1, "%s: bad volume", optarg);
}

void
opt_enc(struct aparams *par)
{
	int len;

	len = aparams_strtoenc(par, optarg);
	if (len == 0 || optarg[len] != '\0')
		errx(1, "%s: bad encoding", optarg);
}

int
opt_hdr(void)
{
	if (strcmp("auto", optarg) == 0)
		return HDR_AUTO;
	if (strcmp("raw", optarg) == 0)
		return HDR_RAW;
	if (strcmp("wav", optarg) == 0)
		return HDR_WAV;
	errx(1, "%s: bad header specification", optarg);
}

int
opt_xrun(void)
{
	if (strcmp("ignore", optarg) == 0)
		return XRUN_IGNORE;
	if (strcmp("sync", optarg) == 0)
		return XRUN_SYNC;
	if (strcmp("error", optarg) == 0)
		return XRUN_ERROR;
	errx(1, "%s: onderrun/overrun policy", optarg);
}

int
opt_mode(void)
{
	if (strcmp("play", optarg) == 0)
		return MODE_PLAY;
	if (strcmp("rec", optarg) == 0)
		return MODE_REC;
	if (strcmp("duplex", optarg) == 0)
		return MODE_PLAY | MODE_REC;
	errx(1, "%s: bad mode", optarg);
}

/*
 * Arguments of -i, -o and -s options are stored in a list.
 */
struct farg {
	SLIST_ENTRY(farg) entry;
	struct aparams ipar;	/* input (read) parameters */
	struct aparams opar;	/* output (write) parameters */
	unsigned vol;		/* last requested volume */
	char *name;		/* optarg pointer (no need to copy it */
	int hdr;		/* header format */
	int xrun;		/* overrun/underrun policy */
};

SLIST_HEAD(farglist, farg);

/*
 * Add a farg entry to the given list, corresponding
 * to the given file name.
 */
void
farg_add(struct farglist *list,
    struct aparams *ipar, struct aparams *opar, unsigned vol,
    int hdr, int xrun, char *optarg)
{
	struct farg *fa;
	size_t namelen;

	fa = malloc(sizeof(struct farg));
	if (fa == NULL)
		err(1, "%s", optarg);

	if (hdr == HDR_AUTO) {
		namelen = strlen(optarg);
		if (namelen >= 4 &&
		    strcasecmp(optarg + namelen - 4, ".wav") == 0) {
			fa->hdr = HDR_WAV;
			DPRINTF("%s: assuming wav file format\n", optarg);
		} else {
			fa->hdr = HDR_RAW;
			DPRINTF("%s: assuming headerless file\n", optarg);
		}
	} else
		fa->hdr = hdr;
	fa->xrun = xrun;
	fa->ipar = *ipar;
	fa->opar = *opar;
	fa->vol = vol;
	fa->name = optarg;
	SLIST_INSERT_HEAD(list, fa, entry);
}

/*
 * Open an input file and setup converter if necessary.
 */
void
newinput(struct farg *fa)
{
	int fd;
	struct wav *f;
	struct aproc *proc;
	struct abuf *buf;
	unsigned nfr;

	if (strcmp(fa->name, "-") == 0) {
		fd = STDIN_FILENO;
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
			warn("stdin");
		fa->name = "stdin";
	} else {
		fd = open(fa->name, O_RDONLY | O_NONBLOCK, 0666);
		if (fd < 0)
			err(1, "%s", fa->name);
	}
	/*
	 * XXX : we should round rate, right ?
	 */
	f = wav_new_in(&wav_ops, fd, fa->name, &fa->ipar, fa->hdr);
	if (f == NULL) {
		if (fd != STDIN_FILENO)
			close(fd);
		return;
	}
	nfr = dev_bufsz * fa->ipar.rate / dev_rate;
	buf = abuf_new(nfr, &fa->ipar);
	proc = rpipe_new((struct file *)f);
	aproc_setout(proc, buf);
	abuf_fill(buf); /* XXX: move this in dev_attach() ? */
	dev_attach(fa->name, buf, &fa->ipar, fa->xrun,
	    NULL, NULL, 0, ADATA_UNIT);
	dev_setvol(buf, MIDI_TO_ADATA(fa->vol));
}

/*
 * Open an output file and setup converter if necessary.
 */
void
newoutput(struct farg *fa)
{
	int fd;
	struct wav *f;
	struct aproc *proc;
	struct abuf *buf;
	unsigned nfr;

	if (strcmp(fa->name, "-") == 0) {
		fd = STDOUT_FILENO;
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
			warn("stdout");
		fa->name = "stdout";
	} else {
		fd = open(fa->name,
		    O_WRONLY | O_TRUNC | O_CREAT | O_NONBLOCK, 0666);
		if (fd < 0)
			err(1, "%s", fa->name);
	}
	/*
	 * XXX : we should round rate, right ?
	 */
	f = wav_new_out(&wav_ops, fd, fa->name, &fa->opar, fa->hdr);
	if (f == NULL) {
		if (fd != STDOUT_FILENO)
			close(fd);
		return;
	}
	nfr = dev_bufsz * fa->opar.rate / dev_rate;
	proc = wpipe_new((struct file *)f);
	buf = abuf_new(nfr, &fa->opar);
	aproc_setin(proc, buf);
	dev_attach(fa->name, NULL, NULL, 0, buf, &fa->opar, fa->xrun, 0);
}

void
setsig(void)
{
	struct sigaction sa;

	quit_flag = 0;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigint;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		DPRINTF("sigaction(int) failed\n");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		DPRINTF("sigaction(term) failed\n");
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		DPRINTF("sigaction(hup) failed\n");
#ifdef DEBUG
	sa.sa_handler = sigusr1;
	if (sigaction(SIGUSR1, &sa, NULL) < 0)
		DPRINTF("sigaction(usr1) failed\n");
	sa.sa_handler = sigusr2;
	if (sigaction(SIGUSR2, &sa, NULL) < 0)
		DPRINTF("sigaction(usr2) failed1n");
#endif
}

void
unsetsig(void)
{
	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
#ifdef DEBUG
	if (sigaction(SIGUSR2, &sa, NULL) < 0)
		DPRINTF("unsetsig(usr2): sigaction failed\n");
	if (sigaction(SIGUSR1, &sa, NULL) < 0)
		DPRINTF("unsetsig(usr1): sigaction failed\n");
#endif
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		DPRINTF("unsetsig(hup): sigaction failed\n");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		DPRINTF("unsetsig(term): sigaction failed\n");
	if (sigaction(SIGINT, &sa, NULL) < 0)
		DPRINTF("unsetsig(int): sigaction failed\n");
}

void
getbasepath(char *base, size_t size)
{
	uid_t uid;
	struct stat sb;

	uid = geteuid();
	snprintf(base, PATH_MAX, "/tmp/aucat-%u", uid);
	if (mkdir(base, 0700) < 0) {
		if (errno != EEXIST)
			err(1, "mkdir(\"%s\")", base);
	}
	if (stat(base, &sb) < 0)
		err(1, "stat(\"%s\")", base);
	if (sb.st_uid != uid || (sb.st_mode & 077) != 0)
		errx(1, "%s has wrong permissions", base);
}

void
aucat_usage(void)
{
	(void)fputs("usage: " PROG_AUCAT " [-lnu] [-b nframes] "
	    "[-C min:max] [-c min:max] [-e enc] [-f device]\n"
	    "\t[-h fmt] [-i file] [-m mode] [-o file] [-r rate] [-s socket]\n"
	    "\t[-U unit] [-v volume] [-x policy]\n",
	    stderr);
}

int
aucat_main(int argc, char **argv)
{
	int c, u_flag, l_flag, n_flag, hdr, xrun, suspend = 0, unit;
	struct farg *fa;
	struct farglist  ifiles, ofiles, sfiles;
	struct aparams ipar, opar, dipar, dopar;
	char base[PATH_MAX], path[PATH_MAX];
	unsigned bufsz, mode;
	char *devpath;
	unsigned volctl;

	aparams_init(&ipar, 0, 1, 44100);
	aparams_init(&opar, 0, 1, 44100);
	u_flag = 0;
	l_flag = 0;
	n_flag = 0;
	unit = -1;
	devpath = NULL;
	SLIST_INIT(&ifiles);
	SLIST_INIT(&ofiles);
	SLIST_INIT(&sfiles);
	hdr = HDR_AUTO;
	xrun = XRUN_IGNORE;
	volctl = MIDI_MAXCTL;
	bufsz = 44100 * 4 / 15; /* XXX: use milliseconds, not frames */
	mode = 0;

	while ((c = getopt(argc, argv, "nb:c:C:e:r:h:x:v:i:o:f:m:lus:U:")) != -1) {
		switch (c) {
		case 'n':
			n_flag = 1;
			break;
		case 'm':
			mode = opt_mode();
			break;
		case 'h':
			hdr = opt_hdr();
			break;
		case 'x':
			xrun = opt_xrun();
			break;
		case 'c':
			opt_ch(&ipar);
			break;
		case 'C':
			opt_ch(&opar);
			break;
		case 'e':
			opt_enc(&ipar);
			aparams_copyenc(&opar, &ipar);
			break;
		case 'r':
			opt_rate(&ipar);
			opar.rate = ipar.rate;
			break;
		case 'v':
			opt_vol(&volctl);
			break;
		case 'i':
			farg_add(&ifiles, &ipar, &opar, volctl,
			    hdr, xrun, optarg);
			break;
		case 'o':
			farg_add(&ofiles, &ipar, &opar, volctl,
			    hdr, xrun, optarg);
			break;
		case 's':
			farg_add(&sfiles, &ipar, &opar, volctl,
			    hdr, xrun, optarg);
			break;
		case 'f':
			if (devpath)
				err(1, "only one -f allowed");
			devpath = optarg;
			dipar = opar;
			dopar = ipar;
			break;
		case 'l':
			l_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'b':
			if (sscanf(optarg, "%u", &bufsz) != 1 || bufsz == 0) {
				fprintf(stderr, "%s: bad buf size\n", optarg);
				exit(1);
			}
			break;
		case 'U':
			if (sscanf(optarg, "%u", &unit) != 1) {
				fprintf(stderr, "%s: bad device number\n", optarg);
				exit(1);
			}
			break;
		default:
			aucat_usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (!devpath) {
		dopar = ipar;
		dipar = opar;
	}
	if (!l_flag && SLIST_EMPTY(&ifiles) &&
	    SLIST_EMPTY(&ofiles) && argc > 0) {
		/*
		 * Legacy mode: if no -i or -o options are provided, and
		 * there are arguments then assume the arguments are files
		 * to play.
		 */
		for (c = 0; c < argc; c++)
			if (legacy_play(devpath, argv[c]) != 0) {
				errx(1, "%s: could not play\n", argv[c]);
			}
		exit(0);
	} else if (argc > 0) {
		aucat_usage();
		exit(1);
	}

	if (!l_flag && (!SLIST_EMPTY(&sfiles) || unit >= 0))
		errx(1, "can't use -s or -U without -l");
	if ((l_flag || mode != 0) &&
	    (!SLIST_EMPTY(&ofiles) || !SLIST_EMPTY(&ifiles)))
		errx(1, "can't use -l, -m and -s with -o or -i");
	if (!mode) {
		if (l_flag || !SLIST_EMPTY(&ifiles))
			mode |= MODE_PLAY;
		if (l_flag || !SLIST_EMPTY(&ofiles))
			mode |= MODE_REC;
		if (!mode) {
			aucat_usage();
			exit(1);
		}
	}
	if (n_flag) {
		if (devpath != NULL || l_flag)
			errx(1, "can't use -n with -f or -l");
		if (SLIST_EMPTY(&ifiles) || SLIST_EMPTY(&ofiles))
			errx(1, "both -i and -o are required with -n");
	}

	/*
	 * if there are no sockets paths provided use the default
	 */
	if (l_flag && SLIST_EMPTY(&sfiles)) {
		farg_add(&sfiles, &dopar, &dipar,
		    volctl, HDR_RAW, XRUN_IGNORE, DEFAULT_OPT);
	}

	if (!u_flag) {
		/*
		 * Calculate "best" device parameters. Iterate over all
		 * inputs and outputs and find the maximum sample rate
		 * and channel number.
		 */
		aparams_init(&dipar, dipar.cmin, dipar.cmax, dipar.rate);
		aparams_init(&dopar, dopar.cmin, dopar.cmax, dopar.rate);
		SLIST_FOREACH(fa, &ifiles, entry) {
			aparams_grow(&dopar, &fa->ipar);
		}
		SLIST_FOREACH(fa, &ofiles, entry) {
			aparams_grow(&dipar, &fa->opar);
		}
		SLIST_FOREACH(fa, &sfiles, entry) {
			aparams_grow(&dopar, &fa->ipar);
			aparams_grow(&dipar, &fa->opar);
		}
	}

	if (l_flag) {
		getbasepath(base, sizeof(base));
		if (unit < 0)
			unit = 0;
	}
	setsig();
	filelist_init();

	/*
	 * Open the device. Give half of the buffer to the device,
	 * the other half is for the socket/files
	 */
	if (n_flag) {
		dev_loopinit(&dipar, &dopar, bufsz);
	} else {
		if (!dev_init(devpath,
			(mode & MODE_REC) ? &dipar : NULL,
			(mode & MODE_PLAY) ? &dopar : NULL,
			bufsz)) {
			errx(1, "%s: can't open device", 
			    devpath ? devpath : "<default>");
		}
	}

	/*
	 * Create buffers for all input and output pipes.
	 */
	while (!SLIST_EMPTY(&ifiles)) {
		fa = SLIST_FIRST(&ifiles);
		SLIST_REMOVE_HEAD(&ifiles, entry);
		newinput(fa);
		free(fa);
	}
	while (!SLIST_EMPTY(&ofiles)) {
		fa = SLIST_FIRST(&ofiles);
		SLIST_REMOVE_HEAD(&ofiles, entry);
		newoutput(fa);
		free(fa);
	}
	while (!SLIST_EMPTY(&sfiles)) {
		fa = SLIST_FIRST(&sfiles);
		SLIST_REMOVE_HEAD(&sfiles, entry);
		opt_new(fa->name, &fa->opar, &fa->ipar, MIDI_TO_ADATA(fa->vol));
		free(fa);
	}
	if (l_flag) {
		snprintf(path, sizeof(path), "%s/%s%u", base,
		    DEFAULT_SOFTAUDIO, unit);
		listen_new(&listen_ops, path);
		if (debug_level == 0 && daemon(0, 0) < 0)
			err(1, "daemon");
	}

	/*
	 * loop, start audio
	 */
	for (;;) {
		if (quit_flag) {
			break;
		}
		if ((!dev_rec || dev_rec->u.io.file == NULL) &&
		    (!dev_play || dev_play->u.io.file == NULL) && !n_flag) {
			fprintf(stderr, "device desappeared, terminating\n");
			break;
		}
		if (!file_poll())
			break;
		if ((!dev_mix || dev_mix->u.mix.idle > 2 * dev_bufsz) &&
		    (!dev_sub || dev_sub->u.sub.idle > 2 * dev_bufsz)) {
			if (!l_flag)
				break;
			if (!suspend) {
				DPRINTF("suspending\n");
				suspend = 1;
				dev_stop();
				dev_clear();
			}
		}
		if ((dev_mix && dev_mix->u.mix.idle == 0) ||
		    (dev_sub && dev_sub->u.sub.idle == 0)) {
			if (suspend) {
				DPRINTF("resuming\n");
				suspend = 0;
				dev_start();
			}
		}
	}
	if (l_flag) {
		filelist_unlisten();
		if (rmdir(base) < 0)
			warn("rmdir(\"%s\")", base);
	}
	if (suspend) {
		DPRINTF("resuming to drain\n");
		suspend = 0;
		dev_start();
	}
	if (n_flag) {
		dev_loopdone();
	} else
		dev_done();
	filelist_done();
	unsetsig();
	return 0;
}

void
midicat_usage(void)
{
	(void)fputs("usage: " PROG_MIDICAT " [-l] [-f device] "
	    "[-i file] [-o file] [-U unit]\n",
	    stderr);
}
int
midicat_main(int argc, char **argv)
{
	static struct aparams noparams = { 1, 0, 0, 0, 0, 0, 0, 0 };
	int c, l_flag, unit, fd;
	char base[PATH_MAX], path[PATH_MAX];
	char *input, *output, *devpath;
	struct file *dev, *stdx, *f;
	struct aproc *p, *send, *recv;
	struct abuf *buf;

	l_flag = 0;
	unit = -1;
	devpath = NULL;
	output = NULL;
	input = NULL;

	while ((c = getopt(argc, argv, "i:o:lf:U:")) != -1) {
		switch (c) {
		case 'i':
			if (input != NULL)
				errx(1, "only one -i allowed");
			input = optarg;
			break;
		case 'o':
			if (output != NULL)
				errx(1, "only one -o allowed");
			output = optarg;
			break;
		case 'f':
			devpath = optarg;
			break;
		case 'l':
			l_flag = 1;
			break;
		case 'U':
			if (sscanf(optarg, "%u", &unit) != 1) {
				fprintf(stderr, "%s: bad device number\n", optarg);
				exit(1);
			}
			break;
		default:
			midicat_usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0 || (!input && !output && !l_flag)) {
		midicat_usage();
		exit(1);
	}
	if (!l_flag && unit >= 0)
		errx(1, "can't use -U without -l");
	if (l_flag) {
		if (input || output)
			errx(1, "can't use -i or -o with -l");
		getbasepath(base, sizeof(path));
		if (unit < 0)
			unit = 0;
	}
	setsig();
	filelist_init();

	if (l_flag) {
		thrubox = thru_new("thru");
		thrubox->refs++;
		snprintf(path, sizeof(path), "%s/%s%u", base,
		    DEFAULT_MIDITHRU, unit);
		listen_new(&listen_ops, path);
		if (debug_level == 0 && daemon(0, 0) < 0)
			err(1, "daemon");
	}
	if (input || output) {
		dev = (struct file *)miofile_new(&miofile_ops, devpath,
		     output ? 1 : 0, input ? 1 : 0);
		if (dev == NULL)
			errx(1, "%s: can't open device", 
			    devpath ? devpath : "<default>");
	} else
		dev = NULL;
	if (input) {
		send = wpipe_new(dev);
		send->refs++;
		if (strcmp(input, "-") == 0) {
			fd = STDIN_FILENO;
			if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
				warn("stdin");
		} else {
			fd = open(input, O_RDONLY | O_NONBLOCK, 0666);
			if (fd < 0)
				err(1, "%s", input);
		}
		stdx = (struct file *)pipe_new(&pipe_ops, fd, "stdin");
		p = rpipe_new(stdx);
		buf = abuf_new(3125, &noparams);
		aproc_setout(p, buf);
		aproc_setin(send, buf);
	} else
		send = NULL;
	if (output) {
		recv = rpipe_new(dev);
		recv->refs++;
		if (strcmp(output, "-") == 0) {
			fd = STDOUT_FILENO;
			if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
				warn("stdout");
		} else {
			fd = open(output,
			    O_WRONLY | O_TRUNC | O_CREAT | O_NONBLOCK, 0666);
			if (fd < 0)
				err(1, "%s", output);
		}
		stdx = (struct file *)pipe_new(&pipe_ops, fd, "stdout");
		p = wpipe_new(stdx);
		buf = abuf_new(3125, &noparams);
		aproc_setin(p, buf);
		aproc_setout(recv, buf);
	} else
		recv = NULL;

	/*
	 * loop, start processing
	 */
	for (;;) {
		if (quit_flag) {
			break;
		}
		if (!file_poll())
			break;
	}
	if (l_flag) {
		filelist_unlisten();
		if (rmdir(base) < 0)
			warn("rmdir(\"%s\")", base);
	}
	if (thrubox) {
	restart_thrubox:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc && aproc_depend(thrubox, f->rproc)) {
				file_eof(f);
				goto restart_thrubox;
			}
		}
		while (!LIST_EMPTY(&thrubox->ibuflist)) {
			if (!file_poll())
				break;
		}
		thrubox->refs--;
		aproc_del(thrubox);
		thrubox = NULL;
		while (file_poll())
			; /* nothing */
	}
	if (send) {
	restart_send:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc && aproc_depend(send, f->rproc)) {
				file_eof(f);
				goto restart_send;
			}
		}
		while (!LIST_EMPTY(&send->ibuflist)) {
			if (!file_poll())
				break;
		}
		send->refs--;
		aproc_del(send);
		send = NULL;
	}
	if (recv) {
		if (recv->u.io.file)
			file_eof(recv->u.io.file);
		while (!LIST_EMPTY(&recv->obuflist)) {
			if (!file_poll())
				break;
		}
		recv->refs--;
		aproc_del(recv);
		recv = NULL;
	}
	filelist_done();
	unsetsig();
	return 0;
}


int
main(int argc, char **argv)
{
	char *prog;

	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;
	if (strcmp(prog, PROG_AUCAT) == 0) {
		set_debug_level("AUCAT_DEBUG");
		return aucat_main(argc, argv);
	} else if (strcmp(prog, PROG_MIDICAT) == 0) {
		set_debug_level("MIDICAT_DEBUG");
		return midicat_main(argc, argv);
	} else {
		fprintf(stderr, "%s: can't determine program to run\n", prog);
	}
	return 1;
}
