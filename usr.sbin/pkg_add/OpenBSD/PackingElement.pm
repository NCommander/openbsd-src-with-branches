# $OpenBSD: PackingElement.pm,v 1.16 2004/08/03 12:29:45 espie Exp $
#
# Copyright (c) 2003 Marc Espie.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
# PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This is the basic class, which is mostly abstract, except for
# setKeyword and Factory.
# It does provide base methods for stuff under it, though.

use strict;
use warnings;
use OpenBSD::PackageInfo;
package OpenBSD::PackingElement;
use File::Basename;
our %keyword;
our %oldkeyword;

sub Factory
{
	local $_ = shift;
	if (m/^\@(\S+)\s*/) {
		my $cmd = $1;
		my $args = $';

		if (defined $keyword{$cmd}) {
			$keyword{$cmd}->add(@_, $args);
		} elsif (defined $oldkeyword{$cmd}) {
			$oldkeyword{$cmd}->add(@_, $args);
			print STDERR "Warning: obsolete construct: \@$cmd $args\n";
		} else {
		    print STDERR "Unknown element: \@$cmd $args\n";
		    exit(1);
		}
	} else {
			OpenBSD::PackingElement::File->add(@_, $_);
	}
}

sub setKeyword 
{
	my ($class, $k) = @_;
	$keyword{$k} = $class;
}

sub setOldKeyword {
	my ($class, $k) = @_;
	$oldkeyword{$k} = $class;
}

sub category() { 'items' }

