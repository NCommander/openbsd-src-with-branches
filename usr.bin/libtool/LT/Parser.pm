# $OpenBSD: Parser.pm,v 1.8 2012/07/12 07:35:45 espie Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
# Copyright (c) 2012 Marc Espie <espie@openbsd.org>
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
use feature qw(say switch state);

package LT::Parser;
use File::Basename;
use Cwd qw(abs_path);
use LT::Util;
use LT::Library;
use LT::Trace;

my $calls = 0;

sub build_cache
{
	my ($self, $lainfo, $level) = @_;
	my $o = $lainfo->{cached} = {
	    deplibs => [], libdirs => [], result => []};
	$self->internal_resolve_la($o, $lainfo->deplib_list, 
	    $level+1);
	push(@{$o->{deplibs}}, @{$lainfo->deplib_list});
	if ($lainfo->{libdir} ne '') {
		push(@{$o->{libdirs}}, $lainfo->{libdir});
	}
	for my $e (qw(deplibs libdirs result)) {
		if (@{$o->{$e}} > 50) {
			$o->{$e} = reverse_zap_duplicates_ref($o->{$e});
		}
	}
}

sub internal_resolve_la
{
	my ($self, $o, $args, $level) = @_;
	$level //= 0;
	tsay {"resolve level: $level"};
	$o->{pthread} = 0;
	foreach my $a (@$args) {
		if ($a eq '-pthread') {
			$o->{pthread}++;
			next;
		}
		push(@{$o->{result}}, $a);
		next if $a !~ m/\.la$/;
		require LT::LaFile;
		my $lainfo = LT::LaFile->parse($a);
		if  (!exists $lainfo->{cached}) {
			$self->build_cache($lainfo, $level+1);
		}
		$o->{pthread} += $lainfo->{cached}{pthread};
		for my $e (qw(deplibs libdirs result)) {
			push(@{$o->{$e}}, @{$lainfo->{cached}{$e}});
		}
	}
	$calls++;
}

END
{
	LT::Trace::print { "Calls to resolve_la: $calls\n" } if $calls;
}

# resolve .la files until a level with empty dependency_libs is reached.
sub resolve_la
{
	my ($self, $deplibs, $libdirs) = @_;

	tsay {"argvstring (pre resolve_la): @{$self->{args}}"};
	my $o = { result => [], deplibs => $deplibs, libdirs => $libdirs};

	$self->internal_resolve_la($o, $self->{args});
	if ($o->{pthread}) {
		unshift(@{$o->{result}}, '-pthread');
		unshift(@{$o->{deplibs}}, '-pthread');
	}

	tsay {"argvstring (post resolve_la): @{$self->{args}}"};
	$self->{args} = $o->{result};
}

# parse link flags and arguments
# eliminate all -L and -l flags in the argument string and add the
# corresponding directories and library names to the dirs/libs hashes.
# fill deplibs, to be taken up as dependencies in the resulting .la file...
# set up a hash for library files which haven't been found yet.
# deplibs are formed by collecting the original -L/-l flags, plus
# any .la files passed on the command line, EXCEPT when the .la file
# does not point to a shared library.
# pass 1
# -Lfoo, -lfoo, foo.a, foo.la
# recursively find .la files corresponding to -l flags; if there is no .la
# file, just inspect the library file itself for any dependencies.
sub internal_parse_linkargs1
{
	my ($self, $deplibs, $gp, $dirs, $libs, $args, $level) = @_;

	$level //= 0;
	tsay {"parse_linkargs1, level: $level"};
	tsay {"  args: @$args"};
	my $result   = $self->{result};

	# first read all directories where we can search libraries
	foreach my $a (@$args) {
		if ($a =~ m/^-L(.*)/) {
			if (!exists $dirs->{$1}) {
				$dirs->{$1} = 1;
				tsay {"    adding $a to deplibs"} 
				    if $level == 0;
				push @$deplibs, $a;
			}
		}
	}
	foreach my $a (@$args) {
		tsay {"  processing $a"};
		if (!$a || $a eq '' || $a =~ m/^\s+$/) {
			# skip empty arguments
		} elsif ($a eq '-pthread' && !$self->{pthread}) {
			# XXX special treatment since it's not a -l flag
			push @$deplibs, $a;
			$self->{pthread} = 1;
			push(@$result, $a);
		} elsif ($a =~ m/^-L(.*)/) {
			# already read earlier, do nothing
		} elsif ($a =~ m/^-R(.*)/) {
			# -R options originating from .la resolution
			# those from @ARGV are in @Ropts
			$gp->add_R($1);
		} elsif ($a =~ m/^-l(\S+)/) {
			my @largs = ();
			my $key = $1;
			if (!exists $libs->{$key}) {
				$libs->{$key} = LT::Library->new($key);
				require LT::LaFile;
				my $lafile = LT::LaFile->find($key, $dirs);
				if ($lafile) {
					$libs->{$key}->{lafile} = $lafile;
					my $absla = abs_path($lafile);
					tsay {"    adding $absla to deplibs"} 
					    if $level == 0;
					push @$deplibs, $absla;
					push @$result, $lafile;
					next;
				} else {
					$libs->{$key}->resolve_library($dirs, 1, 0, 'notyet', $gp);
					my @deps = $libs->{$key}->inspect;
					foreach my $d (@deps) {
						my $k = basename($d);
						$k =~ s/^(\S+)\.so.*$/$1/;
						$k =~ s/^lib//;
						push(@largs, "-l$k");
					}
				}
			}
			tsay {"    adding $a to deplibs"} if $level == 0;
			push @$deplibs, $a;
			push(@$result, $a);
			my $dummy = []; # no need to add deplibs recursively
			$self->internal_parse_linkargs1($dummy, $gp, $dirs, $libs,
			       	\@largs, $level+1) if @largs;
		} elsif ($a =~ m/(\S+\/)*(\S+)\.a$/) {
			(my $key = $2) =~ s/^lib//;
			if (!exists $libs->{$key}) {
				$libs->{$key} = LT::Library->new($key);
			}
			$dirs->{abs_dir($a)} = 1;
			$libs->{$key}->{fullpath} = $a;
			push(@$result, $a);
		} elsif ($a =~ m/(\S+\/)*(\S+)\.la$/) {
			(my $key = $2) =~ s/^lib//;
			$dirs->{abs_dir($a)} = 1;
			my $fulla = abs_path($a);
			require LT::LaFile;
			my $lainfo = LT::LaFile->parse($fulla);
			my $dlname = $lainfo->{'dlname'};
			my $oldlib = $lainfo->{'old_library'};
			my $libdir = $lainfo->{'libdir'};
			if ($dlname ne '') {
				if (!exists $libs->{$key}) {
					$libs->{$key} = LT::Library->new($key);
					$libs->{$key}->{lafile} = $fulla;
				}
			}
			push(@$result, $a);
			push(@$deplibs, $fulla) if $libdir ne '';
		} elsif ($a =~ m/(\S+\/)*(\S+)\.so(\.\d+){2}/) {
			(my $key = $2) =~ s/^lib//;
			$dirs->{abs_dir($a)} = 1;
			if (!exists $libs->{$key}) {
				$libs->{$key} = LT::Library->new($key);
			}
			# not really normal argument
			# -lfoo should be used instead, so convert it
			push(@$result, "-l$key");
		} else {
			push(@$result, $a);
		}
	}
}

