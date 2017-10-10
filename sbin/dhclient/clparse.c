/*	$OpenBSD: clparse.c,v 1.130 2017/10/09 21:33:11 krw Exp $	*/

/* Parser for dhclient config and lease files. */

/*
 * Copyright (c) 1997 The Internet Software Consortium.
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

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/if_arp.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "dhctoken.h"
#include "log.h"

void parse_client_statement(FILE *, char *);
int parse_X(FILE *, uint8_t *, int);
int parse_option_list(FILE *, uint8_t *, size_t);
void parse_interface_declaration(FILE *, char *);
struct client_lease *parse_client_lease_statement(FILE *, char *);
void parse_client_lease_declaration(FILE *, struct client_lease *, char *);
int parse_option_decl(FILE *, struct option_data *);
void parse_reject_statement(FILE *);
void add_lease(struct client_lease_tq *, struct client_lease *);

void
add_lease(struct client_lease_tq *tq, struct client_lease *lease)
{
	struct client_lease	*lp, *nlp;

	if (lease == NULL)
		return;

	/*
	 * The new lease will supersede a lease with the same ssid
	 * AND the same Client Identifier AND the same
	 * IP address.
	 */
	TAILQ_FOREACH_SAFE(lp, tq, next, nlp) {
		if (lp->ssid_len != lease->ssid_len)
			continue;
		if (memcmp(lp->ssid, lease->ssid, lp->ssid_len) != 0)
			continue;
		if ((lease->options[DHO_DHCP_CLIENT_IDENTIFIER].len != 0) &&
		    ((lp->options[DHO_DHCP_CLIENT_IDENTIFIER].len !=
		    lease->options[DHO_DHCP_CLIENT_IDENTIFIER].len) ||
		    memcmp(lp->options[DHO_DHCP_CLIENT_IDENTIFIER].data,
		    lease->options[DHO_DHCP_CLIENT_IDENTIFIER].data,
		    lp->options[DHO_DHCP_CLIENT_IDENTIFIER].len)))
			continue;
		if (lp->address.s_addr != lease->address.s_addr)
			continue;

		TAILQ_REMOVE(tq, lp, next);
		free_client_lease(lp);
	}

	TAILQ_INSERT_TAIL(tq, lease, next);
}

/*
 * client-conf-file :== client-declarations EOF
 * client-declarations :== <nil>
 *			 | client-declaration
 *			 | client-declarations client-declaration
 */
void
read_client_conf(char *name)
{
	FILE *cfile;
	int token;

	new_parse(path_dhclient_conf);

	TAILQ_INIT(&config->static_leases);
	TAILQ_INIT(&config->reject_list);

	/* Set some defaults. */
	config->link_timeout = 10;	/* secs before going daemon w/o link */
	config->timeout = 30;		/* secs to wait for an OFFER */
	config->select_interval = 0;	/* secs to wait for other OFFERs */
	config->reboot_timeout = 1;	/* secs before giving up on reboot */
	config->retry_interval = 1;	/* secs before asking for OFFER */
	config->backoff_cutoff = 10;	/* max secs between packet retries */
	config->initial_interval = 1;	/* secs before 1st retry */

	config->requested_options
	    [config->requested_option_count++] = DHO_SUBNET_MASK;
	config->requested_options
	    [config->requested_option_count++] = DHO_BROADCAST_ADDRESS;
	config->requested_options
	    [config->requested_option_count++] = DHO_TIME_OFFSET;
	/* RFC 3442 says CLASSLESS_STATIC_ROUTES must be before ROUTERS! */
	config->requested_options
	    [config->requested_option_count++] = DHO_CLASSLESS_STATIC_ROUTES;
	config->requested_options
	    [config->requested_option_count++] = DHO_ROUTERS;
	config->requested_options
	    [config->requested_option_count++] = DHO_DOMAIN_NAME;
	config->requested_options
	    [config->requested_option_count++] = DHO_DOMAIN_SEARCH;
	config->requested_options
	    [config->requested_option_count++] = DHO_DOMAIN_NAME_SERVERS;
	config->requested_options
	    [config->requested_option_count++] = DHO_HOST_NAME;
	config->requested_options
	    [config->requested_option_count++] = DHO_BOOTFILE_NAME;
	config->requested_options
	    [config->requested_option_count++] = DHO_TFTP_SERVER;

	if ((cfile = fopen(path_dhclient_conf, "r")) != NULL) {
		do {
			token = peek_token(NULL, cfile);
			if (token == EOF)
				break;
			parse_client_statement(cfile, name);
		} while (1);
		fclose(cfile);
	}
}

