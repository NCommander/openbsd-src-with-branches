/*	$OpenBSD: tcpdump.c,v 1.27 2002/01/23 23:32:20 mickey Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char copyright[] =
    "@(#) Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997\n\
The Regents of the University of California.  All rights reserved.\n";
static const char rcsid[] =
    "@(#) $Header: /cvs/src/usr.sbin/tcpdump/tcpdump.c,v 1.27 2002/01/23 23:32:20 mickey Exp $ (LBL)";
#endif

/*
 * tcpdump - monitor tcp/ip traffic on an ethernet.
 *
 * First written in 1987 by Van Jacobson, Lawrence Berkeley Laboratory.
 * Mercilessly hacked and occasionally improved since then via the
 * combined efforts of Van, Steve McCanne and Craig Leres of LBL.
 */

#include <sys/types.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <pcap.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "interface.h"
#include "addrtoname.h"
#include "machdep.h"
#include "setsignal.h"
#include "gmt2local.h"

int aflag;			/* translate network and broadcast addresses */
int dflag;			/* print filter code */
int eflag;			/* print ethernet header */
int fflag;			/* don't translate "foreign" IP address */
int nflag;			/* leave addresses as numbers */
int Nflag;			/* remove domains from printed host names */
int Oflag = 1;			/* run filter code optimizer */
int pflag;			/* don't go promiscuous */
int qflag;			/* quick (shorter) output */
int Sflag;			/* print raw TCP sequence numbers */
int tflag = 1;			/* print packet arrival time */
int vflag;			/* verbose */
int xflag;			/* print packet in hex */
int Xflag;			/* print packet in emacs-hexl style */

int packettype;


char *program_name;

int32_t thiszone;		/* seconds offset from gmt to local time */

/* Externs */
extern void bpf_dump(struct bpf_program *, int);

/* Forwards */
RETSIGTYPE cleanup(int);
extern __dead void usage(void) __attribute__((volatile));

/* Length of saved portion of packet. */
int snaplen = DEFAULT_SNAPLEN;

struct printer {
	pcap_handler f;
	int type;
};

/* XXX needed if using old bpf.h */
#ifndef DLT_ATM_RFC1483
#define DLT_ATM_RFC1483 11
#endif

static struct printer printers[] = {
	{ ether_if_print,	DLT_EN10MB },
	{ ether_if_print,	DLT_IEEE802 },
	{ sl_if_print,		DLT_SLIP },
	{ sl_bsdos_if_print,	DLT_SLIP_BSDOS },
	{ ppp_if_print,		DLT_PPP },
	{ fddi_if_print,	DLT_FDDI },
	{ null_if_print,	DLT_NULL },
	{ raw_if_print,		DLT_RAW },
	{ atm_if_print,		DLT_ATM_RFC1483 },
	{ loop_if_print, 	DLT_LOOP },
	{ enc_if_print, 	DLT_ENC },
	{ pflog_if_print, 	DLT_PFLOG },
	{ NULL,			0 },
};

static pcap_handler
lookup_printer(int type)
{
	struct printer *p;

	for (p = printers; p->f; ++p)
		if (type == p->type)
			return p->f;

	error("unknown data link type 0x%x", type);
	/* NOTREACHED */
}

static pcap_t *pd;

extern int optind;
extern int opterr;
extern char *optarg;

