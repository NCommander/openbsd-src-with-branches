/*	$OpenBSD: vionet.c,v 1.9 2024/02/03 21:41:35 dv Exp $	*/

/*
 * Copyright (c) 2023 Dave Voutila <dv@openbsd.org>
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
#include <sys/socket.h>
#include <sys/types.h>

#include <dev/pci/virtio_pcireg.h>
#include <dev/pv/virtioreg.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "virtio.h"
#include "vmd.h"

#define VIRTIO_NET_F_MAC	(1 << 5)
#define RXQ	0
#define TXQ	1

extern char *__progname;
extern struct vmd_vm *current_vm;

struct packet {
	uint8_t	*buf;
	size_t	 len;
};

static int vionet_rx(struct vionet_dev *, int);
static ssize_t vionet_rx_copy(struct vionet_dev *, int, const struct iovec *,
    int, size_t);
static ssize_t vionet_rx_zerocopy(struct vionet_dev *, int,
    const struct iovec *, int);
static void vionet_rx_event(int, short, void *);
static uint32_t handle_io_read(struct viodev_msg *, struct virtio_dev *,
    int8_t *);
static int handle_io_write(struct viodev_msg *, struct virtio_dev *);
static int vionet_notify_tx(struct virtio_dev *);
static int vionet_notifyq(struct virtio_dev *);

static void dev_dispatch_vm(int, short, void *);
static void handle_sync_io(int, short, void *);

/* Device Globals */
struct event ev_tap;
struct event ev_inject;
int pipe_inject[2];
#define READ	0
#define WRITE	1
struct iovec iov_rx[VIONET_QUEUE_SIZE];
struct iovec iov_tx[VIONET_QUEUE_SIZE];


