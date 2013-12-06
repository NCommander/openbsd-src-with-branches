/*	$OpenBSD: options.c,v 1.56 2013/07/11 01:34:00 krw Exp $	*/

/* DHCP options parsing and reassembly. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include "dhcpd.h"

int parse_option_buffer(struct option_data *, unsigned char *, int);

/*
 * Parse options out of the specified buffer, storing addresses of
 * option values in options. Return 0 if errors, 1 if not.
 */
int
parse_option_buffer(struct option_data *options, unsigned char *buffer,
    int length)
{
	unsigned char *s, *t, *end = buffer + length;
	int len, code;

	for (s = buffer; *s != DHO_END && s < end; ) {
		code = s[0];

		/* Pad options don't have a length - just skip them. */
		if (code == DHO_PAD) {
			s++;
			continue;
		}

		/*
		 * All options other than DHO_PAD and DHO_END have a one-byte
		 * length field. It could be 0! Make sure that the length byte
		 * is present, and all the data is available.
		 */
		if (s + 1 < end) {
			len = s[1];
			if (s + 1 + len < end) {
				; /* option data is all there. */
			} else {
				warning("option %s (%d) larger than buffer.",
				    dhcp_options[code].name, len);
				return (0);
			}
		} else {
			warning("option %s has no length field.",
			    dhcp_options[code].name);
			return (0);
		}

		/*
		 * Strip trailing NULs from ascii ('t') options. They
		 * will be treated as DHO_PAD options. i.e. ignored. RFC 2132
		 * says "Options containing NVT ASCII data SHOULD NOT include
		 * a trailing NULL; however, the receiver of such options
		 * MUST be prepared to delete trailing nulls if they exist."
		 */
		if (dhcp_options[code].format[0] == 't') {
			while (len > 0 && s[len + 1] == '\0')
				len--;
		}

		/*
		 * If we haven't seen this option before, just make
		 * space for it and copy it there.
		 */
		if (!options[code].data) {
			if (!(t = calloc(1, len + 1)))
				error("Can't allocate storage for option %s.",
				    dhcp_options[code].name);
			/*
			 * Copy and NUL-terminate the option (in case
			 * it's an ASCII string).
			 */
			memcpy(t, &s[2], len);
			t[len] = 0;
			options[code].len = len;
			options[code].data = t;
		} else {
			/*
			 * If it's a repeat, concatenate it to whatever
			 * we last saw.
			 */
			t = calloc(1, len + options[code].len + 1);
			if (!t)
				error("Can't expand storage for option %s.",
				    dhcp_options[code].name);
			memcpy(t, options[code].data, options[code].len);
			memcpy(t + options[code].len, &s[2], len);
			options[code].len += len;
			t[options[code].len] = 0;
			free(options[code].data);
			options[code].data = t;
		}
		s += len + 2;
	}

	return (1);
}

/*
 * Copy as many options as fit in buflen bytes of buf. Return the
 * offset of the start of the last option copied. A caller can check
 * to see if it's DHO_END to decide if all the options were copied.
 */
int
cons_options(struct option_data *options)
{
	unsigned char *buf = client->bootrequest_packet.options;
	int buflen = 576 - DHCP_FIXED_LEN;
	int ix, incr, length, bufix, code, lastopt = -1;

	memset(buf, 0, buflen);

	memcpy(buf, DHCP_OPTIONS_COOKIE, 4);
	if (options[DHO_DHCP_MESSAGE_TYPE].data) {
		memcpy(&buf[4], DHCP_OPTIONS_MESSAGE_TYPE, 3);
		buf[6] = options[DHO_DHCP_MESSAGE_TYPE].data[0];
		bufix = 7;
	} else
		bufix = 4;

	for (code = DHO_SUBNET_MASK; code < DHO_END; code++) {
		if (!options[code].data || code == DHO_DHCP_MESSAGE_TYPE)
			continue;

		length = options[code].len;
		if (bufix + length + 2*((length+254)/255) >= buflen)
			return (lastopt);

		lastopt = bufix;
		ix = 0;

		while (length) {
			incr = length > 255 ? 255 : length;

			buf[bufix++] = code;
			buf[bufix++] = incr;
			memcpy(buf + bufix, options[code].data + ix, incr);

			length -= incr;
			ix += incr;
			bufix += incr;
		}
	}

	if (bufix < buflen) {
		buf[bufix] = DHO_END;
		lastopt = bufix;
	}

	return (lastopt);
}

/*
 * Format the specified option so that a human can easily read it.
 */