int
main(int argc, char **argv)
{
	register int cnt, op, i;
	bpf_u_int32 localnet, netmask;
	register char *cp, *infile, *cmdbuf, *device, *RFileName, *WFileName;
	pcap_handler printer;
	struct bpf_program fcode;
	RETSIGTYPE (*oldhandler)(int);
	u_char *pcap_userdata;
	char ebuf[PCAP_ERRBUF_SIZE];

	cnt = -1;
	device = NULL;
	infile = NULL;
	RFileName = NULL;
	WFileName = NULL;
	if ((cp = strrchr(argv[0], '/')) != NULL)
		program_name = cp + 1;
	else
		program_name = argv[0];

	if (abort_on_misalignment(ebuf, sizeof(ebuf)) < 0)
		error("%s", ebuf);

	opterr = 0;
	while ((op = getopt(argc, argv, "ac:defF:i:lnNOpqr:s:StT:vw:xXY")) != -1)
		switch (op) {

		case 'a':
			++aflag;
			break;

		case 'c':
			cnt = atoi(optarg);
			if (cnt <= 0)
				error("invalid packet count %s", optarg);
			break;

		case 'd':
			++dflag;
			break;

		case 'e':
			++eflag;
			break;

		case 'f':
			++fflag;
			break;

		case 'F':
			infile = optarg;
			break;

		case 'i':
			device = optarg;
			break;

		case 'l':
#ifdef HAVE_SETLINEBUF
			setlinebuf(stdout);
#else
			setvbuf(stdout, NULL, _IOLBF, 0);
#endif
			break;

		case 'n':
			++nflag;
			break;

		case 'N':
			++Nflag;
			break;

		case 'O':
			Oflag = 0;
			break;

		case 'p':
			++pflag;
			break;

		case 'q':
			++qflag;
			break;

		case 'r':
			RFileName = optarg;
			break;

		case 's':
			snaplen = atoi(optarg);
			if (snaplen <= 0)
				error("invalid snaplen %s", optarg);
			break;

		case 'S':
			++Sflag;
			break;

		case 't':
			--tflag;
			break;

		case 'T':
			if (strcasecmp(optarg, "vat") == 0)
				packettype = PT_VAT;
			else if (strcasecmp(optarg, "wb") == 0)
				packettype = PT_WB;
			else if (strcasecmp(optarg, "rpc") == 0)
				packettype = PT_RPC;
			else if (strcasecmp(optarg, "rtp") == 0)
				packettype = PT_RTP;
			else if (strcasecmp(optarg, "rtcp") == 0)
				packettype = PT_RTCP;
			else if (strcasecmp(optarg, "cnfp") == 0)
				packettype = PT_CNFP;
			else if (strcasecmp(optarg, "sack") == 0)
				snaplen = SACK_SNAPLEN;
			else
				error("unknown packet type `%s'", optarg);
			break;

		case 'v':
			++vflag;
			break;

		case 'w':
			WFileName = optarg;
			break;
#ifdef YYDEBUG
		case 'Y':
			{
			/* Undocumented flag */
			extern int yydebug;
			yydebug = 1;
			}
			break;
#endif
		case 'x':
			++xflag;
			break;

		case 'X':
			++Xflag;
			if (xflag == 0) ++xflag;
			break;

		default:
			usage();
			/* NOTREACHED */
		}

	if (aflag && nflag)
		error("-a and -n options are incompatible");

	if (tflag > 0)
		thiszone = gmt2local(0);

	if (RFileName != NULL) {
		/*
		 * We don't need network access, so set it back to the user id.
		 * Also, this prevents the user from reading anyone's
		 * trace file.
		 */
		seteuid(getuid());
		setuid(getuid());

		pd = pcap_open_offline(RFileName, ebuf);
		if (pd == NULL)
			error("%s", ebuf);
		localnet = 0;
		netmask = 0;
		if (fflag != 0)
			error("-f and -r options are incompatible");
	} else {
		if (device == NULL) {
			device = pcap_lookupdev(ebuf);
			if (device == NULL)
				error("%s", ebuf);
		}
		pd = pcap_open_live(device, snaplen, !pflag, 1000, ebuf);
		if (pd == NULL)
			error("%s", ebuf);
		i = pcap_snapshot(pd);
		if (snaplen < i) {
			warning("snaplen raised from %d to %d", snaplen, i);
			snaplen = i;
		}
		if (pcap_lookupnet(device, &localnet, &netmask, ebuf) < 0) {
			warning("%s", ebuf);
			localnet = 0;
			netmask = 0;
		}

		/*
		 * Let user own process after socket has been opened.
		 */
		seteuid(getuid());
		setuid(getuid());
	}
	if (infile)
		cmdbuf = read_infile(infile);
	else
		cmdbuf = copy_argv(&argv[optind]);

	if (pcap_compile(pd, &fcode, cmdbuf, Oflag, netmask) < 0)
		error("%s", pcap_geterr(pd));
	if (dflag) {
		bpf_dump(&fcode, dflag);
		exit(0);
	}
	init_addrtoname(localnet, netmask);

	(void)setsignal(SIGTERM, cleanup);
	(void)setsignal(SIGINT, cleanup);
	/* Cooperate with nohup(1) */
	if ((oldhandler = setsignal(SIGHUP, cleanup)) != SIG_DFL)
		(void)setsignal(SIGHUP, oldhandler);

	if (pcap_setfilter(pd, &fcode) < 0)
		error("%s", pcap_geterr(pd));
	if (WFileName) {
		pcap_dumper_t *p;

		p = pcap_dump_open(pd, WFileName);
		if (p == NULL)
			error("%s", pcap_geterr(pd));
		{
			FILE *fp = (FILE *)p;	/* XXX touching pcap guts! */
			fflush(fp);
			setvbuf(fp, NULL, _IONBF, 0);
		}
		printer = pcap_dump;
		pcap_userdata = (u_char *)p;
	} else {
		printer = lookup_printer(pcap_datalink(pd));
		pcap_userdata = 0;
	}
	if (RFileName == NULL) {
		(void)fprintf(stderr, "%s: listening on %s\n",
		    program_name, device);
		(void)fflush(stderr);
	}
	if (pcap_loop(pd, cnt, printer, pcap_userdata) < 0) {
		(void)fprintf(stderr, "%s: pcap_loop: %s\n",
		    program_name, pcap_geterr(pd));
		exit(1);
	}
	pcap_close(pd);
	exit(0);
}