__dead void
vionet_main(int fd, int fd_vmm)
{
	struct virtio_dev	 dev;
	struct vionet_dev	*vionet = NULL;
	struct viodev_msg 	 msg;
	struct vmd_vm	 	 vm;
	struct vm_create_params	*vcp;
	ssize_t			 sz;
	int			 ret;

	/*
	 * stdio - needed for read/write to disk fds and channels to the vm.
	 * vmm + proc - needed to create shared vm mappings.
	 */
	if (pledge("stdio vmm proc", NULL) == -1)
		fatal("pledge");

	/* Initialize iovec arrays. */
	memset(iov_rx, 0, sizeof(iov_rx));
	memset(iov_tx, 0, sizeof(iov_tx));

	/* Receive our vionet_dev, mostly preconfigured. */
	sz = atomicio(read, fd, &dev, sizeof(dev));
	if (sz != sizeof(dev)) {
		ret = errno;
		log_warn("failed to receive vionet");
		goto fail;
	}
	if (dev.dev_type != VMD_DEVTYPE_NET) {
		ret = EINVAL;
		log_warn("received invalid device type");
		goto fail;
	}
	dev.sync_fd = fd;
	vionet = &dev.vionet;

	log_debug("%s: got vionet dev. tap fd = %d, syncfd = %d, asyncfd = %d"
	    ", vmm fd = %d", __func__, vionet->data_fd, dev.sync_fd,
	    dev.async_fd, fd_vmm);

	/* Receive our vm information from the vm process. */
	memset(&vm, 0, sizeof(vm));
	sz = atomicio(read, dev.sync_fd, &vm, sizeof(vm));
	if (sz != sizeof(vm)) {
		ret = EIO;
		log_warnx("failed to receive vm details");
		goto fail;
	}
	vcp = &vm.vm_params.vmc_params;
	current_vm = &vm;
	setproctitle("%s/vionet%d", vcp->vcp_name, vionet->idx);
	log_procinit("vm/%s/vionet%d", vcp->vcp_name, vionet->idx);

	/* Now that we have our vm information, we can remap memory. */
	ret = remap_guest_mem(&vm, fd_vmm);
	if (ret) {
		fatal("%s: failed to remap", __func__);
		goto fail;
	}

	/*
	 * We no longer need /dev/vmm access.
	 */
	close_fd(fd_vmm);
	if (pledge("stdio", NULL) == -1)
		fatal("pledge2");

	/* If we're restoring hardware, re-initialize virtqueue hva's. */
	if (vm.vm_state & VM_STATE_RECEIVED) {
		struct virtio_vq_info *vq_info;
		void *hva = NULL;

		vq_info = &dev.vionet.vq[TXQ];
		if (vq_info->q_gpa != 0) {
			log_debug("%s: restoring TX virtqueue for gpa 0x%llx",
			    __func__, vq_info->q_gpa);
			hva = hvaddr_mem(vq_info->q_gpa,
			    vring_size(VIONET_QUEUE_SIZE));
			if (hva == NULL)
				fatalx("%s: hva == NULL", __func__);
			vq_info->q_hva = hva;
		}

		vq_info = &dev.vionet.vq[RXQ];
		if (vq_info->q_gpa != 0) {
			log_debug("%s: restoring RX virtqueue for gpa 0x%llx",
			    __func__, vq_info->q_gpa);
			hva = hvaddr_mem(vq_info->q_gpa,
			    vring_size(VIONET_QUEUE_SIZE));
			if (hva == NULL)
				fatalx("%s: hva == NULL", __func__);
			vq_info->q_hva = hva;
		}
	}

	/* Initialize our packet injection pipe. */
	if (pipe2(pipe_inject, O_NONBLOCK) == -1) {
		log_warn("%s: injection pipe", __func__);
		goto fail;
	}

	/* Initialize libevent so we can start wiring event handlers. */
	event_init();

	/* Wire up an async imsg channel. */
	log_debug("%s: wiring in async vm event handler (fd=%d)", __func__,
		dev.async_fd);
	if (vm_device_pipe(&dev, dev_dispatch_vm)) {
		ret = EIO;
		log_warnx("vm_device_pipe");
		goto fail;
	}

	/* Wire up event handling for the tap fd. */
	log_debug("%s: wiring in tap fd handler (fd=%d)", __func__,
	    vionet->data_fd);
	event_set(&ev_tap, vionet->data_fd, EV_READ | EV_PERSIST,
	    vionet_rx_event, &dev);

	/* Add an event for injected packets. */
	log_debug("%s: wiring in packet injection handler (fd=%d)", __func__,
	    pipe_inject[READ]);
	event_set(&ev_inject, pipe_inject[READ], EV_READ | EV_PERSIST,
	    vionet_rx_event, &dev);

	/* Configure our sync channel event handler. */
	log_debug("%s: wiring in sync channel handler (fd=%d)", __func__,
		dev.sync_fd);
	imsg_init(&dev.sync_iev.ibuf, dev.sync_fd);
	dev.sync_iev.handler = handle_sync_io;
	dev.sync_iev.data = &dev;
	dev.sync_iev.events = EV_READ;
	imsg_event_add(&dev.sync_iev);

	/* Send a ready message over the sync channel. */
	log_debug("%s: telling vm %s device is ready", __func__, vcp->vcp_name);
	memset(&msg, 0, sizeof(msg));
	msg.type = VIODEV_MSG_READY;
	imsg_compose_event(&dev.sync_iev, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
	    sizeof(msg));

	/* Send a ready message over the async channel. */
	log_debug("%s: sending async ready message", __func__);
	ret = imsg_compose_event(&dev.async_iev, IMSG_DEVOP_MSG, 0, 0, -1,
	    &msg, sizeof(msg));
	if (ret == -1) {
		log_warnx("%s: failed to send async ready message!", __func__);
		goto fail;
	}

	/* Engage the event loop! */
	ret = event_dispatch();

	/* Cleanup */
	if (ret == 0) {
		close_fd(dev.sync_fd);
		close_fd(dev.async_fd);
		close_fd(vionet->data_fd);
		close_fd(pipe_inject[READ]);
		close_fd(pipe_inject[WRITE]);
		_exit(ret);
		/* NOTREACHED */
	}
fail:
	/* Try firing off a message to the vm saying we're dying. */
	memset(&msg, 0, sizeof(msg));
	msg.type = VIODEV_MSG_ERROR;
	msg.data = ret;
	imsg_compose(&dev.sync_iev.ibuf, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
	    sizeof(msg));
	imsg_flush(&dev.sync_iev.ibuf);

	close_fd(dev.sync_fd);
	close_fd(dev.async_fd);
	close_fd(pipe_inject[READ]);
	close_fd(pipe_inject[WRITE]);
	if (vionet != NULL)
		close_fd(vionet->data_fd);

	_exit(ret);
}

