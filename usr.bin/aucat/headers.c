/*	$OpenBSD: headers.c,v 1.10 2010/01/10 21:47:41 ratchov Exp $	*/
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aparams.h"
#include "conf.h"
#include "wav.h"

/*
 * Encoding IDs used in .wav headers.
 */
#define WAV_ENC_PCM	1
#define WAV_ENC_ALAW	6
#define WAV_ENC_ULAW	7

struct wavriff {
	char magic[4];
	uint32_t size;
	char type[4];
} __packed;

struct wavchunk {
	char id[4];
	uint32_t size;
} __packed;

struct wavfmt {
	uint16_t fmt;
	uint16_t nch;
	uint32_t rate;
	uint32_t byterate;
	uint16_t blkalign;
	uint16_t bits;
} __packed;

char wav_id_riff[4] = { 'R', 'I', 'F', 'F' };
char wav_id_wave[4] = { 'W', 'A', 'V', 'E' };
char wav_id_data[4] = { 'd', 'a', 't', 'a' };
char wav_id_fmt[4] = { 'f', 'm', 't', ' ' };

int
wav_readfmt(int fd, unsigned csize, struct aparams *par, short **map)
{
	struct wavfmt fmt;
	unsigned nch, cmax, rate, bits, enc;

	if (csize < sizeof(fmt)) {
		warnx("bogus format chunk");
		return 0;
	}
	if (read(fd, &fmt, sizeof(fmt)) != sizeof(fmt)) {
		warn("riff_read: chunk");
		return 0;
	}
	enc = letoh16(fmt.fmt);
	switch (enc) {
	case WAV_ENC_PCM:
		*map = NULL;
		break;
	case WAV_ENC_ALAW:
		*map = wav_alawmap;
		break;
	case WAV_ENC_ULAW:
		*map = wav_ulawmap;
		break;
	default:
		errx(1, "%u: unsupported encoding in .wav file", enc);
	}
	nch = letoh16(fmt.nch);
	if (nch == 0) {
		warnx("zero number of channels");
		return 0;
	}
	cmax = par->cmin + nch - 1;
	if (cmax >= NCHAN_MAX) {
		warnx("%u:%u: bad range", par->cmin, cmax);
		return 0;
	}
	rate = letoh32(fmt.rate);
	if (rate < RATE_MIN || rate > RATE_MAX) {
		warnx("%u: bad sample rate", rate);
		return 0;
	}
	bits = letoh16(fmt.bits);
	if (bits == 0 || bits > 32) {
		warnx("%u: bad number of bits", bits);
		return 0;
	}
	if (enc == WAV_ENC_PCM) {
		par->bps = (bits + 7) / 8;
		par->bits = bits;
		par->le = 1;
		par->sig = (bits <= 8) ? 0 : 1;	/* ask microsoft why... */
	} else {
		if (bits != 8) {
			warnx("%u: mulaw/alaw encoding not 8-bit", bits);
			return 0;
		}
		par->bits = 8 * sizeof(short);
		par->bps = sizeof(short);
		par->le = NATIVE_LE;
		par->sig = 1;
	}
	par->msb = 1;
	par->cmax = cmax;
	par->rate = rate;
	return 1;
}

int
wav_readhdr(int fd, struct aparams *par, off_t *datasz, short **map)
{
	struct wavriff riff;
	struct wavchunk chunk;
	unsigned csize, rsize, pos = 0;
	int fmt_done = 0;

	if (lseek(fd, 0, SEEK_SET) < 0) {
		warn("lseek: 0");
		return 0;
	}
	if (read(fd, &riff, sizeof(riff)) != sizeof(riff)) {
		warn("wav_readhdr: header");
		return 0;
	}
	if (memcmp(&riff.magic, &wav_id_riff, 4) != 0 ||
	    memcmp(&riff.type, &wav_id_wave, 4)) {
		warnx("not a wave file");
		return 0;
	}
	rsize = letoh32(riff.size);
	while (pos + sizeof(struct wavchunk) <= rsize) {
		if (read(fd, &chunk, sizeof(chunk)) != sizeof(chunk)) {
			warn("wav_readhdr: chunk");
			return 0;
		}
		csize = letoh32(chunk.size);
		if (memcmp(chunk.id, wav_id_fmt, 4) == 0) {
			if (!wav_readfmt(fd, csize, par, map))
				return 0;
			fmt_done = 1;
		} else if (memcmp(chunk.id, wav_id_data, 4) == 0) {
			*datasz = csize;
			break;
		} else {
#ifdef DEBUG
			if (debug_level >= 1) 
				warnx("ignoring chuck <%.4s>\n", chunk.id);
#endif
		}

		/*
		 * next chunk
		 */
		pos += sizeof(struct wavchunk) + csize;
		if (lseek(fd, sizeof(riff) + pos, SEEK_SET) < 0) {
			warn("lseek");
			return 0;
		}
	}
	if (!fmt_done) {
		warnx("missing format chunk");
		return 0;
	}
	return 1;
}

int
wav_writehdr(int fd, struct aparams *par)
{
	off_t datasz;
	unsigned nch = par->cmax - par->cmin + 1;
	struct {
		struct wavriff riff;
		struct wavchunk fmt_hdr;
		struct wavfmt fmt;
		struct wavchunk data_hdr;
	} hdr;

	datasz = lseek(fd, 0, SEEK_CUR);
	if (datasz < 0) {
		warn("wav_writehdr: lseek(end)");
		return 0;
	}
	if (datasz >= sizeof(hdr))
		datasz -= sizeof(hdr);
	else
		datasz = 0;

	/*
	 * Check that encoding is supported by .wav file format.
	 */
	if (par->bits > 8 && !par->le) {
		warnx("samples must be little endian");
		return 0;
	}
	if (8 * par->bps - par->bits >= 8) {
		warnx("padding must be less than 8 bits");
		return 0;
	}
	if ((par->bits <= 8 && par->sig) || (par->bits > 8 && !par->sig)) {
		warnx("samples with more (less) than 8 bits must be signed "
		    "(unsigned)");
		return 0;
	}
	if (8 * par->bps != par->bits && !par->msb) {
		warnx("samples must be MSB justified");
		return 0;
	}

	memcpy(hdr.riff.magic, wav_id_riff, 4);
	memcpy(hdr.riff.type, wav_id_wave, 4);
	hdr.riff.size = htole32(datasz + sizeof(hdr) - sizeof(hdr.riff));

	memcpy(hdr.fmt_hdr.id, wav_id_fmt, 4);
	hdr.fmt_hdr.size = htole32(sizeof(hdr.fmt));
	hdr.fmt.fmt = htole16(1);
	hdr.fmt.nch = htole16(nch);
	hdr.fmt.rate = htole32(par->rate);
	hdr.fmt.byterate = htole32(par->rate * par->bps * nch);
	hdr.fmt.bits = htole16(par->bits);
	hdr.fmt.blkalign = 0;

	memcpy(hdr.data_hdr.id, wav_id_data, 4);
	hdr.data_hdr.size = htole32(datasz);

	if (lseek(fd, 0, SEEK_SET) < 0) {
		warn("wav_writehdr: lseek");
		return 0;
	}
	if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		warn("wav_writehdr: write");
		return 0;
	}
	return 1;
}
