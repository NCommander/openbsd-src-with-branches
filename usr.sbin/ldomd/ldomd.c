/*	$OpenBSD: ldomd.c,v 1.1 2012/10/26 18:26:13 kettenis Exp $	*/

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
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ds.h"
#include "mdesc.h"
#include "util.h"
#include "ldomd.h"

struct hv_io {
	uint64_t	hi_cookie;
	void		*hi_addr;
	size_t		hi_len;
};

#define HVIOCREAD	_IOW('h', 0, struct hv_io)
#define HVIOCWRITE	_IOW('h', 1, struct hv_io)

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

struct hvctl_reconfig {
	uint64_t	hvmdp;
	uint32_t	guestid;
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
		struct hvctl_reconfig	reconfig;
		struct hvctl_guest_op	guestop;
		struct hvctl_res_status	resstat;
	} msg;
};

#define HVCTL_OP_GET_HVCONFIG	3
#define HVCTL_OP_RECONFIGURE	4
#define HVCTL_OP_GUEST_START	5
#define HVCTL_OP_GUEST_STOP	6
#define HVCTL_OP_GET_RES_STAT	11

#define HVCTL_RES_GUEST		0

#define HVCTL_INFO_GUEST_STATE		0
#define HVCTL_INFO_GUEST_SOFT_STATE	1
#define HVCTL_INFO_GUEST_UTILISATION	3

TAILQ_HEAD(guest_head, guest) guests;

void add_guest(struct md_node *);
void map_domain_services(struct md *);

void frag_init(void);
void add_frag_mblock(struct md_node *);
void add_frag(uint64_t);
void delete_frag(uint64_t);
uint64_t alloc_frag(void);

void hv_update_md(struct guest *guest);
void hv_write(uint64_t, void *, size_t);

int hvctl_seq = 1;
int hvctl_fd;

void *hvmd_buf;
size_t hvmd_len;
struct md *hvmd;
uint64_t hv_mdpa;

int
main(int argc, char **argv)
{
	struct hvctl_msg msg;
	struct hv_io hi;
	ssize_t nbytes;
	uint64_t code;
	struct md_header hdr;
	struct md_node *node;
	struct md_prop *prop;
	struct guest *guest;

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

	hv_mdpa = msg.msg.hvcnf.hvmdp;

	hi.hi_cookie = hv_mdpa;
	hi.hi_addr = &hdr;
	hi.hi_len = sizeof(hdr);

	if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
		err(1, "ioctl");

	hvmd_len = sizeof(hdr) + hdr.node_blk_sz + hdr.name_blk_sz +
	    hdr.data_blk_sz;
	hvmd_buf = xmalloc(hvmd_len);

	hi.hi_cookie = hv_mdpa;
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

	frag_init();

	TAILQ_FOREACH(guest, &guests, link) {
		struct ds_conn *dc;
		char path[64];

		if (strcmp(guest->name, "primary") == 0)
			continue;

		snprintf(path, sizeof(path), "/dev/ldom-%s", guest->name);
		dc = ds_conn_open(path, guest);
		ds_conn_register_service(dc, &var_config_service);
	}

	ds_conn_serve();

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
	struct hv_io hi;
	struct md_header hdr;
	void *buf;
	size_t len;

	guest = xmalloc(sizeof(*guest));

	if (!md_get_prop_str(hvmd, node, "name", &guest->name))
		goto free;
	if (!md_get_prop_val(hvmd, node, "gid", &guest->gid))
		goto free;
	if (!md_get_prop_val(hvmd, node, "mdpa", &guest->mdpa))
		goto free;

	hi.hi_cookie = guest->mdpa;
	hi.hi_addr = &hdr;
	hi.hi_len = sizeof(hdr);

	if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
		err(1, "ioctl");

	len = sizeof(hdr) + hdr.node_blk_sz + hdr.name_blk_sz +
	    hdr.data_blk_sz;
	buf = xmalloc(len);

	hi.hi_cookie = guest->mdpa;
	hi.hi_addr = buf;
	hi.hi_len = len;

	if (ioctl(hvctl_fd, HVIOCREAD, &hi) == -1)
		err(1, "ioctl");

	guest->node = node;
	guest->md = md_ingest(buf, len);
	if (strcmp(guest->name, "primary") == 0)
		map_domain_services(guest->md);

	TAILQ_INSERT_TAIL(&guests, guest, link);
	return;

free:
	free(guest);
}

