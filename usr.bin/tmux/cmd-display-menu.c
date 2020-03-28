/* $OpenBSD: cmd-display-menu.c,v 1.8 2020/03/28 09:39:44 nicm Exp $ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Display a menu on a client.
 */

static enum cmd_retval	cmd_display_menu_exec(struct cmd *,
			    struct cmdq_item *);
static enum cmd_retval	cmd_display_popup_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_display_menu_entry = {
	.name = "display-menu",
	.alias = "menu",

	.args = { "c:t:T:x:y:", 1, -1 },
	.usage = "[-c target-client] " CMD_TARGET_PANE_USAGE " [-T title] "
		 "[-x position] [-y position] name key command ...",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_display_menu_exec
};

const struct cmd_entry cmd_display_popup_entry = {
	.name = "display-popup",
	.alias = "popup",

	.args = { "CEKc:d:h:R:t:w:x:y:", 0, -1 },
	.usage = "[-CEK] [-c target-client] [-d start-directory] [-h height] "
	         "[-R shell-command] " CMD_TARGET_PANE_USAGE " [-w width] "
	         "[-x position] [-y position] [command line ...]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_display_popup_exec
};

static void
cmd_display_menu_get_position(struct client *c, struct cmdq_item *item,
    struct args *args, u_int *px, u_int *py, u_int w, u_int h)
{
	struct winlink		*wl = item->target.wl;
	struct window_pane	*wp = item->target.wp;
	struct style_range	*sr;
	const char		*xp, *yp;
	int			 at = status_at_line(c);
	u_int			 ox, oy, sx, sy;

	xp = args_get(args, 'x');
	if (xp == NULL)
		*px = 0;
	else if (strcmp(xp, "R") == 0)
		*px = c->tty.sx - 1;
	else if (strcmp(xp, "C") == 0)
		*px = (c->tty.sx - 1) / 2 - w / 2;
	else if (strcmp(xp, "P") == 0) {
		tty_window_offset(&c->tty, &ox, &oy, &sx, &sy);
		if (wp->xoff >= ox)
			*px = wp->xoff - ox;
		else
			*px = 0;
	} else if (strcmp(xp, "M") == 0 && item->shared->mouse.valid) {
		if (item->shared->mouse.x > w / 2)
			*px = item->shared->mouse.x - w / 2;
		else
			*px = 0;
	} else if (strcmp(xp, "W") == 0) {
		if (at == -1)
			*px = 0;
		else {
			TAILQ_FOREACH(sr, &c->status.entries[0].ranges, entry) {
				if (sr->type != STYLE_RANGE_WINDOW)
					continue;
				if (sr->argument == (u_int)wl->idx)
					break;
			}
			if (sr != NULL)
				*px = sr->start;
			else
				*px = 0;
		}
	} else
		*px = strtoul(xp, NULL, 10);
	if ((*px) + w >= c->tty.sx)
		*px = c->tty.sx - w;

	yp = args_get(args, 'y');
	if (yp == NULL)
		*py = 0;
	else if (strcmp(yp, "C") == 0)
		*py = (c->tty.sy - 1) / 2 + h / 2;
	else if (strcmp(yp, "P") == 0) {
		tty_window_offset(&c->tty, &ox, &oy, &sx, &sy);
		if (wp->yoff + wp->sy >= oy)
			*py = wp->yoff + wp->sy - oy;
		else
			*py = 0;
	} else if (strcmp(yp, "M") == 0 && item->shared->mouse.valid)
		*py = item->shared->mouse.y + h;
	else if (strcmp(yp, "S") == 0) {
		if (at == -1)
			*py = c->tty.sy;
		else if (at == 0)
			*py = status_line_size(c) + h;
		else
			*py = at;
	} else
		*py = strtoul(yp, NULL, 10);
	if (*py < h)
		*py = 0;
	else
		*py -= h;
	if ((*py) + h >= c->tty.sy)
		*py = c->tty.sy - h;
}

