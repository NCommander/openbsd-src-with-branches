# ex:ts=8 sw=4:
# $OpenBSD: PackageRepositoryList.pm,v 1.8 2007/05/13 13:12:21 espie Exp $
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
	my $class = shift;
	return bless {list => [], avail => undef }, $class;
}

sub add
{
	my $self = shift;
	push @{$self->{list}}, @_;
	if (@_ > 0) {
		$self->{avail} = undef;
	}
}

sub find
{
	my ($self, $pkgname, $arch, $srcpath) = @_;

	for my $repo (@{$self->{list}}) {
		my $pkg;

		for (my $retry = 5; $retry < 60; $retry *= 2) {
			undef $repo->{lasterror};
			$pkg = $repo->find($pkgname, $arch, $srcpath);
			if (!defined $pkg && defined $repo->{lasterror} && 
			    $repo->{lasterror} == 421 && 
			    defined $self->{avail} &&
			    $self->{avail}->{$pkgname} eq $repo) { 
				print STDERR "Temporary error, sleeping $retry seconds\n";
				sleep($retry);
			} else {
				last;
			}
		}
		return $pkg if defined $pkg;
	}
	return;
}

sub grabPlist
{
	my ($self, $pkgname, $arch, $code) = @_;

	for my $repo (@{$self->{list}}) {
		my $plist;

		for (my $retry = 5; $retry < 60; $retry *= 2) {
			undef $repo->{lasterror};
			$plist = $repo->grabPlist($pkgname, $arch, $code);
			if (!defined $plist && defined $repo->{lasterror} && 
			    $repo->{lasterror} == 421 && 
			    defined $self->{avail} &&
			    $self->{avail}->{$pkgname} eq $repo) { 
				print STDERR "Temporary error, sleeping $retry seconds\n";
				sleep($retry);
			} else {
				last;
			}
		}
		return $plist if defined $plist;
	}
	return;
}

sub available
{
	my $self = shift;

	if (!defined $self->{avail}) {
		my $available_packages = {};
		foreach my $loc (reverse @{$self->{list}}) {
		    foreach my $pkg (@{$loc->list()}) {
		    	$available_packages->{$pkg} = $loc;
		    }
		}
		$self->{avail} = $available_packages;
	}
	return keys %{$self->{avail}};
}

sub _first_of
{
	my ($self, $method, $filter, @args) = @_;
	for my $repo (@{$self->{list}}) {
		my @l = $repo->$method(@args);
		if (defined $filter) {
			@l = &$filter(@l);
		}
		if (@l > 0) {
			return @l;
		}
	}
	return ();
}

sub find_partialstem
{
	my ($self, $partial, $filter) = @_;
	return $self->_first_of('find_partialstem', $filter, $partial);
}

sub findstem
{
	my ($self, $stem, $filter) = @_;
	return $self->_first_of('findstem', $filter, $stem);
}

sub match_spec
{
	my ($self, $spec, $filter) = @_;
	return $self->_first_of('match_spec', $filter, $spec);
}

sub match
{
	my ($self, $spec, $filter) = @_;
	return $self->_first_of('match', $filter, $spec);
}

sub cleanup
{
	my $self = shift;
	for my $repo (@{$self->{list}}) {
		$repo->cleanup;
	}
}

1;
