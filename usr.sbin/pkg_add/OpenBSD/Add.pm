# ex:ts=8 sw=4:
# $OpenBSD: Add.pm,v 1.57 2007/05/28 12:16:55 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Add;
use OpenBSD::Error;
use OpenBSD::PackageInfo;
use OpenBSD::ArcCheck;
use File::Copy;

sub manpages_index
{
	my ($state) = @_;
	return unless defined $state->{mandirs};
	my $destdir = $state->{destdir};
	require OpenBSD::Makewhatis;

	while (my ($k, $v) = each %{$state->{mandirs}}) {
		my @l = map { $destdir.$_ } @$v;
		if ($state->{not}) {
			print "Merging manpages in $destdir$k: ", join(@l), "\n" if $state->{verbose};
		} else {
			try { 
				OpenBSD::Makewhatis::merge($destdir.$k, \@l); 
			} catchall {
				print STDERR "Error in makewhatis: $_\n";
			};
		}
	}
}

sub register_installation
{
	my ($dir, $dest, $plist) = @_;
	return if $main::not;
	mkdir($dest);
	for my $i (info_names()) {
		copy($dir.$i, $dest);
	}
	$plist->to_installation;
}

sub validate_plist
{
	my ($plist, $state) = @_;

	$state->{totsize} = 0;

	$plist->prepare_for_addition($state, $plist->pkgname);
	$state->{totsize} = 1 if $state->{totsize} == 0;
	$plist->{totsize} = $state->{totsize};
}

sub borked_installation
{
	my ($plist, $dir, $not, @msg) = @_;

	Fatal @msg if $not;
	use OpenBSD::PackingElement;


	my $borked = borked_package($plist->pkgname);
	# fix packing list for pkg_delete
	$plist->{items} = $plist->{done};

	# last file may have not copied correctly
	my $last = $plist->{items}->[@{$plist->{items}}-1];
	if ($last->IsFile() && defined($last->{md5})) {
	    require OpenBSD::md5;

	    my $old = $last->{md5};
	    my $lastname;
	    if (defined $last->{tempname}) {
	    	$lastname = $last->{tempname};
	    } else {
	    	$lastname = $last->fullname;
	    }
	    $last->{md5} = OpenBSD::md5::fromfile($lastname);
	    if ($old ne $last->{md5}) {
		print "Adjusting md5 for $lastname from ",
		    unpack('H*', $old), " to ", unpack('H*', $last->{md5}), "\n";
	    }
	}
	OpenBSD::PackingElement::Cwd->add($plist, '.');
	my $pkgname = $plist->pkgname;
	$plist->{name}->{name} = $borked;
	my $dest = installed_info($borked);
	register_installation($dir, $dest, $plist);
	Fatal @msg, ", partial installation recorded as $borked";
}

# used by newuser/newgroup to deal with options.
package OpenBSD::PackingElement;
use OpenBSD::Error;

my ($uidcache, $gidcache);

sub prepare_for_addition
{
}

sub install
{
}

sub set_modes
{
	my ($self, $name) = @_;

	if (defined $self->{owner} || defined $self->{group}) {
		require OpenBSD::IdCache;

		if (!defined $uidcache) {
			$uidcache = OpenBSD::UidCache->new;
			$gidcache = OpenBSD::GidCache->new;
		}
		my ($uid, $gid);
		if (-l $name) {
			($uid, $gid) = (lstat $name)[4,5];
		} else {
			($uid, $gid) = (stat $name)[4,5];
		}
		if (defined $self->{owner}) {
			$uid = $uidcache->lookup($self->{owner}, $uid);
		}
		if (defined $self->{group}) {
			$gid = $gidcache->lookup($self->{group}, $gid);
		}
		chown $uid, $gid, $name;
	}
	if (defined $self->{mode}) {
		my $v = $self->{mode};
		if ($v =~ m/^\d+$/) {
			chmod oct($v), $name;
		} else {
			System('chmod', $self->{mode}, $name);
		}
	}
}

package OpenBSD::PackingElement::ExtraInfo;
use OpenBSD::Error;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;

	if ($state->{cdrom_only} && $self->{cdrom} ne 'yes') {
	    Warn "Package $pkgname is not for cdrom.\n";
	    $state->{problems}++;
	}
	if ($state->{ftp_only} && $self->{ftp} ne 'yes') {
	    Warn "Package $pkgname is not for ftp.\n";
	    $state->{problems}++;
	}
}

package OpenBSD::PackingElement::NewAuth;
use OpenBSD::Error;

sub add_entry
{
	shift;	# get rid of self
	my $l = shift;
	while (@_ >= 2) {
		my $f = shift;
		my $v = shift;
		next if !defined $v or $v eq '';
		if ($v =~ m/^\!/) {
			push(@$l, $f, $');
		} else {
			push(@$l, $f, $v);
		}
	}
}

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;
	my $ok = $self->check;
	if (defined $ok) {
		if ($ok == 0) {
			Warn $self->type, " ",  $self->{name}, 
			    " does not match\n";
			$state->{problems}++;
		}
	}
	$self->{okay} = $ok;
}

