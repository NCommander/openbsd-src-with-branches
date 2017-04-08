/*	$OpenBSD: virtio.c,v 1.40 2017/03/26 22:19:47 mlarkin Exp $	*/

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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

#include <sys/param.h>	/* PAGE_SIZE */
#include <sys/socket.h>

#include <machine/vmmvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pv/virtioreg.h>
#include <dev/pv/vioblkreg.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <event.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pci.h"
#include "vmd.h"
#include "vmm.h"
#include "virtio.h"
#include "loadfile.h"

extern char *__progname;

struct viornd_dev viornd;
struct vioblk_dev *vioblk;
struct vionet_dev *vionet;
struct vmmci_dev vmmci;

int nr_vionet;

#define MAXPHYS	(64 * 1024)	/* max raw I/O transfer size */

#define VIRTIO_NET_F_MAC	(1<<5)

#define VMMCI_F_TIMESYNC	(1<<0)
#define VMMCI_F_ACK		(1<<1)

const char *
vioblk_cmd_name(uint32_t type)
{
	switch (type) {
	case VIRTIO_BLK_T_IN: return "read";
	case VIRTIO_BLK_T_OUT: return "write";
	case VIRTIO_BLK_T_SCSI_CMD: return "scsi read";
	case VIRTIO_BLK_T_SCSI_CMD_OUT: return "scsi write";
	case VIRTIO_BLK_T_FLUSH: return "flush";
	case VIRTIO_BLK_T_FLUSH_OUT: return "flush out";
	case VIRTIO_BLK_T_GET_ID: return "get id";
	default: return "unknown";
	}
}

static void
dump_descriptor_chain(struct vring_desc *desc, int16_t dxx)
{
	log_debug("descriptor chain @ %d", dxx);
	do {
		log_debug("desc @%d addr/len/flags/next = 0x%llx / 0x%x "
		    "/ 0x%x / 0x%x",
		    dxx,
		    desc[dxx].addr,
		    desc[dxx].len,
		    desc[dxx].flags,
		    desc[dxx].next);
		dxx = desc[dxx].next;
	} while (desc[dxx].flags & VRING_DESC_F_NEXT);

	log_debug("desc @%d addr/len/flags/next = 0x%llx / 0x%x / 0x%x "
	    "/ 0x%x",
	    dxx,
	    desc[dxx].addr,
	    desc[dxx].len,
	    desc[dxx].flags,
	    desc[dxx].next);
}

static const char *
virtio_reg_name(uint8_t reg)
{
	switch (reg) {
	case VIRTIO_CONFIG_DEVICE_FEATURES: return "device feature";
	case VIRTIO_CONFIG_GUEST_FEATURES: return "guest feature";
	case VIRTIO_CONFIG_QUEUE_ADDRESS: return "queue address";
	case VIRTIO_CONFIG_QUEUE_SIZE: return "queue size";
	case VIRTIO_CONFIG_QUEUE_SELECT: return "queue select";
	case VIRTIO_CONFIG_QUEUE_NOTIFY: return "queue notify";
	case VIRTIO_CONFIG_DEVICE_STATUS: return "device status";
	case VIRTIO_CONFIG_ISR_STATUS: return "isr status";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI: return "device config 0";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4: return "device config 1";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 8: return "device config 2";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 12: return "device config 3";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 16: return "device config 4";
	default: return "unknown";
	}
}

uint32_t
vring_size(uint32_t vq_size)
{
	uint32_t allocsize1, allocsize2;

	/* allocsize1: descriptor table + avail ring + pad */
	allocsize1 = VIRTQUEUE_ALIGN(sizeof(struct vring_desc) * vq_size
	    + sizeof(uint16_t) * (2 + vq_size));
	/* allocsize2: used ring + pad */
	allocsize2 = VIRTQUEUE_ALIGN(sizeof(uint16_t) * 2
	    + sizeof(struct vring_used_elem) * vq_size);

	return allocsize1 + allocsize2;
}

/* Update queue select */
void
viornd_update_qs(void)
{
	/* Invalid queue? */
	if (viornd.cfg.queue_select > 0)
		return;

	/* Update queue address/size based on queue select */
	viornd.cfg.queue_address = viornd.vq[viornd.cfg.queue_select].qa;
	viornd.cfg.queue_size = viornd.vq[viornd.cfg.queue_select].qs;
}

/* Update queue address */
void
viornd_update_qa(void)
{
	/* Invalid queue? */
	if (viornd.cfg.queue_select > 0)
		return;

	viornd.vq[viornd.cfg.queue_select].qa = viornd.cfg.queue_address;
}

int
viornd_notifyq(void)
{
	uint64_t q_gpa;
	uint32_t vr_sz;
	size_t sz;
	int ret;
	char *buf, *rnd_data;
	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;

	ret = 0;

	/* Invalid queue? */
	if (viornd.cfg.queue_notify > 0)
		return (0);

	vr_sz = vring_size(VIORND_QUEUE_SIZE);
	q_gpa = viornd.vq[viornd.cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	buf = calloc(1, vr_sz);
	if (buf == NULL) {
		log_warn("calloc error getting viornd ring");
		return (0);
	}

	if (read_mem(q_gpa, buf, vr_sz)) {
		free(buf);
		return (0);
	}

	desc = (struct vring_desc *)(buf);
	avail = (struct vring_avail *)(buf +
	    viornd.vq[viornd.cfg.queue_notify].vq_availoffset);
	used = (struct vring_used *)(buf +
	    viornd.vq[viornd.cfg.queue_notify].vq_usedoffset);

	sz = desc[avail->ring[avail->idx]].len;
	if (sz > MAXPHYS)
		fatal("viornd descriptor size too large (%zu)", sz);

	rnd_data = malloc(sz);

	if (rnd_data != NULL) {
		arc4random_buf(rnd_data, desc[avail->ring[avail->idx]].len);
		if (write_mem(desc[avail->ring[avail->idx]].addr,
		    rnd_data, desc[avail->ring[avail->idx]].len)) {
			log_warnx("viornd: can't write random data @ "
			    "0x%llx",
			    desc[avail->ring[avail->idx]].addr);
		} else {
			/* ret == 1 -> interrupt needed */
			/* XXX check VIRTIO_F_NO_INTR */
			ret = 1;
			viornd.cfg.isr_status = 1;
			used->ring[used->idx].id = avail->ring[avail->idx];
			used->ring[used->idx].len =
			    desc[avail->ring[avail->idx]].len;
			used->idx++;

			if (write_mem(q_gpa, buf, vr_sz)) {
				log_warnx("viornd: error writing vio ring");
			}
		}
		free(rnd_data);
	} else
		fatal("memory allocation error for viornd data");

	free(buf);

	return (ret);
}

int
virtio_rnd_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *unused, uint8_t sz)
{
	*intr = 0xFF;

	if (dir == 0) {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			log_warnx("%s: illegal write %x to %s",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			viornd.cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			viornd.cfg.queue_address = *data;
			viornd_update_qa();
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			viornd.cfg.queue_select = *data;
			viornd_update_qs();
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			viornd.cfg.queue_notify = *data;
			if (viornd_notifyq())
				*intr = 1;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			viornd.cfg.device_status = *data;
			break;
		}
	} else {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = viornd.cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = viornd.cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			*data = viornd.cfg.queue_address;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			*data = viornd.cfg.queue_size;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = viornd.cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = viornd.cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			*data = viornd.cfg.device_status;
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = viornd.cfg.isr_status;
			viornd.cfg.isr_status = 0;
			break;
		}
	}
	return (0);
}