static enum cmd_retval
cmd_display_menu_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct client		*c;
	struct session		*s = item->target.s;
	struct winlink		*wl = item->target.wl;
	struct window_pane	*wp = item->target.wp;
	struct cmd_find_state	*fs = &item->target;
	struct menu		*menu = NULL;
	struct menu_item	 menu_item;
	const char		*key;
	char			*title, *name;
	int			 flags = 0, i;
	u_int			 px, py;

	if ((c = cmd_find_client(item, args_get(args, 'c'), 0)) == NULL)
		return (CMD_RETURN_ERROR);
	if (c->overlay_draw != NULL)
		return (CMD_RETURN_NORMAL);

	if (args_has(args, 'T'))
		title = format_single(NULL, args_get(args, 'T'), c, s, wl, wp);
	else
		title = xstrdup("");

	menu = menu_create(title);

	for (i = 0; i != args->argc; /* nothing */) {
		name = args->argv[i++];
		if (*name == '\0') {
			menu_add_item(menu, NULL, item, c, fs);
			continue;
		}

		if (args->argc - i < 2) {
			cmdq_error(item, "not enough arguments");
			free(title);
			menu_free(menu);
			return (CMD_RETURN_ERROR);
		}
		key = args->argv[i++];

		menu_item.name = name;
		menu_item.key = key_string_lookup_string(key);
		menu_item.command = args->argv[i++];

		menu_add_item(menu, &menu_item, item, c, fs);
	}
	free(title);
	if (menu == NULL) {
		cmdq_error(item, "invalid menu arguments");
		return (CMD_RETURN_ERROR);
	}
	if (menu->count == 0) {
		menu_free(menu);
		return (CMD_RETURN_NORMAL);
	}
	cmd_display_menu_get_position(c, item, args, &px, &py, menu->width + 4,
	    menu->count + 2);

	if (!item->shared->mouse.valid)
		flags |= MENU_NOMOUSE;
	if (menu_display(menu, flags, item, px, py, c, fs, NULL, NULL) != 0)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static enum cmd_retval
cmd_display_popup_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct client		*c;
	struct cmd_find_state	*fs = &item->target;
	const char		*value, *cmd = NULL, **lines = NULL;
	const char		*shellcmd = NULL;
	char			*cwd, *cause;
	int			 flags = 0;
	u_int			 px, py, w, h, nlines = 0;

	if ((c = cmd_find_client(item, args_get(args, 'c'), 0)) == NULL)
		return (CMD_RETURN_ERROR);
	if (args_has(args, 'C')) {
		server_client_clear_overlay(c);
		return (CMD_RETURN_NORMAL);
	}
	if (c->overlay_draw != NULL)
		return (CMD_RETURN_NORMAL);

	if (args->argc >= 1)
		cmd = args->argv[0];
	if (args->argc >= 2) {
		lines = (const char **)args->argv + 1;
		nlines = args->argc - 1;
	}

	if (nlines != 0)
		h = popup_height(nlines, lines) + 2;
	else
		h = c->tty.sy / 2;
	if (args_has(args, 'h')) {
		h = args_percentage(args, 'h', 1, c->tty.sy, c->tty.sy, &cause);
		if (cause != NULL) {
			cmdq_error(item, "height %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	if (nlines != 0)
		w = popup_width(item, nlines, lines, c, fs) + 2;
	else
		w = c->tty.sx / 2;
	if (args_has(args, 'w')) {
		w = args_percentage(args, 'w', 1, c->tty.sx, c->tty.sx, &cause);
		if (cause != NULL) {
			cmdq_error(item, "width %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	if (w > c->tty.sx - 1)
		w = c->tty.sx - 1;
	if (h > c->tty.sy - 1)
		h = c->tty.sy - 1;
	cmd_display_menu_get_position(c, item, args, &px, &py, w, h);

	value = args_get(args, 'd');
	if (value != NULL)
		cwd = format_single(NULL, value, c, fs->s, fs->wl, fs->wp);
	else
		cwd = xstrdup(server_client_get_cwd(c, fs->s));

	value = args_get(args, 'R');
	if (value != NULL)
		shellcmd = format_single(NULL, value, c, fs->s, fs->wl, fs->wp);

	if (args_has(args, 'K'))
		flags |= POPUP_WRITEKEYS;
	if (args_has(args, 'E') > 1)
		flags |= POPUP_CLOSEEXITZERO;
	else if (args_has(args, 'E'))
		flags |= POPUP_CLOSEEXIT;
	if (popup_display(flags, item, px, py, w, h, nlines, lines, shellcmd,
	    cmd, cwd, c, fs) != 0)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}
