/*
 * Copyright (C) 2000-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: dig.c,v 1.157.2.7 2002/03/12 03:55:57 marka Exp $ */

#include <config.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include <isc/app.h>
#include <isc/netaddr.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>
#include <isc/task.h>

#include <dns/byaddr.h>
#include <dns/fixedname.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatatype.h>
#include <dns/rdataclass.h>
#include <dns/result.h>

#include <dig/dig.h>

extern ISC_LIST(dig_lookup_t) lookup_list;
extern dig_serverlist_t server_list;
extern ISC_LIST(dig_searchlist_t) search_list;

#define ADD_STRING(b, s) { 				\
	if (strlen(s) >= isc_buffer_availablelength(b)) \
 		return (ISC_R_NOSPACE); 		\
	else 						\
		isc_buffer_putstr(b, s); 		\
}


extern isc_boolean_t have_ipv4, have_ipv6, specified_source,
	usesearch, qr;
extern in_port_t port;
extern unsigned int timeout;
extern isc_mem_t *mctx;
extern dns_messageid_t id;
extern int sendcount;
extern int ndots;
extern int tries;
extern int lookup_counter;
extern int exitcode;
extern isc_sockaddr_t bind_address;
extern char keynametext[MXNAME];
extern char keyfile[MXNAME];
extern char keysecret[MXNAME];
extern dns_tsigkey_t *key;
extern isc_boolean_t validated;
extern isc_taskmgr_t *taskmgr;
extern isc_task_t *global_task;
extern isc_boolean_t free_now;
dig_lookup_t *default_lookup = NULL;

extern isc_boolean_t debugging, memdebugging;
static char *batchname = NULL;
static FILE *batchfp = NULL;
static char *argv0;

static char domainopt[DNS_NAME_MAXTEXT];

static isc_boolean_t short_form = ISC_FALSE, printcmd = ISC_TRUE,
	nibble = ISC_FALSE, plusquest = ISC_FALSE, pluscomm = ISC_FALSE,
	multiline = ISC_FALSE;

static const char *opcodetext[] = {
	"QUERY",
	"IQUERY",
	"STATUS",
	"RESERVED3",
	"NOTIFY",
	"UPDATE",
	"RESERVED6",
	"RESERVED7",
	"RESERVED8",
	"RESERVED9",
	"RESERVED10",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15"
};

static const char *rcodetext[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"YXDOMAIN",
	"YXRRSET",
	"NXRRSET",
	"NOTAUTH",
	"NOTZONE",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15",
	"BADVERS"
};

extern char *progname;

static void
print_usage(FILE *fp) {
	fputs(
"Usage:  dig [@global-server] [domain] [q-type] [q-class] {q-opt}\n"
"        {global-d-opt} host [@local-server] {local-d-opt}\n"
"        [ host [@local-server] {local-d-opt} [...]]\n", fp);
}

static void
usage(void) {
	print_usage(stderr);
	fputs("\nUse \"dig -h\" (or \"dig -h | more\") "
	      "for complete list of options\n", stderr);
	exit(1);
}

static void
help(void) {
	print_usage(stdout);
	fputs(
"Where:  domain	  are in the Domain Name System\n"
"        q-class  is one of (in,hs,ch,...) [default: in]\n"
"        q-type   is one of (a,any,mx,ns,soa,hinfo,axfr,txt,...) [default:a]\n"
"                 (Use ixfr=version for type ixfr)\n"
"        q-opt    is one of:\n"
"                 -x dot-notation     (shortcut for in-addr lookups)\n"
"                 -n                  (nibble form for reverse IPv6 lookups)\n"
"                 -f filename         (batch mode)\n"
"                 -b address          (bind to source address)\n"
"                 -p port             (specify port number)\n"
"                 -t type             (specify query type)\n"
"                 -c class            (specify query class)\n"
"                 -k keyfile          (specify tsig key file)\n"
"                 -y name:key         (specify named base64 tsig key)\n"
"        d-opt    is of the form +keyword[=value], where keyword is:\n"
"                 +[no]vc             (TCP mode)\n"
"                 +[no]tcp            (TCP mode, alternate syntax)\n"
"                 +time=###           (Set query timeout) [5]\n"
"                 +tries=###          (Set number of UDP attempts) [3]\n"
"                 +domain=###         (Set default domainname)\n"
"                 +bufsize=###        (Set EDNS0 Max UDP packet size)\n"
"                 +ndots=###          (Set NDOTS value)\n"
"                 +[no]search         (Set whether to use searchlist)\n"
"                 +[no]defname        (Ditto)\n"
"                 +[no]recursive      (Recursive mode)\n"
"                 +[no]ignore         (Don't revert to TCP for TC responses.)"
"\n"
"                 +[no]fail           (Don't try next server on SERVFAIL)\n"
"                 +[no]besteffort     (Try to parse even illegal messages)\n"
"                 +[no]aaonly         (Set AA flag in query)\n"
"                 +[no]adflag         (Set AD flag in query)\n"
"                 +[no]cdflag         (Set CD flag in query)\n"
"                 +[no]cmd            (Control display of command line)\n"
"                 +[no]comments       (Control display of comment lines)\n"
"                 +[no]question       (Control display of question)\n"
"                 +[no]answer         (Control display of answer)\n"
"                 +[no]authority      (Control display of authority)\n"
"                 +[no]additional     (Control display of additional)\n"
"                 +[no]stats          (Control display of statistics)\n"
"                 +[no]short          (Disable everything except short\n"
"                                      form of answer)\n"
"                 +[no]all            (Set or clear all display flags)\n"
"                 +[no]qr             (Print question before sending)\n"
"                 +[no]nssearch       (Search all authoritative nameservers)\n"
"                 +[no]identify       (ID responders in short answers)\n"
"                 +[no]trace          (Trace delegation down from root)\n"
"                 +[no]dnssec         (Request DNSSEC records)\n"
"                 +[no]multiline      (Print records in an expanded format)\n"
"        global d-opts and servers (before host name) affect all queries.\n"
"        local d-opts and servers (after host name) affect only that lookup.\n",
	stdout);
}