void
vioblk_update_qa(struct vioblk_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 0)
		return;

	dev->vq[dev->cfg.queue_select].qa = dev->cfg.queue_address;
}

void
vioblk_update_qs(struct vioblk_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 0)
		return;

	/* Update queue address/size based on queue select */
	dev->cfg.queue_address = dev->vq[dev->cfg.queue_select].qa;
	dev->cfg.queue_size = dev->vq[dev->cfg.queue_select].qs;
}

static char *
vioblk_do_read(struct vioblk_dev *dev, off_t sector, ssize_t sz)
{
	char *buf;

	buf = malloc(sz);
	if (buf == NULL) {
		log_warn("malloc errror vioblk read");
		return (NULL);
	}

	if (lseek(dev->fd, sector * VIRTIO_BLK_SECTOR_SIZE,
	    SEEK_SET) == -1) {
		log_warn("seek error in vioblk read");
		free(buf);
		return (NULL);
	}

	if (read(dev->fd, buf, sz) != sz) {
		log_warn("vioblk read error");
		free(buf);
		return (NULL);
	}

	return buf;
}

static int
vioblk_do_write(struct vioblk_dev *dev, off_t sector, char *buf, ssize_t sz)
{
	if (lseek(dev->fd, sector * VIRTIO_BLK_SECTOR_SIZE,
	    SEEK_SET) == -1) {
		log_warn("seek error in vioblk write");
		return (1);
	}

	if (write(dev->fd, buf, sz) != sz) {
		log_warn("vioblk write error");
		return (1);
	}

	return (0);
}

/*
 * XXX in various cases, ds should be set to VIRTIO_BLK_S_IOERR, if we can
 * XXX cant trust ring data from VM, be extra cautious.
 */
