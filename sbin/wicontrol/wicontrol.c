/*	$OpenBSD: wicontrol.c,v 1.18 2001/06/25 18:07:35 drahn Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$FreeBSD: wicontrol.c,v 1.6 1999/05/22 16:12:49 wpaul Exp $
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#include <net/ethernet.h>

#include <machine/if_wavelan_ieee.h>
#else
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wireg.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#if !defined(lint)
static const char copyright[] = "@(#) Copyright (c) 1997, 1998, 1999\
	Bill Paul. All rights reserved.";
static const char rcsid[] =
	"@(#) $OpenBSD: wicontrol.c,v 1.18 2001/06/25 18:07:35 drahn Exp $";
#endif

static void wi_getval		__P((char *, struct wi_req *));
static void wi_setval		__P((char *, struct wi_req *));
static void wi_printstr		__P((struct wi_req *));
static void wi_setstr		__P((char *, int, char *));
static void wi_setbytes		__P((char *, int, char *, int));
static void wi_setword		__P((char *, int, char *));
static void wi_sethex		__P((char *, int, char *));
static void wi_printwords	__P((struct wi_req *));
static void wi_printbool	__P((struct wi_req *));
static void wi_printhex		__P((struct wi_req *));
static void wi_dumpinfo		__P((char *));
static void wi_setkeys		__P((char *, int, char *));
static void wi_printkeys	__P((struct wi_req *));
static void wi_printcardid	__P((struct wi_req *, int));
static void wi_dumpstats	__P((char *));
static void usage		__P((void));

static void
wi_getval(iface, wreq)
	char			*iface;
	struct wi_req		*wreq;
{
	struct ifreq		ifr;
	int			s;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s == -1)
		err(1, "socket");

	if (ioctl(s, SIOCGWAVELAN, &ifr) == -1)
		err(1, "SIOCGWAVELAN");

	close(s);
}

static void
wi_setval(iface, wreq)
	char			*iface;
	struct wi_req		*wreq;
{
	struct ifreq		ifr;
	int			s;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s == -1)
		err(1, "socket");

	if (ioctl(s, SIOCSWAVELAN, &ifr) == -1)
		err(1, "SIOCSWAVELAN");

	close(s);
}

void
wi_printstr(wreq)
	struct wi_req		*wreq;
{
	char			*ptr;
	int			i, max;

	if (wreq->wi_type == WI_RID_SERIALNO) {
		ptr = (char *)&wreq->wi_val;
		max = MIN(sizeof(wreq->wi_val) - 1, (wreq->wi_len - 1) * 2);
		for (i = 0; i < max; i++) {
			if (ptr[i] == '\0')
				ptr[i] = ' ';
		}
	} else {
		int len = letoh16(wreq->wi_val[0]);

		ptr = (char *)&wreq->wi_val[1];
		max = MIN(sizeof(wreq->wi_val) - 1, len);
		for (i = 0; i < max; i++) {
			if (ptr[i] == '\0')
				ptr[i] = ' ';
		}
	}

	ptr[i] = '\0';
	printf("[ %s ]", ptr);
}

void
wi_setstr(iface, code, str)
	char			*iface;
	int			code;
	char			*str;
{
	struct wi_req		wreq;

	if (str == NULL)
		errx(1, "must specify string");

	bzero((char *)&wreq, sizeof(wreq));

	if (strlen(str) > 30)
		errx(1, "string too long");

	wreq.wi_type = code;
	wreq.wi_len = 18;
	wreq.wi_val[0] = htole16(strlen(str));
	bcopy(str, (char *)&wreq.wi_val[1], strlen(str));

	wi_setval(iface, &wreq);
}

void
wi_setbytes(iface, code, bytes, len)
	char			*iface;
	int			code;
	char			*bytes;
	int			len;
{
	struct wi_req		wreq;

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_type = code;
	wreq.wi_len = (len / 2) + 1;
	bcopy(bytes, (char *)&wreq.wi_val[0], len);

	wi_setval(iface, &wreq);
}

void
wi_setword(iface, code, word)
	char			*iface;
	int			code;
	char                    *word;
{
	struct wi_req		wreq;
	int                     value = strtol(word, NULL, 10);

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_type = code;
	wreq.wi_len = 2;
	wreq.wi_val[0] = htole16(value);

	wi_setval(iface, &wreq);
}

void
wi_sethex(iface, code, str)
	char			*iface;
	int			code;
	char			*str;
{
	struct ether_addr	*addr;

	if (str == NULL)
		errx(1, "must specify address");

	addr = ether_aton(str);

	if (addr == NULL)
		errx(1, "badly formatted address");

	wi_setbytes(iface, code, (char *)addr, ETHER_ADDR_LEN);
}

static int
wi_hex2int(c)
        char                    c;
{
        if (c >= '0' && c <= '9')
                return (c - '0');
	if (c >= 'A' && c <= 'F')
	        return (c - 'A' + 10);
	if (c >= 'a' && c <= 'f')
                return (c - 'a' + 10);

	return (0); 
}

static void
wi_str2key(s, k)
        char                    *s;
        struct wi_key           *k;
{
        int                     n, i;
        char                    *p;

        /* Is this a hex string? */
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                /* Yes, convert to int. */
                n = 0;
                p = (char *)&k->wi_keydat[0];
                for (i = 2; i < strlen(s); i+= 2) {
                        *p++ = (wi_hex2int(s[i]) << 4) + wi_hex2int(s[i + 1]);
                        n++;
                }
                k->wi_keylen = htole16(n);
        } else {
                /* No, just copy it in. */
                bcopy(s, k->wi_keydat, strlen(s));
                k->wi_keylen = htole16(strlen(s));
        }
}