/*
 * Callback from dighost.c to print the received message.
 */
void
received(int bytes, isc_sockaddr_t *from, dig_query_t *query) {
	isc_uint64_t diff;
	isc_time_t now;
	isc_result_t result;
	time_t tnow;
	char fromtext[ISC_SOCKADDR_FORMATSIZE];

	isc_sockaddr_format(from, fromtext, sizeof(fromtext));

	result = isc_time_now(&now);
	check_result(result, "isc_time_now");

	if (query->lookup->stats && !short_form) {
		diff = isc_time_microdiff(&now, &query->time_sent);
		printf(";; Query time: %ld msec\n", (long int)diff/1000);
		printf(";; SERVER: %s(%s)\n", fromtext, query->servname);
		time(&tnow);
		printf(";; WHEN: %s", ctime(&tnow));
		if (query->lookup->doing_xfr) {
			printf(";; XFR size: %d records\n",
			       query->rr_count);
		} else {
			printf(";; MSG SIZE  rcvd: %d\n", bytes);

		}
		if (key != NULL) {
			if (!validated)
				puts(";; WARNING -- Some TSIG could not "
				     "be validated");
		}
		if ((key == NULL) && (keysecret[0] != 0)) {
			puts(";; WARNING -- TSIG key was not used.");
		}
		puts("");
	} else if (query->lookup->identify && !short_form) {
		diff = isc_time_microdiff(&now, &query->time_sent);
		printf(";; Received %u bytes from %s(%s) in %d ms\n\n",
		       bytes, fromtext, query->servname,
		       (int)diff/1000);
	}
}

/*
 * Callback from dighost.c to print that it is trying a server.
 * Not used in dig.
 * XXX print_trying
 */
void
trying(char *frm, dig_lookup_t *lookup) {
	UNUSED(frm);
	UNUSED(lookup);
}

/*
 * Internal print routine used to print short form replies.
 */
static isc_result_t
say_message(dns_rdata_t *rdata, dig_query_t *query, isc_buffer_t *buf) {
	isc_result_t result;
	isc_uint64_t diff;
	isc_time_t now;
	char store[sizeof("12345678901234567890")];

	if (query->lookup->trace || query->lookup->ns_search_only) {
		result = dns_rdatatype_totext(rdata->type, buf);
		if (result != ISC_R_SUCCESS)
			return (result);
		ADD_STRING(buf, " ");
	}
	result = dns_rdata_totext(rdata, NULL, buf);
	check_result(result, "dns_rdata_totext");
	if (query->lookup->identify) {
		result = isc_time_now(&now);
		if (result != ISC_R_SUCCESS)
			return (result);
		diff = isc_time_microdiff(&now, &query->time_sent);
		ADD_STRING(buf, " from server ");
		ADD_STRING(buf, query->servname);
		snprintf(store, 19, " in %d ms.", (int)diff/1000);
		ADD_STRING(buf, store);
	}
	ADD_STRING(buf, "\n");
	return (ISC_R_SUCCESS);
}

/*
 * short_form message print handler.  Calls above say_message()
 */
static isc_result_t
short_answer(dns_message_t *msg, dns_messagetextflag_t flags,
	     isc_buffer_t *buf, dig_query_t *query)
{
	dns_name_t *name;
	dns_rdataset_t *rdataset;
	isc_buffer_t target;
	isc_result_t result, loopresult;
	dns_name_t empty_name;
	char t[4096];
	dns_rdata_t rdata = DNS_RDATA_INIT;

	UNUSED(flags);

	dns_name_init(&empty_name, NULL);
	result = dns_message_firstname(msg, DNS_SECTION_ANSWER);
	if (result == ISC_R_NOMORE)
		return (ISC_R_SUCCESS);
	else if (result != ISC_R_SUCCESS)
		return (result);

	for (;;) {
		name = NULL;
		dns_message_currentname(msg, DNS_SECTION_ANSWER, &name);

		isc_buffer_init(&target, t, sizeof(t));

		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			loopresult = dns_rdataset_first(rdataset);
			while (loopresult == ISC_R_SUCCESS) {
				dns_rdataset_current(rdataset, &rdata);
				result = say_message(&rdata, query,
						     buf);
				check_result(result, "say_message");
				loopresult = dns_rdataset_next(rdataset);
				dns_rdata_reset(&rdata);
			}
		}
		result = dns_message_nextname(msg, DNS_SECTION_ANSWER);
		if (result == ISC_R_NOMORE)
			break;
		else if (result != ISC_R_SUCCESS)
			return (result);
	}

	return (ISC_R_SUCCESS);
}

