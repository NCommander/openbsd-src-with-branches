/*	$OpenBSD: ntp.c,v 1.5 2002/05/16 10:42:29 jakob Exp $	*/

/*
 * Copyright (c) 1996, 1997 by N.M. Maclaren. All rights reserved.
 * Copyright (c) 1996, 1997 by University of Cambridge. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the university may be used to
 *    endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


/* NTP definitions.  Note that these assume 8-bit bytes - sigh.  There
 * is little point in parameterising everything, as it is neither
 * feasible nor useful.  It would be very useful if more fields could
 * be defined as unspecified.  The NTP packet-handling routines
 * contain a lot of extra assumptions. */

#define JAN_1970   2208988800.0		/* 1970 - 1900 in seconds */
#define NTP_SCALE  4294967296.0		/* 2^32, of course! */

#define NTP_MODE_CLIENT       3		/* NTP client mode */
#define NTP_MODE_SERVER       4		/* NTP server mode */
#define NTP_VERSION           3		/* The current version */
#define NTP_VERSION_MIN       1		/* The minum valid version */
#define NTP_VERSION_MAX       4		/* The maximum valid version */
#define NTP_STRATUM_MIN       1		/* The minum valid stratum */
#define NTP_STRATUM_MAX      15		/* The maximum valid stratum */
#define NTP_INSANITY     3600.0		/* Errors beyond this are hopeless */

#define NTP_PACKET_MIN       48		/* Without authentication */
#define NTP_PACKET_MAX       68		/* With authentication (ignored) */

#define NTP_DISP_FIELD        8		/* Offset of dispersion field */
#define NTP_REFERENCE        16		/* Offset of reference timestamp */
#define NTP_ORIGINATE        24		/* Offset of originate timestamp */
#define NTP_RECEIVE          32		/* Offset of receive timestamp */
#define NTP_TRANSMIT         40		/* Offset of transmit timestamp */

#define MAX_QUERIES         25
#define MAX_DELAY           15

#define MILLION_L    1000000l		/* For conversion to/from timeval */
#define MILLION_D       1.0e6		/* Must be equal to MILLION_L */


struct ntp_data {
	unsigned char status;
	unsigned char version;
	unsigned char mode;
	unsigned char stratum;
	unsigned char polling;
	unsigned char precision;
	double dispersion;
	double reference;
	double originate;
	double receive;
	double transmit;
	double current;
};


void	ntp_client(const char *, struct timeval *, struct timeval *);
int	sync_ntp(int, const struct sockaddr *, double *, double *);
void	make_packet(struct ntp_data *);
int	write_packet(int, const struct sockaddr *, struct ntp_data *);
int	read_packet(int, struct ntp_data *, double *, double *, double *);
void	pack_ntp(unsigned char *, int, struct ntp_data *);
void	unpack_ntp(struct ntp_data *, unsigned char *, int);
double	current_time(double);
void	create_timeval(double, struct timeval *, struct timeval *);

void
ntp_client(const char *hostname, struct timeval *new, struct timeval *adjust)
{
	struct hostent *hp;
	struct servent *sp, ssp;
	struct protoent *pp, ppp;
	struct sockaddr_in server, peer;

	int s;
	int packets = 0;
	double offset, error;

	if ((hp = gethostbyname(hostname)) == NULL)
		errx(1, "%s: %s", hostname, hstrerror(h_errno));

	if ((sp = getservbyname("ntp", "udp")) == NULL) {
		sp = &ssp;
		sp->s_port = 123;
		sp->s_proto = "udp";
	}

	if ((pp = getprotobyname(sp->s_proto)) == NULL) {
		pp = &ppp;
		pp->p_proto = 17;
	}

	if ((s = socket(AF_INET, SOCK_DGRAM, pp->p_proto)) == -1)
		err(1, "Could not create socket");

	bzero(&peer, sizeof(peer));
	peer.sin_family = AF_INET;
	peer.sin_port = sp->s_port;
	(void) memcpy(&(peer.sin_addr.s_addr), hp->h_addr, hp->h_length);

	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;

	if (bind(s, (struct sockaddr *) &server, sizeof(server)))
		err(1, "Could not bind to socket");

	packets = sync_ntp(s, (struct sockaddr *) &peer, &offset, &error);

	close(s);

	if (packets == 0)
		errx(1, "No acceptable packets received");

	if (error > NTP_INSANITY)
		errx(1, "Unable to get a reasonable time estimate");

	fprintf(stderr,"Correction: %.6f +/- %.6f\n", offset,error);

	create_timeval(offset, new, adjust);
}