/*
 * Update the gpa and hva of the virtqueue.
 */
static void
vionet_update_qa(struct vionet_dev *dev)
{
	struct virtio_vq_info *vq_info;
	void *hva = NULL;

	/* Invalid queue? */
	if (dev->cfg.queue_select > 1)
		return;

	vq_info = &dev->vq[dev->cfg.queue_select];
	vq_info->q_gpa = (uint64_t)dev->cfg.queue_pfn * VIRTIO_PAGE_SIZE;
	dev->cfg.queue_pfn = vq_info->q_gpa >> 12;

	if (vq_info->q_gpa == 0)
		vq_info->q_hva = NULL;

	hva = hvaddr_mem(vq_info->q_gpa, vring_size(VIONET_QUEUE_SIZE));
	if (hva == NULL)
		fatalx("%s: hva == NULL", __func__);

	vq_info->q_hva = hva;
}

/*
 * Update the queue size.
 */
static void
vionet_update_qs(struct vionet_dev *dev)
{
	struct virtio_vq_info *vq_info;

	/* Invalid queue? */
	if (dev->cfg.queue_select > 1) {
		log_warnx("%s: !!! invalid queue selector %d", __func__,
		    dev->cfg.queue_select);
		dev->cfg.queue_size = 0;
		return;
	}

	vq_info = &dev->vq[dev->cfg.queue_select];

	/* Update queue pfn/size based on queue select */
	dev->cfg.queue_pfn = vq_info->q_gpa >> 12;
	dev->cfg.queue_size = vq_info->qs;
}

/*
 * vionet_rx
 *
 * Pull packet from the provided fd and fill the receive-side virtqueue. We
 * selectively use zero-copy approaches when possible.
 *
 * Returns 1 if guest notification is needed. Otherwise, returns -1 on failure
 * or 0 if no notification is needed.
 */