/*
 * Callback from dighost.c to print the reply from a server
 */
isc_result_t
printmessage(dig_query_t *query, dns_message_t *msg, isc_boolean_t headers) {
	isc_result_t result;
	dns_messagetextflag_t flags;
	isc_buffer_t *buf = NULL;
	unsigned int len = OUTPUTBUF;
	const dns_master_style_t *style;

	if (multiline)
		style = &dns_master_style_default;
	else
		style = &dns_master_style_debug;

	if (query->lookup->cmdline[0] != 0) {
		if (!short_form)
			fputs(query->lookup->cmdline, stdout);
		query->lookup->cmdline[0]=0;
	}
	debug("printmessage(%s %s %s)", headers ? "headers" : "noheaders",
	      query->lookup->comments ? "comments" : "nocomments",
	      short_form ? "short_form" : "long_form");

	flags = 0;
	if (!headers) {
		flags |= DNS_MESSAGETEXTFLAG_NOHEADERS;
		flags |= DNS_MESSAGETEXTFLAG_NOCOMMENTS;
	}
	if (!query->lookup->comments)
		flags |= DNS_MESSAGETEXTFLAG_NOCOMMENTS;

	result = ISC_R_SUCCESS;

	result = isc_buffer_allocate(mctx, &buf, len);
	check_result(result, "isc_buffer_allocate");

	if (query->lookup->comments && !short_form) {
		if (query->lookup->cmdline[0] != 0)
			printf("; %s\n", query->lookup->cmdline);
		if (msg == query->lookup->sendmsg)
			printf(";; Sending:\n");
		else
			printf(";; Got answer:\n");

		if (headers) {
			printf(";; ->>HEADER<<- opcode: %s, status: %s, "
			       "id: %u\n",
			       opcodetext[msg->opcode], rcodetext[msg->rcode],
			       msg->id);
			printf(";; flags:");
			if ((msg->flags & DNS_MESSAGEFLAG_QR) != 0)
				printf(" qr");
			if ((msg->flags & DNS_MESSAGEFLAG_AA) != 0)
				printf(" aa");
			if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0)
				printf(" tc");
			if ((msg->flags & DNS_MESSAGEFLAG_RD) != 0)
				printf(" rd");
			if ((msg->flags & DNS_MESSAGEFLAG_RA) != 0)
				printf(" ra");
			if ((msg->flags & DNS_MESSAGEFLAG_AD) != 0)
				printf(" ad");
			if ((msg->flags & DNS_MESSAGEFLAG_CD) != 0)
				printf(" cd");

			printf("; QUERY: %u, ANSWER: %u, "
			       "AUTHORITY: %u, ADDITIONAL: %u\n",
			       msg->counts[DNS_SECTION_QUESTION],
			       msg->counts[DNS_SECTION_ANSWER],
			       msg->counts[DNS_SECTION_AUTHORITY],
			       msg->counts[DNS_SECTION_ADDITIONAL]);
		}
	}

repopulate_buffer:

	if (query->lookup->comments && headers && !short_form)
	{
		result = dns_message_pseudosectiontotext(msg,
			 DNS_PSEUDOSECTION_OPT,
			 style, flags, buf);
		if (result == ISC_R_NOSPACE) {
buftoosmall:
			len += OUTPUTBUF;
			isc_buffer_free(&buf);
			result = isc_buffer_allocate(mctx, &buf, len);
			if (result == ISC_R_SUCCESS)
				goto repopulate_buffer;
			else
				return (result);
		}
		check_result(result,
		     "dns_message_pseudosectiontotext");
	}

	if (query->lookup->section_question && headers) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						       DNS_SECTION_QUESTION,
						       style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
		}
	}
	if (query->lookup->section_answer) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						       DNS_SECTION_ANSWER,
						       style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
		} else {
			result = short_answer(msg, flags, buf, query);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "short_answer");
		}
	}
	if (query->lookup->section_authority) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						       DNS_SECTION_AUTHORITY,
						       style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
		}
	}
	if (query->lookup->section_additional) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						      DNS_SECTION_ADDITIONAL,
						      style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
			/*
			 * Only print the signature on the first record.
			 */
			if (headers) {
				result = dns_message_pseudosectiontotext(
						   msg,
						   DNS_PSEUDOSECTION_TSIG,
						   style, flags, buf);
				if (result == ISC_R_NOSPACE)
					goto buftoosmall;
				check_result(result,
					  "dns_message_pseudosectiontotext");
				result = dns_message_pseudosectiontotext(
						   msg,
						   DNS_PSEUDOSECTION_SIG0,
						   style, flags, buf);
				if (result == ISC_R_NOSPACE)
					goto buftoosmall;
				check_result(result,
					   "dns_message_pseudosectiontotext");
			}
		}
	}
	if (headers && query->lookup->comments && !short_form)
		printf("\n");

	printf("%.*s", (int)isc_buffer_usedlength(buf),
	       (char *)isc_buffer_base(buf));
	isc_buffer_free(&buf);
	return (result);
}

