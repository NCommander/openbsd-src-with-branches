/*	$OpenBSD: ksyms.c,v 1.2 2013/08/29 20:22:14 naddy Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <sys/exec.h>
#ifdef _NLIST_DO_ELF
#include <elf_abi.h>
#endif

#include "hangman.h"

int	ksyms_elf_parse();

void
kgetword()
{
	uint tries;
	off_t pos;
	int buflen;
	char symbuf[1 + BUFSIZ], *sym, *end;
	size_t symlen;

	for (tries = 0; tries < MAXBADWORDS; tries++) {
		pos = arc4random_uniform(ksymsize);
		if (lseek(ksyms, pos + ksymoffs, SEEK_SET) == -1)
			continue;
		buflen = read(ksyms, symbuf, BUFSIZ);
		if (buflen < 0)
			continue;

		/*
		 * The buffer is hopefully large enough to hold at least
		 * a complete symbol, i.e. two occurences of NUL, or
		 * one occurence of NUL and the buffer containing the end
		 * of the string table. We make sure the buffer will be
		 * NUL terminated in all cases.
		 */
		if (buflen + pos >= ksymsize)
			buflen = ksymsize - pos;
		*(end = symbuf + buflen) = '\0';

		for (sym = symbuf; *sym != '\0'; sym++) ;
		if (sym == end)
			continue;

		symlen = strlen(++sym);
		if (symlen < MINLEN || symlen > MAXLEN)
			continue;

		break;
	}

	if (tries >= MAXBADWORDS) {
		mvcur(0, COLS - 1, LINES -1, 0);
		endwin();
		errx(1, "can't seem a suitable kernel symbol in %s",
		    Dict_name);
	}

	strlcpy(Word, sym, sizeof Word);
	strlcpy(Known, sym, sizeof Known);
	for (sym = Known; *sym != '\0'; sym++) {
		if (*sym == '-')
			*sym = '_';	/* try not to confuse player */
		if (isalpha(*sym))
			*sym = '-';
	}
}

int
ksetup()
{
	if ((ksyms = open(Dict_name, O_RDONLY)) < 0)
		return ksyms;

	/*
	 * Relaxed header check - /dev/ksyms is either a native a.out
	 * binary or a native ELF binary.
	 */

#ifdef _NLIST_DO_ELF
	if (ksyms_elf_parse() == 0)
		return 0;
#endif

	close(ksyms);
	errno = ENOEXEC;
	return -1;
}

#ifdef _NLIST_DO_ELF
int
ksyms_elf_parse()
{
	Elf_Ehdr eh;
	Elf_Shdr sh;
	uint s;

	if (lseek(ksyms, 0, SEEK_SET) == -1)
		return -1;

	if (read(ksyms, &eh, sizeof eh) != sizeof eh)
		return -1;

	if (!IS_ELF(eh))
		return -1;

	if (lseek(ksyms, eh.e_shoff, SEEK_SET) == -1)
		return -1;

	ksymoffs = 0;
	ksymsize = 0;

	for (s = 0; s < eh.e_shnum; s++) {
		if (read(ksyms, &sh, sizeof sh) != sizeof sh)
			return -1;

		/*
		 * There should be two string table sections, one with the
		 * name of the sections themselves, and one with the symbol
		 * names. Just pick the largest one.
		 */
		if (sh.sh_type == SHT_STRTAB) {
			if (ksymsize > (off_t)sh.sh_size)
				continue;

			ksymoffs = (off_t)sh.sh_offset;
			ksymsize = (off_t)sh.sh_size;
		}
	}

	if (ksymsize == 0)
		return -1;

	return 0;
}
#endif