static int
vionet_rx(struct vionet_dev *dev, int fd)
{
	uint16_t idx, hdr_idx;
	char *vr = NULL;
	size_t chain_len = 0, iov_cnt;
	struct vring_desc *desc, *table;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_vq_info *vq_info;
	struct iovec *iov;
	int notify = 0;
	ssize_t sz;

	if (!(dev->cfg.device_status
	    & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK)) {
		log_warnx("%s: driver not ready", __func__);
		return (0);
	}

	vq_info = &dev->vq[RXQ];
	idx = vq_info->last_avail;
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: vr == NULL", __func__);

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	table = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	used = (struct vring_used *)(vr + vq_info->vq_usedoffset);
	used->flags |= VRING_USED_F_NO_NOTIFY;

	while (idx != avail->idx) {
		hdr_idx = avail->ring[idx & VIONET_QUEUE_MASK];
		desc = &table[hdr_idx & VIONET_QUEUE_MASK];
		if (!DESC_WRITABLE(desc)) {
			log_warnx("%s: invalid descriptor state", __func__);
			goto reset;
		}

		iov = &iov_rx[0];
		iov_cnt = 1;

		/*
		 * First descriptor should be at least as large as the
		 * virtio_net_hdr. It's not technically required, but in
		 * legacy devices it should be safe to assume.
		 */
		iov->iov_len = desc->len;
		if (iov->iov_len < sizeof(struct virtio_net_hdr)) {
			log_warnx("%s: invalid descriptor length", __func__);
			goto reset;
		}

		/*
		 * Insert the virtio_net_hdr and adjust len/base. We do the
		 * pointer math here before it's a void*.
		 */
		iov->iov_base = hvaddr_mem(desc->addr, iov->iov_len);
		if (iov->iov_base == NULL)
			goto reset;
		memset(iov->iov_base, 0, sizeof(struct virtio_net_hdr));

		/* Tweak the iovec to account for the virtio_net_hdr. */
		iov->iov_len -= sizeof(struct virtio_net_hdr);
		iov->iov_base = hvaddr_mem(desc->addr +
		    sizeof(struct virtio_net_hdr), iov->iov_len);
		if (iov->iov_base == NULL)
			goto reset;
		chain_len = iov->iov_len;

		/*
		 * Walk the remaining chain and collect remaining addresses
		 * and lengths.
		 */
		while (desc->flags & VRING_DESC_F_NEXT) {
			desc = &table[desc->next & VIONET_QUEUE_MASK];
			if (!DESC_WRITABLE(desc)) {
				log_warnx("%s: invalid descriptor state",
				    __func__);
				goto reset;
			}

			/* Collect our IO information. Translate gpa's. */
			iov = &iov_rx[iov_cnt];
			iov->iov_len = desc->len;
			iov->iov_base = hvaddr_mem(desc->addr, iov->iov_len);
			if (iov->iov_base == NULL)
				goto reset;
			chain_len += iov->iov_len;

			/* Guard against infinitely looping chains. */
			if (++iov_cnt >= nitems(iov_rx)) {
				log_warnx("%s: infinite chain detected",
				    __func__);
				goto reset;
			}
		}

		/* Make sure the driver gave us the bare minimum buffers. */
		if (chain_len < VIONET_MIN_TXLEN) {
			log_warnx("%s: insufficient buffers provided",
			    __func__);
			goto reset;
		}

		/*
		 * If we're enforcing hardware address or handling an injected
		 * packet, we need to use a copy-based approach.
		 */
		if (dev->lockedmac || fd != dev->data_fd)
			sz = vionet_rx_copy(dev, fd, iov_rx, iov_cnt,
			    chain_len);
		else
			sz = vionet_rx_zerocopy(dev, fd, iov_rx, iov_cnt);
		if (sz == -1)
			goto reset;
		if (sz == 0)	/* No packets, so bail out for now. */
			break;

		/*
		 * Account for the prefixed header since it wasn't included
		 * in the copy or zerocopy operations.
		 */
		sz += sizeof(struct virtio_net_hdr);

		/* Mark our buffers as used. */
		used->ring[used->idx & VIONET_QUEUE_MASK].id = hdr_idx;
		used->ring[used->idx & VIONET_QUEUE_MASK].len = sz;
		__sync_synchronize();
		used->idx++;
		idx++;
	}

	if (idx != vq_info->last_avail &&
	    !(avail->flags & VRING_AVAIL_F_NO_INTERRUPT)) {
		notify = 1;
		dev->cfg.isr_status |= 1;
	}

	vq_info->last_avail = idx;
	return (notify);
reset:
	log_warnx("%s: requesting device reset", __func__);
	dev->cfg.device_status |= DEVICE_NEEDS_RESET;
	dev->cfg.isr_status |= VIRTIO_CONFIG_ISR_CONFIG_CHANGE;
	return (1);
}

/*
 * vionet_rx_copy
 *
 * Read a packet off the provided file descriptor, validating packet
 * characteristics, and copy into the provided buffers in the iovec array.
 *
 * It's assumed that the provided iovec array contains validated host virtual
 * address translations and not guest physical addreses.
 *
 * Returns number of bytes copied on success, 0 if packet is dropped, and
 * -1 on an error.
 */