int
vioblk_notifyq(struct vioblk_dev *dev)
{
	uint64_t q_gpa;
	uint32_t vr_sz;
	uint16_t idx, cmd_desc_idx, secdata_desc_idx, ds_desc_idx;
	uint8_t ds;
	int ret;
	off_t secbias;
	char *vr, *secdata;
	struct vring_desc *desc, *cmd_desc, *secdata_desc, *ds_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_blk_req_hdr cmd;

	ret = 0;

	/* Invalid queue? */
	if (dev->cfg.queue_notify > 0)
		return (0);

	vr_sz = vring_size(VIOBLK_QUEUE_SIZE);
	q_gpa = dev->vq[dev->cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = calloc(1, vr_sz);
	if (vr == NULL) {
		log_warn("calloc error getting vioblk ring");
		return (0);
	}

	if (read_mem(q_gpa, vr, vr_sz)) {
		log_warnx("error reading gpa 0x%llx", q_gpa);
		goto out;
	}

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_availoffset);
	used = (struct vring_used *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_usedoffset);

	idx = dev->vq[dev->cfg.queue_notify].last_avail & VIOBLK_QUEUE_MASK;

	if ((avail->idx & VIOBLK_QUEUE_MASK) == idx) {
		log_warnx("vioblk queue notify - nothing to do?");
		goto out;
	}

	while (idx != (avail->idx & VIOBLK_QUEUE_MASK)) {

		cmd_desc_idx = avail->ring[idx] & VIOBLK_QUEUE_MASK;
		cmd_desc = &desc[cmd_desc_idx];

		if ((cmd_desc->flags & VRING_DESC_F_NEXT) == 0) {
			log_warnx("unchained vioblk cmd descriptor received "
			    "(idx %d)", cmd_desc_idx);
			goto out;
		}

		/* Read command from descriptor ring */
		if (read_mem(cmd_desc->addr, &cmd, cmd_desc->len)) {
			log_warnx("vioblk: command read_mem error @ 0x%llx",
			    cmd_desc->addr);
			goto out;
		}

		switch (cmd.type) {
		case VIRTIO_BLK_T_IN:
			/* first descriptor */
			secdata_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			secdata_desc = &desc[secdata_desc_idx];

			if ((secdata_desc->flags & VRING_DESC_F_NEXT) == 0) {
				log_warnx("unchained vioblk data descriptor "
				    "received (idx %d)", cmd_desc_idx);
				goto out;
			}

			secbias = 0;
			do {
				/* read the data (use current data descriptor) */
				/*
				 * XXX waste to malloc secdata in vioblk_do_read
				 * and free it here over and over
				 */
				secdata = vioblk_do_read(dev, cmd.sector + secbias,
				    (ssize_t)secdata_desc->len);
				if (secdata == NULL) {
					log_warnx("vioblk: block read error, "
					    "sector %lld", cmd.sector);
					goto out;
				}

				if (write_mem(secdata_desc->addr, secdata,
				    secdata_desc->len)) {
					log_warnx("can't write sector "
					    "data to gpa @ 0x%llx",
					    secdata_desc->addr);
					dump_descriptor_chain(desc, cmd_desc_idx);
					free(secdata);
					goto out;
				}

				free(secdata);

				secbias += (secdata_desc->len / VIRTIO_BLK_SECTOR_SIZE);
				secdata_desc_idx = secdata_desc->next &
				    VIOBLK_QUEUE_MASK;
				secdata_desc = &desc[secdata_desc_idx];
			} while (secdata_desc->flags & VRING_DESC_F_NEXT);

			ds_desc_idx = secdata_desc_idx;
			ds_desc = secdata_desc;

			ds = VIRTIO_BLK_S_OK;
			if (write_mem(ds_desc->addr, &ds, ds_desc->len)) {
				log_warnx("can't write device status data @ "
				    "0x%llx", ds_desc->addr);
				dump_descriptor_chain(desc, cmd_desc_idx);
				goto out;
			}


			ret = 1;
			dev->cfg.isr_status = 1;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].id = cmd_desc_idx;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].len = cmd_desc->len;
			used->idx++;

			dev->vq[dev->cfg.queue_notify].last_avail = avail->idx &
			    VIOBLK_QUEUE_MASK;

			if (write_mem(q_gpa, vr, vr_sz)) {
				log_warnx("vioblk: error writing vio ring");
			}
			break;
		case VIRTIO_BLK_T_OUT:
			secdata_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			secdata_desc = &desc[secdata_desc_idx];

			if ((secdata_desc->flags & VRING_DESC_F_NEXT) == 0) {
				log_warnx("wr vioblk: unchained vioblk data "
				    "descriptor received (idx %d)",
				    cmd_desc_idx);
				goto out;
			}

			if (secdata_desc->len > dev->max_xfer) {
				log_warnx("%s: invalid read size %d requested",
				    __func__, secdata_desc->len);
				goto out;
			}

			secdata = NULL;
			secbias = 0;
			do {
				secdata = realloc(secdata, secdata_desc->len);
				if (secdata == NULL) {
					log_warn("wr vioblk: malloc error, "
					    "len %d", secdata_desc->len);
					goto out;
				}

				if (read_mem(secdata_desc->addr, secdata,
				    secdata_desc->len)) {
					log_warnx("wr vioblk: can't read "
					    "sector data @ 0x%llx",
					    secdata_desc->addr);
					dump_descriptor_chain(desc,
					    cmd_desc_idx);
					free(secdata);
					goto out;
				}

				if (vioblk_do_write(dev, cmd.sector + secbias,
				    secdata, (ssize_t)secdata_desc->len)) {
					log_warnx("wr vioblk: disk write "
					    "error");
					free(secdata);
					goto out;
				}

				secbias += secdata_desc->len /
				    VIRTIO_BLK_SECTOR_SIZE;

				secdata_desc_idx = secdata_desc->next &
				    VIOBLK_QUEUE_MASK;
				secdata_desc = &desc[secdata_desc_idx];
			} while (secdata_desc->flags & VRING_DESC_F_NEXT);

			free(secdata);

			ds_desc_idx = secdata_desc_idx;
			ds_desc = secdata_desc;

			ds = VIRTIO_BLK_S_OK;
			if (write_mem(ds_desc->addr, &ds, ds_desc->len)) {
				log_warnx("wr vioblk: can't write device "
				    "status data @ 0x%llx", ds_desc->addr);
				dump_descriptor_chain(desc, cmd_desc_idx);
				goto out;
			}

			ret = 1;
			dev->cfg.isr_status = 1;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].id =
			    cmd_desc_idx;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].len =
			    cmd_desc->len;
			used->idx++;

			dev->vq[dev->cfg.queue_notify].last_avail = avail->idx &
			    VIOBLK_QUEUE_MASK;
			if (write_mem(q_gpa, vr, vr_sz))
				log_warnx("wr vioblk: error writing vio ring");
			break;
		case VIRTIO_BLK_T_FLUSH:
		case VIRTIO_BLK_T_FLUSH_OUT:
			ds_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			ds_desc = &desc[ds_desc_idx];

			ds = VIRTIO_BLK_S_OK;
			if (write_mem(ds_desc->addr, &ds, ds_desc->len)) {
				log_warnx("fl vioblk: can't write device status "
				    "data @ 0x%llx", ds_desc->addr);
				dump_descriptor_chain(desc, cmd_desc_idx);
				goto out;
			}

			ret = 1;
			dev->cfg.isr_status = 1;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].id =
			    cmd_desc_idx;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].len =
			    cmd_desc->len;
			used->idx++;

			dev->vq[dev->cfg.queue_notify].last_avail = avail->idx &
			    VIOBLK_QUEUE_MASK;
			if (write_mem(q_gpa, vr, vr_sz)) {
				log_warnx("fl vioblk: error writing vio ring");
			}
			break;
		case VIRTIO_BLK_T_GET_ID:
			ds_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			ds_desc = &desc[ds_desc_idx];

			ds = VIRTIO_BLK_S_UNSUPP;
			if (write_mem(ds_desc->addr, &ds, ds_desc->len)) {
				log_warnx("%s: get id : can't write device "
				    "status data @ 0x%llx", __func__,
				    ds_desc->addr);
				dump_descriptor_chain(desc, cmd_desc_idx);
				goto out;
			}

			ret = 1;
			dev->cfg.isr_status = 1;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].id =
			    cmd_desc_idx;
			used->ring[used->idx & VIOBLK_QUEUE_MASK].len =
			    cmd_desc->len;
			used->idx++;

			dev->vq[dev->cfg.queue_notify].last_avail = avail->idx &
			    VIOBLK_QUEUE_MASK;
			if (write_mem(q_gpa, vr, vr_sz)) {
				log_warnx("%s: get id : error writing vio ring",
				    __func__);
			}
			break;
		default:
			log_warnx("%s: unknown command 0x%x", __func__,
			    cmd.type);
			break;
		}

		idx = (idx + 1) & VIOBLK_QUEUE_MASK;
	}