static void
wi_setkeys(iface, idx, key)
        char                    *iface;
        int                     idx;
        char                    *key;
{
        struct wi_req           wreq;
        struct wi_ltv_keys      *keys;
        struct wi_key           *k;

        bzero((char *)&wreq, sizeof(wreq));
        wreq.wi_len = WI_MAX_DATALEN;
        wreq.wi_type = WI_RID_WEP_AVAIL;

        wi_getval(iface, &wreq);
        if (letoh16(wreq.wi_val[0]) == 0)
                err(1, "no WEP option available on this card");

        bzero((char *)&wreq, sizeof(wreq));
        wreq.wi_len = WI_MAX_DATALEN;
        wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;

        wi_getval(iface, &wreq);
        keys = (struct wi_ltv_keys *)&wreq;

        if (key[0] == '0' && (key[1] == 'x' || key[1] == 'X')) {
	        if (strlen(key) > 30)
		        err(1, "encryption key must be no "
			    "more than 28 hex digits long");
	} else {
	        if (strlen(key) > 14)
		        err(1, "encryption key must be no "
			    "more than 14 characters long");
	}

        if (idx > 3)
                err(1, "only 4 encryption keys available");

        k = &keys->wi_keys[idx];
        wi_str2key(key, k);

        wreq.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
        wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
        wi_setval(iface, &wreq);
}

static void
wi_printkeys(wreq)
        struct wi_req           *wreq;
{
        int                     i, j, bn;
        struct wi_key           *k;
        struct wi_ltv_keys      *keys;
        char                    *ptr;

	keys = (struct wi_ltv_keys *)wreq;

	for (i = 0, bn = 0; i < 4; i++, bn = 0) {
                k = &keys->wi_keys[i];
                ptr = (char *)k->wi_keydat;
		for (j = 0; j < letoh16(k->wi_keylen); j++) {
		        if (!isprint((unsigned char)ptr[j])) {
			        bn = 1;
				break;
			}
		}

		if (bn)	{
		        printf("[ 0x");
		        for (j = 0; j < letoh16(k->wi_keylen); j++)
			      printf("%02x", ((unsigned char *) ptr)[j]);
			printf(" ]");
		} else {
		        ptr[j] = '\0';
			printf("[ %s ]", ptr);
		}
        }
}