/* make a clean exit on interrupts */
RETSIGTYPE
cleanup(int signo)
{
	struct pcap_stat stat;
	char buf[1024];

	/* Can't print the summary if reading from a savefile */
	if (pd != NULL && pcap_file(pd) == NULL) {
#if 0
		(void)fflush(stdout);	/* XXX unsafe */
#endif
		(void)write(STDERR_FILENO, "\n", 1);
		if (pcap_stats(pd, &stat) < 0) {
			(void)snprintf(buf, sizeof buf,
			    "pcap_stats: %s\n", pcap_geterr(pd));
			write(STDOUT_FILENO, buf, strlen(buf));
		} else {
			(void)snprintf(buf, sizeof buf,
			    "%d packets received by filter\n", stat.ps_recv);
			write(STDOUT_FILENO, buf, strlen(buf));
			(void)snprintf(buf, sizeof buf,
			    "%d packets dropped by kernel\n", stat.ps_drop);
			write(STDOUT_FILENO, buf, strlen(buf));
		}
	}
	_exit(0);
}

/* dump the buffer in `emacs-hexl' style */
void
default_print_hexl(const u_char *cp, unsigned int length, unsigned int offset)
{
	unsigned int i, j, jm;
	int c;
	char ln[128], buf[128];

	printf("\n");
	for (i = 0; i < length; i += 0x10) {
		snprintf(ln, sizeof(ln), "  %04x: ",
		    (unsigned int)(i + offset));
		jm = length - i;
		jm = jm > 16 ? 16 : jm;

		for (j = 0; j < jm; j++) {
			if ((j % 2) == 1)
				snprintf(buf, sizeof(buf), "%02x ",
				    (unsigned int)cp[i+j]);
			else
				snprintf(buf, sizeof(buf), "%02x",
				    (unsigned int)cp[i+j]);
			strlcat(ln, buf, sizeof ln);
		}
		for (; j < 16; j++) {
			if ((j % 2) == 1)
				snprintf(buf, sizeof buf, "   ");
			else
				snprintf(buf, sizeof buf, "  ");
			strlcat(ln, buf, sizeof ln);
		}

		strlcat(ln, " ", sizeof ln);
		for (j = 0; j < jm; j++) {
			c = cp[i+j];
			c = isprint(c) ? c : '.';
			buf[0] = c;
			buf[1] = '\0';
			strlcat(ln, buf, sizeof ln);
		}
		printf("%s\n", ln);
	}
}

/* Like default_print() but data need not be aligned */
void
default_print_unaligned(register const u_char *cp, register u_int length)
{
	register u_int i, s;
	register int nshorts;

	if (Xflag) {
		/* dump the buffer in `emacs-hexl' style */
		default_print_hexl(cp, length, 0);
	} else {
		/* dump the buffer in old tcpdump style */
		nshorts = (u_int) length / sizeof(u_short);
		i = 0;
		while (--nshorts >= 0) {
			if ((i++ % 8) == 0)
				(void)printf("\n\t\t\t");
			s = *cp++;
			(void)printf(" %02x%02x", s, *cp++);
		}
		if (length & 1) {
			if ((i % 8) == 0)
				(void)printf("\n\t\t\t");
			(void)printf(" %02x", *cp);
		}
	}
}

void
default_print(register const u_char *bp, register u_int length)
{
	register const u_short *sp;
	register u_int i;
	register int nshorts;

	if (Xflag) {
		/* dump the buffer in `emacs-hexl' style */
		default_print_hexl(bp, length, 0);
	} else {
		/* dump the buffer in old tcpdump style */
		if ((long)bp & 1) {
			default_print_unaligned(bp, length);
			return;
		}
		sp = (u_short *)bp;
		nshorts = (u_int) length / sizeof(u_short);
		i = 0;
		while (--nshorts >= 0) {
			if ((i++ % 8) == 0)
				(void)printf("\n\t\t\t");
			(void)printf(" %04x", ntohs(*sp++));
		}
		if (length & 1) {
			if ((i % 8) == 0)
				(void)printf("\n\t\t\t");
			(void)printf(" %02x", *(u_char *)sp);
		}
	}
}

__dead void
usage(void)
{
	extern char version[];
	extern char pcap_version[];

	(void)fprintf(stderr, "%s version %s\n", program_name, version);
	(void)fprintf(stderr, "libpcap version %s\n", pcap_version);
	(void)fprintf(stderr,
"Usage: %s [-adeflnNOpqStvxX] [-c count] [ -F file ]\n", program_name);
	(void)fprintf(stderr,
"\t\t[ -i interface ] [ -r file ] [ -s snaplen ]\n");
	(void)fprintf(stderr,
"\t\t[ -T type ] [ -w file ] [ expression ]\n");
	exit(1);
}