sub install
{
	my ($self, $state) = @_;
	my $auth = $self->{name};
	print "adding ", $self->type, " $auth\n" if $state->{verbose};
	return if $state->{not};
	return if defined $self->{okay};
	my $l=[];
	push(@$l, "-v") if $state->{very_verbose};
	$self->build_args($l);
	VSystem($state->{very_verbose}, $self->command,, @$l, $auth);
}

package OpenBSD::PackingElement::NewUser;

sub command 	{ '/usr/sbin/useradd' }

sub build_args
{
	my ($self, $l) = @_;

	$self->add_entry($l, 
	    '-u', $self->{uid},
	    '-g', $self->{group},
	    '-L', $self->{class},
	    '-c', $self->{comment},
	    '-d', $self->{home},
	    '-s', $self->{shell});
}

package OpenBSD::PackingElement::NewGroup;

sub command { '/usr/sbin/groupadd' }

sub build_args
{
	my ($self, $l) = @_;

	$self->add_entry($l, '-g', $self->{gid});
}

package OpenBSD::PackingElement::Sysctl;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;

	my $name = $self->{name};
	open(my $pipe, '-|', '/sbin/sysctl', '-n', $name);
	my $actual = <$pipe>;
	chomp $actual;
	if ($self->{mode} eq '=' && $actual eq $self->{value}) {
		return;
	}
	if ($self->{mode} eq '>=' && $actual >= $self->{value}) {
		return;
	}
	if ($state->{not}) {
		print "sysctl -w $name != ".
		    $self->{value}, "\n";
		return;
	}
	VSystem($state->{very_verbose}, '/sbin/sysctl', $name.'='.$self->{value});
}
			
package OpenBSD::PackingElement::FileBase;
use OpenBSD::Error;
use File::Basename;
use File::Path;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;
	my $fname = $state->{destdir}.$self->fullname;
	# check for collisions with existing stuff
	if (OpenBSD::Vstat::vexists($fname)) {
		push(@{$state->{colliding}}, $self);
		$state->{problems}++;
		return;
	}
	$state->{totsize} += $self->{size} if defined $self->{size};
	my $s = OpenBSD::Vstat::add($fname, $self->{size}, \$pkgname);
	return unless defined $s;
	if ($s->{ro}) {
		$s->report_ro($state, $fname);
	}
	if ($state->{forced}->{kitchensink} && $state->{not}) {
		return;
	}
	if ($s->avail < 0) {
		$s->report_overflow($state, $fname);
	}
}

sub install
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	if ($state->{replacing}) {
		if ($state->{not}) {
			print "moving tempfile -> $destdir$fullname\n" if $state->{very_verbose};
			return;
		}
		File::Path::mkpath(dirname($destdir.$fullname));
		if (defined $self->{link}) {
			link($destdir.$self->{link}, $destdir.$fullname);
			delete $self->{zap};
		} elsif (defined $self->{symlink}) {
			symlink($self->{symlink}, $destdir.$fullname);
			delete $self->{zap};
		} else {
			rename($self->{tempname}, $destdir.$fullname) or 
			    Fatal "Can't move ", $self->{tempname}, " to $fullname: $!";
			print "moving ", $self->{tempname}, " -> $destdir$fullname\n" if $state->{very_verbose};
			undef $self->{tempname};
		}
	} else {
		my $file = $self->prepare_to_extract($state);

		print "extracting $destdir$fullname\n" if $state->{very_verbose};
		if ($state->{not}) {
			$state->{archive}->skip;
			return;
		} else {
			$file->create;
		}
	}
	$self->set_modes($destdir.$fullname);
}

sub prepare_to_extract
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	my $file=$state->{archive}->next;
	if (!defined $file) {
		Fatal "Error: truncated archive\n";
	}
	$file->{cwd} = $self->cwd;
	if (!$file->check_name($self)) {
		Fatal "Error: archive does not match ", $file->{name}, "!=",
		$self->{name}, "\n";
	}
	if (defined $self->{symlink} || $file->isSymLink) {
		unless (defined $self->{symlink} && $file->isSymLink) {
			Fatal "Error: bogus symlink ", $self->{name}, "\n";
		}
		if (!$file->check_linkname($self->{symlink})) {
			Fatal "Error: archive sl does not match ", $file->{linkname}, "!=",
			$self->{symlink}, "\n";
		}
	} elsif (defined $self->{link} || $file->isHardLink) {
		unless (defined $self->{link} && $file->isHardLink) {
			Fatal "Error: bogus hardlink ", $self->{name}, "\n";
		}
		if (!$file->check_linkname($self->{link})) {
			Fatal "Error: archive hl does not match ", $file->{linkname}, "!=",
			$self->{link}, "!!!\n";
		}
	}

	$file->{name} = $fullname;
	$file->{destdir} = $destdir;
	# faked installation are VERY weird
	if (defined $self->{symlink} && $state->{do_faked}) {
		$file->{linkname} = $destdir.$file->{linkname};
	}
	return $file;
}

