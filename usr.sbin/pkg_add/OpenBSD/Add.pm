# ex:ts=8 sw=4:
# $OpenBSD: Add.pm,v 1.20 2004/11/14 19:50:44 espie Exp $
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

package OpenBSD::Add;
use OpenBSD::Error;
use OpenBSD::PackageInfo;
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
			print "Merging manpages in $destdir$k: ", join(@l), "\n";
		} else {
			eval { OpenBSD::Makewhatis::merge($destdir.$k, \@l); };
			if ($@) {
				print STDERR "Error in makewhatis: $@\n";
			}
		}
	}
}

sub register_installation
{
	my ($dir, $dest, $plist) = @_;
	mkdir($dest);
	for my $i (info_names()) {
		copy($dir.$i, $dest);
	}
	$plist->to_installation();
}

sub validate_plist($$)
{
	my ($plist, $state) = @_;

	my $destdir = $state->{destdir};
	my $problems = 0;
	my $pkgname = $plist->pkgname();
	my $totsize = 0;

	my $extra = $plist->{extrainfo};
	if ($state->{cdrom_only} && ((!defined $extra) || $extra->{cdrom} ne 'yes')) {
	    Warn "Package $pkgname is not for cdrom.\n";
	    $problems++;
	}
	if ($state->{ftp_only} && ((!defined $extra) || $extra->{ftp} ne 'yes')) {
	    Warn "Package $pkgname is not for ftp.\n";
	    $problems++;
	}

	# check for collisions with existing stuff
	my $colliding = [];
	for my $item (@{$plist->{items}}) {
		next unless $item->IsFile();
		my $fname = $destdir.$item->fullname();
		if (OpenBSD::Vstat::vexists($fname)) {
			push(@$colliding, $item);
			$problems++;
		}
		$totsize += $item->{size} if defined $item->{size};
		my $s = OpenBSD::Vstat::add($fname, $item->{size});
		next unless defined $s;
		if ($s->{ro}) {
			Warn "Error: ", $s->{mnt}, " is read-only ($fname)\n";
			$problems++;
		}
		if ($s->avail() < 0) {
			Warn "Error: ", $s->{mnt}, " is not large enough ($fname)\n";
			$problems++;
		}
	}
	if (@$colliding > 0) {
		require OpenBSD::CollisionReport;

		OpenBSD::CollisionReport::collision_report($colliding, $state);
	}
	Fatal "fatal issues" if $problems;
	return $totsize;
}

sub borked_installation
{
	my ($plist, $dir) = @_;

	use OpenBSD::PackingElement;

	my $borked = borked_package();
	# fix packing list for pkg_delete
	$plist->{items} = $plist->{done};

	# last file may have not copied correctly
	my $last = $plist->{items}->[@{$plist->{items}}-1];
	if ($last->IsFile()) {
	    require OpenBSD::md5;

	    my $old = $last->{md5};
	    my $lastname;
	    if (defined $last->{tempname}) {
	    	$lastname = $last->{tempname};
	    } else {
	    	$lastname = $last->fullname();
	    }
	    $last->{md5} = OpenBSD::md5::fromfile($lastname);
	    if ($old ne $last->{md5}) {
		print "Adjusting md5 for $lastname from ",
		    unpack('H*', $old), " to ", unpack('H*', $last->{md5}), "\n";
	    }
	}
	OpenBSD::PackingElement::Cwd->add($plist, '.');
	my $pkgname = $plist->pkgname();
	$plist->{name}->{name} = $borked;
	$plist->{pkgdep} = [];
	my $dest = installed_info($borked);
	register_installation($dir, $dest, $plist);
	Fatal "Installation of $pkgname failed, partial installation recorded as $borked";
}

# used by newuser/newgroup to deal with options.
package OpenBSD::PackingElement;
use OpenBSD::Error;

my ($uidcache, $gidcache);

sub install
{
}

sub available_lib
{
}

sub set_modes
{
	my ($self, $name) = @_;

	if (defined $self->{owner} || defined $self->{group}) {
		require OpenBSD::IdCache;

		if (!defined $uidcache) {
			$uidcache = OpenBSD::UidCache->new();
			$gidcache = OpenBSD::GidCache->new();
		}
		my ($uid, $gid) = (stat $name)[4,5];
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

package OpenBSD::PackingElement::NewUser;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;
	my $user = $self->{name};
	print "adding user $user\n" if $state->{verbose};
	return if $state->{not};
	my $ok = $self->check();
	if (defined $ok) {
		if ($ok == 0) {
			Fatal "user $user does not match\n";
		}
	} else {
		my $l=[];
		push(@$l, "-v") if $state->{very_verbose};
		$self->add_entry($l, 
		    '-u', $self->{uid},
		    '-g', $self->{group},
		    '-L', $self->{class},
		    '-c', $self->{comment},
		    '-d', $self->{home},
		    '-s', $self->{shell});
		VSystem($state->{very_verbose}, '/usr/sbin/useradd', @$l, $user);
	}
}

package OpenBSD::PackingElement::NewGroup;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;
	my $group = $self->{name};
	print "adding group $group\n" if $state->{verbose};
	return if $state->{not};
	my $ok = $self->check();
	if (defined $ok) {
		if ($ok == 0) {
			Fatal "group $group does not match\n";
		}
	} else {
		my $l=[];
		push(@$l, "-v") if $state->{very_verbose};
		$self->add_entry($l, '-g', $self->{gid});
		VSystem($state->{very_verbose}, '/usr/sbin/groupadd', @$l, $group);
	}
}