int
sync_ntp(int fd, const struct sockaddr *peer, double *offset, double *error)
{
	int attempts = 0, accepts = 0, rejects = 0;
	int delay = MAX_DELAY;

	double deadline;
	double a, b, x, y;
	double minerr = 0.1;		/* Maximum ignorable variation */
	double dispersion = 0.0;	/* The source dispersion in seconds */

	struct ntp_data data;

	deadline = current_time(JAN_1970) + delay;
        *offset = 0.0;
        *error = NTP_INSANITY;

        while (accepts < MAX_QUERIES && attempts < 2 * MAX_QUERIES) {
		if (current_time(JAN_1970) > deadline)
			errx(1, "Not enough valid responses received in time");

		make_packet(&data);
		write_packet(fd, peer, &data);

		if (read_packet(fd, &data, &x, &y, &dispersion)) {
			if (++rejects > MAX_QUERIES)
				errx(1, "Too many bad or lost packets");
			else
			  continue;
		} else
			++accepts;

		fprintf(stderr,"Offset: %.6f +/- %.6f disp=%.6f\n",
		    x, y, dispersion);

		if ((a = x - *offset) < 0.0) a = -a;
		if (accepts <= 1) a = 0.0;
		b = *error + y;
		if (y < *error) {
			*offset = x;
			*error = y;
		}

		fprintf(stderr,"Best: %.6f +/- %.6f\n", *offset, *error);

		if (a > b)
			errx(1, "Inconsistent times recieved from NTP server");

		if (*error <= minerr) break;
        }

	return accepts;
}

/* Create an outgoing NTP packet */
void
make_packet(struct ntp_data *data)
{
	data->status = 0;
	data->version = NTP_VERSION;
	data->mode = NTP_MODE_CLIENT;
	data->stratum = 0;
	data->polling = 0;
	data->precision = 0;
	data->reference = data->dispersion = 0.0;
	data->receive = data->originate = 0.0;
	data->current = data->transmit = current_time(JAN_1970);
}

int
write_packet(int fd, const struct sockaddr *peer, struct ntp_data *data)
{
	unsigned char transmit[NTP_PACKET_MIN];
	int length;

	pack_ntp(transmit, NTP_PACKET_MIN, data);
	length = sendto(fd, transmit, NTP_PACKET_MIN, 0, peer, sizeof(*peer));
	if(length <= 0) {
		warnx("Unable to send NTP packet to server");
        	return 1;
	}

	return 0;
}

/* Check the packet and work out the offset and optionally the error.
 * Note that this contains more checking than xntp does. Return 0 for
 * success, 1 for failure. Note that it must not change its arguments
 * if it fails. */
int
read_packet(int fd, struct ntp_data *data, double *off, double *error,
    double *dispersion)
{
	unsigned char receive[NTP_PACKET_MAX+1];
	double delay1, delay2, x, y;
	int length;
	fd_set rfds;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 1000000 * MAX_DELAY / MAX_QUERIES;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	if (select (fd + 1, &rfds, NULL, NULL, &tv) < 1)
		return 1; /* failure or timeout */

	length = recvfrom(fd, receive, NTP_PACKET_MAX + 1, 0, NULL, 0);
	if (length <= 0) {
		warnx("Unable to receive NTP packet from server");
        	return 1;
	}

	if (length < NTP_PACKET_MIN || length > NTP_PACKET_MAX) {
		warnx("Invalid NTP packet size, packet reject");
	        return 1;
	}

	unpack_ntp(data, receive, length);

	if (data->version < NTP_VERSION_MIN ||
	    data->version > NTP_VERSION_MAX) {
		warnx("Invalid NTP version, packet rejected");
		return 1;
	}

	if (data->mode != NTP_MODE_SERVER) {
		warnx("Invalid NTP server mode, packet rejected");
		return 1;
	}

	/* Note that the conventions are very poorly defined in the NTP
	 * protocol, so we have to guess.  Any full NTP server perpetrating
	 * completely unsynchronised packets is an abomination, anyway, so
	 * reject it. */

	delay1 = data->transmit - data->receive;
	delay2 = data->current - data->originate;

	if (data->reference == 0.0 ||
	    data->transmit == 0.0 ||
	    data->receive == 0.0 ||
	    (data->reference != 0.0 && data->receive < data->reference) ||
	    delay1 < 0.0 ||
	    delay1 > NTP_INSANITY ||
	    delay2 < 0.0 ||
	    delay2 > NTP_INSANITY ||
	    data->dispersion > NTP_INSANITY) {
		warnx("Incomprehensible NTP packet rejected");
		return 1;
	}

	if (*dispersion < data->dispersion)
		*dispersion = data->dispersion;

        x = data->receive - data->originate;
        y = (data->transmit == 0.0 ? 0.0 : data->transmit-data->current);
        *off = 0.5*(x+y);
        *error = x-y;
        x = data->current - data->originate;
        if (0.5*x > *error)
		*error = 0.5*x;

	return 0;
}