char *
pretty_print_option(unsigned int code, struct option_data *option,
    int emit_punct)
{
	static char optbuf[32768]; /* XXX */
	int hunksize = 0, numhunk = -1, numelem = 0;
	char fmtbuf[32], *op = optbuf;
	int i, j, k, opleft = sizeof(optbuf);
	unsigned char *data = option->data;
	unsigned char *dp = data;
	int len = option->len;
	struct in_addr foo;
	char comma;

	memset(optbuf, 0, sizeof(optbuf));

	/* Code should be between 0 and 255. */
	if (code > 255) {
		warning("pretty_print_option: bad code %d", code);
		goto done;
	}

	if (emit_punct)
		comma = ',';
	else
		comma = ' ';

	/* Figure out the size of the data. */
	for (i = 0; dhcp_options[code].format[i]; i++) {
		if (!numhunk) {
			warning("%s: Excess information in format string: %s",
			    dhcp_options[code].name,
			    &(dhcp_options[code].format[i]));
			goto done;
		}
		numelem++;
		fmtbuf[i] = dhcp_options[code].format[i];
		switch (dhcp_options[code].format[i]) {
		case 'A':
			--numelem;
			fmtbuf[i] = 0;
			numhunk = 0;
			if (hunksize == 0) {
				warning("%s: no size indicator before A"
				    " in format string: %s",
				    dhcp_options[code].name,
				    dhcp_options[code].format);
				goto done;
			}
			break;
		case 'X':
			for (k = 0; k < len; k++)
				if (!isascii(data[k]) ||
				    !isprint(data[k]))
					break;
			if (k == len) {
				fmtbuf[i] = 't';
				numhunk = -2;
			} else {
				fmtbuf[i] = 'x';
				hunksize++;
				comma = ':';
				numhunk = 0;
			}
			fmtbuf[i + 1] = 0;
			break;
		case 't':
			fmtbuf[i] = 't';
			fmtbuf[i + 1] = 0;
			numhunk = -2;
			break;
		case 'I':
		case 'l':
		case 'L':
			hunksize += 4;
			break;
		case 's':
		case 'S':
			hunksize += 2;
			break;
		case 'b':
		case 'B':
		case 'f':
			hunksize++;
			break;
		case 'e':
			break;
		default:
			warning("%s: garbage in format string: %s",
			    dhcp_options[code].name,
			    &(dhcp_options[code].format[i]));
			goto done;
		}
	}

	/* Check for too few bytes. */
	if (hunksize > len) {
		warning("%s: expecting at least %d bytes; got %d",
		    dhcp_options[code].name, hunksize, len);
		goto done;
	}
	/* Check for too many bytes. */
	if (numhunk == -1 && hunksize < len) {
		warning("%s: expecting only %d bytes: got %d",
		    dhcp_options[code].name, hunksize, len);
		goto done;
	}

	/* If this is an array, compute its size. */
	if (!numhunk)
		numhunk = len / hunksize;
	/* See if we got an exact number of hunks. */
	if (numhunk > 0 && numhunk * hunksize != len) {
		warning("%s: expecting %d bytes: got %d",
		    dhcp_options[code].name, numhunk * hunksize, len);
		goto done;
	}

	/* A one-hunk array prints the same as a single hunk. */
	if (numhunk < 0)
		numhunk = 1;

	/* Cycle through the array (or hunk) printing the data. */
	for (i = 0; i < numhunk; i++) {
		for (j = 0; j < numelem; j++) {
			int opcount;
			size_t oplen;
			switch (fmtbuf[j]) {
			case 't':
				if (emit_punct) {
					*op++ = '"';
					opleft--;
				}
				for (; dp < data + len; dp++) {
					if (!isascii(*dp) ||
					    !isprint(*dp)) {
						if (dp + 1 != data + len ||
						    *dp != 0) {
							size_t oplen;
							snprintf(op, opleft,
							    "\\%03o", *dp);
							oplen = strlen(op);
							op += oplen;
							opleft -= oplen;
						}
					} else if (*dp == '"' ||
					    *dp == '\'' ||
					    *dp == '$' ||
					    *dp == '`' ||
					    *dp == '\\') {
						*op++ = '\\';
						*op++ = *dp;
						opleft -= 2;
					} else {
						*op++ = *dp;
						opleft--;
					}
				}
				if (emit_punct) {
					*op++ = '"';
					opleft--;
				}

				*op = 0;
				break;
			case 'I':
				foo.s_addr = htonl(getULong(dp));
				opcount = strlcpy(op, inet_ntoa(foo), opleft);
				if (opcount >= opleft)
					goto toobig;
				opleft -= opcount;
				dp += 4;
				break;
			case 'l':
				opcount = snprintf(op, opleft, "%ld",
				    (long)getLong(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 4;
				break;
			case 'L':
				opcount = snprintf(op, opleft, "%ld",
				    (unsigned long)getULong(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 4;
				break;
			case 's':
				opcount = snprintf(op, opleft, "%d",
				    getShort(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 2;
				break;
			case 'S':
				opcount = snprintf(op, opleft, "%d",
				    getUShort(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 2;
				break;
			case 'b':
				opcount = snprintf(op, opleft, "%d",
				    *(char *)dp++);
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				break;
			case 'B':
				opcount = snprintf(op, opleft, "%d", *dp++);
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				break;
			case 'x':
				opcount = snprintf(op, opleft, "%x", *dp++);
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				break;
			case 'f':
				opcount = strlcpy(op,
				    *dp++ ? "true" : "false", opleft);
				if (opcount >= opleft)
					goto toobig;
				opleft -= opcount;
				break;
			default:
				warning("Unexpected format code %c", fmtbuf[j]);
			}
			oplen = strlen(op);
			op += oplen;
			opleft -= oplen;
			if (opleft < 1)
				goto toobig;
			if (j + 1 < numelem && comma != ':') {
				*op++ = ' ';
				opleft--;
			}
		}
		if (i + 1 < numhunk) {
			*op++ = comma;
			opleft--;
		}
		if (opleft < 1)
			goto toobig;

	}

done:
	return (optbuf);

toobig:
	memset(optbuf, 0, sizeof(optbuf));
	return (optbuf);
}

void
do_packet(unsigned int from_port, struct in_addr from,
    struct ether_addr *hfrom)
{
	struct dhcp_packet *packet = &client->packet;
	struct option_data options[256];
	struct reject_elem *ap;
	void (*handler)(struct in_addr, struct option_data *, char *);
	char *type, *info;
	int i, rslt, options_valid = 1;

	if (packet->hlen != ETHER_ADDR_LEN) {
#ifdef DEBUG
		debug("Discarding packet with hlen != %s (%u)",
		    ifi->name, packet->hlen);
#endif
		return;
	} else if (memcmp(&ifi->hw_address, packet->chaddr,
	    sizeof(ifi->hw_address))) {
#ifdef DEBUG
		debug("Discarding packet with chaddr != %s (%s)", ifi->name,
		    ether_ntoa((struct ether_addr *)packet->chaddr));
#endif
		return;
	}

	if (client->xid != client->packet.xid) {
#ifdef DEBUG
		debug("Discarding packet with XID != %u (%u)", client->xid,
		    client->packet.xid);
#endif
		return;
	}

	for (ap = config->reject_list; ap; ap = ap->next)
		if (from.s_addr == ap->addr.s_addr) {
#ifdef DEBUG
			debug("Discarding packet from address on reject list "
			    "(%s)", inet_ntoa(from));
#endif
			return;
		}

	memset(options, 0, sizeof(options));

	if (memcmp(&packet->options, DHCP_OPTIONS_COOKIE, 4) == 0) {
		/* Parse the BOOTP/DHCP options field. */
		options_valid = parse_option_buffer(options,
		    &packet->options[4], sizeof(packet->options) - 4);

		/* Only DHCP packets have overload areas for options. */
		if (options_valid &&
		    options[DHO_DHCP_MESSAGE_TYPE].data &&
		    options[DHO_DHCP_OPTION_OVERLOAD].data) {
			if (options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 1)
				options_valid = parse_option_buffer(options,
				    (unsigned char *)packet->file,
				    sizeof(packet->file));
			if (options_valid &&
			    options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 2)
				options_valid = parse_option_buffer(options,
				    (unsigned char *)packet->sname,
				    sizeof(packet->sname));
		}
	}

	type = "<unknown>";
	handler = NULL;

	if (options[DHO_DHCP_MESSAGE_TYPE].data) {
		/* Always try a DHCP packet, even if a bad option was seen. */
		switch (options[DHO_DHCP_MESSAGE_TYPE].data[0]) {
		case DHCPOFFER:
			handler = dhcpoffer;
			type = "DHCPOFFER";
			break;
		case DHCPNAK:
			handler = dhcpnak;
			type = "DHCPNACK";
			break;
		case DHCPACK:
			handler = dhcpack;
			type = "DHCPACK";
			break;
		default:
#ifdef DEBUG
			debug("Discarding DHCP packet of unknown type (%d)",
				options[DHO_DHCP_MESSAGE_TYPE].data[0]);
#endif
			break;
		}
	} else if (options_valid && packet->op == BOOTREPLY) {
		handler = dhcpoffer;
		type = "BOOTREPLY";
	} else {
#ifdef DEBUG
		debug("Discarding packet which is neither DHCP nor BOOTP");
#endif
	}

	rslt = asprintf(&info, "%s from %s (%s)", type, inet_ntoa(from),
	    ether_ntoa(hfrom));
	if (rslt == -1)
		error("no memory for info string");

	if (handler)
		(*handler)(from, options, info);

	free(info);

	for (i = 0; i < 256; i++)
		if (options[i].len && options[i].data)
			free(options[i].data);
}
