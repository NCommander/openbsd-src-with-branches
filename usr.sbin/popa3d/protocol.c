/* $OpenBSD: protocol.c,v 1.2 2001/08/13 20:19:33 camield Exp $ */

/*
 * POP protocol handling.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <stdarg.h>

#include "misc.h"
#include "params.h"
#include "protocol.h"

struct pop_buffer pop_buffer;
static jmp_buf pop_timed_out;

void pop_init(void)
{
	pop_buffer.ptr = pop_buffer.size = 0;
}

void pop_clean(void)
{
	memset(pop_buffer.data, 0, pop_buffer.ptr);
	memmove(pop_buffer.data, &pop_buffer.data[pop_buffer.ptr],
		pop_buffer.size -= pop_buffer.ptr);
	pop_buffer.ptr = 0;
}

int pop_sane(void)
{
	return (unsigned int)pop_buffer.size <= sizeof(pop_buffer.data) &&
	    (unsigned int)pop_buffer.ptr <= (unsigned int)pop_buffer.size;
}

static void pop_timeout(int signum)
{
	signal(SIGALRM, SIG_DFL);
	longjmp(pop_timed_out, 1);
}

static int pop_fetch(void)
{
	signal(SIGALRM, pop_timeout);
	alarm(POP_TIMEOUT);

	pop_buffer.size = read(0, pop_buffer.data, sizeof(pop_buffer.data));

	alarm(0);
	signal(SIGALRM, SIG_DFL);

	pop_buffer.ptr = 0;
	return pop_buffer.size <= 0;
}

static int pop_get_char(void)
{
	if (pop_buffer.ptr >= pop_buffer.size)
	if (pop_fetch()) return -1;

	return (unsigned char)pop_buffer.data[pop_buffer.ptr++];
}

static char *pop_get_line(char *line, int size)
{
	int pos;
	int seen_cr, seen_nul;
	int c;

	pos = 0;
	seen_cr = seen_nul = 0;
	while ((c = pop_get_char()) >= 0) {
		if (c == '\n') {
			if (seen_cr) line[pos - 1] = 0;
			break;
		}
		if (pos < size - 1)
			seen_cr = ((line[pos++] = c) == '\r');
		else
			seen_cr = 0;
		seen_nul |= !c;
	}
	line[pos] = 0;

	if (seen_nul)
		line[0] = 0;

	if (pos || c >= 0)
		return line;
	else
		return NULL;
}

int pop_handle_state(struct pop_command *commands)
{
	char line[POP_BUFFER_SIZE];
	char *params;
	struct pop_command *command;
	int response;

	if (setjmp(pop_timed_out)) return POP_TIMED_OUT;

	while (pop_get_line(line, sizeof(line))) {
		if ((params = strchr(line, ' '))) {
			*params++ = 0;
			if (!*params) params = NULL;
		}

		response = POP_ERROR;
		for (command = commands; command->name; command++)
		if (!strcasecmp(command->name, line)) {
			response = command->handler(params);
			break;
		}

		switch (response) {
		case POP_OK:
			if (pop_reply_ok()) return POP_CRASH;
			break;

		case POP_ERROR:
			if (pop_reply_error()) return POP_CRASH;

		case POP_QUIET:
			break;

		case POP_LEAVE:
			if (pop_reply_ok()) return POP_CRASH;

		default:
			return response;
		}
	}

	return POP_CRASH;
}

char *pop_get_param(char **params)
{
	char *current, *next;

	if ((current = *params)) {
		if ((next = strchr(current, ' '))) {
			*next++ = 0;
			*params = *next ? next : NULL;
		} else
			*params = NULL;

		if (strlen(current) > 40) current = NULL;
	}

	return current;
}

int pop_get_int(char **params)
{
	char *param, *error;
	long value;

	if ((param = pop_get_param(params))) {
		value = strtol(param, &error, 10);
		if (!*param || *error || (value & ~0x3FFFFFFFL)) return -1;

		return value;
	}

	return -1;
}

int pop_reply(char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);

	putc('\r', stdout);
	putc('\n', stdout);

	switch (format[0]) {
	case '+':
	case '-':
		return fflush(stdout);

	case '.':
		if (!format[1]) return fflush(stdout);
	}

	return ferror(stdout);
}

int pop_reply_ok(void)
{
	return pop_reply("+OK");
}

int pop_reply_error(void)
{
	return pop_reply("-ERR");
}

int pop_reply_multiline(int fd, long size, int lines)
{
	char *in_buffer;
	char *out_buffer;
	char *in, *out;
	int in_block, out_block;
	int start, body;

	if (lines >= 0) lines++;

	if (pop_reply_ok()) return 1;

	in_buffer = malloc(RETR_BUFFER_SIZE * 3);
	if (!in_buffer) return 1;
	out_buffer = &in_buffer[RETR_BUFFER_SIZE];

	start = 1;
	body = 0;
	while (size && lines) {
		if (size > RETR_BUFFER_SIZE)
			in_block = read(fd, in_buffer, RETR_BUFFER_SIZE);
		else
			in_block = read(fd, in_buffer, size);
		if (in_block <= 0) {
			free(in_buffer);
			return 1;
		}

		in = in_buffer;
		out = out_buffer;
		while (in < &in_buffer[in_block] && lines)
		switch (*in) {
		case '\n':
			*out++ = '\r';
			*out++ = *in++;
			if (start) body = 1;
			if (body) lines--;
			start = 1;
			break;

		case '.':
			if (start) *out++ = '.';

		default:
			*out++ = *in++;
			start = 0;
		}

		out_block = out - out_buffer;
		if (write_loop(1, out_buffer, out_block) != out_block) {
			free(in_buffer);
			return 1;
		}

		size -= in_block;
	}

	free(in_buffer);

	if (!start)
	if (pop_reply("%s", "")) return 1;

	return 0;
}

int pop_reply_terminate(void)
{
	return pop_reply(".");
}