out:
	free(vr);
	return (ret);
}

int
virtio_blk_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *cookie, uint8_t sz)
{
	struct vioblk_dev *dev = (struct vioblk_dev *)cookie;

	*intr = 0xFF;


	if (dir == 0) {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			log_warnx("%s: illegal write %x to %s",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			dev->cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			dev->cfg.queue_address = *data;
			vioblk_update_qa(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			dev->cfg.queue_select = *data;
			vioblk_update_qs(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			dev->cfg.queue_notify = *data;
			if (vioblk_notifyq(dev))
				*intr = 1;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			dev->cfg.device_status = *data;
			if (dev->cfg.device_status == 0) {
				log_debug("%s: device reset", __func__);
				dev->cfg.guest_feature = 0;
				dev->cfg.queue_address = 0;
				vioblk_update_qa(dev);
				dev->cfg.queue_size = 0;
				vioblk_update_qs(dev);
				dev->cfg.queue_select = 0;
				dev->cfg.queue_notify = 0;
				dev->cfg.isr_status = 0;
				dev->vq[0].last_avail = 0;
			}
			break;
		default:
			break;
		}
	} else {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
			switch (sz) {
			case 4:
				*data = (uint32_t)(dev->sz);
				break;
			case 2:
				*data &= 0xFFFF0000;
				*data |= (uint32_t)(dev->sz) & 0xFFFF;
				break;
			case 1:
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz) & 0xFF;
				break;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 1:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz >> 8) & 0xFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 2:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz >> 16) & 0xFF;
			} else if (sz == 2) {
				*data &= 0xFFFF0000;
				*data |= (uint32_t)(dev->sz >> 16) & 0xFFFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz >> 24) & 0xFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
			switch (sz) {
			case 4:
				*data = (uint32_t)(dev->sz >> 32);
				break;
			case 2:
				*data &= 0xFFFF0000;
				*data |= (uint32_t)(dev->sz >> 32) & 0xFFFF;
				break;
			case 1:
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz >> 32) & 0xFF;
				break;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz >> 40) & 0xFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 6:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz >> 48) & 0xFF;
			} else if (sz == 2) {
				*data &= 0xFFFF0000;
				*data |= (uint32_t)(dev->sz >> 48) & 0xFFFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 7:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->sz >> 56) & 0xFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 8:
			switch (sz) {
			case 4:
				*data = (uint32_t)(dev->max_xfer);
				break;
			case 2:
				*data &= 0xFFFF0000;
				*data |= (uint32_t)(dev->max_xfer) & 0xFFFF;
				break;
			case 1:
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->max_xfer) & 0xFF;
				break;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 9:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->max_xfer >> 8) & 0xFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 10:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->max_xfer >> 16) & 0xFF;
			} else if (sz == 2) {
				*data &= 0xFFFF0000;
				*data |= (uint32_t)(dev->max_xfer >> 16) & 0xFFFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 11:
			if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint32_t)(dev->max_xfer >> 24) & 0xFF;
			}
			/* XXX handle invalid sz */
			break;
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = dev->cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = dev->cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			*data = dev->cfg.queue_address;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			if (sz == 4)
				*data = dev->cfg.queue_size;
			else if (sz == 2) {
				*data &= 0xFFFF0000;
				*data |= (uint16_t)dev->cfg.queue_size;
			} else if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint8_t)dev->cfg.queue_size;
			}
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = dev->cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = dev->cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			if (sz == 4)
				*data = dev->cfg.device_status;
			else if (sz == 2) {
				*data &= 0xFFFF0000;
				*data |= (uint16_t)dev->cfg.device_status;
			} else if (sz == 1) {
				*data &= 0xFFFFFF00;
				*data |= (uint8_t)dev->cfg.device_status;
			}
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = dev->cfg.isr_status;
			dev->cfg.isr_status = 0;
			break;
		}
	}
	return (0);
}

int
virtio_net_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *cookie, uint8_t sz)
{
	struct vionet_dev *dev = (struct vionet_dev *)cookie;

	*intr = 0xFF;
	mutex_lock(&dev->mutex);

	if (dir == 0) {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			log_warnx("%s: illegal write %x to %s",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			dev->cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			dev->cfg.queue_address = *data;
			vionet_update_qa(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			dev->cfg.queue_select = *data;
			vionet_update_qs(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			dev->cfg.queue_notify = *data;
			if (vionet_notifyq(dev))
				*intr = 1;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			dev->cfg.device_status = *data;
			if (dev->cfg.device_status == 0) {
				log_debug("%s: device reset", __func__);
				dev->cfg.guest_feature = 0;
				dev->cfg.queue_address = 0;
				vionet_update_qa(dev);
				dev->cfg.queue_size = 0;
				vionet_update_qs(dev);
				dev->cfg.queue_select = 0;
				dev->cfg.queue_notify = 0;
				dev->cfg.isr_status = 0;
				dev->vq[0].last_avail = 0;
				dev->vq[0].notified_avail = 0;
				dev->vq[1].last_avail = 0;
				dev->vq[1].notified_avail = 0;
			}
			break;
		default:
			break;
		}
	} else {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 1:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 2:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
			*data = dev->mac[reg -
			    VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI];
			break;
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = dev->cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = dev->cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			*data = dev->cfg.queue_address;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			*data = dev->cfg.queue_size;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = dev->cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = dev->cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			*data = dev->cfg.device_status;
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = dev->cfg.isr_status;
			dev->cfg.isr_status = 0;
			break;
		}
	}

	mutex_unlock(&dev->mutex);
	return (0);
}

/*
 * Must be called with dev->mutex acquired.
 */
void
vionet_update_qa(struct vionet_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 1)
		return;

	dev->vq[dev->cfg.queue_select].qa = dev->cfg.queue_address;
}

/*
 * Must be called with dev->mutex acquired.
 */
void
vionet_update_qs(struct vionet_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 1)
		return;

	/* Update queue address/size based on queue select */
	dev->cfg.queue_address = dev->vq[dev->cfg.queue_select].qa;
	dev->cfg.queue_size = dev->vq[dev->cfg.queue_select].qs;
}

