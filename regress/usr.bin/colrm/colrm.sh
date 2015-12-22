#!/bin/sh
#
# Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
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

unset LC_ALL

test_colrm()
{
	args=$1
	stdin=$2
	expected=`echo -n "$3."`
	export LC_CTYPE=en_US.UTF-8
	result=`echo -n "$stdin" | colrm $args ; echo -n .`
	if [ "$result" != "${expected}" ]; then
		echo "echo -n \"$stdin\" | colrm $args"
		echo "expected: \"$expected\""
		echo "result: \"$result\""
		exit 1;
	fi

	if [ -n "$4" ]; then
		expected=`echo -n "$4."`
	fi
	export LC_CTYPE=C
	result=`echo -n "$stdin" | colrm $args ; echo -n .`
	if [ "$result" != "${expected}" ]; then
		echo "[C] echo -n \"$stdin\" | colrm $args"
		echo "expected: \"$expected\""
		echo "result: \"$result\""
		exit 1;
	fi
}

# single byte characters
test_colrm "" "abcd" "abcd"
test_colrm "2" "abcd" "a"
test_colrm "5" "abcd" "abcd"
test_colrm "2 3" "abcd" "ad"
test_colrm "5 6" "abcd" "abcd"

# tab characters
test_colrm "" "a\tb" "a\tb"
test_colrm "10" "\tab" "\ta"
test_colrm "9" "\tab" "\t"
test_colrm "8" "\tab" "       "
test_colrm "3 7" "a\tb" "a  b"
test_colrm "7 9" "abcd\txe" "abcd  e"
test_colrm "3 6" "abcd\tef" "ab  ef"

# zero width
test_colrm "2 2" "ax̀b" "ab" "àb"
test_colrm "3 3" "ax̀bx̀c" "ax̀x̀c" \
		 "ax�bx̀c"

# double width
test_colrm "2 3" "a쿿b" "ab" "a�b"
test_colrm "2 2" "a쿿b" "a b" "a��b"
test_colrm "3 3" "a쿿b" "a b" "a�b"
test_colrm "4 4" "a쿿b쿿c" "a쿿쿿c" \
		 "a�b쿿c"

# backspaces
test_colrm "3 3" "ab_cd_e" "ab_d_e"
test_colrm "2 2" "ab_c" "ac"
test_colrm "2 2" "ax̀b" "ab" "àb"
test_colrm "3 3" "ax̀bx̀c" "ax̀x̀c" \
		 "ax�bx̀c"
test_colrm "2 3" "a쿿bcde" "ade" "a�bcde"
test_colrm "2 2" "a쿿bcde" "acde" "a��bcde"
test_colrm "3 3" "a쿿bcde" "abde" "a�bcde"
test_colrm "4 4" "a쿿b쿿c" "a쿿쿿c" \
		 "a�b쿿c"
test_colrm "" "\bx" "\bx"
test_colrm "1" "\bx" "\b"

# invalid bytes and non-printable characters
test_colrm "3 3" "a�b�c" "a��c"
test_colrm "2 2" "a�b" "ab"
test_colrm "3 3" "abc" "ac"
test_colrm "2 2" "ab" "ab"
test_colrm "3 3" "a͸b͸c" "a͸͸c" "a�b͸c"
test_colrm "2 2" "a͸b" "ab" "a�b"

# edge cases
test_colrm "" "" ""
test_colrm "2 2" "\n" "\n"

exit 0