sub new
{
	my ($class, $args) = @_;
	if ($args =~ m|/$| and defined $class->dirclass()) {
		bless { name => $` }, $class->dirclass();
	} else {
	    bless { name => $args }, $class;
	}
}

sub clone
{
	my $object = shift;
	# shallow copy
	my %h = %$object;
	bless \%h, ref($object);
}
	

sub dirclass() { undef }

sub destate
{
}

sub add_object
{
	my ($self, $plist) = @_;
	$self->destate($plist->{state});
	$plist->add2list($self);
	return $self;
}

sub add
{
	my ($class, $plist, @args) = @_;

	my $self = $class->new(@args);
	return $self->add_object($plist);
}

sub needs_keyword() { 1 }
	
sub write
{
	my ($self, $fh) = @_;
	if ($self->needs_keyword()) {
		print $fh "\@", $self->keyword(), " ", $self->stringize(), "\n";
	} else {
		print $fh $self->stringize(), "\n";
	}
}

# needed for comment checking
sub fullstring
{
	my ($self, $fh) = @_;
	if ($self->needs_keyword()) {
		return "\@".$self->keyword()." ".$self->stringize();
	} else {
		return $self->stringize();
	}
}

sub stringize($)
{
	return $_[0]->{name};
}

sub compute_fullname
{
	my ($self, $state, $absolute_okay) = @_;

	$self->{cwd} = $state->{cwd};
	my $fullname = $self->{name};
	if ($fullname =~ m|^/|) {
		unless ($absolute_okay) {
			die "Absolute name forbidden: $fullname";
		}
	} else {
		$fullname = File::Spec->catfile($state->{cwd}, $fullname);
	}
	$fullname = File::Spec->canonpath($fullname);
	$self->{fullname} = $fullname;
	return $fullname;
}

sub compute_modes
{
	my ($self, $state) = @_;
	if (defined $state->{mode}) {
		$self->{mode} = $state->{mode};
	}
	if (defined $state->{owner}) {
		$self->{owner} = $state->{owner};
	}
	if (defined $state->{group}) {
		$self->{group} = $state->{group};
	}
}

sub expand
{
	my $state = $_[2];
	local $_ = $_[1];
	if (m/\%F/) {
		die "Bad expand" unless defined $state->{lastfile};
		s/\%F/$state->{lastfile}->{name}/g;
	}
	if (m/\%D/) {
		die "Bad expand" unless defined $state->{cwd};
		s/\%D/$state->{cwd}/g;
	}
	if (m/\%B/) {
		die "Bad expand" unless defined $state->{lastfile};
		s/\%B/dirname($state->{lastfile}->fullname())/ge;
	}
	if (m/\%f/) {
		die "Bad expand" unless defined $state->{lastfile};
		s/\%f/basename($state->{lastfile}->fullname())/ge;
	}
	return $_;
}
sub IsFile() { 0 }

sub NoDuplicateNames() { 0 }

sub fullname($)
{
	return $_[0]->{fullname};
}

# Abstract class for all file-like elements

package OpenBSD::PackingElement::FileBase;
our @ISA=qw(OpenBSD::PackingElement);
use File::Spec;
sub write
{
	my ($self, $fh) = @_;
	print $fh "\@ignore\n" if defined $self->{ignore};
	print $fh "\@comment no checksum\n" if defined $self->{nochecksum};
	$self->SUPER::write($fh);
	if (defined $self->{md5}) {
		print $fh "\@md5 ", $self->{md5}, "\n";
	}
	if (defined $self->{size}) {
		print $fh "\@size ", $self->{size}, "\n";
	}
	if (defined $self->{symlink}) {
		print $fh "\@symlink ", $self->{symlink}, "\n";
	}
	if (defined $self->{link}) {
		print $fh "\@link ", $self->{link}, "\n";
	}
}

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
	$state->{lastfile} = $self;
	$self->compute_modes($state);
	if (defined $state->{nochecksum}) {
		$self->{nochecksum} = 1;
		undef $state->{nochecksum};
	}
	if (defined $state->{ignore}) {
		$self->{ignore} = 1;
		undef $state->{ignore};
	}
}

sub add_md5
{
	my ($self, $md5) = @_;
	$self->{md5} = $md5;
}

sub add_size
{
	my ($self, $sz) = @_;
	$self->{size} = $sz;
}

# XXX symlink/hardlinks are properties of File,
# because we want to use inheritance for other stuff.

sub make_symlink
{
	my ($self, $linkname) = @_;
	$self->{symlink} = $linkname;
}

sub make_hardlink
{
	my ($self, $linkname) = @_;
	$self->{link} = $linkname;
}

sub IsFile() { 1 }

sub NoDuplicateNames() { 1 }

package OpenBSD::PackingElement::File;
our @ISA=qw(OpenBSD::PackingElement::FileBase);
use OpenBSD::PackageInfo qw(is_info_name);
__PACKAGE__->setKeyword('file');
sub keyword() { "file" }

sub dirclass() { "OpenBSD::PackingElement::Dir" }

sub needs_keyword
{
	my $self = shift;
	return $self->stringize() =~ m/\^@/;
}

sub add_object
{
	my ($self, $plist) = @_;

	$self->destate($plist->{state});
	my $j = is_info_name($self->fullname());
	if ($j) {
		bless $self, "OpenBSD::PackingElement::$j";
		$plist->addunique($self);
	} else {
		$plist->add2list($self);
	}
	return $self;
}

package OpenBSD::PackingElement::Sample;
our @ISA=qw(OpenBSD::PackingElement);
sub NoDuplicateNames() { 1 }
__PACKAGE__->setKeyword('sample');
sub keyword() { "sample" }
sub destate
{
	my ($self, $state) = @_;
	$self->{copyfrom} = $state->{lastfile};
	$self->compute_fullname($state, 1);
	$self->compute_modes($state);
}

sub dirclass() { "OpenBSD::PackingElement::Sampledir" }

package OpenBSD::PackingElement::Sampledir;
our @ISA=qw(OpenBSD::PackingElement::DirBase OpenBSD::PackingElement::Sample);
sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state, 1);
	$self->compute_modes($state);
}

package OpenBSD::PackingElement::InfoFile;
our @ISA=qw(OpenBSD::PackingElement::FileBase);
__PACKAGE__->setKeyword('info');
sub keyword() { "info" }


package OpenBSD::PackingElement::Manpage;
our @ISA=qw(OpenBSD::PackingElement::FileBase);
__PACKAGE__->setKeyword('man');
sub keyword() { "man" }

sub destate
{
	my ($self, $state) = @_;
	$self->SUPER::destate($state);
	my $fname = $self->fullname();
	if ($fname =~ m,^(.*/man)/(?:man|cat).*?/,) {
		my $d = $1;
		$state->{mandirs} = {} unless defined $state->{mandirs};
		$state->{mandirs}->{$d} = [] 
		    unless defined $state->{mandirs}->{$d};
		push(@{$state->{mandirs}->{$d}}, $fname);
	}
}

package OpenBSD::PackingElement::Ignore;
our @ISA=qw(OpenBSD::PackingElement);
__PACKAGE__->setKeyword('ignore');

sub add_object
{
	my ($plist, $self) = @_;
	$plist->{state}->{ignore} = 1;
	return undef;
}

# Comment is very special
package OpenBSD::PackingElement::Comment;
our @ISA=qw(OpenBSD::PackingElement);
__PACKAGE__->setKeyword('comment');
sub keyword() { "comment" }

sub add
{
	my ($class, $plist, @args) = @_;

	if ($args[0] =~ m/^\$OpenBSD(.*)\$\s*$/) {
		OpenBSD::PackingElement::CVSTag->add($plist, @args);
	} elsif ($args[0] =~ m/^MD5:\s*/) {
		$plist->{state}->{lastfile}->add_md5($');
		return undef;
	} elsif ($args[0] =~ m/^subdir\=(.*?)\s+cdrom\=(.*?)\s+ftp\=(.*?)\s*$/) {
		OpenBSD::PackingElement::ExtraInfo->add($plist, $1, $2, $3);
	} elsif ($args[0] eq 'no checksum') {
		$plist->{state}->{nochecksum} = 1;
		return undef;
	} else {
		my $self = $class->new(@args);
		return $self->add_object($plist);
	}
}

package OpenBSD::PackingElement::md5;
our @ISA=qw(OpenBSD::PackingElement);
__PACKAGE__->setKeyword('md5');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->add_md5($');
	return undef;
}

package OpenBSD::PackingElement::CVSTag;
our @ISA=qw(OpenBSD::PackingElement OpenBSD::PackingElement::Comment);
sub category() { 'cvstags'}

package OpenBSD::PackingElement::symlink;
our @ISA=qw(OpenBSD::PackingElement);
__PACKAGE__->setKeyword('symlink');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->make_symlink($');
	return undef;
}

package OpenBSD::PackingElement::hardlink;
our @ISA=qw(OpenBSD::PackingElement);
__PACKAGE__->setKeyword('link');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->make_hardlink($');
	return undef;
}

package OpenBSD::PackingElement::size;
our @ISA=qw(OpenBSD::PackingElement);
__PACKAGE__->setKeyword('size');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->add_size($args[0]);
	return undef;
}

package OpenBSD::PackingElement::Option;
our @ISA=qw(OpenBSD::PackingElement);
__PACKAGE__->setKeyword('option');
sub keyword() { 'option' }

sub add
{
	my ($class, $plist, @args) = @_;
	if ($args[0] eq 'no-default-conflict') {
		shift;
		return OpenBSD::PackingElement::NoDefaultConflict->add($plist);
	} else {
		die "Unknown option: $args[0]";
	}
}

package OpenBSD::PackingElement::NoDefaultConflict;
our @ISA=qw(OpenBSD::PackingElement::Unique);
sub category() { 'no-default-conflict' }
sub keyword() { 'option' }

sub stringize() 
{
	return 'no-default-conflict';
}

sub new
{
	my ($class, @args) = @_;
	bless {}, $class;
}

# The special elements that don't end in the right place
package OpenBSD::PackingElement::ExtraInfo;
our @ISA=qw(OpenBSD::PackingElement);

sub keyword() { 'comment' }
sub category() { 'extrainfo' }


sub new
{
	my ($class, $subdir, $cdrom, $ftp) = @_;
	bless { subdir => $subdir, cdrom => $cdrom, ftp => $ftp}, $class;
}

sub add
{
	my ($class, $plist, @args) = @_;
	my $self = $class->new(@args);
	$plist->addunique($self);
	return $self;
}

sub stringize($)
{
	my $self = $_[0];
	return "subdir=".$self->{subdir}." cdrom=".$self->{cdrom}.
	    " ftp=".$self->{ftp};
}

package OpenBSD::PackingElement::PkgDep;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('pkgdep');
sub keyword() { "pkgdep" }
sub category() { "pkgdep" }

package OpenBSD::PackingElement::PkgConflict;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('pkgcfl');
sub keyword() { "pkgcfl" }
sub category() { "pkgcfl" }


package OpenBSD::PackingElement::NewDepend;

our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('newdepend');
sub category() { "newdepend" }
sub keyword() { "newdepend" }

sub new
{
	my ($class, $args) = @_;
	my ($name, $pattern, $def) = split /\:/, $args;
	my $self = bless { pattern => $pattern, def => $def }, $class;
	# very old packages still work
	if ($name =~ m|/|) {
		$self->{pkgpath} = $name;
	} else {
		$self->{name} = $name;
	}
	return $self;
}

sub stringize($)
{
	my $self = $_[0];
	return (defined $self->{name} ? $self->{name} : $self->{pkgpath}).
	    ':'.$self->{pattern}.':'.$self->{def};
}

package OpenBSD::PackingElement::LibDepend;

our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('libdepend');
sub category() { "libdepend" }
sub keyword() { "libdepend" }

sub new
{
	my ($class, $args) = @_;
	my ($name, $libspec, $pattern, $def)  = split /\:/, $args;
	my $self = bless { libspec => $libspec, pattern => $pattern, 
	    def => $def }, $class;
	# very old packages still work
	if ($name =~ m|/|) {
		$self->{pkgpath} = $name;
	} else {
		$self->{name} = $name;
	}
	return $self;
}

sub stringize($)
{
	my $self = $_[0];
	return (defined $self->{name} ? $self->{name} : $self->{pkgpath}).
	    ':'.$self->{libspec}.':'.$self->{pattern}.':'.$self->{def};
}

package OpenBSD::PackingElement::Unique;
our @ISA=qw(OpenBSD::PackingElement);

sub add 
{
	my ($class, $plist, @args) = @_;

	my $self = $class->new(@args);
	$self->destate($plist->{state});
	$plist->addunique($self);
	return $self;
}

package OpenBSD::PackingElement::Name;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement);

__PACKAGE__->setKeyword('name');
sub keyword() { "name" }
sub category() { "name" }

package OpenBSD::PackingElement::LocalBase;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement);

__PACKAGE__->setKeyword('localbase');
sub keyword() { "localbase" }
sub category() { "localbase" }

package OpenBSD::PackingElement::Cwd;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('cwd');

sub keyword() { 'cwd' }

sub destate
{
	my ($self, $state) = @_;
	$state->{cwd} = $self->{name};
}

package OpenBSD::PackingElement::Owner;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('owner');
sub keyword() { 'owner' }

sub destate
{
	my ($self, $state) = @_;

	if ($self->{name} eq '') {
		undef $state->{owner};
	} else {
		$state->{owner} = $self->{name};
	}
}

package OpenBSD::PackingElement::Group;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('group');
sub keyword() { 'group' }

sub destate
{
	my ($self, $state) = @_;

	if ($self->{name} eq '') {
		undef $state->{group};
	} else {
		$state->{group} = $self->{name};
	}
}

package OpenBSD::PackingElement::Mode;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('mode');
sub keyword() { 'mode' }

sub destate
{
	my ($self, $state) = @_;

	if ($self->{name} eq '') {
		undef $state->{mode};
	} else {
		$state->{mode} = $self->{name};
	}
}

package OpenBSD::PackingElement::Exec;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('exec');

sub keyword() { "exec" }

sub destate
{
	my ($self, $state) = @_;
	$self->{expanded} = $self->expand($self->{name}, $state);
}

package OpenBSD::PackingElement::Unexec;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('unexec');
sub keyword() { "unexec" }

sub destate
{
	my ($self, $state) = @_;
	$self->{expanded} = $self->expand($self->{name}, $state);
}

package OpenBSD::PackingElement::ExtraUnexec;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('extraunexec');
sub keyword() { "extraunexec" }

sub destate
{
	my ($self, $state) = @_;
	$self->{expanded} = $self->expand($self->{name}, $state);
}

package OpenBSD::PackingElement::DirRm;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('dirrm');
sub keyword() { "dirrm" }

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
}

sub NoDuplicateNames() { 1 }

package OpenBSD::PackingElement::DirBase;
sub stringize($)
{
	return $_[0]->{name}."/";
}

sub NoDuplicateNames() { 1 }

package OpenBSD::PackingElement::Dir;
our @ISA=qw(OpenBSD::PackingElement::DirBase OpenBSD::PackingElement);

__PACKAGE__->setKeyword('dir');
sub keyword() { "dir" }

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
	$self->compute_modes($state);
}

sub needs_keyword
{
	my $self = shift;
	return $self->stringize() =~ m/\^@/;
}

package OpenBSD::PackingElement::Fontdir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
__PACKAGE__->setKeyword('fontdir');
sub keyword() { "fontdir" }
sub needs_keyword { 1 }

package OpenBSD::PackingElement::Mandir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
__PACKAGE__->setKeyword('mandir');
sub keyword() { "mandir" }
sub needs_keyword { 1 }

package OpenBSD::PackingElement::Extra;
our @ISA=qw(OpenBSD::PackingElement);

__PACKAGE__->setKeyword('extra');
sub keyword() { 'extra' }

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state, 1);
}

sub dirclass() { "OpenBSD::PackingElement::Extradir" }

package OpenBSD::PackingElement::Extradir;
our @ISA=qw(OpenBSD::PackingElement::DirBase OpenBSD::PackingElement::Extra);

package OpenBSD::PackingElement::SpecialFile;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub add_md5
{
	my ($self, $md5) = @_;
	$self->{md5} = $md5;
}

sub needs_keyword { 0 }

sub write
{
	&OpenBSD::PackingElement::FileBase::write;
}

package OpenBSD::PackingElement::FCONTENTS;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::CONTENTS }

package OpenBSD::PackingElement::FCOMMENT;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::COMMENT }

package OpenBSD::PackingElement::FDESC;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::DESC }

package OpenBSD::PackingElement::FINSTALL;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::INSTALL }

package OpenBSD::PackingElement::FDEINSTALL;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::DEINSTALL }

package OpenBSD::PackingElement::FREQUIRE;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::REQUIRE }

package OpenBSD::PackingElement::FREQUIRED_BY;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::REQUIRED_BY }

package OpenBSD::PackingElement::FDISPLAY;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::DISPLAY }

package OpenBSD::PackingElement::FMTREE_DIRS;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::MTREE_DIRS }

package OpenBSD::PackingElement::Arch;
our @ISA=qw(OpenBSD::PackingElement::Unique);
__PACKAGE__->setKeyword('arch');
sub category() { 'arch' }
sub keyword() { 'arch' }

sub new
{
	my ($class, $args) = @_;
	my @arches= split(/\,/, $args);
	bless { arches => \@arches }, $class;
}

sub stringize($)
{
	my $self = $_[0];
	return join(',',@{$self->{arches}});
}

1;
