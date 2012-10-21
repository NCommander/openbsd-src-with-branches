/*	$OpenBSD: ldomctl.c,v 1.6 2012/10/20 16:44:16 kettenis Exp $	*/

/*
 * Copyright (c) 2012 Mark Kettenis
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
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ds.h"
#include "mdesc.h"

#define DPRINTF(x)	printf x

struct hv_io {
	uint64_t	hi_cookie;
	void		*hi_addr;
	size_t		hi_len;
};

#define HVIOCREAD	_IOW('h', 0, struct hv_io)

#define SIS_NORMAL		0x1
#define SIS_TRANSITION		0x2
#define SOFT_STATE_SIZE		32

#define GUEST_STATE_STOPPED		0x0
#define GUEST_STATE_RESETTING		0x1
#define GUEST_STATE_NORMAL		0x2
#define GUEST_STATE_SUSPENDED		0x3
#define GUEST_STATE_EXITING		0x4
#define GUEST_STATE_UNCONFIGURED	0xff

#define HVCTL_RES_STATUS_DATA_SIZE	40

struct hvctl_header {
	uint16_t	op;
	uint16_t	seq;
	uint16_t	chksum;
	uint16_t	status;
};

struct hvctl_hello {
	uint16_t	major;
	uint16_t	minor;
};

struct hvctl_challenge {
	uint64_t	code;
};

struct hvctl_hvconfig {
	uint64_t	hv_membase;
	uint64_t	hv_memsize;
	uint64_t	hvmdp;
	uint64_t	del_reconf_hvmdp;
	uint32_t	del_reconf_gid;
};

struct hvctl_guest_op {
	uint32_t	guestid;
	uint32_t	code;
};

struct hvctl_res_status {
	uint32_t	res;
	uint32_t	resid;
	uint32_t	infoid;
	uint32_t	code;
	uint8_t         data[HVCTL_RES_STATUS_DATA_SIZE];
};

struct hvctl_rs_guest_state {
	uint64_t	state;
};

struct hvctl_rs_guest_softstate {
	uint8_t		soft_state;
	char		soft_state_str[SOFT_STATE_SIZE];
};

struct hvctl_rs_guest_util {
	uint64_t	lifespan;
	uint64_t	wallclock_delta;
	uint64_t	active_delta;
	uint64_t	stopped_cycles;
	uint64_t	yielded_cycles;
};

struct hvctl_msg {
	struct hvctl_header	hdr;
	union {
		struct hvctl_hello	hello;
		struct hvctl_challenge	clnge;
		struct hvctl_hvconfig	hvcnf;
		struct hvctl_guest_op	guestop;
		struct hvctl_res_status	resstat;
	} msg;
};

#define HVCTL_OP_GET_HVCONFIG	3
#define HVCTL_OP_GUEST_START	5
#define HVCTL_OP_GUEST_STOP	6
#define HVCTL_OP_GET_RES_STAT	11

#define HVCTL_RES_GUEST		0

#define HVCTL_INFO_GUEST_STATE		0
#define HVCTL_INFO_GUEST_SOFT_STATE	1
#define HVCTL_INFO_GUEST_UTILISATION	3

struct command {
	const char *cmd_name;
	void (*cmd_func)(int, char **);
};

__dead void usage(void);

struct guest {
	const char *name;
	uint64_t gid;
	uint64_t mdpa;

	int num_cpus;

	TAILQ_ENTRY(guest) link;
};

TAILQ_HEAD(guest_head, guest) guests;

void add_guest(struct md_node *);
uint64_t find_guest(const char *);

void fetch_pri(void);

void dump(int argc, char **argv);
void guest_start(int argc, char **argv);
void guest_stop(int argc, char **argv);
void guest_status(int argc, char **argv);

struct command commands[] = {
	{ "dump",	dump },
	{ "start",	guest_start },
	{ "stop",	guest_stop },
	{ "status",	guest_status },
	{ NULL,		NULL }
};

int hvctl_seq = 1;
int hvctl_fd;

void *hvmd_buf;
size_t hvmd_len;
struct md *hvmd;

extern void *pri_buf;
extern size_t pri_len;

int
main(int argc, char **argv)
{
	struct command *cmdp;
	struct hvctl_msg msg;
	struct hv_io hi;
	ssize_t nbytes;
	uint64_t code;
	struct md_header hdr;
	struct md_node *node;
	struct md_prop *prop;

	if (argc < 2)
		usage();

	/* Skip program name. */
	argv++;
	argc--;

	for (cmdp = commands; cmdp->cmd_name != NULL; cmdp++)
		if (strcmp(argv[0], cmdp->cmd_name) == 0)
			break;
	if (cmdp->cmd_name == NULL)
		usage();

	hvctl_fd = open("/dev/hvctl", O_RDWR, 0);
	if (hvctl_fd == -1)
		err(1, "open");

	/*
	 * Say "Hello".
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.seq = hvctl_seq++;
	msg.msg.hello.major = 1;
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	code = msg.msg.clnge.code ^ 0xbadbeef20;

	/*
	 * Respond to challenge.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = 2;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.clnge.code = code ^ 0x12cafe42a;
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	/*
	 * Request config.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GET_HVCONFIG;
	msg.hdr.seq = hvctl_seq++;
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");

	hi.hi_cookie = msg.msg.hvcnf.hvmdp;
	hi.hi_addr = &hdr;
	hi.hi_len = sizeof(hdr);

	if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
		err(1, "ioctl");

	hvmd_len = sizeof(hdr) + hdr.node_blk_sz + hdr.name_blk_sz +
	    hdr.data_blk_sz;
	hvmd_buf = xmalloc(hvmd_len);

	hi.hi_cookie = msg.msg.hvcnf.hvmdp;
	hi.hi_addr = hvmd_buf;
	hi.hi_len = hvmd_len;

	if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
		err(1, "ioctl");

	hvmd = md_ingest(hvmd_buf, hvmd_len);
	node = md_find_node(hvmd, "guests");
	TAILQ_INIT(&guests);
	TAILQ_FOREACH(prop, &node->prop_list, link) {
		if (prop->tag == MD_PROP_ARC &&
		    strcmp(prop->name->str, "fwd") == 0)
			add_guest(prop->d.arc.node);
	}

	(cmdp->cmd_func)(argc, argv);

	exit(EXIT_SUCCESS);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s start|stop domain\n", __progname);
	fprintf(stderr, "       %s status [domain]\n", __progname);
	exit(EXIT_FAILURE);
}

void
add_guest(struct md_node *node)
{
	struct guest *guest;
	struct md_prop *prop;

	guest = xmalloc (sizeof(*guest));

	if (!md_get_prop_str(hvmd, node, "name", &guest->name))
		goto free;
	if (!md_get_prop_val(hvmd, node, "gid", &guest->gid))
		goto free;
	if (!md_get_prop_val(hvmd, node, "mdpa", &guest->mdpa))
		goto free;

	guest->num_cpus = 0;
	TAILQ_FOREACH(prop, &node->prop_list, link) {
		if (prop->tag == MD_PROP_ARC &&
		    strcmp(prop->name->str, "fwd") == 0) {
			if (strcmp(prop->d.arc.node->name->str, "cpu") == 0)
				guest->num_cpus++;
		}
	}

	TAILQ_INSERT_TAIL(&guests, guest, link);
free:
	free(guest);
}

uint64_t
find_guest(const char *name)
{
	struct guest *guest;

	TAILQ_FOREACH(guest, &guests, link) {
		if (strcmp(guest->name, name) == 0)
			return guest->gid;
	}

	errx(EXIT_FAILURE, "unknown guest '%s'", name);
}

void
fetch_pri(void)
{
	struct ldc_conn lc;
	ssize_t nbytes;
	int fd;

	fd = open("/dev/spds", O_RDWR, 0);
	if (fd == -1)
		err(1, "open");

	memset(&lc, 0, sizeof(lc));
	lc.lc_fd = fd;
	lc.lc_rx_data = ds_rx_msg;

	while (pri_buf == NULL) {
		struct ldc_pkt lp;

		bzero(&lp, sizeof(lp));
		nbytes = read(fd, &lp, sizeof(lp));
		if (nbytes != sizeof(lp))
			err(1, "read");

#if 0
	{
		uint64_t *msg = (uint64_t *)&lp;
		int i;

		for (i = 0; i < 8; i++)
			printf("%02x: %016llx\n", i, msg[i]);
	}
#endif

		switch (lp.type) {
		case LDC_CTRL:
			ldc_rx_ctrl(&lc, &lp);
			break;
		case LDC_DATA:
			ldc_rx_data(&lc, &lp);
			break;
		default:
			DPRINTF(("%0x02/%0x02/%0x02\n", lp.type, lp.stype,
			    lp.ctrl));
			ldc_reset(&lc);
			break;
		}
	}

	close(fd);
}

void
dump(int argc, char **argv)
{
	struct guest *guest;
	struct hv_io hi;
	struct md_header hdr;
	void *md_buf;
	size_t md_len;
	char *name;
	FILE *fp;

	if (argc != 1)
		usage();

	fp = fopen("hv.md", "w");
	if (fp == NULL)
		err(1, "fopen");
	fwrite(hvmd_buf, hvmd_len, 1, fp);
	fclose(fp);

	fetch_pri();

	fp = fopen("pri", "w");
	if (fp == NULL)
		err(1, "fopen");
	fwrite(pri_buf, pri_len, 1, fp);
	fclose(fp);

	TAILQ_FOREACH(guest, &guests, link) {
		hi.hi_cookie = guest->mdpa;
		hi.hi_addr = &hdr;
		hi.hi_len = sizeof(hdr);

		if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
			err(1, "ioctl");

		md_len = sizeof(hdr) + hdr.node_blk_sz + hdr.name_blk_sz +
		    hdr.data_blk_sz;
		md_buf = xmalloc(md_len);

		hi.hi_cookie = guest->mdpa;
		hi.hi_addr = md_buf;
		hi.hi_len = md_len;

		if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
			err(1, "ioctl");

		if (asprintf(&name, "%s.md", guest->name) == -1)
			err(1, "asprintf");

		fp = fopen(name, "w");
		if (fp == NULL)
			err(1, "fopen");
		fwrite(md_buf, md_len, 1, fp);
		fclose(fp);

		free(name);
		free(md_buf);
	}
}

void
guest_start(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;

	if (argc < 2)
		usage();

	/*
	 * Start guest domain.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GUEST_START;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.guestop.guestid = find_guest(argv[1]);
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}

void
guest_stop(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;

	if (argc < 2)
		usage();

	/*
	 * Stop guest domain.
	 */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_GUEST_STOP;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.guestop.guestid = find_guest(argv[1]);
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
}