ssize_t
vionet_rx_copy(struct vionet_dev *dev, int fd, const struct iovec *iov,
    int iov_cnt, size_t chain_len)
{
	static uint8_t		 buf[VIONET_HARD_MTU];
	struct packet		*pkt = NULL;
	struct ether_header	*eh = NULL;
	uint8_t			*payload = buf;
	size_t			 i, chunk, nbytes, copied = 0;
	ssize_t			 sz;

	/* If reading from the tap(4), try to right-size the read. */
	if (fd == dev->data_fd)
		nbytes = MIN(chain_len, VIONET_HARD_MTU);
	else if (fd == pipe_inject[READ])
		nbytes = sizeof(struct packet);
	else {
		log_warnx("%s: invalid fd: %d", __func__, fd);
		return (-1);
	}

	/*
	 * Try to pull a packet. The fd should be non-blocking and we don't
	 * care if we under-read (i.e. sz != nbytes) as we may not have a
	 * packet large enough to fill the buffer.
	 */
	sz = read(fd, buf, nbytes);
	if (sz == -1) {
		if (errno != EAGAIN) {
			log_warn("%s: error reading packet", __func__);
			return (-1);
		}
		return (0);
	} else if (fd == dev->data_fd && sz < VIONET_MIN_TXLEN) {
		/* If reading the tap(4), we should get valid ethernet. */
		log_warnx("%s: invalid packet size", __func__);
		return (0);
	} else if (sz != sizeof(struct packet)) {
		log_warnx("%s: invalid injected packet object", __func__);
		return (0);
	}

	/* Decompose an injected packet, if that's what we're working with. */
	if (fd == pipe_inject[READ]) {
		pkt = (struct packet *)buf;
		if (pkt->buf == NULL) {
			log_warnx("%s: invalid injected packet, no buffer",
			    __func__);
			return (0);
		}
		if (sz < VIONET_MIN_TXLEN || sz > VIONET_MAX_TXLEN) {
			log_warnx("%s: invalid injected packet size", __func__);
			goto drop;
		}
		payload = pkt->buf;
		sz = (ssize_t)pkt->len;
	}

	/* Validate the ethernet header, if required. */
	if (dev->lockedmac) {
		eh = (struct ether_header *)(payload);
		if (!ETHER_IS_MULTICAST(eh->ether_dhost) &&
		    memcmp(eh->ether_dhost, dev->mac,
		    sizeof(eh->ether_dhost)) != 0)
			goto drop;
	}

	/* Truncate one last time to the chain length, if shorter. */
	sz = MIN(chain_len, (size_t)sz);

	/*
	 * Copy the packet into the provided buffers. We can use memcpy(3)
	 * here as the gpa was validated and translated to an hva previously.
	 */
	for (i = 0; (int)i < iov_cnt && (size_t)sz > copied; i++) {
		chunk = MIN(iov[i].iov_len, (size_t)(sz - copied));
		memcpy(iov[i].iov_base, payload + copied, chunk);
		copied += chunk;
	}

drop:
	/* Free any injected packet buffer. */
	if (pkt != NULL)
		free(pkt->buf);

	return (copied);
}

/*
 * vionet_rx_zerocopy
 *
 * Perform a vectorized read from the given fd into the guest physical memory
 * pointed to by iovecs.
 *
 * Returns number of bytes read on success, -1 on error, or 0 if EAGAIN was
 * returned by readv.
 *
 */
static ssize_t
vionet_rx_zerocopy(struct vionet_dev *dev, int fd, const struct iovec *iov,
    int iov_cnt)
{
	ssize_t		sz;

	if (dev->lockedmac) {
		log_warnx("%s: zerocopy not available for locked lladdr",
		    __func__);
		return (-1);
	}

	sz = readv(fd, iov, iov_cnt);
	if (sz == -1 && errno == EAGAIN)
		return (0);
	return (sz);
}


/*
 * vionet_rx_event
 *
 * Called when new data can be received on the tap fd of a vionet device.
 */
static void
vionet_rx_event(int fd, short event, void *arg)
{
	struct virtio_dev *dev = (struct virtio_dev *)arg;

	if (!(event & EV_READ))
		fatalx("%s: invalid event type", __func__);

	if (vionet_rx(&dev->vionet, fd) > 0)
		virtio_assert_pic_irq(dev, 0);
}

static int
vionet_notifyq(struct virtio_dev *dev)
{
	struct vionet_dev *vionet = &dev->vionet;

	switch (vionet->cfg.queue_notify) {
	case RXQ:
		event_add(&ev_tap, NULL);
		event_add(&ev_inject, NULL);
		break;
	case TXQ:
		return vionet_notify_tx(dev);
	default:
		/*
		 * Catch the unimplemented queue ID 2 (control queue) as
		 * well as any bogus queue IDs.
		 */
		log_debug("%s: notify for unimplemented queue ID %d",
		    __func__, vionet->cfg.queue_notify);
		break;
	}

	return (0);
}

