# ex:ts=8 sw=4:
# $OpenBSD: PackageRepository.pm,v 1.30 2007/05/14 10:00:08 espie Exp $
#
# Copyright (c) 2007 Marc Espie <espie@openbsd.org>
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

# XXX: we want to be able to load PackageRepository::Installed stand-alone,
# so we put the only common method into PackageRepositoryBase.
#
# later, when we load the base PackageRepository, we tweak the inheritance
# of PackageRepository::Installed to have full access...

package OpenBSD::PackageRepositoryBase;

sub match
{
	my ($self, $search, $filter) = @_;
	if (defined $filter) {
		return &$filter($search->match_repo($self));
	} else {
		return $search->match_repo($self);
	}
}

package OpenBSD::PackageRepository::Installed;

our @ISA = (qw(OpenBSD::PackageRepositoryBase));

use OpenBSD::PackageInfo;

my $singleton = bless {}, __PACKAGE__;

sub new
{
	return $singleton;
}

sub find
{
	my ($repository, $name, $arch, $srcpath) = @_;
	my $self;

	if (is_installed($name)) {
		$self = OpenBSD::PackageLocation->new($repository, $name);
		$self->{dir} = installed_info($name);
	}
	return $self;
}

sub grabPlist
{
	my ($repository, $name, $arch, $code) = @_;
	require OpenBSD::PackingList;
	return  OpenBSD::PackingList->from_installation($name, $code);
}

sub available
{
	return installed_packages();
}

sub list
{
	my @list = installed_packages();
	return \@list;
}

sub stemlist
{
	return installed_stems();
}

sub wipe_info
{
}

sub may_exist
{
	my ($self, $name) = @_;
	return is_installed($name);
}

1;
