# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.6 2004/11/01 19:14:26 espie Exp $
#
# Copyright (c) 2004 Marc Espie <espie@openbsd.org>
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
#

use strict;
use warnings;

use OpenBSD::Delete;

package OpenBSD::PackingElement;
sub can_update
{
	my ($self, $okay) = @_;

	$$okay = $self->updatable();
}

sub validate_depend
{
}
sub updatable() { 1 }

sub extract
{
}

package OpenBSD::PackingElement::FileBase;
use File::Temp qw/tempfile/;

sub extract
{
	my ($self, $state) = @_;

	my $file = $self->prepare_to_extract($state);

	if (defined $self->{link} || defined $self->{symlink}) {
		$self->{tempname} = 1;
		return;
	}
	my ($fh, $tempname) = tempfile(DIR => dirname($file->{destdir}.$file->{name}));

	print "extracting $tempname\n" if $state->{very_verbose};
	$file->{name} = $tempname;
	$file->create();
	$self->{tempname} = $tempname;
}

package OpenBSD::PackingElement::Dir;
sub extract
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname();
	my $destdir = $state->{destdir};

	print "new directory ", $destdir, $fullname, "\n" if $state->{very_verbose};
	return if $state->{not};
	File::Path::mkpath($destdir.$fullname);
}

package OpenBSD::PackingElement::Sample;
sub extract
{
}

package OpenBSD::PackingElement::Sampledir;
sub extract
{
}

package OpenBSD::PackingElement::ScriptFile;
sub updatable() { 0 }

package OpenBSD::PackingElement::ExeclikeAction;
sub updatable() { 0 }

package OpenBSD::PackingElement::LibDepend;
sub validate_depend
{
	my ($self, $okay, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	if (!OpenBSD::PkgSpec::match($self->{pattern}, $replacement)) {
		$$okay = 0;
	}
}

package OpenBSD::PackingElement::NewDepend;
sub validate_depend
{
	my ($self, $okay, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	if (!OpenBSD::PkgSpec::match($self->{pattern}, $replacement)) {
		$$okay = 0;
	}
}

package OpenBSD::Update;
use OpenBSD::RequiredBy;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;

sub can_do
{
	my ($toreplace, $replacement, $state) = @_;

	my $wantlist = [];
	my $r = OpenBSD::RequiredBy->new($toreplace);
	my $okay = 1;
	if (-f $$r) {
		$wantlist = $r->list();
		my $done_wanted = {};
		for my $wanting (@$wantlist) {
			next if defined $done_wanted->{$wanting};
			$done_wanted->{$wanting} = 1;
			print "Verifying dependencies still match for $wanting\n";
			my $p2 = OpenBSD::PackingList->fromfile(installed_info($wanting).CONTENTS,
			    \&OpenBSD::PackingList::DependOnly);
			$p2->visit('validate_depend', \$okay, $wanting, $toreplace, $replacement);
		}
	}

	my $plist = OpenBSD::PackingList->fromfile(installed_info($toreplace).CONTENTS);
	$plist->visit('can_update', \$okay);
	eval {
		OpenBSD::Delete::validate_plist($plist, $state->{destdir});
	};
	if ($@) {
		return 0;
	}

	$plist->{wantlist} = $wantlist;
	
	return $okay ? $plist : $okay;
}

sub adjust_dependency
{
	my ($dep, $from, $into) = @_;

	my $contents = installed_info($dep).CONTENTS;
	my $plist = OpenBSD::PackingList->fromfile($contents);
	my $items = [];
	for my $item (@{$plist->{pkgdep}}) {
		next if $item->{'name'} eq $from;
		push(@$items, $item);
	}
	$plist->{pkgdep} = $items;
	OpenBSD::PackingElement::PkgDep->add($plist, $into);
	$plist->tofile($contents);
}
1;