/*
 * Must be called with dev->mutex acquired.
 */
int
vionet_enq_rx(struct vionet_dev *dev, char *pkt, ssize_t sz, int *spc)
{
	uint64_t q_gpa;
	uint32_t vr_sz;
	uint16_t idx, pkt_desc_idx, hdr_desc_idx;
	ptrdiff_t off;
	int ret;
	char *vr;
	struct vring_desc *desc, *pkt_desc, *hdr_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct vring_used_elem *ue;

	ret = 0;

	if (!(dev->cfg.device_status & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK))
		return ret;

	vr_sz = vring_size(VIONET_QUEUE_SIZE);
	q_gpa = dev->vq[0].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = calloc(1, vr_sz);
	if (vr == NULL) {
		log_warn("rx enq: calloc error getting vionet ring");
		return (0);
	}

	if (read_mem(q_gpa, vr, vr_sz)) {
		log_warnx("rx enq: error reading gpa 0x%llx", q_gpa);
		goto out;
	}

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + dev->vq[0].vq_availoffset);
	used = (struct vring_used *)(vr + dev->vq[0].vq_usedoffset);

	idx = dev->vq[0].last_avail & VIONET_QUEUE_MASK;

	if ((dev->vq[0].notified_avail & VIONET_QUEUE_MASK) == idx) {
		log_debug("vionet queue notify - no space, dropping packet");
		goto out;
	}

	hdr_desc_idx = avail->ring[idx] & VIONET_QUEUE_MASK;
	hdr_desc = &desc[hdr_desc_idx];

	pkt_desc_idx = hdr_desc->next & VIONET_QUEUE_MASK;
	pkt_desc = &desc[pkt_desc_idx];

	/* must be not readable */
	if ((pkt_desc->flags & VRING_DESC_F_WRITE) == 0) {
		log_warnx("unexpected readable rx descriptor %d",
		    pkt_desc_idx);
		goto out;
	}

	/* Write packet to descriptor ring */
	if (write_mem(pkt_desc->addr, pkt, sz)) {
		log_warnx("vionet: rx enq packet write_mem error @ "
		    "0x%llx", pkt_desc->addr);
		goto out;
	}

	ret = 1;
	dev->cfg.isr_status = 1;
	ue = &used->ring[used->idx & VIONET_QUEUE_MASK];
	ue->id = hdr_desc_idx;
	ue->len = hdr_desc->len + sz;
	used->idx++;
	dev->vq[0].last_avail = (dev->vq[0].last_avail + 1);
	*spc = dev->vq[0].notified_avail - dev->vq[0].last_avail;

	off = (char *)ue - vr;
	if (write_mem(q_gpa + off, ue, sizeof *ue))
		log_warnx("vionet: error writing vio ring");
	else {
		off = (char *)&used->idx - vr;
		if (write_mem(q_gpa + off, &used->idx, sizeof used->idx))
			log_warnx("vionet: error writing vio ring");
	}
out:
	free(vr);
	return (ret);
}

/*
 * vionet_rx
 *
 * Enqueue data that was received on a tap file descriptor
 * to the vionet device queue.
 *
 * Must be called with dev->mutex acquired.
 */
static int
vionet_rx(struct vionet_dev *dev)
{
	char buf[PAGE_SIZE];
	int hasdata, num_enq = 0, spc = 0;
	struct ether_header *eh;
	ssize_t sz;

	do {
		sz = read(dev->fd, buf, sizeof buf);
		if (sz == -1) {
			/*
			 * If we get EAGAIN, No data is currently available.
			 * Do not treat this as an error.
			 */
			if (errno != EAGAIN)
				log_warn("unexpected read error on vionet "
				    "device");
		} else if (sz != 0) {
			eh = (struct ether_header *)buf;
			if (!dev->lockedmac || sz < ETHER_HDR_LEN ||
			    ETHER_IS_MULTICAST(eh->ether_dhost) ||
			    memcmp(eh->ether_dhost, dev->mac,
			    sizeof(eh->ether_dhost)) == 0)
				num_enq += vionet_enq_rx(dev, buf, sz, &spc);
		} else if (sz == 0) {
			log_debug("process_rx: no data");
			hasdata = 0;
			break;
		}

		hasdata = fd_hasdata(dev->fd);
	} while (spc && hasdata);

	dev->rx_pending = hasdata;
	return (num_enq);
}

/*
 * vionet_rx_event
 *
 * Called from the event handling thread when new data can be
 * received on the tap fd of a vionet device.
 */
static void
vionet_rx_event(int fd, short kind, void *arg)
{
	struct vionet_dev *dev = arg;

	mutex_lock(&dev->mutex);

	/*
	 * We already have other data pending to be received. The data that
	 * has become available now will be enqueued to the vionet_dev
	 * later.
	 */
	if (dev->rx_pending) {
		mutex_unlock(&dev->mutex);
		return;
	}

	if (vionet_rx(dev) > 0) {
		/* XXX: vcpu_id */
		vcpu_assert_pic_irq(dev->vm_id, 0, dev->irq);
	}

	mutex_unlock(&dev->mutex);
}

/*
 * vionet_process_rx
 *
 * Processes any remaining pending receivable data for a vionet device.
 * Called on VCPU exit. Although we poll on the tap file descriptor of
 * a vionet_dev in a separate thread, this function still needs to be
 * called on VCPU exit: it can happen that not all data fits into the
 * receive queue of the vionet_dev immediately. So any outstanding data
 * is handled here.
 *
 * Parameters:
 *  vm_id: VM ID of the VM for which to process vionet events
 */
void
vionet_process_rx(uint32_t vm_id)
{
	int i;

	for (i = 0 ; i < nr_vionet; i++) {
		mutex_lock(&vionet[i].mutex);
		if (!vionet[i].rx_added) {
			mutex_unlock(&vionet[i].mutex);
			continue;
		}

		if (vionet[i].rx_pending) {
			if (vionet_rx(&vionet[i])) {
				vcpu_assert_pic_irq(vm_id, 0, vionet[i].irq);
			}
		}
		mutex_unlock(&vionet[i].mutex);
	}
}