static int
vionet_notify_tx(struct virtio_dev *dev)
{
	uint16_t idx, hdr_idx;
	size_t chain_len, iov_cnt;
	ssize_t dhcpsz = 0, sz;
	int notify = 0;
	char *vr = NULL, *dhcppkt = NULL;
	struct vionet_dev *vionet = &dev->vionet;
	struct vring_desc *desc, *table;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_vq_info *vq_info;
	struct ether_header *eh;
	struct iovec *iov;
	struct packet pkt;

	if (!(vionet->cfg.device_status
	    & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK)) {
		log_warnx("%s: driver not ready", __func__);
		return (0);
	}

	vq_info = &vionet->vq[TXQ];
	idx = vq_info->last_avail;
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: vr == NULL", __func__);

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	table = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	used = (struct vring_used *)(vr + vq_info->vq_usedoffset);

	while (idx != avail->idx) {
		hdr_idx = avail->ring[idx & VIONET_QUEUE_MASK];
		desc = &table[hdr_idx & VIONET_QUEUE_MASK];
		if (DESC_WRITABLE(desc)) {
			log_warnx("%s: invalid descriptor state", __func__);
			goto reset;
		}

		iov = &iov_tx[0];
		iov_cnt = 0;
		chain_len = 0;

		/*
		 * As a legacy device, we most likely will receive a lead
		 * descriptor sized to the virtio_net_hdr. However, the framing
		 * is not guaranteed, so check for packet data.
		 */
		iov->iov_len = desc->len;
		if (iov->iov_len < sizeof(struct virtio_net_hdr)) {
			log_warnx("%s: invalid descriptor length", __func__);
			goto reset;
		} else if (iov->iov_len > sizeof(struct virtio_net_hdr)) {
			/* Chop off the virtio header, leaving packet data. */
			iov->iov_len -= sizeof(struct virtio_net_hdr);
			chain_len += iov->iov_len;
			iov->iov_base = hvaddr_mem(desc->addr +
			    sizeof(struct virtio_net_hdr), iov->iov_len);
			if (iov->iov_base == NULL)
				goto reset;
			iov_cnt++;
		}

		/*
		 * Walk the chain and collect remaining addresses and lengths.
		 */
		while (desc->flags & VRING_DESC_F_NEXT) {
			desc = &table[desc->next & VIONET_QUEUE_MASK];
			if (DESC_WRITABLE(desc)) {
				log_warnx("%s: invalid descriptor state",
				    __func__);
				goto reset;
			}

			/* Collect our IO information, translating gpa's. */
			iov = &iov_tx[iov_cnt];
			iov->iov_len = desc->len;
			iov->iov_base = hvaddr_mem(desc->addr, iov->iov_len);
			if (iov->iov_base == NULL)
				goto reset;
			chain_len += iov->iov_len;

			/* Guard against infinitely looping chains. */
			if (++iov_cnt >= nitems(iov_tx)) {
				log_warnx("%s: infinite chain detected",
				    __func__);
				goto reset;
			}
		}

		/* Check if we've got a minimum viable amount of data. */
		if (chain_len < VIONET_MIN_TXLEN) {
			sz = chain_len;
			goto drop;
		}

		/*
		 * Packet inspection for ethernet header (if using a "local"
		 * interface) for possibility of a DHCP packet or (if using
		 * locked lladdr) for validating ethernet header.
		 *
		 * To help preserve zero-copy semantics, we require the first
		 * descriptor with packet data contains a large enough buffer
		 * for this inspection.
		 */
		iov = &iov_tx[0];
		if (vionet->lockedmac) {
			if (iov->iov_len < ETHER_HDR_LEN) {
				log_warnx("%s: insufficient header data",
				    __func__);
				goto drop;
			}
			eh = (struct ether_header *)iov->iov_base;
			if (memcmp(eh->ether_shost, vionet->mac,
			    sizeof(eh->ether_shost)) != 0) {
				log_warnx("%s: bad source address %s",
				    __func__, ether_ntoa((struct ether_addr *)
					eh->ether_shost));
				sz = chain_len;
				goto drop;
			}
		}
		if (vionet->local) {
			dhcpsz = dhcp_request(dev, iov->iov_base, iov->iov_len,
			    &dhcppkt);
			log_debug("%s: detected dhcp request of %zu bytes",
			    __func__, dhcpsz);
		}

		/* Write our packet to the tap(4). */
		sz = writev(vionet->data_fd, iov_tx, iov_cnt);
		if (sz == -1 && errno != ENOBUFS) {
			log_warn("%s", __func__);
			goto reset;
		}
		sz += sizeof(struct virtio_net_hdr);
drop:
		used->ring[used->idx & VIONET_QUEUE_MASK].id = hdr_idx;
		used->ring[used->idx & VIONET_QUEUE_MASK].len = sz;
		__sync_synchronize();
		used->idx++;
		idx++;

		/* Facilitate DHCP reply injection, if needed. */
		if (dhcpsz > 0) {
			pkt.buf = dhcppkt;
			pkt.len = dhcpsz;
			sz = write(pipe_inject[WRITE], &pkt, sizeof(pkt));
			if (sz == -1 && errno != EAGAIN) {
				log_warn("%s: packet injection", __func__);
				free(pkt.buf);
			} else if (sz == -1 && errno == EAGAIN) {
				log_debug("%s: dropping dhcp reply", __func__);
				free(pkt.buf);
			} else if (sz != sizeof(pkt)) {
				log_warnx("%s: failed packet injection",
				    __func__);
				free(pkt.buf);
			}
			log_debug("%s: injected dhcp reply with %ld bytes",
			    __func__, sz);
		}
	}

	if (idx != vq_info->last_avail &&
	    !(avail->flags & VRING_AVAIL_F_NO_INTERRUPT)) {
		notify = 1;
		vionet->cfg.isr_status |= 1;
	}

	vq_info->last_avail = idx;
	return (notify);
reset:
	log_warnx("%s: requesting device reset", __func__);
	vionet->cfg.device_status |= DEVICE_NEEDS_RESET;
	vionet->cfg.isr_status |= VIRTIO_CONFIG_ISR_CONFIG_CHANGE;
	return (1);
}