void
guest_status(int argc, char **argv)
{
	struct hvctl_msg msg;
	ssize_t nbytes;
	struct hvctl_rs_guest_state state;
	struct hvctl_rs_guest_softstate softstate;
	struct hvctl_rs_guest_util util;
	struct guest *guest;
	uint64_t gid = -1;
	uint64_t total_cycles, yielded_cycles;
	double utilisation = 0.0;
	const char *state_str;
	char buf[64];

	if (argc < 1 || argc > 2)
		usage();
	if (argc == 2)
		gid = find_guest(argv[1]);

	TAILQ_FOREACH(guest, &guests, link) {
		if (gid != -1 && guest->gid != gid)
			continue;

		/*
		 * Request status.
		 */
		bzero(&msg, sizeof(msg));
		msg.hdr.op = HVCTL_OP_GET_RES_STAT;
		msg.hdr.seq = hvctl_seq++;
		msg.msg.resstat.res = HVCTL_RES_GUEST;
		msg.msg.resstat.resid = guest->gid;
		msg.msg.resstat.infoid = HVCTL_INFO_GUEST_STATE;
		nbytes = write(hvctl_fd, &msg, sizeof(msg));
		if (nbytes != sizeof(msg))
			err(1, "write");

		bzero(&msg, sizeof(msg));
		nbytes = read(hvctl_fd, &msg, sizeof(msg));
		if (nbytes != sizeof(msg))
			err(1, "read");

		memcpy(&state, msg.msg.resstat.data, sizeof(state));
		switch (state.state) {
		case GUEST_STATE_STOPPED:
			state_str = "stopped";
			break;
		case GUEST_STATE_RESETTING:
			state_str = "resetting";
			break;
		case GUEST_STATE_NORMAL:
			state_str = "running";

			bzero(&msg, sizeof(msg));
			msg.hdr.op = HVCTL_OP_GET_RES_STAT;
			msg.hdr.seq = hvctl_seq++;
			msg.msg.resstat.res = HVCTL_RES_GUEST;
			msg.msg.resstat.resid = guest->gid;
			msg.msg.resstat.infoid = HVCTL_INFO_GUEST_SOFT_STATE;
			nbytes = write(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "write");

			bzero(&msg, sizeof(msg));
			nbytes = read(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "read");

			memcpy(&softstate, msg.msg.resstat.data,
			   sizeof(softstate));

			bzero(&msg, sizeof(msg));
			msg.hdr.op = HVCTL_OP_GET_RES_STAT;
			msg.hdr.seq = hvctl_seq++;
			msg.msg.resstat.res = HVCTL_RES_GUEST;
			msg.msg.resstat.resid = guest->gid;
			msg.msg.resstat.infoid = HVCTL_INFO_GUEST_UTILISATION;
			nbytes = write(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "write");

			bzero(&msg, sizeof(msg));
			nbytes = read(hvctl_fd, &msg, sizeof(msg));
			if (nbytes != sizeof(msg))
				err(1, "read");

			memcpy(&util, msg.msg.resstat.data, sizeof(util));

			total_cycles = util.active_delta * guest->num_cpus
			    - util.stopped_cycles;
			yielded_cycles = util.yielded_cycles;
			if (yielded_cycles <= total_cycles)
				utilisation = (100.0 * (total_cycles
				    - yielded_cycles)) / total_cycles;
			else
				utilisation = 0.0;

			break;
		case GUEST_STATE_SUSPENDED:
			state_str = "suspended";
			break;
		case GUEST_STATE_EXITING:
			state_str = "exiting";
			break;
		case GUEST_STATE_UNCONFIGURED:
			state_str = "unconfigured";
			break;
		default:
			snprintf(buf, sizeof(buf), "unknown (%lld)",
			    state.state);
			state_str = buf;
			break;
		}

		if (state.state != GUEST_STATE_NORMAL)
			printf("%-16s  %-16s\n", guest->name, state_str);
		else
			printf("%-16s  %-16s  %-32s  %3.0f%%\n", guest->name,
			       state_str, softstate.soft_state_str,
			       utilisation);
	}
}
