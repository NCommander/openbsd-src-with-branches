/*	$OpenBSD: autest.c,v 1.5 2003/02/04 08:01:50 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
 * All rights reserved.
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

/* XXX ADPCM is currently pretty broken... diagnosis and fix welcome */
#undef	USE_ADPCM

#include "adpcm.h"
#include "law.h"

int main(int, char **);
void check_encoding(int, audio_encoding_t *);
void check_encoding_mono(int, audio_encoding_t *);
void check_encoding_stereo(int, audio_encoding_t *);
void enc_ulaw_8(int, audio_encoding_t *, int);
void enc_alaw_8(int, audio_encoding_t *, int);
void enc_ulinear_8(int, audio_encoding_t *, int);
void enc_ulinear_16(int, audio_encoding_t *, int, int);
void enc_slinear_8(int, audio_encoding_t *, int);
void enc_slinear_16(int, audio_encoding_t *, int, int);
void enc_adpcm_8(int, audio_encoding_t *, int);
void audio_wait(int);

#define	PLAYSECS	2

#define	DEFAULT_DEV	"/dev/sound"

int
main(int argc, char **argv)
{
	audio_info_t ainfo;
	char *fname = NULL;
	int fd, i, c;

	while ((c = getopt(argc, argv, "f:")) != -1) {
		switch (c) {
		case 'f':
			fname = optarg;
			break;
		case '?':
		default:
			fprintf(stderr, "%s [-f device]\n", argv[0]);
			return (1);
		}
	}

	if (fname == NULL)
		fname = DEFAULT_DEV;

	fd = open(fname, O_RDWR, 0);
	if (fd == -1)
		err(1, "open");


	if (ioctl(fd, AUDIO_GETINFO, &ainfo) == -1)
		err(1, "%s: audio_getinfo", fname);

	for (i = 0; ; i++) {
		audio_encoding_t enc;

		enc.index = i;
		if (ioctl(fd, AUDIO_GETENC, &enc) == -1)
			break;
		check_encoding(fd, &enc);
	}
	close(fd);

	return (0);
}

void
check_encoding(int fd, audio_encoding_t *enc)
{
	printf("%s:%d%s",
	    enc->name,
	    enc->precision,
	    (enc->flags & AUDIO_ENCODINGFLAG_EMULATED) ? "*" : "");
	fflush(stdout);
	check_encoding_mono(fd, enc);
	check_encoding_stereo(fd, enc);
	printf("\n");
}

void
check_encoding_mono(int fd, audio_encoding_t *enc)
{
	int skipped = 0;

	printf("...mono");
	fflush(stdout);

	if (enc->precision == 8) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULAW:
			enc_ulaw_8(fd, enc, 1);
			break;
		case AUDIO_ENCODING_ALAW:
			enc_alaw_8(fd, enc, 1);
			break;
		case AUDIO_ENCODING_ULINEAR:
		case AUDIO_ENCODING_ULINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_8(fd, enc, 1);
			break;
		case AUDIO_ENCODING_SLINEAR:
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_8(fd, enc, 1);
			break;
		case AUDIO_ENCODING_ADPCM:
			enc_adpcm_8(fd, enc, 1);
			break;
		default:
			skipped = 1;
		}
	}

	if (enc->precision == 16) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULINEAR_LE:
			enc_ulinear_16(fd, enc, 1, LITTLE_ENDIAN);
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_16(fd, enc, 1, BIG_ENDIAN);
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			enc_slinear_16(fd, enc, 1, LITTLE_ENDIAN);
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_16(fd, enc, 1, BIG_ENDIAN);
			break;
		default:
			skipped = 1;
		}
	}

	if (skipped)
		printf("[skip]");
}

void
check_encoding_stereo(int fd, audio_encoding_t *enc)
{
	int skipped = 0;

	printf("...stereo");
	fflush(stdout);

	if (enc->precision == 8) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULAW:
			enc_ulaw_8(fd, enc, 2);
			break;
		case AUDIO_ENCODING_ALAW:
			enc_alaw_8(fd, enc, 2);
			break;
		case AUDIO_ENCODING_ULINEAR:
		case AUDIO_ENCODING_ULINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_8(fd, enc, 2);
			break;
		case AUDIO_ENCODING_SLINEAR:
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_8(fd, enc, 2);
			break;
		case AUDIO_ENCODING_ADPCM:
			enc_adpcm_8(fd, enc, 2);
			break;
		default:
			skipped = 1;
		}
	}

	if (enc->precision == 16) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULINEAR_LE:
			enc_ulinear_16(fd, enc, 2, LITTLE_ENDIAN);
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_16(fd, enc, 2, BIG_ENDIAN);
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			enc_slinear_16(fd, enc, 2, LITTLE_ENDIAN);
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_16(fd, enc, 2, BIG_ENDIAN);
			break;
		default:
			skipped = 1;
		}
	}

	if (skipped)
		printf("[skip]");
}

