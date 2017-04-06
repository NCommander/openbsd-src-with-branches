# ex:ts=8 sw=4:
# $OpenBSD: PackageLocator.pm,v 1.107 2017/03/01 10:35:24 espie Exp $
#
# Copyright (c) 2003-2010 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackageLocator;

use OpenBSD::PackageRepositoryList;
use OpenBSD::PackageRepository;

my $default_path;

sub build_default_path
{
	my ($self, $state) = @_;
	$default_path = OpenBSD::PackageRepositoryList->new($state);

	my $path_set = 0;

	if (defined $ENV{TRUSTED_PKG_PATH}) {
		my $v = $ENV{TRUSTED_PKG_PATH};
		$v =~ s/^\:+//o;
		$v =~ s/\:+$//o;
		while (my $o = OpenBSD::PackageRepository->parse(\$v, $state)) {
			$o->{trusted} = 1;
			$default_path->add($o);
		}
		$path_set = 1;
	}
	if (defined $ENV{PKG_PATH}) {
		my $v = $ENV{PKG_PATH};
		$v =~ s/^\:+//o;
		$v =~ s/\:+$//o;
		while (my $o = OpenBSD::PackageRepository->parse(\$v, $state)) {
			$default_path->add($o);
		}
		$path_set = 1;
	}
	return if $path_set;
	$default_path->add(OpenBSD::PackageRepository->new("./", $state)->can_be_empty);
	return if $state->defines('NOINSTALLPATH');

	if (defined(my $i = $state->installpath)) {
		while (my $o = OpenBSD::PackageRepository->parse(\$i, $state)) {
			$default_path->add($o);
		}
	}
}

sub default_path
{
	if (!defined $default_path) {
		&build_default_path;
	}
	return $default_path;
}

sub path_parse
{
	my ($self, $pkgname, $state, $path) = (@_, './');
	if ($pkgname =~ m/^(.*[\/\:])(.*)/) {
		($pkgname, $path) = ($2, $1);
	}

	return (OpenBSD::PackageRepository->new($path, $state), $pkgname);
}

sub find
{
	my ($self, $url, $state) = @_;

	my $package;
	if ($url =~ m/[\/\:]/o) {
		my ($repository, $pkgname) = $self->path_parse($url, $state);
		$package = $repository->find($pkgname);
		if (defined $package) {
			$self->default_path($state)->add($repository);
		}
	} else {
		$package = $self->default_path($state)->find($url);
	}
	return $package;
}

sub grabPlist
{
	my ($self, $url, $code, $state) = @_;

	my $plist;
	if ($url =~ m/[\/\:]/o) {
		my ($repository, $pkgname) = $self->path_parse($url, $state);
		$plist = $repository->grabPlist($pkgname, $code);
		if (defined $plist) {
			$self->default_path($state)->add($repository);
		}
	} else {
		$plist = $self->default_path($state)->grabPlist($url, $code);
	}
	return $plist;
}

sub match_locations
{
	my ($self, @search) = @_;
	my $state = pop @search;
	return $self->default_path($state)->match_locations(@search);
}

1;