package OpenBSD::PackingElement::EndFake;
sub install
{
	my ($self, $state) = @_;

	$state->{end_faked} = 1;
}

package OpenBSD::PackingElement::Sample;
use OpenBSD::Error;
use File::Copy;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;
	if (!defined $self->{copyfrom}) {
		Fatal "\@sample element does not reference a valid file\n";
	}
	my $fname = $state->{destdir}.$self->fullname;
	# If file already exists, we won't change it
	if (OpenBSD::Vstat::vexists($fname)) {
		return;
	}
	my $size = $self->{copyfrom}->{size};
	$state->{totsize} += $size if defined $size;
	my $s = OpenBSD::Vstat::add($fname, $size, \$pkgname);
	return unless defined $s;
	if ($s->{ro}) {
		$s->report_ro($state, $fname);
	}
	if ($state->{forced}->{kitchensink} && $state->{not}) {
		return;
	}
	if ($s->avail < 0) {
		$s->report_overflow($state, $fname);
	}
}

sub install
{
	my ($self, $state) = @_;

	my $destdir = $state->{destdir};
	my $filename = $destdir.$self->fullname;
	my $orig = $self->{copyfrom};
	my $origname = $destdir.$orig->fullname;
	if (-e $filename) {
		if ($state->{verbose}) {
		    print "The existing file $filename has NOT been changed\n";
		    if (defined $orig->{md5}) {
			require OpenBSD::md5;

			my $md5 = OpenBSD::md5::fromfile($filename);
			if ($md5 eq $orig->{md5}) {
			    print "(but it seems to match the sample file $origname)\n";
			} else {
			    print "It does NOT match the sample file $origname\n";
			    print "You may wish to update it manually\n";
			}
		    }
		}
	} else {
		if ($state->{not}) {
			print "The file $filename would be installed from $origname\n";
		} else {
			if (!copy($origname, $filename)) {
				Warn "File $filename could not be installed:\n\t$!\n";
			}
			$self->set_modes($filename);
			if ($state->{verbose}) {
			    print "installed $filename from $origname\n";
			}
		}
	}
}

package OpenBSD::PackingElement::Sampledir;

sub install
{
	&OpenBSD::PackingElement::Dir::install;
}

package OpenBSD::PackingElement::Mandir;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	$state->print("You may wish to add ", $self->fullname, " to /etc/man.conf\n");
}

package OpenBSD::PackingElement::Manpage;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	$self->register_manpage($state) unless $state->{not};
}

package OpenBSD::PackingElement::InfoFile;
use File::Basename;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	return if $state->{not};
	my $fullname = $state->{destdir}.$self->fullname;
	VSystem($state->{very_verbose}, 
	    "install-info", "--info-dir=".dirname($fullname), $fullname);
}

package OpenBSD::PackingElement::Shell;
sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	return if $state->{not};
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};
	# go append to /etc/shells if needed
	open(my $shells, '<', $destdir.'/etc/shells') or return;
	local $_;
	while(<$shells>) {
		s/^\#.*//;
		return if $_ =~ m/^\Q$fullname\E\s*$/;
	}
	close($shells);
	open(my $shells2, '>>', $destdir.'/etc/shells') or return;
	print $shells2 $fullname, "\n";
	close $shells2;
	print "Shell $fullname appended to $destdir/etc/shells\n";
}

package OpenBSD::PackingElement::Dir;
sub install
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	print "new directory ", $destdir, $fullname, "\n" if $state->{very_verbose};
	return if $state->{not};
	File::Path::mkpath($destdir.$fullname);
	$self->set_modes($destdir.$fullname);
}

package OpenBSD::PackingElement::Exec;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;

	$self->run($state);
}

package OpenBSD::PackingElement::Lib;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	return if $state->{do_faked};
	$self->mark_ldconfig_directory($state->{destdir});
}

package OpenBSD::PackingElement::SpecialFile;
use OpenBSD::PackageInfo;
use OpenBSD::Error;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;

	my $fname = installed_info($pkgname).$self->{name};
	my $cname = $state->{dir}.'/'.$self->{name};
	my $size = $self->{size};
	if (!defined $size) {
		$size = (stat $cname)[7];
	}
	if ($self->exec_on_add) {
		my $s2 = OpenBSD::Vstat::filestat($cname);
		if (defined $s2 && $s2->{noexec}) {
			$s2->report_noexec($state, $cname);
		}
	}
	my $s = OpenBSD::Vstat::add($fname, $self->{size}, \$pkgname);
	return unless defined $s;
	if ($s->{ro}) {
		$s->report_ro($state, $fname);
	}
	if ($s->{noexec} && $self->exec_on_delete) {
		$s->report_noexec($state, $fname);
	}
	if ($state->{forced}->{kitchensink} && $state->{not}) {
		return;
	}
	if ($s->avail < 0) {
		$s->report_overflow($state, $fname);
	}
}

1;