void
map_domain_services(struct md *md)
{
	struct md_node *node;
	const char *name;
	char source[64];
	char target[64];
	int unit = 0;

	TAILQ_FOREACH(node, &md->node_list, link) {
		if (strcmp(node->name->str, "virtual-device-port") != 0)
			continue;

		if (!md_get_prop_str(md, node, "vldc-svc-name", &name))
			continue;

		if (strncmp(name, "ldom-", 5) != 0 ||
		    strcmp(name, "ldom-primary") == 0)
			continue;

		snprintf(source, sizeof(source), "/dev/ldom%d", unit++);
		snprintf(target, sizeof(target), "/dev/%s", name);
		unlink(target);
		symlink(source, target);
	}
}

struct frag {
	TAILQ_ENTRY(frag) link;
	uint64_t base;
};

TAILQ_HEAD(frag_head, frag) free_frags;

uint64_t fragsize;

void
frag_init(void)
{
	struct md_node *node;
	struct md_prop *prop;

	node = md_find_node(hvmd, "frag_space");
	md_get_prop_val(hvmd, node, "fragsize", &fragsize);
	TAILQ_INIT(&free_frags);
	TAILQ_FOREACH(prop, &node->prop_list, link) {
		if (prop->tag == MD_PROP_ARC &&
		    strcmp(prop->name->str, "fwd") == 0)
			add_frag_mblock(prop->d.arc.node);
	}
}

void
add_frag_mblock(struct md_node *node)
{
	uint64_t base, size;
	struct guest *guest;

	md_get_prop_val(hvmd, node, "base", &base);
	md_get_prop_val(hvmd, node, "size", &size);
	while (size > fragsize) {
		add_frag(base);
		size -= fragsize;
		base += fragsize;
	}

	delete_frag(hv_mdpa);
	TAILQ_FOREACH(guest, &guests, link)
		delete_frag(guest->mdpa);
}

void
add_frag(uint64_t base)
{
	struct frag *frag;

	frag = xmalloc(sizeof(*frag));
	frag->base = base;
	TAILQ_INSERT_TAIL(&free_frags, frag, link);
}

void
delete_frag(uint64_t base)
{
	struct frag *frag;
	struct frag *tmp;

	TAILQ_FOREACH_SAFE(frag, &free_frags, link, tmp) {
		if (frag->base == base) {
			TAILQ_REMOVE(&free_frags, frag, link);
			free(frag);
		}
	}
}

uint64_t
alloc_frag(void)
{
	struct frag *frag;
	uint64_t base;

	frag = TAILQ_FIRST(&free_frags);
	if (frag == NULL)
		return -1;

	TAILQ_REMOVE(&free_frags, frag, link);
	base = frag->base;
	free(frag);

	return base;
}

void
hv_update_md(struct guest *guest)
{
	struct hvctl_msg msg;
	size_t nbytes;
	void *buf;
	size_t size;
	uint64_t mdpa;

	mdpa = alloc_frag();
	size = md_exhume(guest->md, &buf);
	hv_write(mdpa, buf, size);
	add_frag(guest->mdpa);
	guest->mdpa = mdpa;
	free(buf);

	md_set_prop_val(hvmd, guest->node, "mdpa", guest->mdpa);

	mdpa = alloc_frag();
	size = md_exhume(hvmd, &buf);
	hv_write(mdpa, buf, size);
	add_frag(hv_mdpa);
	hv_mdpa = mdpa;
	free(buf);

	/* Update config.  */
	bzero(&msg, sizeof(msg));
	msg.hdr.op = HVCTL_OP_RECONFIGURE;
	msg.hdr.seq = hvctl_seq++;
	msg.msg.reconfig.guestid = -1;
	msg.msg.reconfig.hvmdp = hv_mdpa;
	nbytes = write(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "write");

	bzero(&msg, sizeof(msg));
	nbytes = read(hvctl_fd, &msg, sizeof(msg));
	if (nbytes != sizeof(msg))
		err(1, "read");
	printf("status %d\n", msg.hdr.status);
}

void
hv_write(uint64_t addr, void *buf, size_t len)
{
	struct hv_io hi;

	hi.hi_cookie = addr;
	hi.hi_addr = buf;
	hi.hi_len = len;

	if (ioctl(hvctl_fd, HVIOCWRITE, &hi) == -1)
		err(1, "ioctl");
}
