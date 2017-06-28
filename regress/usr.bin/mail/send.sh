#!/bin/sh
#
# Copyright (c) 2017 Anton Lindqvist <anton@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

set -e

testseq() {
	stdin=$1
	exp=$(echo "$2")
	act=$(echo -n "$stdin" | ./edit mail -En unknown)
	[ "$exp" = "$act" ] && return 0

	echo input:
	echo ">>>${stdin}<<<"
	echo -n "$stdin" | hexdump -C
	echo expected:
	echo ">>>${exp}<<<"
	echo -n "$exp" | hexdump -C
	echo actual:
	echo ">>>${act}<<<"
	echo -n "$act" | hexdump -C

	return 1
}

MALLOC_OPTIONS=S
export MALLOC_OPTIONS

# NL: New line.
testseq "\n" "Subject: \r\n"

# VERASE: Delete character.
testseq "\0177" "Subject: "
testseq "a\0177" "Subject: a\b \b"

# VINTR: Kill letter.
testseq "\0003\0003" \
	"Subject: ^C\r\n(Interrupt -- one more to kill letter)\r\nSubject: "

# VKILL: Kill line.
testseq "\0025" "Subject: "
testseq "ab\0025" "Subject: ab\b\b  \b\b"

# VWERASE: Delete word.
testseq "\0027" "Subject: "
testseq "ab\0027" "Subject: ab\b\b  \b\b"
testseq "ab cd\0027\0027" "Subject: ab cd\b\b  \b\b\b\b\b   \b\b\b"
