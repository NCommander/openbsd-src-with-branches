# $OpenBSD: Trace.pm,v 1.3 2012/07/06 11:30:41 espie Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
# Copyright (c) 2012 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;
use feature qw(say state);

package LT::Trace;
use Exporter 'import';
our @EXPORT = qw(tprint tsay);

sub print(&)
{
	my $val = shift;
	if (defined $ENV{TRACE_LIBTOOL}) {
		state $trace_file;
		if (!defined $trace_file) {
			open $trace_file, '>>', $ENV{TRACE_LIBTOOL};
		}
		if (defined $trace_file) {
			print $trace_file (&$val);
		}
	}
}

my $trace_level = 0;

sub set
{
	my $class = shift;
	$trace_level = shift;
}

sub tprint(&;$)
{
	my ($args, $level) = @_;

	$level = 1 if !defined $level;

	if ($trace_level >= $level) {
		print (&$args);
	}
}

sub tsay(&;$)
{
	my ($args, $level) = @_;

	$level = 1 if !defined $level;

	if ($trace_level >= $level) {
		say (&$args);
	}
}

1;
