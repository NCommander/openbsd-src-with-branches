# ex:ts=8 sw=4:
# $OpenBSD: PackingElement.pm,v 1.51 2004/10/11 13:46:17 espie Exp $
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
use OpenBSD::PackageInfo;

# perl ipc
require 5.008_000;

# This is the basic class, which is mostly abstract, except for
# setKeyword and Factory.
# It does provide base methods for stuff under it, though.
package OpenBSD::PackingElement;
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
	bless { name => $args }, $class;
}

sub clone
{
	my $object = shift;
	# shallow copy
	my %h = %$object;
	bless \%h, ref($object);
}
	

sub register_manpage
{
}

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
	my $s = $self->stringize();
	if ($self->needs_keyword()) {
		$s = " $s" unless $s eq '';
		print $fh "\@", $self->keyword(), "$s\n";
	} else {
		print $fh "$s\n";
	}
}

# needed for comment checking
sub fullstring
{
	my ($self, $fh) = @_;
	my $s = $self->stringize();
	if ($self->needs_keyword()) {
		$s = " $s" unless $s eq '';
		return "\@".$self->keyword().$s;
	} else {
		return $s;
	}
}

sub stringize($)
{
	return $_[0]->{name};
}

sub IsFile() { 0 }

sub NoDuplicateNames() { 0 }

# Basic class hierarchy

# various stuff that's only linked to objects before/after them
# this class doesn't have real objects: no valid new nor clone...
package OpenBSD::PackingElement::Annotation;
our @ISA=qw(OpenBSD::PackingElement);
sub new { die "Can't create annotation objects" }

# concrete objects
package OpenBSD::PackingElement::Object;
our @ISA=qw(OpenBSD::PackingElement);

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