void
wi_printcardid(wreq, chip_id)
	struct wi_req *wreq;
	int chip_id;
{
	char *chip_name;

	if (wreq->wi_len < 4)
		return;

	/* Copied from wi_get_id() in if_wi.c */
	switch (chip_id) {
	case WI_NIC_EVB2:
		chip_name = "PRISM I HFA3841(EVB2)";
		break;
	case WI_NIC_HWB3763:
		chip_name = "PRISM II HWB3763 rev.B";
		break;
	case WI_NIC_HWB3163:
		chip_name = "PRISM II HWB3163 rev.A";
		break;
	case WI_NIC_HWB3163B:
		chip_name = "PRISM II HWB3163 rev.B";
		break;
	case WI_NIC_EVB3:
		chip_name = "PRISM II  HFA3842(EVB3)";
		break;
	case WI_NIC_HWB1153:
		chip_name = "PRISM I HFA1153";
		break;
	case WI_NIC_P2_SST:
		chip_name = "PRISM II HWB3163 SST-flash";
		break;
	case WI_NIC_PRISM2_5:
		chip_name = "PRISM 2.5 ISL3873";
		break;
	case 1:
		chip_name = "Lucent";
		break;
	case 2:
		chip_name = "Sony";
		break;
	case 5:
		chip_name = "Airport";
		break;
	default:
		asprintf(&chip_name, "Unknown (%d)", chip_id);
		break;
	}
	printf("[ %s, Firmware %i.%i variant %i ]", chip_name,
	    letoh16(wreq->wi_val[2]), letoh16(wreq->wi_val[3]),
	    letoh16(wreq->wi_val[1]));
}

void
wi_printwords(wreq)
	struct wi_req		*wreq;
{
	int			i;

	printf("[ ");
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%d ", letoh16(wreq->wi_val[i]));
	printf("]");
}

void
wi_printbool(wreq)
	struct wi_req		*wreq;
{
	if (letoh16(wreq->wi_val[0]))
		printf("[ On ]");
	else
		printf("[ Off ]");
}

void
wi_printhex(wreq)
	struct wi_req		*wreq;
{
	int			i;
	unsigned char		*c;

	c = (unsigned char *)&wreq->wi_val;

	printf("[ ");
	for (i = 0; i < (wreq->wi_len - 1) * 2; i++) {
		printf("%02x", c[i]);
		if (i < ((wreq->wi_len - 1) * 2) - 1)
			printf(":");
	}

	printf(" ]");
}

#define WI_STRING		0x01
#define WI_BOOL			0x02
#define WI_WORDS		0x03
#define WI_HEXBYTES		0x04
#define WI_KEYSTRUCT            0x05
#define WI_CARDINFO             0x06

struct wi_table {
	int			wi_code;
	int			wi_type;
	char			*wi_str;
};

