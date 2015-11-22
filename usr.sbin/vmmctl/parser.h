/*	$OpenBSD$	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/queue.h>
#include <imsg.h>

#ifndef VMMCTL_PARSER_H
#define VMMCTL_PARSER_H

#define VMM_CONF	"/etc/vmm.conf"

enum actions {
	NONE,
	CMD_CREATE,
	CMD_ENABLE,
	CMD_DISABLE,
	CMD_START,
	CMD_TERMINATE,
	CMD_INFO,
	CMD_LOAD
};

struct parse_result {
	enum actions	 action;
	uint32_t	 id;
	char		*name;
	char		*path;
	size_t		 size;
	size_t		 nifs;
	size_t		 ndisks;
	char		**disks;
	int		 disable;
};

struct imsgbuf	*ibuf;

/* main.c */
int	 vmmaction(struct parse_result *);
int	 parse_ifs(struct parse_result *, char *, int);
int	 parse_size(struct parse_result *, char *, long long);
int	 parse_disk(struct parse_result *, char *);
int	 parse_vmid(struct parse_result *, char *, uint32_t);

/* parser.c */
struct parse_result *
	 parse(int, char *[]);

/* parse.y */
int	 parse_config(const char *);
int	 cmdline_symset(char *);

/* vmmctl.c */
int	 create_imagefile(char *, long);
int	 start_vm(const char *, int, int, int, char **, char *);
int	 start_vm_complete(struct imsg *, int *);
void	 enable_vmm(void);
int	 enable_vmm_complete(struct imsg *, int *);
void	 disable_vmm(void);
int	 disable_vmm_complete(struct imsg *, int *);
void	 terminate_vm(uint32_t);
int	 terminate_vm_complete(struct imsg *, int *);
void	 get_info_vm(uint32_t);
int	 add_info(struct imsg *, int *);
void	 print_vm_info(struct vm_info_result *, size_t);

#endif /* VMMCTL_PARSER_H */
