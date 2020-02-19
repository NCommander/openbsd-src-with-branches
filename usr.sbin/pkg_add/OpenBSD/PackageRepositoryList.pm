# ex:ts=8 sw=4:
# $OpenBSD: PackageRepositoryList.pm,v 1.31 2017/05/29 12:28:54 espie Exp $
#
# Copyright (c) 2003-2006 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackageRepositoryList;

sub new
{
	my ($class, $state) = @_;
	return bless {l => [], k => {}, state => $state}, $class;
}

sub filter_new
{
	my $self = shift;
	my @l = ();
	for my $r (@_) {
		next if !defined $r;
		next if $self->{k}{$r};
		$self->{k}{$r} = 1;
		push @l, $r;
	}
	return @l;
}

sub add
{
	my $self = shift;
	push @{$self->{l}}, $self->filter_new(@_);
}

sub prepend
{
	my $self = shift;
	unshift @{$self->{l}}, $self->filter_new(@_);
}

sub do_something
{
	my ($self, $do, $pkgname, @args) = @_;
	if (defined $pkgname && $pkgname eq '-') {
		return OpenBSD::PackageRepository->pipe->new($self->{state})->$do($pkgname, @args);
	}
	for my $repo (@{$self->{l}}) {
		my $r = $repo->$do($pkgname, @args);
		return $r if defined $r;
	}
	return undef;
}

sub find
{
	my ($self, @args) = @_;

	return $self->do_something('find', @args);
}

sub grabPlist
{
	my ($self, @args) = @_;
	return $self->do_something('grabPlist', @args);
}

sub match_locations
{
	my ($self, @search) = @_;
	my $result = [];
	for my $repo (@{$self->{l}}) {
		my $l = $repo->match_locations(@search);
		if ($search[0]->{keep_all}) {
			push(@$result, @$l);
		} elsif (@$l > 0) {
			return $l;
		}
	}
	return $result;
}

1;
