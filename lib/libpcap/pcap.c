/*	$OpenBSD: pcap.c,v 1.9 2005/11/18 11:05:39 djm Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997, 1998
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "pcap-int.h"

static const char pcap_version_string[] = "OpenBSD libpcap";

int
pcap_dispatch(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{

	if (p->sf.rfile != NULL)
		return (pcap_offline_read(p, cnt, callback, user));
	return (pcap_read(p, cnt, callback, user));
}

int
pcap_loop(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	register int n;

	for (;;) {
		if (p->sf.rfile != NULL)
			n = pcap_offline_read(p, cnt, callback, user);
		else {
			/*
			 * XXX keep reading until we get something
			 * (or an error occurs)
			 */
			do {
				n = pcap_read(p, cnt, callback, user);
			} while (n == 0);
		}
		if (n <= 0)
			return (n);
		if (cnt > 0) {
			cnt -= n;
			if (cnt <= 0)
				return (0);
		}
	}
}

struct singleton {
	struct pcap_pkthdr *hdr;
	const u_char *pkt;
};


static void
pcap_oneshot(u_char *userData, const struct pcap_pkthdr *h, const u_char *pkt)
{
	struct singleton *sp = (struct singleton *)userData;
	*sp->hdr = *h;
	sp->pkt = pkt;
}

const u_char *
pcap_next(pcap_t *p, struct pcap_pkthdr *h)
{
	struct singleton s;

	s.hdr = h;
	if (pcap_dispatch(p, 1, pcap_oneshot, (u_char*)&s) <= 0)
		return (0);
	return (s.pkt);
}

struct pkt_for_fakecallback {
	struct pcap_pkthdr *hdr;
	const u_char **pkt;
};

static void
pcap_fakecallback(u_char *userData, const struct pcap_pkthdr *h,
    const u_char *pkt)
{
	struct pkt_for_fakecallback *sp = (struct pkt_for_fakecallback *)userData;

	*sp->hdr = *h;
	*sp->pkt = pkt;
}

int 
pcap_next_ex(pcap_t *p, struct pcap_pkthdr **pkt_header,
    const u_char **pkt_data)
{
	struct pkt_for_fakecallback s;

	s.hdr = &p->pcap_header;
	s.pkt = pkt_data;

	/* Saves a pointer to the packet headers */
	*pkt_header= &p->pcap_header;

	if (p->sf.rfile != NULL) {
		int status;

		/* We are on an offline capture */
		status = pcap_offline_read(p, 1, pcap_fakecallback,
		    (u_char *)&s);

		/*
		 * Return codes for pcap_offline_read() are:
		 *   -  0: EOF
		 *   - -1: error
		 *   - >1: OK
		 * The first one ('0') conflicts with the return code of
		 * 0 from pcap_read() meaning "no packets arrived before
		 * the timeout expired", so we map it to -2 so you can
		 * distinguish between an EOF from a savefile and a
		 * "no packets arrived before the timeout expired, try
		 * again" from a live capture.
		 */
		if (status == 0)
			return (-2);
		else
			return (status);
	}

	/*
	 * Return codes for pcap_read() are:
	 *   -  0: timeout
	 *   - -1: error
	 *   - -2: loop was broken out of with pcap_breakloop()
	 *   - >1: OK
	 * The first one ('0') conflicts with the return code of 0 from
	 * pcap_offline_read() meaning "end of file".
	*/
	return (pcap_read(p, 1, pcap_fakecallback, (u_char *)&s));
}

/*
 * Force the loop in "pcap_read()" or "pcap_read_offline()" to terminate.
 */
void
pcap_breakloop(pcap_t *p)
{
	p->break_loop = 1;
}

int
pcap_datalink(pcap_t *p)
{
	return (p->linktype);
}

int
pcap_list_datalinks(pcap_t *p, int **dlt_buffer)
{
	if (p->dlt_count == 0) {
		/*
		 * We couldn't fetch the list of DLTs, which means
		 * this platform doesn't support changing the
		 * DLT for an interface.  Return a list of DLTs
		 * containing only the DLT this device supports.
		 */
		*dlt_buffer = (int*)malloc(sizeof(**dlt_buffer));
		if (*dlt_buffer == NULL) {
			(void)snprintf(p->errbuf, sizeof(p->errbuf),
			    "malloc: %s", pcap_strerror(errno));
			return (-1);
		}
		**dlt_buffer = p->linktype;
		return (1);
	} else {
		*dlt_buffer = (int*)malloc(sizeof(**dlt_buffer) * p->dlt_count);
		if (*dlt_buffer == NULL) {
			(void)snprintf(p->errbuf, sizeof(p->errbuf),
			    "malloc: %s", pcap_strerror(errno));
			return (-1);
		}
		(void)memcpy(*dlt_buffer, p->dlt_list,
		    sizeof(**dlt_buffer) * p->dlt_count);
		return (p->dlt_count);
	}
}

struct dlt_choice {
	const char *name;
	const char *description;
	int	dlt;
};