/*
 * lease-file :== client-lease-statements EOF
 * client-lease-statements :== <nil>
 *		     | client-lease-statements LEASE client-lease-statement
 */
void
read_client_leases(char *name, struct client_lease_tq *tq)
{
	FILE	*cfile;
	int	 token;

	TAILQ_INIT(tq);

	if ((cfile = fopen(path_dhclient_db, "r")) == NULL)
		return;

	new_parse(path_dhclient_db);

	do {
		token = next_token(NULL, cfile);
		if (token == EOF)
			break;
		if (token != TOK_LEASE) {
			log_warnx("%s: expecting lease", log_procname);
			break;
		}
		add_lease(tq, parse_client_lease_statement(cfile, name));
	} while (1);
	fclose(cfile);
}

/*
 * client-declaration :==
 *	TOK_SEND option-decl |
 *	TOK_DEFAULT option-decl |
 *	TOK_SUPERSEDE option-decl |
 *	TOK_APPEND option-decl |
 *	TOK_PREPEND option-decl |
 *	TOK_REQUEST option-list |
 *	TOK_REQUIRE option-list |
 *	TOK_IGNORE option-list |
 *	TOK_TIMEOUT number |
 *	TOK_RETRY number |
 *	TOK_SELECT_TIMEOUT number |
 *	TOK_REBOOT number |
 *	TOK_BACKOFF_CUTOFF number |
 *	TOK_INITIAL_INTERVAL number |
 *	interface-declaration |
 *	TOK_LEASE client-lease-statement |
 *	TOK_REJECT reject-statement
 */
void
parse_client_statement(FILE *cfile, char *name)
{
	uint8_t		 optlist[DHO_COUNT];
	char		*string;
	int		 code, count, token;

	token = next_token(NULL, cfile);

	switch (token) {
	case TOK_SEND:
		parse_option_decl(cfile, &config->send_options[0]);
		break;
	case TOK_DEFAULT:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_DEFAULT;
		break;
	case TOK_SUPERSEDE:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_SUPERSEDE;
		break;
	case TOK_APPEND:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_APPEND;
		break;
	case TOK_PREPEND:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_PREPEND;
		break;
	case TOK_REQUEST:
		count = parse_option_list(cfile, optlist, sizeof(optlist));
		if (count != -1) {
			config->requested_option_count = count;
			memcpy(config->requested_options, optlist,
			    sizeof(config->requested_options));
		}
		break;
	case TOK_REQUIRE:
		count = parse_option_list(cfile, optlist, sizeof(optlist));
		if (count != -1) {
			config->required_option_count = count;
			memcpy(config->required_options, optlist,
			    sizeof(config->required_options));
		}
		break;
	case TOK_IGNORE:
		count = parse_option_list(cfile, optlist, sizeof(optlist));
		if (count != -1) {
			config->ignored_option_count = count;
			memcpy(config->ignored_options, optlist,
			    sizeof(config->ignored_options));
		}
		break;
	case TOK_LINK_TIMEOUT:
		if (parse_lease_time(cfile, &config->link_timeout) == 1)
			parse_semi(cfile);
		break;
	case TOK_TIMEOUT:
		if (parse_lease_time(cfile, &config->timeout) == 1)
			parse_semi(cfile);
		break;
	case TOK_RETRY:
		if (parse_lease_time(cfile, &config->retry_interval) == 1)
			parse_semi(cfile);
		break;
	case TOK_SELECT_TIMEOUT:
		if (parse_lease_time(cfile, &config->select_interval) == 1)
			parse_semi(cfile);
		break;
	case TOK_REBOOT:
		if (parse_lease_time(cfile, &config->reboot_timeout) == 1)
			parse_semi(cfile);
		break;
	case TOK_BACKOFF_CUTOFF:
		if (parse_lease_time(cfile, &config->backoff_cutoff) == 1)
			parse_semi(cfile);
		break;
	case TOK_INITIAL_INTERVAL:
		if (parse_lease_time(cfile, &config->initial_interval) == 1)
			parse_semi(cfile);
		break;
	case TOK_INTERFACE:
		parse_interface_declaration(cfile, name);
		break;
	case TOK_LEASE:
		add_lease(&config->static_leases,
		    parse_client_lease_statement(cfile, name));
		break;
	case TOK_REJECT:
		parse_reject_statement(cfile);
		break;
	case TOK_FILENAME:
		string = parse_string(cfile, NULL);
		free(config->filename);
		config->filename = string;
		parse_semi(cfile);
		break;
	case TOK_SERVER_NAME:
		string = parse_string(cfile, NULL);
		free(config->server_name);
		config->server_name = string;
		parse_semi(cfile);
		break;
	case TOK_FIXED_ADDR:
		if (parse_ip_addr(cfile, &config->address) == 1)
			parse_semi(cfile);
		break;
	case TOK_NEXT_SERVER:
		if (parse_ip_addr(cfile, &config->next_server) == 1)
			parse_semi(cfile);
		break;
	default:
		parse_warn("expecting statement.");
		if (token != ';')
			skip_to_semi(cfile);
		break;
	}
}