/*
 * print the greeting message when the program first starts up.
 */
static void
printgreeting(int argc, char **argv, dig_lookup_t *lookup) {
	int i;
	int remaining;
	static isc_boolean_t first = ISC_TRUE;
	char append[MXNAME];

	if (printcmd) {
		lookup->cmdline[sizeof(lookup->cmdline) - 1] = 0;
		snprintf(lookup->cmdline, sizeof(lookup->cmdline),
			 "%s; <<>> DiG " VERSION " <<>>",
			 first?"\n":"");
		i = 1;
		while (i < argc) {
			snprintf(append, sizeof(append), " %s", argv[i++]);
			remaining = sizeof(lookup->cmdline) -
				    strlen(lookup->cmdline) - 1;
			strncat(lookup->cmdline, append, remaining);
		}
		remaining = sizeof(lookup->cmdline) -
			    strlen(lookup->cmdline) - 1;
		strncat(lookup->cmdline, "\n", remaining);
		if (first) {
			snprintf(append, sizeof (append), 
				 ";; global options: %s %s\n",
			       short_form ? "short_form" : "",
			       printcmd ? "printcmd" : "");
			first = ISC_FALSE;
			remaining = sizeof(lookup->cmdline) -
				    strlen(lookup->cmdline) - 1;
			strncat(lookup->cmdline, append, remaining);
		}
	}
}

/*
 * Reorder an argument list so that server names all come at the end.
 * This is a bit of a hack, to allow batch-mode processing to properly
 * handle the server options.
 */
static void
reorder_args(int argc, char *argv[]) {
	int i, j;
	char *ptr;
	int end;

	debug("reorder_args()");
	end = argc - 1;
	while (argv[end][0] == '@') {
		end--;
		if (end == 0)
			return;
	}
	debug("arg[end]=%s", argv[end]);
	for (i = 1; i < end - 1; i++) {
		if (argv[i][0] == '@') {
			debug("arg[%d]=%s", i, argv[i]);
			ptr = argv[i];
			for (j = i + 1; j < end; j++) {
				debug("Moving %s to %d", argv[j], j - 1);
				argv[j - 1] = argv[j];
			}
			debug("moving %s to end, %d", ptr, end - 1);
			argv[end - 1] = ptr;
			end--;
			if (end < 1)
				return;
		}
	}
}

static isc_uint32_t
parse_uint(char *arg, const char *desc, isc_uint32_t max) {
	char *endp;
	isc_uint32_t tmp;

	tmp = strtoul(arg, &endp, 10);
	if (*endp != '\0')
		fatal("%s '%s' must be numeric", desc, arg);
	if (tmp > max)
		fatal("%s '%s' out of range", desc, arg);
	return (tmp);
}

/*
 * We're not using isc_commandline_parse() here since the command line
 * syntax of dig is quite a bit different from that which can be described
 * by that routine.
 * XXX doc options
 */

