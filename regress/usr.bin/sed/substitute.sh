#!/bin/sh
#
#	$OpenBSD: substitute.sh,v 1.3 2011/09/17 09:20:28 schwarze Exp $
#
# Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
#
# Permission to use, copy, modify, and distribute this test suite for any
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

# Test suite for the sed(1) -E substitute command, checking
# the handling of multiple zero-length matches in particular.

# error counter
err=0

# test function for one specific flag; arguments are:
# input string
# regular expression to replace
# substitution flag
# wanted output
tf() {
	in=$1
	patt="s/$2/x/$3"
	want=$4
	hexwant=`echo $want | hexdump -C`
	hexout=`echo "$in" | sed -E "$patt" | hexdump -C`
	if [ "X$hexout" != "X$hexwant" ]; then
		echo "patt: $patt input: \"$in\\\\n\""
		echo "want:" $hexwant
		echo "got: " $hexout
	fi
	[ -z "$in" ] && want=""
	hexwant=`echo -n $want | hexdump -C`
	hexout=`echo -n "$in" | sed -E "$patt" | hexdump -C`
	if [ "X$hexout" != "X$hexwant" ]; then
		echo "patt: $patt input: \"$in\\\\0\""
		echo "want:" $hexwant
		echo "got: " $hexout
	fi
}

# test function for various flags; arguments are:
# input string
# regular expression to replace
# wanted output for /g (global substitution)
# wanted output for /1 (substitution of first match) and so on
t() {
	# global substitution
	in=$1
	expr=$2
	want=$3
	shift 3
	tf "$in" "$expr" g "$want"

	# substitution with specific index
	num=1
	while [ $# -gt 0 ]; do
		want=$1
		shift
		tf "$in" "$expr" "$num" "$want"
		num=$((num+1))
	done

	# substitution with excessive index
	tf "$in" "$expr" "$num" "$in"
}

t '' ^ x x
t '' '()' x x
t '' '$' x x
t '' '^|$' x x
t a ^ xa xa
t a '()' xax xa ax
t a '$' ax ax
t a '\<' xa xa
t a '^|a' x x
t a '^|$' xax xa ax
t a '^|a|$' x x
t a 'a|$' x x
t a '\<|a' x x
t ab ^ xab xab
t ab '()' xaxbx xab axb abx
t ab '$' abx abx
t ab '\<' xab xab
t ab '^|a' xb xb
t ab '^|b' xax xab ax
t ab '^|$' xabx xab abx
t ab '^|a|$' xbx xb abx
t ab '^|b|$' xax xab ax
t ab '^|a|b|$' xx xb ax
t ab '^|ab|$' x x
t ab 'a|()' xbx xb abx
t ab 'a|$' xbx xb abx
t ab 'ab|$' x x
t ab 'b|()' xax xab ax
t ab 'b|$' ax ax
t ab '\<|a' xb xb
t ab '\<|b' xax xab ax
t abc '^|b' xaxc xabc axc
t abc '^|b|$' xaxcx xabc axc abcx
t abc '^|bc|$' xax xabc ax
t abc 'ab|()' xcx xc abcx
t abc 'ab|$' xcx xc abcx
t abc 'b|()' xaxcx xabc axc abcx
t abc 'bc|()' xax xabc ax
t abc 'b|$' axcx axc abcx
t aa a xx xa ax
t aa 'a|()' xx xa ax
t aa 'a*' x x
t a:a: '\<' xa:xa: xa:a: a:xa:
t a:a: '\<..' xx xa: a:x

exit $err