void
enc_ulinear_8(int fd, audio_encoding_t *enc, int chans)
{
	audio_info_t inf;
	u_int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		warn("setinfo");
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		warn("getinfo");
		goto out;
	}

	samples = (u_int8_t *)malloc(inf.play.sample_rate * chans);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		u_int8_t v;

		d = 127.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * 440.0));
		d = rintf(d + 127.0);
		v = d;

		for (j = 0; j < chans; j++) {
			*p = v;
			p++;
		}
	}

	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans);
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_slinear_8(int fd, audio_encoding_t *enc, int chans)
{
	audio_info_t inf;
	int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		warn("setinfo");
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		warn("getinfo");
		goto out;
	}

	samples = (int8_t *)malloc(inf.play.sample_rate * chans);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		int8_t v;

		d = 127.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * 440.0));
		d = rintf(d);
		v = d;

		for (j = 0; j < chans; j++) {
			*p = v;
			p++;
		}
	}

	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans);
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_slinear_16(int fd, audio_encoding_t *enc, int chans, int order)
{
	audio_info_t inf;
	u_int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		warn("setinfo");
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		warn("getinfo");
		goto out;
	}

	samples = (int8_t *)malloc(inf.play.sample_rate * chans * 2);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		int16_t v;

		d = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * 440.0));
		d = rintf(d);
		v = d;

		for (j = 0; j < chans; j++) {
			if (order == LITTLE_ENDIAN) {
				*p = (v & 0x00ff) >> 0;
				p++;
				*p = (v & 0xff00) >> 8;
				p++;
			} else {
				*p = (v & 0xff00) >> 8;
				p++;
				*p = (v & 0x00ff) >> 0;
				p++;
			}
		}
	}

	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans * 2);
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_ulinear_16(int fd, audio_encoding_t *enc, int chans, int order)
{
	audio_info_t inf;
	u_int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		warn("setinfo");
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		warn("getinfo");
		goto out;
	}

	samples = (u_int8_t *)malloc(inf.play.sample_rate * chans * 2);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		u_int16_t v;

		d = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * 440.0));
		d = rintf(d + 32767.0);
		v = d;

		for (j = 0; j < chans; j++) {
			if (order == LITTLE_ENDIAN) {
				*p = (v >> 0) & 0xff;
				p++;
				*p = (v >> 8) & 0xff;
				p++;
			} else {
				*p = (v >> 8) & 0xff;
				p++;
				*p = (v >> 0) & 0xff;
				p++;
			}
		}
	}

	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans * 2);
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_adpcm_8(int fd, audio_encoding_t *enc, int chans)
{
	audio_info_t inf;
	struct adpcm_state adsts;
	int16_t *samples = NULL;
	int i, j;
	char *outbuf = NULL, *sbuf = NULL, *p;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		warn("setinfo");
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		warn("getinfo");
		goto out;
	}

	bzero(&adsts, sizeof(adsts));

	samples = (int16_t *)malloc(inf.play.sample_rate * sizeof(*samples));
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	sbuf = (char *)malloc(inf.play.sample_rate / 2);
	if (sbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < inf.play.sample_rate; i++) {
		float d;

		d = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * 440.0));
		samples[i] = rintf(d);
	}

	outbuf = (char *)malloc((inf.play.sample_rate / 2) * chans);
	if (outbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < PLAYSECS; i++) {
		adpcm_coder(samples, sbuf, inf.play.sample_rate, &adsts);

		for (i = 0, p = outbuf; i < inf.play.sample_rate / 2; i++) {
			for (j = 0; j < chans; j++, p++) {
				*p = sbuf[i];
			}
		}

		write(fd, outbuf, (inf.play.sample_rate / 2) * chans);
	}
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
	if (outbuf != NULL)
		free(outbuf);
	if (sbuf != NULL)
		free(sbuf);
}

void
enc_ulaw_8(int fd, audio_encoding_t *enc, int chans)
{
	audio_info_t inf;
	int16_t *samples = NULL;
	int i, j;
	u_int8_t *outbuf = NULL, *p;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		warn("setinfo");
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		warn("getinfo");
		goto out;
	}

	samples = (int16_t *)calloc(inf.play.sample_rate, sizeof(*samples));
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	outbuf = (u_int8_t *)malloc(inf.play.sample_rate * chans);
	if (outbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < inf.play.sample_rate; i++) {
		float x;

		x = 32765.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * 440.0));
		samples[i] = x;
	}

	for (i = 0, p = outbuf; i < inf.play.sample_rate; i++) {
		for (j = 0; j < chans; j++) {
			*p = linear2ulaw(samples[i]);
			p++;
		}
	}

	for (i = 0; i < PLAYSECS; i++) {
		write(fd, outbuf, inf.play.sample_rate * chans);
	}
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
	if (outbuf != NULL)
		free(outbuf);
}

void
enc_alaw_8(int fd, audio_encoding_t *enc, int chans)
{
	audio_info_t inf;
	int16_t *samples = NULL;
	int i, j;
	u_int8_t *outbuf = NULL, *p;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		warn("setinfo");
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		warn("getinfo");
		goto out;
	}

	samples = (int16_t *)calloc(inf.play.sample_rate, sizeof(*samples));
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	outbuf = (u_int8_t *)malloc(inf.play.sample_rate * chans);
	if (outbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < inf.play.sample_rate; i++) {
		float x;

		x = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * 440.0));
		samples[i] = x;
	}

	for (i = 0, p = outbuf; i < inf.play.sample_rate; i++) {
		for (j = 0; j < chans; j++) {
			*p = linear2alaw(samples[i]);
			p++;
		}
	}

	for (i = 0; i < PLAYSECS; i++) {
		write(fd, outbuf, inf.play.sample_rate * chans);
	}
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
	if (outbuf != NULL)
		free(outbuf);
}

void
audio_wait(int fd)
{
	if (ioctl(fd, AUDIO_DRAIN, NULL) == -1)
		warn("drain");
}
