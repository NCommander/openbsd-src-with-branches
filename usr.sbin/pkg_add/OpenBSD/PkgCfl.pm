# ex:ts=8 sw=4:
# $OpenBSD: PkgCfl.pm,v 1.3 2004/09/14 22:49:36 espie Exp $
#
# Copyright (c) 2003-2004 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PkgCfl;
use OpenBSD::PackageName;

sub glob2re
{
	local $_ = shift;
	s/\./\\\./g;
	s/\+/\\\+/g;
	s/\*/\.\*/g;
	s/\?/\./g;
	return "^$_\$";
}

sub make_conflict_list($)
{
	my ($class, $plist) = @_;
	my $l = [];

	unless (defined $plist->{'no-default-conflict'}) {
		my $stem = (OpenBSD::PackageName::splitname $plist->pkgname())[0];
		push(@$l, sub { grep { m/^\Q$stem\E-\d.*$/ } @_; });
	}
	if (defined $plist->{pkgcfl}) {
		for my $cfl (@{$plist->{pkgcfl}}) {
			my $re = glob2re($cfl->{name});
			push(@$l, sub { grep { m/$re/ } @_; });
		}
	}
	if (defined $plist->{conflict}) {
		for my $cfl (@{$plist->{conflict}}) {
		    push(@$l, sub { OpenBSD::PackageName::pkgspec_match($cfl->{name}, @_); });
		}
	}
	bless $l, $class;
}

sub conflicts_with
{
	my ($self, @pkgnames) = @_;
	for my $cfl (@$self) {
		my @l = &$cfl(@pkgnames);
		if (@l) {
			return @l;
		}
	}
	return 0;
}

1;