sub parse_linkargs1
{
	my ($self, $deplibs, $gp, $dirs, $libs, $args) = @_;
	$self->{result} = [];
	$self->internal_parse_linkargs1($deplibs, $gp, $dirs, $libs, 
	    $self->{args});
	$self->{args} = $self->{result};
}

# pass 2
# -Lfoo, -lfoo, foo.a
# no recursion in pass 2
# fill orderedlibs array, which is the sequence of shared libraries
#   after resolving all .la
# (this list may contain duplicates)
# fill staticlibs array, which is the sequence of static and convenience
#   libraries
# XXX the variable $parser->{seen_la_shared} will register whether or not
#     a .la file is found which refers to a shared library and which is not
#     yet installed
#     this is used to decide where to link executables and create wrappers
sub parse_linkargs2
{
	my ($self, $gp, $orderedlibs, $staticlibs, $dirs, $libs) = @_;
	tsay {"parse_linkargs2"};
	tsay {"  args: @{$self->{args}}"};
	$self->{result} = [];
	my $result = $self->{result};

	foreach my $a (@{$self->{args}}) {
		tsay {"  processing $a"};
		if (!$a || $a eq '' || $a =~ m/^\s+$/) {
			# skip empty arguments
		} elsif ($a eq '-lc') {
			# don't link explicitly with libc (just remove -lc)
		} elsif ($a eq '-pthread' && !$self->{pthread}) {
			# XXX special treatment since it's not a -l flag
			$self->{pthread} = 1;
			push(@$result, $a);
		} elsif ($a =~ m/^-L(.*)/) {
			if (!exists $dirs->{$1}) {
				$dirs->{$1} = 1;
			}
		} elsif ($a =~ m/^-R(.*)/) {
			# -R options originating from .la resolution
			# those from @ARGV are in @Ropts
			$gp->add_R($1);
		} elsif ($a =~ m/^-l(.*)/) {
			my @largs = ();
			my $key = $1;
			if (!exists $libs->{$key}) {
				$libs->{$key} = LT::Library->new($key);
			}
			push @$orderedlibs, $key;
		} elsif ($a =~ m/(\S+\/)*(\S+)\.a$/) {
			(my $key = $2) =~ s/^lib//;
			if (!exists $libs->{$key}) {
				$libs->{$key} = LT::Library->new($key);
			}
			$libs->{$key}->{fullpath} = $a;
			push(@$staticlibs, $a);
		} elsif ($a =~ m/(\S+\/)*(\S+)\.la$/) {
			(my $key = $2) =~ s/^lib//;
			my $d = abs_dir($a);
			$dirs->{$d} = 1;
			my $fulla = abs_path($a);
			require LT::LaFile;
			my $lainfo = LT::LaFile->parse($fulla);
			my $dlname = $lainfo->stringize('dlname');
			my $oldlib = $lainfo->stringize('old_library');
			my $installed = $lainfo->stringize('installed');
			if ($dlname ne '' && $installed eq 'no') {
				tsay {"seen uninstalled la shared in $a"};
				$self->{seen_la_shared} = 1;
			}
			if ($dlname eq '' && -f "$d/$ltdir/$oldlib") {
				push @$staticlibs, "$d/$ltdir/$oldlib";
			} else {
				if (!exists $libs->{$key}) {
					$libs->{$key} = LT::Library->new($key);
					$libs->{$key}->{lafile} = $fulla;
				}
				push @$orderedlibs, $key;
			}
		} elsif ($a =~ m/^-Wl,(\S+)/) {
			# libtool accepts a list of -Wl options separated
			# by commas, and possibly with a trailing comma
			# which is not accepted by the linker
			my @Wlflags = split(/,/, $1);
			foreach my $f (@Wlflags) {
				push(@$result, "-Wl,$f");
			}
		} else {
			push(@$result, $a);
		}
	}
	tsay {"end parse_linkargs2"};
	return $self->{result};
}

sub new
{
	my ($class, $args) = @_;
	bless { args => $args, pthread => 0 }, $class;
}
1;