static void
dev_dispatch_vm(int fd, short event, void *arg)
{
	struct virtio_dev	*dev = arg;
	struct vionet_dev	*vionet = &dev->vionet;
	struct imsgev		*iev = &dev->async_iev;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg	 	 imsg;
	ssize_t			 n = 0;
	int			 verbose;

	if (dev == NULL)
		fatalx("%s: missing vionet pointer", __func__);

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("%s: imsg_read", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: pipe dead (EV_READ)", __func__);
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("%s: msgbuf_write", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: pipe dead (EV_WRITE)", __func__);
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get", __func__);
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DEVOP_HOSTMAC:
			IMSG_SIZE_CHECK(&imsg, vionet->hostmac);
			memcpy(vionet->hostmac, imsg.data,
			    sizeof(vionet->hostmac));
			log_debug("%s: set hostmac", __func__);
			break;
		case IMSG_VMDOP_PAUSE_VM:
			log_debug("%s: pausing", __func__);
			event_del(&ev_tap);
			event_del(&ev_inject);
			break;
		case IMSG_VMDOP_UNPAUSE_VM:
			log_debug("%s: unpausing", __func__);
			if (vionet->cfg.device_status
			    & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK) {
				event_add(&ev_tap, NULL);
				event_add(&ev_inject, NULL);
			}
			break;
		case IMSG_CTL_VERBOSE:
			IMSG_SIZE_CHECK(&imsg, &verbose);
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

/*
 * Synchronous IO handler.
 *
 */
static void
handle_sync_io(int fd, short event, void *arg)
{
	struct virtio_dev *dev = (struct virtio_dev *)arg;
	struct imsgev *iev = &dev->sync_iev;
	struct imsgbuf *ibuf = &iev->ibuf;
	struct viodev_msg msg;
	struct imsg imsg;
	ssize_t n;
	int8_t intr = INTR_STATE_NOOP;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("%s: imsg_read", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: pipe dead (EV_READ)", __func__);
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("%s: msgbuf_write", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: pipe dead (EV_WRITE)", __func__);
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatalx("%s: imsg_get (n=%ld)", __func__, n);
		if (n == 0)
			break;

		/* Unpack our message. They ALL should be dev messeges! */
		IMSG_SIZE_CHECK(&imsg, &msg);
		memcpy(&msg, imsg.data, sizeof(msg));
		imsg_free(&imsg);

		switch (msg.type) {
		case VIODEV_MSG_DUMP:
			/* Dump device */
			n = atomicio(vwrite, dev->sync_fd, dev, sizeof(*dev));
			if (n != sizeof(*dev)) {
				log_warnx("%s: failed to dump vionet device",
				    __func__);
				break;
			}
		case VIODEV_MSG_IO_READ:
			/* Read IO: make sure to send a reply */
			msg.data = handle_io_read(&msg, dev, &intr);
			msg.data_valid = 1;
			msg.state = intr;
			imsg_compose_event(iev, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
			    sizeof(msg));
			break;
		case VIODEV_MSG_IO_WRITE:
			/* Write IO: no reply needed */
			if (handle_io_write(&msg, dev) == 1)
				virtio_assert_pic_irq(dev, 0);
			break;
		case VIODEV_MSG_SHUTDOWN:
			event_del(&dev->sync_iev.ev);
			event_del(&ev_tap);
			event_del(&ev_inject);
			event_loopbreak();
			return;
		default:
			fatalx("%s: invalid msg type %d", __func__, msg.type);
		}
	}
	imsg_event_add(iev);
}

static int
handle_io_write(struct viodev_msg *msg, struct virtio_dev *dev)
{
	struct vionet_dev *vionet = &dev->vionet;
	uint32_t data = msg->data;
	int intr = 0;

	switch (msg->reg) {
	case VIRTIO_CONFIG_DEVICE_FEATURES:
	case VIRTIO_CONFIG_QUEUE_SIZE:
	case VIRTIO_CONFIG_ISR_STATUS:
		log_warnx("%s: illegal write %x to %s", __progname, data,
		    virtio_reg_name(msg->reg));
		break;
	case VIRTIO_CONFIG_GUEST_FEATURES:
		vionet->cfg.guest_feature = data;
		break;
	case VIRTIO_CONFIG_QUEUE_PFN:
		vionet->cfg.queue_pfn = data;
		vionet_update_qa(vionet);
		break;
	case VIRTIO_CONFIG_QUEUE_SELECT:
		vionet->cfg.queue_select = data;
		vionet_update_qs(vionet);
		break;
	case VIRTIO_CONFIG_QUEUE_NOTIFY:
		vionet->cfg.queue_notify = data;
		if (vionet_notifyq(dev))
			intr = 1;
		break;
	case VIRTIO_CONFIG_DEVICE_STATUS:
		vionet->cfg.device_status = data;
		if (vionet->cfg.device_status == 0) {
			vionet->cfg.guest_feature = 0;

			vionet->cfg.queue_pfn = 0;
			vionet_update_qa(vionet);

			vionet->cfg.queue_size = 0;
			vionet_update_qs(vionet);

			vionet->cfg.queue_select = 0;
			vionet->cfg.queue_notify = 0;
			vionet->cfg.isr_status = 0;
			vionet->vq[RXQ].last_avail = 0;
			vionet->vq[RXQ].notified_avail = 0;
			vionet->vq[TXQ].last_avail = 0;
			vionet->vq[TXQ].notified_avail = 0;
			virtio_deassert_pic_irq(dev, msg->vcpu);

			event_del(&ev_tap);
			event_del(&ev_inject);
		}
		break;
	}
	return (intr);
}

static uint32_t
handle_io_read(struct viodev_msg *msg, struct virtio_dev *dev, int8_t *intr)
{
	struct vionet_dev *vionet = &dev->vionet;
	uint32_t data;

	switch (msg->reg) {
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 1:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 2:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
		data = vionet->mac[msg->reg -
		    VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI];
		break;
	case VIRTIO_CONFIG_DEVICE_FEATURES:
		data = vionet->cfg.device_feature;
		break;
	case VIRTIO_CONFIG_GUEST_FEATURES:
		data = vionet->cfg.guest_feature;
		break;
	case VIRTIO_CONFIG_QUEUE_PFN:
		data = vionet->cfg.queue_pfn;
		break;
	case VIRTIO_CONFIG_QUEUE_SIZE:
		data = vionet->cfg.queue_size;
		break;
	case VIRTIO_CONFIG_QUEUE_SELECT:
		data = vionet->cfg.queue_select;
		break;
	case VIRTIO_CONFIG_QUEUE_NOTIFY:
		data = vionet->cfg.queue_notify;
		break;
	case VIRTIO_CONFIG_DEVICE_STATUS:
		data = vionet->cfg.device_status;
		break;
	case VIRTIO_CONFIG_ISR_STATUS:
		data = vionet->cfg.isr_status;
		vionet->cfg.isr_status = 0;
		if (intr != NULL)
			*intr = INTR_STATE_DEASSERT;
		break;
	default:
		return (0xFFFFFFFF);
	}

	return (data);
}