static void
plus_option(char *option, isc_boolean_t is_batchfile,
	    dig_lookup_t *lookup)
{
	char option_store[256];
	char *cmd, *value, *ptr;
	isc_boolean_t state = ISC_TRUE;

	strncpy(option_store, option, sizeof(option_store));
	option_store[sizeof(option_store)-1]=0;
	ptr = option_store;
	cmd=next_token(&ptr,"=");
	if (cmd == NULL) {
		printf(";; Invalid option %s\n",option_store);
		return;
	}
	value=ptr;
	if (strncasecmp(cmd,"no",2)==0) {
		cmd += 2;
		state = ISC_FALSE;
	}
	switch (cmd[0]) {
	case 'a':
		switch (cmd[1]) {
		case 'a': /* aaflag */
			lookup->aaonly = state;
			break;
		case 'd': 
			switch (cmd[2]) {
			case 'd': /* additional */
				lookup->section_additional = state;
				break;
			case 'f': /* adflag */
				lookup->adflag = state;
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'l': /* all */
			lookup->section_question = state;
			lookup->section_authority = state;
			lookup->section_answer = state;
			lookup->section_additional = state;
			lookup->comments = state;
			lookup->stats = state;
			printcmd = state;
			break;
		case 'n': /* answer */
			lookup->section_answer = state;
			break;
		case 'u': /* authority */
			lookup->section_authority = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'b':
		switch (cmd[1]) {
		case 'e':/* besteffort */
			lookup->besteffort = state;
			break;
		case 'u':/* bufsize */
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			lookup->udpsize = (isc_uint16_t) parse_uint(value,
						    "buffer size", COMMSIZE);
			if (lookup->udpsize > COMMSIZE)
				lookup->udpsize = COMMSIZE;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'c':
		switch (cmd[1]) {
		case 'd':/* cdflag */
			lookup->cdflag = state;
			break;
		case 'm': /* cmd */
			printcmd = state;
			break;
		case 'o': /* comments */
			lookup->comments = state;
			if (lookup == default_lookup)
				pluscomm = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'd':
		switch (cmd[1]) {
		case 'e': /* defname */
			usesearch = state;
			break;
		case 'n': /* dnssec */	
			lookup->dnssec = state;
			break;
		case 'o': /* domain */	
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			strncpy(domainopt, value, sizeof(domainopt));
			domainopt[sizeof(domainopt)-1] = '\0';
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'f': /* fail */
		lookup->servfail_stops = state;
		break;
	case 'i':
		switch (cmd[1]) {
		case 'd': /* identify */
			lookup->identify = state;
			break;
		case 'g': /* ignore */
		default: /* Inherets default for compatibility */
			lookup->ignore = ISC_TRUE;
		}
		break;
	case 'm': /* multiline */
		multiline = state;
		break;
	case 'n':
		switch (cmd[1]) {
		case 'd': /* ndots */
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			ndots = parse_uint(value, "ndots", MAXNDOTS);
			break;
		case 's': /* nssearch */
			lookup->ns_search_only = state;
			if (state) {
				lookup->trace_root = ISC_TRUE;
				lookup->recurse = ISC_FALSE;
				lookup->identify = ISC_TRUE;
				lookup->stats = ISC_FALSE;
				lookup->comments = ISC_FALSE;
				lookup->section_additional = ISC_FALSE;
				lookup->section_authority = ISC_FALSE;
				lookup->section_question = ISC_FALSE;
				lookup->rdtype = dns_rdatatype_ns;
				lookup->rdtypeset = ISC_TRUE;
				short_form = ISC_TRUE;
			}
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'q': 
		switch (cmd[1]) {
		case 'r': /* qr */
			qr = state;
			break;
		case 'u': /* question */
			lookup->section_question = state;
			if (lookup == default_lookup)
				plusquest = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'r': /* recurse */
		lookup->recurse = state;
		break;
	case 's':
		switch (cmd[1]) {
		case 'e': /* search */
			usesearch = state;
			break;
		case 'h': /* short */
			short_form = state;
			if (state) {
				printcmd = ISC_FALSE;
				lookup->section_additional = ISC_FALSE;
				lookup->section_answer = ISC_TRUE;
				lookup->section_authority = ISC_FALSE;
				lookup->section_question = ISC_FALSE;
				lookup->comments = ISC_FALSE;
				lookup->stats = ISC_FALSE;
			}
			break;
		case 't': /* stats */
			lookup->stats = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 't':
		switch (cmd[1]) {
		case 'c': /* tcp */
			if (!is_batchfile)
				lookup->tcp_mode = state;
			break;
		case 'i': /* timeout */
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			timeout = parse_uint(value, "timeout", MAXTIMEOUT);
			if (timeout == 0)
				timeout = 1;
			break;
		case 'r':
			switch (cmd[2]) {
			case 'a': /* trace */
				lookup->trace = state;
				lookup->trace_root = state;
				if (state) {
					lookup->recurse = ISC_FALSE;
					lookup->identify = ISC_TRUE;
					lookup->comments = ISC_FALSE;
					lookup->stats = ISC_FALSE;
					lookup->section_additional = ISC_FALSE;
					lookup->section_authority = ISC_TRUE;
					lookup->section_question = ISC_FALSE;
				}
				break;
			case 'i': /* tries */
				if (value == NULL)
					goto need_value;
				if (!state)
					goto invalid_option;
				lookup->retries = parse_uint(value, "retries",
						       MAXTRIES);
				if (lookup->retries == 0)
					lookup->retries = 1;
				break;
			default:
				goto invalid_option;
			}
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'v':
		if (!is_batchfile)
			lookup->tcp_mode = state;
		break;
	default:
	invalid_option:
	need_value:
		fprintf(stderr, "Invalid option: +%s\n",
			 option);
		usage();
	}
	return;
}

/*
 * ISC_TRUE returned if value was used
 */
static isc_boolean_t
dash_option(char *option, char *next, dig_lookup_t **lookup,
	    isc_boolean_t *open_type_class,
		isc_boolean_t *firstarg,
		int argc, char **argv)
{
	char cmd, *value, *ptr;
	isc_result_t result;
	isc_boolean_t value_from_next;
	isc_textregion_t tr;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	char textname[MXNAME];
	struct in_addr in4;
	struct in6_addr in6;

	cmd = option[0];
	if (strlen(option) > 1) {
		value_from_next = ISC_FALSE;
		value = &option[1];
	} else {
		value_from_next = ISC_TRUE;
		value = next;
	}
	switch (cmd) {
	case 'd':
		debugging = ISC_TRUE;
		return (ISC_FALSE);
	case 'h':
		help();
		exit(0);
		break;
	case 'm': /* memdebug */
		/* memdebug is handled in preparse_args() */
		return (ISC_FALSE);
	case 'n':
		nibble = ISC_TRUE;
		return (ISC_FALSE);
	}
	if (value == NULL)
		goto invalid_option;
	switch (cmd) {
	case 'b':
		if (have_ipv6 && inet_pton(AF_INET6, value, &in6) == 1)
			isc_sockaddr_fromin6(&bind_address, &in6, 0);
		else if (have_ipv4 && inet_pton(AF_INET, value, &in4) == 1)
			isc_sockaddr_fromin(&bind_address, &in4, 0);
		else
			fatal("invalid address %s", value);
		specified_source = ISC_TRUE;
		return (value_from_next);
	case 'c':
		if ((*lookup)->rdclassset) {
			fprintf(stderr, ";; Warning, extra class option\n");
		}
		*open_type_class = ISC_FALSE;
		tr.base = value;
		tr.length = strlen(value);
		result = dns_rdataclass_fromtext(&rdclass,
						 (isc_textregion_t *)&tr);
		if (result == ISC_R_SUCCESS) {
			(*lookup)->rdclass = rdclass;
			(*lookup)->rdclassset = ISC_TRUE;
		} else
			fprintf(stderr, ";; Warning, ignoring "
				"invalid class %s\n",
				value);
		return (value_from_next);
	case 'f':
		batchname = value;
		return (value_from_next);
	case 'k':
		strncpy(keyfile, value, sizeof(keyfile));
		keyfile[sizeof(keyfile)-1]=0;
		return (value_from_next);
	case 'p':
		port = (in_port_t) parse_uint(value, "port number", MAXPORT);
		return (value_from_next);
	case 't':
		*open_type_class = ISC_FALSE;
		if (strncasecmp(value, "ixfr=", 5) == 0) {
			rdtype = dns_rdatatype_ixfr;
			result = ISC_R_SUCCESS;
		} else {
			tr.base = value;
			tr.length = strlen(value);
			result = dns_rdatatype_fromtext(&rdtype,
						(isc_textregion_t *)&tr);
			if (result == ISC_R_SUCCESS &&
			    rdtype == dns_rdatatype_ixfr)
			{
				result = DNS_R_UNKNOWN;
			}
		}
		if (result == ISC_R_SUCCESS) {
			if ((*lookup)->rdtypeset) {
				fprintf(stderr, ";; Warning, "
						"extra type option\n");
			}
			if (rdtype == dns_rdatatype_ixfr) {
				(*lookup)->rdtype = dns_rdatatype_ixfr;
				(*lookup)->rdtypeset = ISC_TRUE;
				(*lookup)->ixfr_serial =
					parse_uint(&value[5], "serial number",
					  	MAXSERIAL);
				(*lookup)->section_question = plusquest;
				(*lookup)->comments = pluscomm;
			} else {
				(*lookup)->rdtype = rdtype;
				(*lookup)->rdtypeset = ISC_TRUE;
				if (rdtype == dns_rdatatype_axfr) {
					(*lookup)->section_question = plusquest;
					(*lookup)->comments = pluscomm;
				}
				(*lookup)->ixfr_serial = ISC_FALSE;
			}
		} else
			fprintf(stderr, ";; Warning, ignoring "
				 "invalid type %s\n",
				 value);
		return (value_from_next);
	case 'y':
		ptr = next_token(&value,":");
		if (ptr == NULL) {
			usage();
		}
		strncpy(keynametext, ptr, sizeof(keynametext));
		keynametext[sizeof(keynametext)-1]=0;
		ptr = next_token(&value, "");
		if (ptr == NULL)
			usage();
		strncpy(keysecret, ptr, sizeof(keysecret));
		keysecret[sizeof(keysecret)-1]=0;
		return (value_from_next);
	case 'x':
		*lookup = clone_lookup(default_lookup, ISC_TRUE);
		if (get_reverse(textname, value, nibble) == ISC_R_SUCCESS) {
			strncpy((*lookup)->textname, textname,
				sizeof((*lookup)->textname));
			debug("looking up %s", (*lookup)->textname);
			(*lookup)->trace_root = ISC_TF((*lookup)->trace  ||
						(*lookup)->ns_search_only);
			(*lookup)->nibble = nibble;
			if (!(*lookup)->rdtypeset)
				(*lookup)->rdtype = dns_rdatatype_ptr;
			if (!(*lookup)->rdclassset)
				(*lookup)->rdclass = dns_rdataclass_in;
			(*lookup)->new_search = ISC_TRUE;
			if (*lookup && *firstarg)
			{
				printgreeting(argc, argv, *lookup);
				*firstarg = ISC_FALSE;
			}
			ISC_LIST_APPEND(lookup_list, *lookup, link);
		} else {
			fprintf(stderr, "Invalid IP address %s\n", value);
			exit(1);
		}
		return (value_from_next);
	invalid_option:
	default:
		fprintf(stderr, "Invalid option: -%s\n", option);
		usage();
	}
	return (ISC_FALSE);
}

/*
 * Because we may be trying to do memory allocation recording, we're going
 * to need to parse the arguments for the -m *before* we start the main
 * argument parsing routine.
 * I'd prefer not to have to do this, but I am not quite sure how else to
 * fix the problem.  Argument parsing in dig involves memory allocation
 * by its nature, so it can't be done in the main argument parser.
 */
static void
preparse_args(int argc, char **argv) {
	int rc;
	char **rv;

	rc = argc;
	rv = argv;
	for (rc--, rv++; rc > 0; rc--, rv++) {
		if (strcmp(rv[0], "-m") == 0) {
			memdebugging = ISC_TRUE;
			isc_mem_debugging = ISC_MEM_DEBUGTRACE |
				ISC_MEM_DEBUGRECORD;
			return;
		}
	}
}


static void
parse_args(isc_boolean_t is_batchfile, isc_boolean_t config_only,
	   int argc, char **argv) {
	isc_result_t result;
	isc_textregion_t tr;
	isc_boolean_t firstarg = ISC_TRUE;
	dig_server_t *srv = NULL;
	dig_lookup_t *lookup = NULL;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	isc_boolean_t open_type_class = ISC_TRUE;
	char batchline[MXNAME];
	int bargc;
	char *bargv[64];
	int rc;
	char **rv;
#ifndef NOPOSIX
	char *homedir;
	char rcfile[256];
#endif
	char *input;

	/*
	 * The semantics for parsing the args is a bit complex; if
	 * we don't have a host yet, make the arg apply globally,
	 * otherwise make it apply to the latest host.  This is
	 * a bit different than the previous versions, but should
	 * form a consistent user interface.
	 *
	 * First, create a "default lookup" which won't actually be used
	 * anywhere, except for cloning into new lookups
	 */

	debug("parse_args()");
	if (!is_batchfile) {
		debug("making new lookup");
		default_lookup = make_empty_lookup();

#ifndef NOPOSIX
		/*
		 * Treat .digrc as a special batchfile
		 */
		homedir = getenv("HOME");
		if (homedir != NULL)
			snprintf(rcfile, sizeof(rcfile), "%s/.digrc", homedir);
		else
			strcpy(rcfile, ".digrc");
		batchfp = fopen(rcfile, "r");
		if (batchfp != NULL) {
			while (fgets(batchline, sizeof(batchline),
				     batchfp) != 0) {
				debug("config line %s", batchline);
				bargc = 1;
				input = batchline;
				bargv[bargc] = next_token(&input, " \t\r\n");
				while ((bargv[bargc] != NULL) &&
				       (bargc < 62)) {
					bargc++;
					bargv[bargc] = next_token(&input, " \t\r\n");
				}

				bargv[0] = argv[0];
				argv0 = argv[0];

				reorder_args(bargc, (char **)bargv);
				parse_args(ISC_TRUE, ISC_TRUE, bargc,
					   (char **)bargv);
			}
			fclose(batchfp);
		}
#endif
	}

	lookup = default_lookup;

	rc = argc;
	rv = argv;
	for (rc--, rv++; rc > 0; rc--, rv++) {
		debug("main parsing %s", rv[0]);
		if (strncmp(rv[0], "%", 1) == 0)
			break;
		if (strncmp(rv[0], "@", 1) == 0) {
			srv = make_server(&rv[0][1]);
			ISC_LIST_APPEND(lookup->my_server_list,
					srv, link);
		} else if (rv[0][0] == '+') {
			plus_option(&rv[0][1], is_batchfile,
				    lookup);
		} else if (rv[0][0] == '-') {
			if (rc <= 1) {
				if (dash_option(&rv[0][1], NULL,
						&lookup, &open_type_class,
						&firstarg, argc, argv)) {
					rc--;
					rv++;
				}
			} else {
				if (dash_option(&rv[0][1], rv[1],
						&lookup, &open_type_class,
						&firstarg, argc, argv)) {
					rc--;
					rv++;
				}
			}
		} else {
			/*
			 * Anything which isn't an option
			 */
			if (open_type_class) {
				if (strncmp(rv[0], "ixfr=", 5) == 0) {
					rdtype = dns_rdatatype_ixfr;
					result = ISC_R_SUCCESS;
				} else {
					tr.base = rv[0];
					tr.length = strlen(rv[0]);
					result = dns_rdatatype_fromtext(&rdtype,
						     	(isc_textregion_t *)&tr);
					if (result == ISC_R_SUCCESS &&
					    rdtype == dns_rdatatype_ixfr)
					{
						result = DNS_R_UNKNOWN;
						fprintf(stderr, ";; Warning, "
							"ixfr requires a "
							"serial number\n");
						continue;
					}
				}
				if (result == ISC_R_SUCCESS)
				{
					if (lookup->rdtypeset) {
						fprintf(stderr, ";; Warning, "
							"extra type option\n");
					}
					if (rdtype == dns_rdatatype_ixfr) {
						lookup->rdtype = dns_rdatatype_ixfr;
						lookup->rdtypeset = ISC_TRUE;
						lookup->ixfr_serial =
							parse_uint(&rv[0][5],
							  	"serial number",
							  	MAXSERIAL);
						lookup->section_question = plusquest;
						lookup->comments = pluscomm;
					} else {
						lookup->rdtype = rdtype;
						lookup->rdtypeset = ISC_TRUE;
						if (rdtype == dns_rdatatype_axfr) {
							lookup->section_question =
								plusquest;
							lookup->comments = pluscomm;
						}
						lookup->ixfr_serial = ISC_FALSE;
					}
					continue;
				}
				result = dns_rdataclass_fromtext(&rdclass,
						     (isc_textregion_t *)&tr);
				if (result == ISC_R_SUCCESS) {
					if (lookup->rdclassset) {
						fprintf(stderr, ";; Warning, "
							"extra class option\n");
					}
					lookup->rdclass = rdclass;
					lookup->rdclassset = ISC_TRUE;
					continue;
				}
			}
			if (!config_only) {
				lookup = clone_lookup(default_lookup,
						      ISC_TRUE);
				if (firstarg) {
					printgreeting(argc, argv, lookup);
					firstarg = ISC_FALSE;
				}
				strncpy(lookup->textname, rv[0], 
					sizeof(lookup->textname));
				lookup->textname[sizeof(lookup->textname)-1]=0;
				lookup->trace_root = ISC_TF(lookup->trace  ||
						     lookup->ns_search_only);
				lookup->new_search = ISC_TRUE;
				ISC_LIST_APPEND(lookup_list, lookup, link);
				debug("looking up %s", lookup->textname);
			}
			/* XXX Error message */
		}
	}
	/*
	 * If we have a batchfile, seed the lookup list with the
	 * first entry, then trust the callback in dighost_shutdown
	 * to get the rest
	 */
	if ((batchname != NULL) && !(is_batchfile)) {
		if (strcmp(batchname, "-") == 0)
			batchfp = stdin;
		else
			batchfp = fopen(batchname, "r");
		if (batchfp == NULL) {
			perror(batchname);
			if (exitcode < 8)
				exitcode = 8;
			fatal("Couldn't open specified batch file");
		}
		/* XXX Remove code dup from shutdown code */
	next_line:
		if (fgets(batchline, sizeof(batchline), batchfp) != 0) {
			bargc = 1;
			debug("batch line %s", batchline);
			if (batchline[0] == '\r' || batchline[0] == '\n'
			    || batchline[0] == '#' || batchline[0] == ';')
				goto next_line;
			input = batchline;
			bargv[bargc] = next_token(&input, " \t\r\n");
			while ((bargv[bargc] != NULL) && (bargc < 14)) {
				bargc++;
				bargv[bargc] = next_token(&input, " \t\r\n");
			}

			bargv[0] = argv[0];
			argv0 = argv[0];

			reorder_args(bargc, (char **)bargv);
			parse_args(ISC_TRUE, ISC_FALSE, bargc, (char **)bargv);
		}
	}
	/*
	 * If no lookup specified, search for root
	 */
	if ((lookup_list.head == NULL) && !config_only) {
		lookup = clone_lookup(default_lookup, ISC_TRUE);
		lookup->trace_root = ISC_TF(lookup->trace ||
					    lookup->ns_search_only);
		lookup->new_search = ISC_TRUE;
		strcpy(lookup->textname, ".");
		lookup->rdtype = dns_rdatatype_ns;
		lookup->rdtypeset = ISC_TRUE;
		if (firstarg) {
			printgreeting(argc, argv, lookup);
			firstarg = ISC_FALSE;
		}
		ISC_LIST_APPEND(lookup_list, lookup, link);
	}
}

/*
 * Callback from dighost.c to allow program-specific shutdown code.  Here,
 * Here, we're possibly reading from a batch file, then shutting down for
 * real if there's nothing in the batch file to read.
 */
void
dighost_shutdown(void) {
	char batchline[MXNAME];
	int bargc;
	char *bargv[16];
	char *input;


	if (batchname == NULL) {
		isc_app_shutdown();
		return;
	}

	if (feof(batchfp)) {
		batchname = NULL;
		isc_app_shutdown();
		if (batchfp != stdin)
			fclose(batchfp);
		return;
	}

	if (fgets(batchline, sizeof(batchline), batchfp) != 0) {
		debug("batch line %s", batchline);
		bargc = 1;
		input = batchline;
		bargv[bargc] = next_token(&input, " \t\r\n");
		while ((bargv[bargc] != NULL) && (bargc < 14)) {
			bargc++;
			bargv[bargc] = next_token(&input, " \t\r\n");
		}

		bargv[0] = argv0;

		reorder_args(bargc, (char **)bargv);
		parse_args(ISC_TRUE, ISC_FALSE, bargc, (char **)bargv);
		start_lookup();
	} else {
		batchname = NULL;
		if (batchfp != stdin)
			fclose(batchfp);
		isc_app_shutdown();
		return;
	}
}

int
main(int argc, char **argv) {
	isc_result_t result;
	dig_server_t *s, *s2;

	ISC_LIST_INIT(lookup_list);
	ISC_LIST_INIT(server_list);
	ISC_LIST_INIT(search_list);

	debug("main()");
	preparse_args(argc, argv);
	progname = argv[0];
	result = isc_app_start();
	check_result(result, "isc_app_start");
	setup_libs();
	parse_args(ISC_FALSE, ISC_FALSE, argc, argv);
	setup_system();
	if (domainopt[0] != '\0') {
		set_search_domain(domainopt);
		usesearch = ISC_TRUE;
	}
	result = isc_app_onrun(mctx, global_task, onrun_callback, NULL);
	check_result(result, "isc_app_onrun");
	isc_app_run();
	s = ISC_LIST_HEAD(default_lookup->my_server_list);
	while (s != NULL) {
		debug("freeing server %p belonging to %p",
		      s, default_lookup);
		s2 = s;
		s = ISC_LIST_NEXT(s, link);
		ISC_LIST_DEQUEUE(default_lookup->my_server_list, s2, link);
		isc_mem_free(mctx, s2);
	}
	isc_mem_free(mctx, default_lookup);
	if (batchname != NULL) {
		if (batchfp != stdin)
			fclose(batchfp);
		batchname = NULL;
	}
	cancel_all();
	destroy_libs();
	isc_app_finish();
	return (exitcode);
}