int
parse_X(FILE *cfile, uint8_t *buf, int max)
{
	int	 token;
	char	*val;
	int	 len;

	token = peek_token(&val, cfile);
	if (token == TOK_NUMBER_OR_NAME) {
		len = 0;
		for (token = ':'; token == ':';
		     token = next_token(NULL, cfile)) {
			if (parse_hex(cfile, &buf[len]) == 0)
				break;
			if (++len == max)
				break;
			if (peek_token(NULL, cfile) == ';')
				return len;
		}
		if (token != ':') {
			parse_warn("expecting ':'.");
			skip_to_semi(cfile);
			return -1;
		} else {
			parse_warn("expecting hex value.");
			skip_to_semi(cfile);
			return -1;
		}
	} else if (token == TOK_STRING) {
		token = next_token(&val, cfile);
		len = strlen(val);
		if (len + 1 > max) {
			parse_warn("string constant too long.");
			skip_to_semi(cfile);
			return -1;
		}
		memcpy(buf, val, len + 1);
	} else {
		token = next_token(NULL, cfile);
		parse_warn("expecting string or hex data.");
		if (token != ';')
			skip_to_semi(cfile);
		return -1;
	}
	return len;
}

/*
 * option-list :== option_name |
 *		   option_list COMMA option_name
 */
int
parse_option_list(FILE *cfile, uint8_t *list, size_t sz)
{
	unsigned int	 ix, j;
	int		 i;
	int		 token;
	char		*val;

	memset(list, DHO_PAD, sz);
	ix = 0;
	do {
		token = next_token(&val, cfile);
		if (token == ';' && ix == 0) {
			/* Empty list. */
			return 0;
		}
		if (is_identifier(token) == 0) {
			parse_warn("expecting option name.");
			goto syntaxerror;
		}
		/*
		 * 0 (DHO_PAD) and 255 (DHO_END) are not valid in option
		 * lists.  They are not really options and it makes no sense
		 * to request, require or ignore them.
		 */

		i = name_to_code(val);
		if (i == DHO_END) {
			parse_warn("expecting option name.");
			goto syntaxerror;
		}
		if (ix == sz) {
			parse_warn("too many options.");
			goto syntaxerror;
		}
		/* Avoid storing duplicate options in the list. */
		for (j = 0; j < ix && list[j] != i; j++)
			;
		if (j == ix)
			list[ix++] = i;
		token = peek_token(NULL, cfile);
		if (token == ',')
			token = next_token(NULL, cfile);
	} while (token == ',');

	if (parse_semi(cfile) != 0)
		return ix;

syntaxerror:
	if (token != ';')
		skip_to_semi(cfile);
	return -1;
}

/*
 * interface-declaration :==
 *	INTERFACE string LBRACE client-declarations RBRACE
 */