package OpenBSD::PackingElement::Sysctl;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;

	my $name = $self->{name};
	open(my $pipe, '-|', '/sbin/sysctl', $name);
	my $actual = <$pipe>;
	chomp $actual;
	$actual =~ s/^\Q$name\E\s*\=\s*//;
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

sub install
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname();
	my $destdir = $state->{destdir};

	if ($state->{replacing}) {
		return if $state->{not};
		File::Path::mkpath(dirname($destdir.$fullname));
		if (defined $self->{link}) {
			link($destdir.$self->{link}, $destdir.$fullname);
		} elsif (defined $self->{symlink}) {
			symlink($self->{symlink}, $destdir.$fullname);
		} else {
			rename($self->{tempname}, $destdir.$fullname) or 
			    Fatal "Can't move ", $self->{tempname}, " to $fullname: $!";
			print "moving ", $self->{tempname}, " -> $destdir$fullname\n" if $state->{very_verbose};
			undef $self->{tempname};
		}
	} else {
		my $file = $self->prepare_to_extract($state);

		print "extracting $destdir$fullname\n" if $state->{very_verbose};
		return if $state->{not};
		$file->create();
	}
	$self->set_modes($destdir.$fullname);
}

sub prepare_to_extract
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname();
	my $destdir = $state->{destdir};

	my $file=$state->{archive}->next();
	if ($file->{name} ne $self->{name}) {
		Fatal "Error: archive does not match ", $file->{name}, "!=",
		$self->{name}, "\n";
	}
	if (defined $self->{symlink} || $file->isSymLink()) {
		unless (defined $self->{symlink} && $file->isSymLink()) {
			Fatal "Error: bogus symlink ", $self->{name}, "\n";
		}
		if ($self->{symlink} ne $file->{linkname}) {
			Fatal "Error: archive sl does not match ", $file->{linkname}, "!=",
			$self->{symlink}, "\n";
		}
	} elsif (defined $self->{link} || $file->isHardLink()) {
		unless (defined $self->{link} && $file->isHardLink()) {
			Fatal "Error: bogus hardlink ", $self->{name}, "\n";
		}
		my $linkname = $file->{linkname};
		if (defined $self->{cwd}) {
			$linkname = $self->cwd().'/'.$linkname;
		}
		if ($self->{link} ne $linkname) {
			Fatal "Error: archive hl does not match ", $linkname, "!=",
			$self->{link}, "!!!\n";
		}
	}

	$file->{name} = $fullname;
	$file->{cwd} = $self->cwd();
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

sub install
{
	my ($self, $state) = @_;

	my $destdir = $state->{destdir};
	my $filename = $destdir.$self->fullname();
	my $orig = $self->{copyfrom};
	if (!defined $orig) {
		Fatal "\@sample element does not reference a valid file\n";
	}
	my $origname = $destdir.$orig->fullname();
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
	$state->print("You may wish to add ", $self->fullname(), " to /etc/man.conf\n");
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
	my $fullname = $state->{destdir}.$self->fullname();
	VSystem($state->{very_verbose}, 
	    "install-info", "--info-dir=".dirname($fullname), $fullname);
}

package OpenBSD::PackingElement::Shell;
sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	return if $state->{not};
	my $fullname = $self->fullname();
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
	my $fullname = $self->fullname();
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

sub available_lib
{
	my ($self, $avail, $pkgname) = @_;
	my $fullname = $self->fullname();

	if ($fullname =~ m/^(.*\.so\.\d+)\.(\d+)$/) {
		my ($stem, $minor) = ($1, $2);
		if (!defined $avail->{"$stem"} || $avail->{"$stem"}->[0] < $minor) {
			$avail->{"$stem"} = [$minor, $pkgname];
		}
	}
}

package OpenBSD::PackingElement::Arch;

sub check
{
	my ($self, $forced_arch) = @_;

	my ($machine_arch, $arch);
	for my $ok (@{$self->{arches}}) {
		return 1 if $ok eq '*';
		if (defined $forced_arch) {
			if ($ok eq $forced_arch) {
				return 1;
			} else {
				next;
			}
		}
		if (!defined $machine_arch) {
			chomp($machine_arch = `/usr/bin/arch -s`);
		}
		return 1 if $ok eq $machine_arch;
		if (!defined $arch) {
			chomp($arch = `/usr/bin/uname -m`);
		}
		return 1 if $ok eq $arch;
	}
	return undef;
}


1;