static struct wi_table wi_table[] = {
	{ WI_RID_SERIALNO, WI_STRING, "NIC serial number:\t\t\t" },
	{ WI_RID_NODENAME, WI_STRING, "Station name:\t\t\t\t" },
	{ WI_RID_OWN_SSID, WI_STRING, "SSID for IBSS creation:\t\t\t" },
	{ WI_RID_CURRENT_SSID, WI_STRING, "Current netname (SSID):\t\t\t" },
	{ WI_RID_DESIRED_SSID, WI_STRING, "Desired netname (SSID):\t\t\t" },
	{ WI_RID_CURRENT_BSSID, WI_HEXBYTES, "Current BSSID:\t\t\t\t" },
	{ WI_RID_CHANNEL_LIST, WI_WORDS, "Channel list:\t\t\t\t" },
	{ WI_RID_OWN_CHNL, WI_WORDS, "IBSS channel:\t\t\t\t" },
	{ WI_RID_CURRENT_CHAN, WI_WORDS, "Current channel:\t\t\t" },
	{ WI_RID_COMMS_QUALITY, WI_WORDS, "Comms quality/signal/noise:\t\t" },
	{ WI_RID_PROMISC, WI_BOOL, "Promiscuous mode:\t\t\t" },
	{ WI_RID_PORTTYPE, WI_WORDS, "Port type (1=BSS, 3=ad-hoc):\t\t"},
	{ WI_RID_MAC_NODE, WI_HEXBYTES, "MAC address:\t\t\t\t"},
	{ WI_RID_TX_RATE, WI_WORDS, "TX rate (selection):\t\t\t"},
	{ WI_RID_CUR_TX_RATE, WI_WORDS, "TX rate (actual speed):\t\t\t"},
	{ WI_RID_MAX_DATALEN, WI_WORDS, "Maximum data length:\t\t\t"},
	{ WI_RID_RTS_THRESH, WI_WORDS, "RTS/CTS handshake threshold:\t\t"},
	{ WI_RID_CREATE_IBSS, WI_BOOL, "Create IBSS:\t\t\t\t" },
	{ WI_RID_MICROWAVE_OVEN, WI_WORDS, "Microwave oven robustness:\t\t"},
	{ WI_RID_ROAMING_MODE, WI_WORDS, "Roaming mode(1:firm,3:disable):\t\t"},
	{ WI_RID_SYSTEM_SCALE, WI_WORDS, "Access point density:\t\t\t" },
	{ WI_RID_PM_ENABLED, WI_WORDS, "Power Mgmt (1=on, 0=off):\t\t" },
	{ WI_RID_MAX_SLEEP, WI_WORDS, "Max sleep time:\t\t\t\t" },
	{ WI_RID_STA_IDENTITY, WI_CARDINFO, "Card info:\t\t\t\t" },
	{ 0, NULL }
};

static struct wi_table wi_crypt_table[] = {
        { WI_RID_ENCRYPTION, WI_BOOL, "WEP encryption:\t\t\t\t" },
        { WI_RID_AUTH_CNTL, WI_WORDS,
	  "Authentication type \n(1=OpenSys, 2=Shared Key):\t\t" },
        { WI_RID_TX_CRYPT_KEY, WI_WORDS, "TX encryption key:\t\t\t" },
        { WI_RID_DEFLT_CRYPT_KEYS, WI_KEYSTRUCT, "Encryption keys:\t\t\t" },
        { 0, NULL }
};

static void
wi_dumpinfo(iface)
	char			*iface;
{
	struct wi_req		wreq;
	int			i, has_wep, chip_id;
	struct wi_table		*w;

	/* Get chip ID. */
	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_type = WI_RID_CARD_ID;
	wreq.wi_len = 5;
	wi_getval(iface, &wreq);
	chip_id = letoh16(wreq.wi_val[0]);

	/* Check for WEP support. */
	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_type = WI_RID_WEP_AVAIL;
	wreq.wi_len = 2;
	wi_getval(iface, &wreq);
	has_wep = letoh16(wreq.wi_val[0]);

	w = wi_table;

	for (i = 0; w[i].wi_type; i++) {
		bzero((char *)&wreq, sizeof(wreq));

		wreq.wi_len = WI_MAX_DATALEN;
		wreq.wi_type = w[i].wi_code;

		wi_getval(iface, &wreq);
		printf("%s", w[i].wi_str);
		switch(w[i].wi_type) {
		case WI_STRING:
			wi_printstr(&wreq);
			break;
		case WI_WORDS:
			wi_printwords(&wreq);
			break;
		case WI_BOOL:
			wi_printbool(&wreq);
			break;
		case WI_HEXBYTES:
			wi_printhex(&wreq);
			break;
		case WI_CARDINFO:
			wi_printcardid(&wreq, chip_id);
			break;
		default:
			break;
		}	
		printf("\n");
	}

	if (has_wep) {
		w = wi_crypt_table;
		for (i = 0; w[i].wi_type; i++) {
			bzero((char *)&wreq, sizeof(wreq));

			wreq.wi_len = WI_MAX_DATALEN;
			wreq.wi_type = w[i].wi_code;

			wi_getval(iface, &wreq);
			printf("%s", w[i].wi_str);
			switch(w[i].wi_type) {
			case WI_STRING:
				wi_printstr(&wreq);
				break;
			case WI_WORDS:
				if (wreq.wi_type == WI_RID_TX_CRYPT_KEY)
					wreq.wi_val[0] =
					   htole16(letoh16(wreq.wi_val[0]) + 1);
				wi_printwords(&wreq);
				break;
			case WI_BOOL:
				wi_printbool(&wreq);
				break;
			case WI_HEXBYTES:
				wi_printhex(&wreq);
				break;
			case WI_KEYSTRUCT:
				wi_printkeys(&wreq);
				break;
			default:
				break;
			}	
			printf("\n");
		}
	}
}