/*
 * Must be called with dev->mutex acquired.
 */
void
vionet_notify_rx(struct vionet_dev *dev)
{
	uint64_t q_gpa;
	uint32_t vr_sz;
	char *vr;
	struct vring_avail *avail;

	vr_sz = vring_size(VIONET_QUEUE_SIZE);
	q_gpa = dev->vq[dev->cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = malloc(vr_sz);
	if (vr == NULL) {
		log_warn("malloc error getting vionet ring");
		return;
	}

	if (read_mem(q_gpa, vr, vr_sz)) {
		log_warnx("error reading gpa 0x%llx", q_gpa);
		free(vr);
		return;
	}

	/* Compute offset into avail ring */
	avail = (struct vring_avail *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_availoffset);

	dev->rx_added = 1;
	dev->vq[0].notified_avail = avail->idx;

	free(vr);
}

/*
 * Must be called with dev->mutex acquired.
 *
 * XXX cant trust ring data from VM, be extra cautious.
 * XXX advertise link status to guest
 */
int
vionet_notifyq(struct vionet_dev *dev)
{
	uint64_t q_gpa;
	uint32_t vr_sz;
	uint16_t idx, pkt_desc_idx, hdr_desc_idx, dxx;
	size_t pktsz;
	int ret, num_enq, ofs;
	char *vr, *pkt;
	struct vring_desc *desc, *pkt_desc, *hdr_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct ether_header *eh;

	vr = pkt = NULL;
	ret = 0;

	/* Invalid queue? */
	if (dev->cfg.queue_notify != 1) {
		vionet_notify_rx(dev);
		goto out;
	}

	vr_sz = vring_size(VIONET_QUEUE_SIZE);
	q_gpa = dev->vq[dev->cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = calloc(1, vr_sz);
	if (vr == NULL) {
		log_warn("calloc error getting vionet ring");
		goto out;
	}

	if (read_mem(q_gpa, vr, vr_sz)) {
		log_warnx("error reading gpa 0x%llx", q_gpa);
		goto out;
	}

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_availoffset);
	used = (struct vring_used *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_usedoffset);

	num_enq = 0;

	idx = dev->vq[dev->cfg.queue_notify].last_avail & VIONET_QUEUE_MASK;

	if ((avail->idx & VIONET_QUEUE_MASK) == idx) {
		log_warnx("vionet tx queue notify - nothing to do?");
		goto out;
	}

	while ((avail->idx & VIONET_QUEUE_MASK) != idx) {
		hdr_desc_idx = avail->ring[idx] & VIONET_QUEUE_MASK;
		hdr_desc = &desc[hdr_desc_idx];
		pktsz = 0;

		dxx = hdr_desc_idx;
		do {
			pktsz += desc[dxx].len;
			dxx = desc[dxx].next;
		} while (desc[dxx].flags & VRING_DESC_F_NEXT);

		pktsz += desc[dxx].len;

		/* Remove virtio header descriptor len */
		pktsz -= hdr_desc->len;

		/*
		 * XXX check sanity pktsz
		 * XXX too long and  > PAGE_SIZE checks
		 *     (PAGE_SIZE can be relaxed to 16384 later)
		 */
		pkt = malloc(pktsz);
		if (pkt == NULL) {
			log_warn("malloc error alloc packet buf");
			goto out;
		}

		ofs = 0;
		pkt_desc_idx = hdr_desc->next & VIONET_QUEUE_MASK;
		pkt_desc = &desc[pkt_desc_idx];

		while (pkt_desc->flags & VRING_DESC_F_NEXT) {
			/* must be not writable */
			if (pkt_desc->flags & VRING_DESC_F_WRITE) {
				log_warnx("unexpected writable tx desc "
				    "%d", pkt_desc_idx);
				goto out;
			}

			/* Read packet from descriptor ring */
			if (read_mem(pkt_desc->addr, pkt + ofs,
			    pkt_desc->len)) {
				log_warnx("vionet: packet read_mem error "
				    "@ 0x%llx", pkt_desc->addr);
				goto out;
			}

			ofs += pkt_desc->len;
			pkt_desc_idx = pkt_desc->next & VIONET_QUEUE_MASK;
			pkt_desc = &desc[pkt_desc_idx];
		}

		/* Now handle tail descriptor - must be not writable */
		if (pkt_desc->flags & VRING_DESC_F_WRITE) {
			log_warnx("unexpected writable tx descriptor %d",
			    pkt_desc_idx);
			goto out;
		}

		/* Read packet from descriptor ring */
		if (read_mem(pkt_desc->addr, pkt + ofs,
		    pkt_desc->len)) {
			log_warnx("vionet: packet read_mem error @ "
			    "0x%llx", pkt_desc->addr);
			goto out;
		}

		/* reject other source addresses */
		if (dev->lockedmac && pktsz >= ETHER_HDR_LEN &&
		    (eh = (struct ether_header *)pkt) &&
		    memcmp(eh->ether_shost, dev->mac,
		    sizeof(eh->ether_shost)) != 0)
			log_debug("vionet: wrong source address %s for vm %d",
			    ether_ntoa((struct ether_addr *)
			    eh->ether_shost), dev->vm_id);
		/* XXX signed vs unsigned here, funky cast */
		else if (write(dev->fd, pkt, pktsz) != (int)pktsz) {
			log_warnx("vionet: tx failed writing to tap: "
			    "%d", errno);
			goto out;
		}

		ret = 1;
		dev->cfg.isr_status = 1;
		used->ring[used->idx & VIONET_QUEUE_MASK].id = hdr_desc_idx;
		used->ring[used->idx & VIONET_QUEUE_MASK].len = hdr_desc->len;
		used->idx++;

		dev->vq[dev->cfg.queue_notify].last_avail =
		    (dev->vq[dev->cfg.queue_notify].last_avail + 1);
		num_enq++;

		idx = dev->vq[dev->cfg.queue_notify].last_avail &
		    VIONET_QUEUE_MASK;
	}

	if (write_mem(q_gpa, vr, vr_sz)) {
		log_warnx("vionet: tx error writing vio ring");
	}

out:
	free(vr);
	free(pkt);

	return (ret);
}

int
vmmci_ctl(unsigned int cmd)
{
	struct timeval tv = { 0, 0 };

	if ((vmmci.cfg.device_status &
	    VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK) == 0)
		return (-1);

	if (cmd == vmmci.cmd)
		return (0);

	switch (cmd) {
	case VMMCI_NONE:
		break;
	case VMMCI_SHUTDOWN:
	case VMMCI_REBOOT:
		/* Update command */
		vmmci.cmd = cmd;

		/*
		 * vmm VMs do not support powerdown, send a reboot request
		 * instead and turn it off after the triple fault.
		 */
		if (cmd == VMMCI_SHUTDOWN)
			cmd = VMMCI_REBOOT;

		/* Trigger interrupt */
		vmmci.cfg.isr_status = VIRTIO_CONFIG_ISR_CONFIG_CHANGE;
		vcpu_assert_pic_irq(vmmci.vm_id, 0, vmmci.irq);

		/* Add ACK timeout */
		tv.tv_sec = VMMCI_TIMEOUT;
		evtimer_add(&vmmci.timeout, &tv);
		break;
	default:
		fatalx("invalid vmmci command: %d", cmd);
	}

	return (0);
}

void
vmmci_ack(unsigned int cmd)
{
	struct timeval	 tv = { 0, 0 };

	switch (cmd) {
	case VMMCI_NONE:
		break;
	case VMMCI_SHUTDOWN:
		/*
		 * The shutdown was requested by the VM if we don't have
		 * a pending shutdown request.  In this case add a short
		 * timeout to give the VM a chance to reboot before the
		 * timer is expired.
		 */
		if (vmmci.cmd == 0) {
			log_debug("%s: vm %u requested shutdown", __func__,
			    vmmci.vm_id);
			tv.tv_sec = VMMCI_TIMEOUT;
			evtimer_add(&vmmci.timeout, &tv);
			return;
		}
		/* FALLTHROUGH */
	case VMMCI_REBOOT:
		/*
		 * If the VM acknowleged our shutdown request, give it
		 * enough time to shutdown or reboot gracefully.  This
		 * might take a considerable amount of time (running
		 * rc.shutdown on the VM), so increase the timeout before
		 * killing it forcefully.
		 */
		if (cmd == vmmci.cmd &&
		    evtimer_pending(&vmmci.timeout, NULL)) {
			log_debug("%s: vm %u acknowledged shutdown request",
			    __func__, vmmci.vm_id);
			tv.tv_sec = VMMCI_SHUTDOWN_TIMEOUT;
			evtimer_add(&vmmci.timeout, &tv);
		}
		break;
	default:
		log_warnx("%s: illegal request %u", __func__, cmd);
		break;
	}
}

void
vmmci_timeout(int fd, short type, void *arg)
{
	log_debug("%s: vm %u shutdown", __progname, vmmci.vm_id);
	vm_shutdown(vmmci.cmd == VMMCI_REBOOT ? VMMCI_REBOOT : VMMCI_SHUTDOWN);
}

int
vmmci_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *unused, uint8_t sz)
{
	*intr = 0xFF;

	if (dir == 0) {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			log_warnx("%s: illegal write %x to %s",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			vmmci.cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			vmmci.cfg.queue_address = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			vmmci.cfg.queue_select = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			vmmci.cfg.queue_notify = *data;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			vmmci.cfg.device_status = *data;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
			vmmci_ack(*data);
			break;
		}
	} else {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
			*data = vmmci.cmd;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
			/* Update time once when reading the first register */
			gettimeofday(&vmmci.time, NULL);
			*data = (uint64_t)vmmci.time.tv_sec;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 8:
			*data = (uint64_t)vmmci.time.tv_sec << 32;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 12:
			*data = (uint64_t)vmmci.time.tv_usec;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 16:
			*data = (uint64_t)vmmci.time.tv_usec << 32;
			break;
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = vmmci.cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = vmmci.cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			*data = vmmci.cfg.queue_address;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			*data = vmmci.cfg.queue_size;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = vmmci.cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = vmmci.cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			*data = vmmci.cfg.device_status;
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = vmmci.cfg.isr_status;
			vmmci.cfg.isr_status = 0;
			break;
		}
	}
	return (0);
}