void
parse_interface_declaration(FILE *cfile, char *name)
{
	char	*val;
	int	 token;

	token = next_token(&val, cfile);
	if (token != TOK_STRING) {
		parse_warn("expecting string.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	if (strcmp(name, val) != 0) {
		skip_to_semi(cfile);
		return;
	}

	token = next_token(&val, cfile);
	if (token != '{') {
		parse_warn("expecting '{'.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	do {
		token = peek_token(&val, cfile);
		if (token == EOF) {
			parse_warn("unterminated interface declaration.");
			return;
		}
		if (token == '}')
			break;
		parse_client_statement(cfile, name);
	} while (1);
	token = next_token(&val, cfile);
}

/*
 * client-lease-statement :==
 *	RBRACE client-lease-declarations LBRACE
 *
 *	client-lease-declarations :==
 *		<nil> |
 *		client-lease-declaration |
 *		client-lease-declarations client-lease-declaration
 */
struct client_lease *
parse_client_lease_statement(FILE *cfile, char *name)
{
	struct client_lease	*lease;
	int			 token;

	token = next_token(NULL, cfile);
	if (token != '{') {
		parse_warn("expecting '{'.");
		if (token != ';')
			skip_to_semi(cfile);
		return NULL;
	}

	lease = calloc(1, sizeof(*lease));
	if (lease == NULL)
		fatal("lease");

	do {
		token = peek_token(NULL, cfile);
		if (token == EOF) {
			parse_warn("unterminated lease declaration.");
			free_client_lease(lease);
			return NULL;
		}
		if (token == '}')
			break;
		parse_client_lease_declaration(cfile, lease, name);
	} while (1);
	token = next_token(NULL, cfile);

	return lease;
}

/*
 * client-lease-declaration :==
 *	BOOTP |
 *	INTERFACE string |
 *	FIXED_ADDR ip_address |
 *	FILENAME string |
 *	SERVER_NAME string |
 *	OPTION option-decl |
 *	RENEW time-decl |
 *	REBIND time-decl |
 *	EXPIRE time-decl
 */
void
parse_client_lease_declaration(FILE *cfile, struct client_lease *lease,
    char *name)
{
	char		*val;
	unsigned int	 len;
	int		 token;

	token = next_token(&val, cfile);

	switch (token) {
	case TOK_BOOTP:
		/* 'bootp' is just a comment. See BOOTP_LEASE(). */
		break;
	case TOK_INTERFACE:
		token = next_token(&val, cfile);
		if (token != TOK_STRING) {
			parse_warn("expecting string.");
			if (token != ';')
				skip_to_semi(cfile);
			return;
		}
		if (strcmp(name, val) != 0) {
			if (lease->is_static == 0)
				parse_warn("wrong interface name.");
			skip_to_semi(cfile);
			return;
		}
		break;
	case TOK_FIXED_ADDR:
		if (parse_ip_addr(cfile, &lease->address) == 0)
			return;
		break;
	case TOK_NEXT_SERVER:
		if (parse_ip_addr(cfile, &lease->next_server) == 0)
			return;
		break;
	case TOK_FILENAME:
		lease->filename = parse_string(cfile, NULL);
		break;
	case TOK_SERVER_NAME:
		lease->server_name = parse_string(cfile, NULL);
		break;
	case TOK_SSID:
		val = parse_string(cfile, &len);
		if (val && len <= sizeof(lease->ssid)) {
			memset(lease->ssid, 0, sizeof(lease->ssid));
			memcpy(lease->ssid, val, len);
			lease->ssid_len = len;
		}
		free(val);
		break;
	case TOK_RENEW:
		lease->renewal = parse_date(cfile);
		return;
	case TOK_REBIND:
		lease->rebind = parse_date(cfile);
		return;
	case TOK_EXPIRE:
		lease->expiry = parse_date(cfile);
		return;
	case TOK_OPTION:
		parse_option_decl(cfile, lease->options);
		return;
	default:
		parse_warn("expecting lease declaration.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	parse_semi(cfile);
}

int
parse_option_decl(FILE *cfile, struct option_data *options)
{
	char		*val;
	int		 token;
	uint8_t		 buf[4];
	uint8_t		 cidr[5];
	uint8_t		 hunkbuf[1024];
	unsigned int	 hunkix = 0;
	char		*fmt;
	struct in_addr	 ip_addr;
	uint8_t		*dp;
	int		 len, code;
	int		 nul_term = 0;

	token = next_token(&val, cfile);
	if (is_identifier(token) == 0) {
		parse_warn("expecting identifier.");
		if (token != ';')
			skip_to_semi(cfile);
		return -1;
	}

	/* Look up the actual option info. */
	code = name_to_code(val);
	if (code == DHO_END) {
		parse_warn("unknown option name.");
		skip_to_semi(cfile);
		return -1;
	}

	/* Parse the option data. */
	do {
		for (fmt = code_to_format(code); *fmt; fmt++) {
			if (*fmt == 'A')
				break;
			switch (*fmt) {
			case 'X':
				len = parse_X(cfile, &hunkbuf[hunkix],
				    sizeof(hunkbuf) - hunkix);
				if (len == -1)
					return -1;
				hunkix += len;
				dp = NULL;
				break;
			case 't': /* Text string. */
				val = parse_string(cfile, &len);
				if (val == NULL)
					return -1;
				if (hunkix + len + 1 > sizeof(hunkbuf)) {
					parse_warn("option data buffer "
					    "overflow");
					skip_to_semi(cfile);
					return -1;
				}
				memcpy(&hunkbuf[hunkix], val, len + 1);
				nul_term = 1;
				hunkix += len;
				free(val);
				dp = NULL;
				break;
			case 'I': /* IP address. */
				if (parse_ip_addr(cfile, &ip_addr) == 0)
					return -1;
				len = sizeof(ip_addr);
				dp = (uint8_t *)&ip_addr;
				break;
			case 'l':	/* Signed 32-bit integer. */
				if (parse_decimal(cfile, buf, 'l') == 0) {
					parse_warn("expecting signed 32-bit "
					    "integer.");
					skip_to_semi(cfile);
					return -1;
				}
				len = 4;
				dp = buf;
				break;
			case 'L':	/* Unsigned 32-bit integer. */
				if (parse_decimal(cfile, buf, 'L') == 0) {
					parse_warn("expecting unsigned 32-bit "
					    "integer.");
					skip_to_semi(cfile);
					return -1;
				}
				len = 4;
				dp = buf;
				break;
			case 'S':	/* Unsigned 16-bit integer. */
				if (parse_decimal(cfile, buf, 'S') == 0) {
					parse_warn("expecting unsigned 16-bit "
					    "integer.");
					skip_to_semi(cfile);
					return -1;
				}
				len = 2;
				dp = buf;
				break;
			case 'B':	/* Unsigned 8-bit integer. */
				if (parse_decimal(cfile, buf, 'B') == 0) {
					parse_warn("expecting unsigned 8-bit "
					    "integer.");
					skip_to_semi(cfile);
					return -1;
				}
				len = 1;
				dp = buf;
				break;
			case 'f': /* Boolean flag. */
				if (parse_boolean(cfile, buf) == 0)
					return -1;
				len = 1;
				dp = buf;
				break;
			case 'C':
				if (parse_cidr(cfile, cidr) == 0)
					return -1;
				len = 1 + (cidr[0] + 7) / 8;
				dp = cidr;
				break;
			default:
				log_warnx("%s: bad format %c in "
				    "parse_option_param", log_procname, *fmt);
				skip_to_semi(cfile);
				return -1;
			}
			if (dp != NULL && len > 0) {
				if (hunkix + len > sizeof(hunkbuf)) {
					parse_warn("option data buffer "
					    "overflow");
					skip_to_semi(cfile);
					return -1;
				}
				memcpy(&hunkbuf[hunkix], dp, len);
				hunkix += len;
			}
		}
		token = peek_token(NULL, cfile);
		if (*fmt == 'A' && token == ',')
			token = next_token(NULL, cfile);
	} while (*fmt == 'A' && token == ',');

	if (parse_semi(cfile) == 0)
		return -1;

	options[code].data = malloc(hunkix + nul_term);
	if (options[code].data == NULL)
		fatal("option data");
	memcpy(options[code].data, hunkbuf, hunkix + nul_term);
	options[code].len = hunkix;
	return code;
}

void
parse_reject_statement(FILE *cfile)
{
	struct in_addr		 addr;
	struct reject_elem	*elem;
	int			 token;

	do {
		if (parse_ip_addr(cfile, &addr) == 0)
			return;

		elem = malloc(sizeof(*elem));
		if (elem == NULL)
			fatal("reject address");

		elem->addr = addr;
		TAILQ_INSERT_TAIL(&config->reject_list, elem, next);

		token = peek_token(NULL, cfile);
		if (token == ',')
			token = next_token(NULL, cfile);
	} while (token == ',');

	parse_semi(cfile);
}