static void
wi_dumpstats(iface)
	char			*iface;
{
	struct wi_req		wreq;
	struct wi_counters	*c;

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_IFACE_STATS;

	wi_getval(iface, &wreq);

	c = (struct wi_counters *)&wreq.wi_val;

	/* XXX native byte order */
	printf("Transmitted unicast frames:\t\t%d\n",
	    c->wi_tx_unicast_frames);
	printf("Transmitted multicast frames:\t\t%d\n",
	    c->wi_tx_multicast_frames);
	printf("Transmitted fragments:\t\t\t%d\n",
	    c->wi_tx_fragments);
	printf("Transmitted unicast octets:\t\t%d\n",
	    c->wi_tx_unicast_octets);
	printf("Transmitted multicast octets:\t\t%d\n",
	    c->wi_tx_multicast_octets);
	printf("Single transmit retries:\t\t%d\n",
	    c->wi_tx_single_retries);
	printf("Multiple transmit retries:\t\t%d\n",
	    c->wi_tx_multi_retries);
	printf("Transmit retry limit exceeded:\t\t%d\n",
	    c->wi_tx_retry_limit);
	printf("Transmit discards:\t\t\t%d\n",
	    c->wi_tx_discards);
	printf("Transmit discards due to wrong SA:\t%d\n",
	    c->wi_tx_discards_wrong_sa);
	printf("Received unicast frames:\t\t%d\n",
	    c->wi_rx_unicast_frames);
	printf("Received multicast frames:\t\t%d\n",
	    c->wi_rx_multicast_frames);
	printf("Received fragments:\t\t\t%d\n",
	    c->wi_rx_fragments);
	printf("Received unicast octets:\t\t%d\n",
	    c->wi_rx_unicast_octets);
	printf("Received multicast octets:\t\t%d\n",
	    c->wi_rx_multicast_octets);
	printf("Receive FCS errors:\t\t\t%d\n",
	    c->wi_rx_fcs_errors);
	printf("Receive discards due to no buffer:\t%d\n",
	    c->wi_rx_discards_nobuf);
	printf("Can't decrypt WEP frame:\t\t%d\n",
	    c->wi_rx_WEP_cant_decrypt);
	printf("Received message fragments:\t\t%d\n",
	    c->wi_rx_msg_in_msg_frags);
	printf("Received message bad fragments:\t\t%d\n",
	    c->wi_rx_msg_in_bad_msg_frags);
}

static void
usage()
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s interface "
	    "[-o] [-t tx rate] [-n network name] [-s station name]\n"
	    "       [-e 0|1] [-k key [-v 1|2|3|4]] [-T 1|2|3|4]\n"
	    "       [-c 0|1] [-q SSID] [-p port type] [-a access point density]\n"
	    "       [-m MAC address] [-d max data length] [-r RTS threshold]\n"
	    "       [-f frequency] [-M 0|1] [-P 0|1] [-S max sleep duration]\n"
	    "       [-A 0|1 ] [-R 1|3]\n", __progname);
	exit(1);
}

struct wi_func {
        int   key;
        void (*function) (char *, int, char *);
        int   wi_code;
        char  *optarg;
};