static struct dlt_choice dlts[] = {
#define DLT_CHOICE(code, description) { #code, description, code }
DLT_CHOICE(DLT_NULL, "no link-layer encapsulation"),
DLT_CHOICE(DLT_EN10MB, "Ethernet (10Mb)"),
DLT_CHOICE(DLT_EN3MB, "Experimental Ethernet (3Mb)"),
DLT_CHOICE(DLT_AX25, "Amateur Radio AX.25"),
DLT_CHOICE(DLT_PRONET, "Proteon ProNET Token Ring"),
DLT_CHOICE(DLT_CHAOS, "Chaos"),
DLT_CHOICE(DLT_IEEE802, "IEEE 802 Networks"),
DLT_CHOICE(DLT_ARCNET, "ARCNET"),
DLT_CHOICE(DLT_SLIP, "Serial Line IP"),
DLT_CHOICE(DLT_PPP, "Point-to-point Protocol"),
DLT_CHOICE(DLT_FDDI, "FDDI"),
DLT_CHOICE(DLT_ATM_RFC1483, "LLC/SNAP encapsulated atm"),
DLT_CHOICE(DLT_LOOP, "loopback type (af header)"),
DLT_CHOICE(DLT_ENC, "IPSEC enc type (af header, spi, flags)"),
DLT_CHOICE(DLT_RAW, "raw IP"),
DLT_CHOICE(DLT_SLIP_BSDOS, "BSD/OS Serial Line IP"),
DLT_CHOICE(DLT_PPP_BSDOS, "BSD/OS Point-to-point Protocol"),
DLT_CHOICE(DLT_OLD_PFLOG, "Packet filter logging, old (XXX remove?)"),
DLT_CHOICE(DLT_PFSYNC, "Packet filter state syncing"),
DLT_CHOICE(DLT_PPP_ETHER, "PPP over Ethernet; session only w/o ether header"),
DLT_CHOICE(DLT_IEEE802_11, "IEEE 802.11 wireless"),
DLT_CHOICE(DLT_PFLOG, "Packet filter logging, by pcap people"),
DLT_CHOICE(DLT_IEEE802_11_RADIO, "IEEE 802.11 plus WLAN header"),
#undef DLT_CHOICE
	{ NULL, NULL, -1}
};

int
pcap_datalink_name_to_val(const char *name)
{
	int i;

	for (i = 0; dlts[i].name != NULL; i++) {
		/* Skip leading "DLT_" */
		if (strcasecmp(dlts[i].name + 4, name) == 0)
			return (dlts[i].dlt);
	}
	return (-1);
}

const char *
pcap_datalink_val_to_name(int dlt)
{
	int i;

	for (i = 0; dlts[i].name != NULL; i++) {
		if (dlts[i].dlt == dlt)
			return (dlts[i].name + 4); /* Skip leading "DLT_" */
	}
	return (NULL);
}

const char *
pcap_datalink_val_to_description(int dlt)
{
	int i;

	for (i = 0; dlts[i].name != NULL; i++) {
		if (dlts[i].dlt == dlt)
			return (dlts[i].description);
	}
	return (NULL);
}

int
pcap_snapshot(pcap_t *p)
{
	return (p->snapshot);
}

int
pcap_is_swapped(pcap_t *p)
{
	return (p->sf.swapped);
}

int
pcap_major_version(pcap_t *p)
{
	return (p->sf.version_major);
}

int
pcap_minor_version(pcap_t *p)
{
	return (p->sf.version_minor);
}

FILE *
pcap_file(pcap_t *p)
{
	return (p->sf.rfile);
}

int
pcap_fileno(pcap_t *p)
{
	return (p->fd);
}

void
pcap_perror(pcap_t *p, char *prefix)
{
	fprintf(stderr, "%s: %s\n", prefix, p->errbuf);
}

int
pcap_get_selectable_fd(pcap_t *p)
{
	return (p->fd);
}

char *
pcap_geterr(pcap_t *p)
{
	return (p->errbuf);
}

int
pcap_getnonblock(pcap_t *p, char *errbuf)
{
	int fdflags;

	fdflags = fcntl(p->fd, F_GETFL, 0);
	if (fdflags == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "F_GETFL: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	if (fdflags & O_NONBLOCK)
		return (1);
	else
		return (0);
}

int
pcap_setnonblock(pcap_t *p, int nonblock, char *errbuf)
{
	int fdflags;

	fdflags = fcntl(p->fd, F_GETFL, 0);
	if (fdflags == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "F_GETFL: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	if (nonblock)
		fdflags |= O_NONBLOCK;
	else
		fdflags &= ~O_NONBLOCK;
	if (fcntl(p->fd, F_SETFL, fdflags) == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "F_SETFL: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	return (0);
}

/*
 * Not all systems have strerror().
 */
char *
pcap_strerror(int errnum)
{
#ifdef HAVE_STRERROR
	return (strerror(errnum));
#else
	extern int sys_nerr;
	extern const char *const sys_errlist[];
	static char ebuf[20];

	if ((unsigned int)errnum < sys_nerr)
		return ((char *)sys_errlist[errnum]);
	(void)snprintf(ebuf, sizeof ebuf, "Unknown error: %d", errnum);
	return(ebuf);
#endif
}

pcap_t *
pcap_open_dead(int linktype, int snaplen)
{
	pcap_t *p;

	p = malloc(sizeof(*p));
	if (p == NULL)
		return NULL;
	memset (p, 0, sizeof(*p));
	p->snapshot = snaplen;
	p->linktype = linktype;
	p->fd = -1;
	return p;
}

void
pcap_close(pcap_t *p)
{
	/*XXX*/
	if (p->fd >= 0)
		close(p->fd);
	if (p->sf.rfile != NULL) {
		(void)fclose(p->sf.rfile);
		if (p->sf.base != NULL)
			free(p->sf.base);
	} else if (p->buffer != NULL)
		free(p->buffer);
#ifdef linux
	if (p->md.device != NULL)
		free(p->md.device);
#endif
	pcap_freecode(&p->fcode);
	if (p->dlt_list != NULL)
		free(p->dlt_list);
	free(p);
}

const char *
pcap_lib_version(void)
{
	return (pcap_version_string);
}