void
virtio_init(struct vmop_create_params *vmc, int *child_disks, int *child_taps)
{
	struct vm_create_params *vcp = &vmc->vmc_params;
	static const uint8_t zero_mac[6];
	uint8_t id;
	uint8_t i;
	int ret, rng;
	off_t sz;

	/* Virtio entropy device */
	if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
	    PCI_PRODUCT_QUMRANET_VIO_RNG, PCI_CLASS_SYSTEM,
	    PCI_SUBCLASS_SYSTEM_MISC,
	    PCI_VENDOR_OPENBSD,
	    PCI_PRODUCT_VIRTIO_ENTROPY, 1, NULL)) {
		log_warnx("%s: can't add PCI virtio rng device",
		    __progname);
		return;
	}

	if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_rnd_io, NULL)) {
		log_warnx("%s: can't add bar for virtio rng device",
		    __progname);
		return;
	}

	memset(&viornd, 0, sizeof(viornd));
	viornd.vq[0].qs = VIORND_QUEUE_SIZE;
	viornd.vq[0].vq_availoffset = sizeof(struct vring_desc) *
	    VIORND_QUEUE_SIZE;
	viornd.vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
	    sizeof(struct vring_desc) * VIORND_QUEUE_SIZE
	    + sizeof(uint16_t) * (2 + VIORND_QUEUE_SIZE));

	if (vcp->vcp_ndisks > 0) {
		vioblk = calloc(vcp->vcp_ndisks, sizeof(struct vioblk_dev));
		if (vioblk == NULL) {
			log_warn("%s: calloc failure allocating vioblks",
			    __progname);
			return;
		}

		/* One virtio block device for each disk defined in vcp */
		for (i = 0; i < vcp->vcp_ndisks; i++) {
			if ((sz = lseek(child_disks[i], 0, SEEK_END)) == -1)
				continue;

			if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
			    PCI_PRODUCT_QUMRANET_VIO_BLOCK,
			    PCI_CLASS_MASS_STORAGE,
			    PCI_SUBCLASS_MASS_STORAGE_SCSI,
			    PCI_VENDOR_OPENBSD,
			    PCI_PRODUCT_VIRTIO_BLOCK, 1, NULL)) {
				log_warnx("%s: can't add PCI virtio block "
				    "device", __progname);
				return;
			}
			if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_blk_io,
			    &vioblk[i])) {
				log_warnx("%s: can't add bar for virtio block "
				    "device", __progname);
				return;
			}
			vioblk[i].vq[0].qs = VIOBLK_QUEUE_SIZE;
			vioblk[i].vq[0].vq_availoffset =
			    sizeof(struct vring_desc) * VIORND_QUEUE_SIZE;
			vioblk[i].vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
			    sizeof(struct vring_desc) * VIOBLK_QUEUE_SIZE
			    + sizeof(uint16_t) * (2 + VIOBLK_QUEUE_SIZE));
			vioblk[i].vq[0].last_avail = 0;
			vioblk[i].fd = child_disks[i];
			vioblk[i].sz = sz / 512;
			vioblk[i].cfg.device_feature = VIRTIO_BLK_F_SIZE_MAX;
			vioblk[i].max_xfer = 1048576;
		}
	}

	if (vcp->vcp_nnics > 0) {
		vionet = calloc(vcp->vcp_nnics, sizeof(struct vionet_dev));
		if (vionet == NULL) {
			log_warn("%s: calloc failure allocating vionets",
			    __progname);
			return;
		}

		nr_vionet = vcp->vcp_nnics;
		/* Virtio network */
		for (i = 0; i < vcp->vcp_nnics; i++) {
			if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
			    PCI_PRODUCT_QUMRANET_VIO_NET, PCI_CLASS_SYSTEM,
			    PCI_SUBCLASS_SYSTEM_MISC,
			    PCI_VENDOR_OPENBSD,
			    PCI_PRODUCT_VIRTIO_NETWORK, 1, NULL)) {
				log_warnx("%s: can't add PCI virtio net device",
				    __progname);
				return;
			}

			if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_net_io,
			    &vionet[i])) {
				log_warnx("%s: can't add bar for virtio net "
				    "device", __progname);
				return;
			}

			ret = pthread_mutex_init(&vionet[i].mutex, NULL);
			if (ret) {
				errno = ret;
				log_warn("%s: could not initialize mutex "
				    "for vionet device", __progname);
				return;
			}

			vionet[i].vq[0].qs = VIONET_QUEUE_SIZE;
			vionet[i].vq[0].vq_availoffset =
			    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE;
			vionet[i].vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
			    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE
			    + sizeof(uint16_t) * (2 + VIONET_QUEUE_SIZE));
			vionet[i].vq[0].last_avail = 0;
			vionet[i].vq[1].qs = VIONET_QUEUE_SIZE;
			vionet[i].vq[1].vq_availoffset =
			    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE;
			vionet[i].vq[1].vq_usedoffset = VIRTQUEUE_ALIGN(
			    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE
			    + sizeof(uint16_t) * (2 + VIONET_QUEUE_SIZE));
			vionet[i].vq[1].last_avail = 0;
			vionet[i].vq[1].notified_avail = 0;
			vionet[i].fd = child_taps[i];
			vionet[i].rx_pending = 0;
			vionet[i].vm_id = vcp->vcp_id;
			vionet[i].irq = pci_get_dev_irq(id);

			event_set(&vionet[i].event, vionet[i].fd,
			    EV_READ | EV_PERSIST, vionet_rx_event, &vionet[i]);
			if (event_add(&vionet[i].event, NULL)) {
				log_warn("could not initialize vionet event "
				    "handler");
				return;
			}

			vionet[i].cfg.device_feature = VIRTIO_NET_F_MAC;

			if (memcmp(zero_mac, &vcp->vcp_macs[i], 6) != 0) {
				/* User-defined address */
				memcpy(&vionet[i].mac, &vcp->vcp_macs[i], 6);
			} else {
				/*
				 * If the address is zero, always randomize
				 * it in vmd(8) because we cannot rely on
				 * the guest OS to do the right thing like
				 * OpenBSD does.  Based on ether_fakeaddr()
				 * from the kernel, incremented by one to
				 * differentiate the source.
				 */
				rng = arc4random();
				vionet[i].mac[0] = 0xfe;
				vionet[i].mac[1] = 0xe1;
				vionet[i].mac[2] = 0xba + 1;
				vionet[i].mac[3] = 0xd0 | ((i + 1) & 0xf);
				vionet[i].mac[4] = rng;
				vionet[i].mac[5] = rng >> 8;
			}
			vionet[i].lockedmac =
			    vmc->vmc_ifflags[i] & VMIFF_LOCKED ? 1 : 0;

			log_debug("%s: vm \"%s\" vio%u lladdr %s%s",
			    __func__, vcp->vcp_name, i,
			    ether_ntoa((void *)vionet[i].mac),
			    vionet[i].lockedmac ? " (locked)" : "");
		}
	}

	/* virtio control device */
	if (pci_add_device(&id, PCI_VENDOR_OPENBSD,
	    PCI_PRODUCT_OPENBSD_CONTROL,
	    PCI_CLASS_COMMUNICATIONS,
	    PCI_SUBCLASS_COMMUNICATIONS_MISC,
	    PCI_VENDOR_OPENBSD,
	    PCI_PRODUCT_VIRTIO_VMMCI, 1, NULL)) {
		log_warnx("%s: can't add PCI vmm control device",
		    __progname);
		return;
	}

	if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, vmmci_io, NULL)) {
		log_warnx("%s: can't add bar for vmm control device",
		    __progname);
		return;
	}

	memset(&vmmci, 0, sizeof(vmmci));
	vmmci.cfg.device_feature = VMMCI_F_TIMESYNC|VMMCI_F_ACK;
	vmmci.vm_id = vcp->vcp_id;
	vmmci.irq = pci_get_dev_irq(id);

	evtimer_set(&vmmci.timeout, vmmci_timeout, NULL);
}