sub fullname($)
{
	return $_[0]->{fullname};
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

# concrete objects with file-like behavior
package OpenBSD::PackingElement::FileObject;
our @ISA=qw(OpenBSD::PackingElement::Object);

sub NoDuplicateNames() { 1 }

sub dirclass() { undef }

sub new
{
	my ($class, $args) = @_;
	if ($args =~ m|/+$| and defined $class->dirclass()) {
		bless { name => $` }, $class->dirclass();
	} else {
		bless { name => $args }, $class;
	}
}

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
}

# exec/unexec and friends
package OpenBSD::PackingElement::Action;
our @ISA=qw(OpenBSD::PackingElement::Object);

# persistent state for following objects
package OpenBSD::PackingElement::State;
our @ISA=qw(OpenBSD::PackingElement::Object);

# meta information, stored elsewhere
package OpenBSD::PackingElement::Meta;
our @ISA=qw(OpenBSD::PackingElement);

package OpenBSD::PackingElement::Unique;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub add_object
{
	my ($self, $plist) = @_;

	$self->destate($plist->{state});
	$plist->addunique($self);
	return $self;
}

# all dependency information
package OpenBSD::PackingElement::Depend;
our @ISA=qw(OpenBSD::PackingElement::Meta);

# Abstract class for all file-like elements
package OpenBSD::PackingElement::FileBase;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

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
	$self->SUPER::destate($state);
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
our @ISA=qw(OpenBSD::PackingElement::FileObject);

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
sub dirclass() { "OpenBSD::PackingElement::Infodir" }

package OpenBSD::PackingElement::Shell;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

__PACKAGE__->setKeyword('shell');
sub keyword() { "shell" }

package OpenBSD::PackingElement::Manpage;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

__PACKAGE__->setKeyword('man');
sub keyword() { "man" }

sub register_manpage
{
	my ($self, $state) = @_;
	my $fname = $self->fullname();
	if ($fname =~ m,^(.*/man)/(?:man|cat).*?/,) {
		my $d = $1;
		$state->{mandirs} = {} unless defined $state->{mandirs};
		$state->{mandirs}->{$d} = [] 
		    unless defined $state->{mandirs}->{$d};
		push(@{$state->{mandirs}->{$d}}, $fname);
    	}
}

package OpenBSD::PackingElement::Lib;
our @ISA=qw(OpenBSD::PackingElement::FileBase);
use File::Basename;

__PACKAGE__->setKeyword('lib');
sub keyword() { "lib" }

our $todo;
my $path;
our @ldconfig = ('/sbin/ldconfig');

sub add_ldconfig_dirs()
{
	my $sub = shift;
	return unless defined $todo;
	for my $d (keys %$todo) {
		&$sub($d);
	}
	$todo={};
}

sub mark_ldconfig_directory
{
	my ($self, $destdir) = @_;
	if (!defined $path) {
		$path={};
		if ($destdir ne '') {
			unshift @ldconfig, 'chroot', $destdir;
		}
		open my $fh, "-|", @ldconfig, "-r";
		if (defined $fh) {
			local $_;
			while (<$fh>) {
				if (m/^\s*search directories:\s*(.*?)\s*$/) {
					for my $d (split(':', $1)) {
						$path->{$d} = 1;
					}
				}
			}
			close($fh);
		} else {
			print STDERR "Can't find ldconfig\n";
		}
	}
	my $d = dirname($self->fullname());
	if ($path->{$d}) {
		$todo = {} unless defined $todo;
		$todo->{$d} = 1;
	}
}

package OpenBSD::PackingElement::Ignore;
our @ISA=qw(OpenBSD::PackingElement::Annotation);
__PACKAGE__->setKeyword('ignore');

sub add
{
	my ($class, $plist, @args) = @_;
	$plist->{state}->{ignore} = 1;
	return undef;
}

# Comment is very special
package OpenBSD::PackingElement::Comment;
our @ISA=qw(OpenBSD::PackingElement::Meta);

__PACKAGE__->setKeyword('comment');
sub keyword() { "comment" }

sub add
{
	my ($class, $plist, @args) = @_;

	if ($args[0] =~ m/^\$OpenBSD(.*)\$\s*$/) {
		return OpenBSD::PackingElement::CVSTag->add($plist, @args);
	} elsif ($args[0] =~ m/^MD5:\s*/) {
		$plist->{state}->{lastfile}->add_md5($');
		return undef;
	} elsif ($args[0] =~ m/^subdir\=(.*?)\s+cdrom\=(.*?)\s+ftp\=(.*?)\s*$/) {
		return OpenBSD::PackingElement::ExtraInfo->add($plist, $1, $2, $3);
	} elsif ($args[0] eq 'no checksum') {
		$plist->{state}->{nochecksum} = 1;
		return undef;
	} else {
		return $class->SUPER::add($plist, @args);
	}
}

package OpenBSD::PackingElement::CVSTag;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { 'comment' }

sub category() { 'cvstags'}

package OpenBSD::PackingElement::md5;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->setKeyword('md5');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->add_md5($');
	return undef;
}

package OpenBSD::PackingElement::symlink;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->setKeyword('symlink');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->make_symlink(@args);
	return undef;
}

package OpenBSD::PackingElement::hardlink;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->setKeyword('link');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->make_hardlink(@args);
	return undef;
}

package OpenBSD::PackingElement::size;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->setKeyword('size');

sub add
{
	my ($class, $plist, @args) = @_;

	$plist->{state}->{lastfile}->add_size($args[0]);
	return undef;
}

package OpenBSD::PackingElement::Option;
our @ISA=qw(OpenBSD::PackingElement::Meta);

__PACKAGE__->setKeyword('option');
sub keyword() { 'option' }

sub new
{
	my ($class, @args) = @_;
	if ($args[0] eq 'no-default-conflict') {
		return OpenBSD::PackingElement::NoDefaultConflict->new();
	} elsif ($args[0] eq 'manual-installation') {
		return OpenBSD::PackingElement::ManualInstallation->new();
	} else {
		die "Unknown option: $args[0]";
	}
}

package OpenBSD::PackingElement::NoDefaultConflict;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::Option);

sub category() { 'no-default-conflict' }

sub stringize() 
{
	return 'no-default-conflict';
}

sub new
{
	my ($class, @args) = @_;
	bless {}, $class;
}

package OpenBSD::PackingElement::ManualInstallation;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::Option);

sub category() { 'manual-installation' }

sub stringize() 
{
	return 'manual-installation';
}

sub new
{
	my ($class, @args) = @_;
	bless {}, $class;
}

# The special elements that don't end in the right place
package OpenBSD::PackingElement::ExtraInfo;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::Comment);

sub category() { 'extrainfo' }

sub new
{
	my ($class, $subdir, $cdrom, $ftp) = @_;
	bless { subdir => $subdir, cdrom => $cdrom, ftp => $ftp}, $class;
}

sub stringize($)
{
	my $self = $_[0];
	return "subdir=".$self->{subdir}." cdrom=".$self->{cdrom}.
	    " ftp=".$self->{ftp};
}

package OpenBSD::PackingElement::Name;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement::Unique);

__PACKAGE__->setKeyword('name');
sub keyword() { "name" }
sub category() { "name" }

package OpenBSD::PackingElement::LocalBase;
our @ISA=qw(OpenBSD::PackingElement::Unique);

__PACKAGE__->setKeyword('localbase');
sub keyword() { "localbase" }
sub category() { "localbase" }

package OpenBSD::PackingElement::Conflict;
our @ISA=qw(OpenBSD::PackingElement::Meta);

__PACKAGE__->setKeyword('conflict');
sub keyword() { "conflict" }
sub category() { "conflict" }

package OpenBSD::PackingElement::PkgConflict;
our @ISA=qw(OpenBSD::PackingElement::Conflict);

__PACKAGE__->setKeyword('pkgcfl');
sub keyword() { "pkgcfl" }
sub category() { "pkgcfl" }

package OpenBSD::PackingElement::PkgDep;
our @ISA=qw(OpenBSD::PackingElement::Depend);

__PACKAGE__->setKeyword('pkgdep');
sub keyword() { "pkgdep" }
sub category() { "pkgdep" }

package OpenBSD::PackingElement::NewDepend;
our @ISA=qw(OpenBSD::PackingElement::Depend);

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
our @ISA=qw(OpenBSD::PackingElement::Depend);

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

package OpenBSD::PackingElement::NewUser;
our @ISA=qw(OpenBSD::PackingElement::Action);
__PACKAGE__->setKeyword("newuser");

sub category() { "users" }
sub keyword() { "newuser" }

sub new
{
	my ($class, $args) = @_;
	my ($name, $uid, $group, $loginclass, $comment, $home, $shell) = 
	    split /\:/, $args;
	bless { name => $name, uid => $uid, group => $group, 
	    class => $loginclass, 
	    comment => $comment, home => $home, shell => $shell }, $class;
}

sub check
{
	my $self = shift;
	my ($name, $passwd, $uid, $gid, $quota, $class, $gcos, $dir, $shell, 
	    $expire) = getpwnam($self->{name});
	return undef unless defined $name;
	if ($self->{uid} =~ m/^\!/) {
		return 0 unless $uid == $';
	}
	if ($self->{group} =~ m/^\!/) {
		my $g = $';
		unless ($g =~ m/^\d+/) {
			$g = getgrnam($g);
			return 0 unless defined $g;
		}
		return 0 unless $gid eq $g;
	}
	if ($self->{class} =~ m/^\!/) {
		return 0 unless $class eq $';
	}
	if ($self->{comment} =~ m/^\!/) {
		return 0 unless $gcos eq $';
	}
	if ($self->{home} =~ m/^\!/) {
		return 0 unless $dir eq $';
	}
	if ($self->{shell} =~ m/^\!/) {
		return 0 unless $shell eq $';
	}
	return 1;
}

sub stringize($)
{
	my $self = $_[0];
	return $self->{name}.':'.$self->{uid}.':'.$self->{group}.':'.
	    $self->{class}.':'.$self->{comment}.':'.$self->{home}.':'.
	    $self->{shell};
}

package OpenBSD::PackingElement::NewGroup;
our @ISA=qw(OpenBSD::PackingElement::Action);

__PACKAGE__->setKeyword("newgroup");

sub category() { "groups" }
sub keyword() { "newgroup" }

sub new
{
	my ($class, $args) = @_;
	my ($name, $gid) = split /\:/, $args;
	bless { name => $name, gid => $gid }, $class;
}

sub check
{
	my $self = shift;
	my ($name, $passwd, $gid, $members) = getgrnam($self->{name});
	return undef unless defined $name;
	if ($self->{gid} =~ m/^\!/) {
		return 0 unless $gid == $';
	}
	return 1;
}

sub stringize($)
{
	my $self = $_[0];
	return $self->{name}.':'.$self->{gid};
}

package OpenBSD::PackingElement::Cwd;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement::State);

__PACKAGE__->setKeyword('cwd');

sub keyword() { 'cwd' }

sub destate
{
	my ($self, $state) = @_;
	$state->{cwd} = $self->{name};
}

package OpenBSD::PackingElement::Owner;
our @ISA=qw(OpenBSD::PackingElement::State);

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
our @ISA=qw(OpenBSD::PackingElement::State);

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
our @ISA=qw(OpenBSD::PackingElement::State);

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

package OpenBSD::PackingElement::ExeclikeAction;
use File::Basename;
our @ISA=qw(OpenBSD::PackingElement::Action);

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

sub destate
{
	my ($self, $state) = @_;
	$self->{expanded} = $self->expand($self->{name}, $state);
}

package OpenBSD::PackingElement::Exec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

__PACKAGE__->setKeyword('exec');

sub keyword() { "exec" }

package OpenBSD::PackingElement::Unexec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

__PACKAGE__->setKeyword('unexec');
sub keyword() { "unexec" }

package OpenBSD::PackingElement::ExtraUnexec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

__PACKAGE__->setKeyword('extraunexec');
sub keyword() { "extraunexec" }

package OpenBSD::PackingElement::DirlikeObject;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

package OpenBSD::PackingElement::DirRm;
our @ISA=qw(OpenBSD::PackingElement::DirlikeObject);

__PACKAGE__->setKeyword('dirrm');
sub keyword() { "dirrm" }

package OpenBSD::PackingElement::DirBase;
our @ISA=qw(OpenBSD::PackingElement::DirlikeObject);

sub stringize($)
{
	my $self = $_[0];
	return $self->{name}."/";
}

package OpenBSD::PackingElement::Dir;
our @ISA=qw(OpenBSD::PackingElement::DirBase);

__PACKAGE__->setKeyword('dir');
sub keyword() { "dir" }

sub destate
{
	my ($self, $state) = @_;
	$self->SUPER::destate($state);
	$self->compute_modes($state);
}

sub needs_keyword
{
	my $self = shift;
	return $self->stringize() =~ m/\^@/;
}

package OpenBSD::PackingElement::Infodir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
sub keyword() { "info" }
sub needs_keyword() { 1 }

package OpenBSD::PackingElement::Fontdir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
__PACKAGE__->setKeyword('fontdir');
sub keyword() { "fontdir" }
sub needs_keyword() { 1 }
sub dirclass() { "OpenBSD::PackingElement::Fontdir" }

our %fonts_todo = ();

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	$fonts_todo{$state->{destdir}.$self->fullname()} = 1;
}

sub reload
{
	my ($self, $state) = @_;
	$fonts_todo{$state->{destdir}.$self->fullname()} = 1;
}

sub update_fontalias
{
	my $dirname = shift;
	my @aliases;

	for my $alias (glob "$dirname/fonts.alias-*") {
		open my $f ,'<', $alias or next;
		push(@aliases, <$f>);
		close $f;
	}
	open my $f, '>', "$dirname/fonts.alias";
	print $f @aliases;
	close $f;
}

sub restore_fontdir
{
	my $dirname = shift;
	if (-f "$dirname/fonts.dir.dist") {
		require OpenBSD::Error;

		unlink("$dirname/fonts.dir");
		OpenBSD::Error::Copy("$dirname/fonts.dir.dist", "$dirname/fonts.dir");
	}
}

sub finish_fontdirs
{
	my @l = keys %fonts_todo;
	if (@l != 0) {
		require OpenBSD::Error;

		map { update_fontalias($_) } @l;
		print "You may wish to update your font path for ", join(' ', @l), "\n";
		eval { OpenBSD::Error::System("/usr/X11R6/bin/mkfontdir", @l); };
		map { restore_fontdir($_) } @l;
		eval { OpenBSD::Error::System("/usr/X11R6/bin/fc-cache", @l); };
	}
}


package OpenBSD::PackingElement::Mandir;
our @ISA=qw(OpenBSD::PackingElement::Dir);

__PACKAGE__->setKeyword('mandir');
sub keyword() { "mandir" }
sub needs_keyword() { 1 }
sub dirclass() { "OpenBSD::PackingElement::Mandir" }

package OpenBSD::PackingElement::Extra;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

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

sub destate
{
	&OpenBSD::PackingElement::Extra::destate;
}

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

package OpenBSD::PackingElement::ScriptFile;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
use OpenBSD::Error;

sub run
{
	my ($self, $state, @args) = @_;

	my $dir = $state->{dir};
	my $verbose = $state->{verbose};
	my $not = $state->{not};
	my $pkgname = $state->{pkgname};
	my $name = $self->{name};

	return if $state->{dont_run_scripts};

	main::ensure_ldconfig($state);
	print $self->beautify(), " script: $dir$name $pkgname ", join(' ', @args), "\n" if $state->{beverbose};
	return if $not;
	chmod 0755, $dir.$name;
	return if System($dir.$name, $pkgname, @args) == 0;
	Fatal $self->beautify()." script borked";
}

package OpenBSD::PackingElement::FCOMMENT;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::COMMENT }

package OpenBSD::PackingElement::FDESC;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::DESC }

package OpenBSD::PackingElement::FINSTALL;
our @ISA=qw(OpenBSD::PackingElement::ScriptFile);
sub category() { OpenBSD::PackageInfo::INSTALL }
sub beautify() { "Install" }

package OpenBSD::PackingElement::FDEINSTALL;
our @ISA=qw(OpenBSD::PackingElement::ScriptFile);
sub category() { OpenBSD::PackageInfo::DEINSTALL }
sub beautify() { "Deinstall" }

package OpenBSD::PackingElement::FREQUIRE;
our @ISA=qw(OpenBSD::PackingElement::ScriptFile);
sub category() { OpenBSD::PackageInfo::REQUIRE }
sub beautify() { "Require" }

package OpenBSD::PackingElement::FREQUIRED_BY;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::REQUIRED_BY }

package OpenBSD::PackingElement::DisplayFile;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
use OpenBSD::Error;

sub prepare
{
	my ($self, $state) = @_;
	unless (defined $state->{display}) {
		require OpenBSD::Temp;
		require File::Temp;

		($state->{display}, $state->{displayname}) = File::Temp::tempfile("display.XXXXXXXXX", DIR => $OpenBSD::Temp::tempbase);
	}
	my $fname = $state->{dir}.$self->{name};
	open(my $src, '<', $fname) or Fatal "Can't open $fname: $!";
	my $dest = $state->{display};
	local $_;
	print $dest "+-------------- ", $state->{pkgname}, "\n";
	while (<$src>) {
		next if m/^\+\-+\s*$/;
		s/^\+ //;
		print $dest $_;
	}
	print $dest "+-------------- ", $state->{pkgname}, "\n";
}

package OpenBSD::PackingElement::FDISPLAY;
our @ISA=qw(OpenBSD::PackingElement::DisplayFile);
sub category() { OpenBSD::PackageInfo::DISPLAY }

package OpenBSD::PackingElement::FUNDISPLAY;
our @ISA=qw(OpenBSD::PackingElement::DisplayFile);
sub category() { OpenBSD::PackageInfo::UNDISPLAY }

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