static struct wi_func wi_opt[] = {
	{ 'k', wi_setkeys, 0, NULL },  /* MUST be first entry in table */
	{ 'a', wi_setword, WI_RID_SYSTEM_SCALE, NULL },
	{ 'c', wi_setword, WI_RID_CREATE_IBSS, NULL },
	{ 'd', wi_setword, WI_RID_MAX_DATALEN, NULL },
	{ 'e', wi_setword, WI_RID_ENCRYPTION, NULL },
	{ 'f', wi_setword, WI_RID_OWN_CHNL, NULL },
	{ 'm', wi_sethex, WI_RID_MAC_NODE, NULL },
	{ 'n', wi_setstr, WI_RID_DESIRED_SSID, NULL },
	{ 'p', wi_setword, WI_RID_PORTTYPE, NULL },
	{ 'q', wi_setstr, WI_RID_OWN_SSID, NULL },
	{ 'r', wi_setword, WI_RID_RTS_THRESH, NULL },
	{ 's', wi_setstr, WI_RID_NODENAME, NULL },
	{ 't', wi_setword, WI_RID_TX_RATE, NULL },
	{ 'A', wi_setword, WI_RID_AUTH_CNTL, NULL },
	{ 'M', wi_setword, WI_RID_MICROWAVE_OVEN, NULL },
	{ 'P', wi_setword, WI_RID_PM_ENABLED, NULL },
	{ 'R', wi_setword, WI_RID_ROAMING_MODE, NULL },
	{ 'S', wi_setword, WI_RID_MAX_SLEEP, NULL },
	{ 'T', wi_setword, WI_RID_TX_CRYPT_KEY, NULL },

	/* These options will never be command line options which is why
	   they are not 'quoted' */
	{ 1, wi_setkeys, 0, NULL }, /* Dummy option for key 0 */
	{ 2, wi_setkeys, 1, NULL }, /* key 1 */
	{ 3, wi_setkeys, 2, NULL }, /* key 2 */
	{ 4, wi_setkeys, 3, NULL }, /* key 3 */
	{ 0, NULL, 0, NULL }
};

int
main(argc, argv)
	int			argc;
	char			*argv[];
{
	char			*iface = "wi0";
	int                     ch, p, dumpstats = 0, dumpinfo = 1, ifspecified = 0;

	if (argc > 1 && argv[1][0] != '-') {
		iface = argv[1];
		memcpy(&argv[1], &argv[2], argc * sizeof(char *));
		argc--;
		ifspecified = 1;
	}

	while((ch = getopt(argc, argv,
	    "a:c:d:e:f:hi:k:m:n:op:q:r:s:t:v:A:M:S:P:R:T:")) != -1) {
	        for (p = 0; ch && wi_opt[p].key; p++)
		        if (ch == wi_opt[p].key) {
			        wi_opt[p].optarg = optarg;
				if (ch == 'T')	/* key 1-4/0-3 kludge */
				        (*optarg)--;
				dumpinfo = ch = 0;
			}
		switch(ch) {
		case 0:
		        break;
		case 'o':
			dumpstats ++;
			break;
		case 'i':
		        if (!ifspecified)
			        iface = optarg;
			break;
		case 'v':
 		        for (p = 0; wi_opt[p].key; p++)
				if (wi_opt[p].key == 
                                    strtol(optarg, NULL, 10)) {
					wi_opt[p].optarg = wi_opt[0].optarg;
                                        /* prevent multiple -v without
                                           multiple -k */
                                        wi_opt[0].optarg = NULL; 
					break;
				}
		       	break;
		case 'h':
		default:
		        usage();
			break;
		}
	}

	if (iface == NULL)
		usage();

	for (p = 0; wi_opt[p].key; p++)
	        if (wi_opt[p].optarg != NULL)
		        wi_opt[p].function(iface, wi_opt[p].wi_code, 
					   wi_opt[p].optarg);

	if (dumpstats)
	        wi_dumpstats(iface);

	if (dumpinfo && !dumpstats)
	        wi_dumpinfo(iface);

	exit(0);
}