/* Pack the essential data into an NTP packet, bypassing struct layout
 * and endian problems.  Note that it ignores fields irrelevant to
 * SNTP. */
void
pack_ntp(unsigned char *packet, int length, struct ntp_data *data)
{
	int i, k;
	double d;

	memset(packet,0, (size_t)length);

	packet[0] = (data->status<<6)|(data->version<<3)|data->mode;
	packet[1] = data->stratum;
	packet[2] = data->polling;
	packet[3] = data->precision;

	d = data->originate/NTP_SCALE;
	for (i = 0; i < 8; ++i) {
		if ((k = (int)(d *= 256.0)) >= 256) k = 255;
		packet[NTP_ORIGINATE+i] = k;
		d -= k;
	}

	d = data->receive/NTP_SCALE;
	for (i = 0; i < 8; ++i) {
		if ((k = (int)(d *= 256.0)) >= 256) k = 255;
		packet[NTP_RECEIVE+i] = k;
		d -= k;
	}

	d = data->transmit/NTP_SCALE;
	for (i = 0; i < 8; ++i) {
		if ((k = (int)(d *= 256.0)) >= 256) k = 255;
		packet[NTP_TRANSMIT+i] = k;
		d -= k;
	}
}

/* Unpack the essential data from an NTP packet, bypassing struct
 * layout and endian problems.  Note that it ignores fields irrelevant
 * to SNTP. */
void
unpack_ntp(struct ntp_data *data, unsigned char *packet, int length)
{
	int i;
	double d;

	data->current = current_time(JAN_1970);

	data->status = (packet[0] >> 6);
	data->version = (packet[0] >> 3)&0x07;
	data->mode = packet[0]&0x07;
	data->stratum = packet[1];
	data->polling = packet[2];
	data->precision = packet[3];

	for (i = 0, d = 0.0; i < 4; ++i)
	    d = 256.0*d+packet[NTP_DISP_FIELD+i];
	data->dispersion = d/65536.0;

	for (i = 0, d = 0.0; i < 8; ++i)
	    d = 256.0*d+packet[NTP_REFERENCE+i];
	data->reference = d/NTP_SCALE;

	for (i = 0, d = 0.0; i < 8; ++i)
	    d = 256.0*d+packet[NTP_ORIGINATE+i];
	data->originate = d/NTP_SCALE;

	for (i = 0, d = 0.0; i < 8; ++i)
	    d = 256.0*d+packet[NTP_RECEIVE+i];
	data->receive = d/NTP_SCALE;

	for (i = 0, d = 0.0; i < 8; ++i)
	    d = 256.0*d+packet[NTP_TRANSMIT+i];
	data->transmit = d/NTP_SCALE;
}

/* Get the current UTC time in seconds since the Epoch plus an offset
 * (usually the time from the beginning of the century to the Epoch)
 * */
double
current_time(double offset)
{
	struct timeval current;

	if (gettimeofday(&current, NULL))
		err(1, "Could not get local time of day");

	return offset + current.tv_sec + 1.0e-6 * current.tv_usec;
}

/* Change offset into current UTC time. This is portable, even if
 * struct timeval uses an unsigned long for tv_sec. */
void
create_timeval(double difference, struct timeval *new, struct timeval *adjust)
{
	struct timeval old;
	long n;

	/* Start by converting to timeval format. Note that we have to
	 * cater for negative, unsigned values. */
	if ((n = (long) difference) > difference)
		--n;
	adjust->tv_sec = n;
	adjust->tv_usec = (long) (MILLION_D * (difference-n));
	errno = 0;
	if (gettimeofday(&old, NULL))
		err(1, "Could not get local time of day");
	new->tv_sec = old.tv_sec + adjust->tv_sec;
	new->tv_usec = (n = (long) old.tv_usec + (long) adjust->tv_usec);

	if (n < 0) {
		new->tv_usec += MILLION_L;
		--new->tv_sec;
	} else if (n >= MILLION_L) {
		new->tv_usec -= MILLION_L;
		++new->tv_sec;
	}
}

#ifdef DEBUG
void
print_packet(const struct ntp_data *data)
{
	printf("status:      %u\n", data->status);
	printf("version:     %u\n", data->version);
	printf("mode:        %u\n", data->mode);
	printf("stratum:     %u\n", data->stratum);
	printf("polling:     %u\n", data->polling);
	printf("precision:   %u\n", data->precision);
	printf("dispersion:  %e\n", data->dispersion);
	printf("reference:   %e\n", data->reference);
	printf("originate:   %e\n", data->originate);
	printf("receive:     %e\n", data->receive);
	printf("transmit:    %e\n", data->transmit);
	printf("current:     %e\n", data->current);
};
#endif
