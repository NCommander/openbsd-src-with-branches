# ex:ts=8 sw=4:
# $OpenBSD: Link.pm,v 1.26 2014/04/16 10:31:27 zhuk Exp $
#
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
use feature qw(say);

# supplement OSConfig with stuff needed.
package LT::OSConfig;
require LT::UList;

my $search_dir_obj = tie(my @search_dir_list, 'LT::UList');

sub fillup_search_dirs
{
	return if @search_dir_list;
	open(my $fh, '-|', '/sbin/ldconfig -r');
	if (!defined $fh) {
		die "Can't run ldconfig\n";
	}
	while (<$fh>) {
		if (m/^\s*search directories:\s*(.*?)\s*$/o) {
			push @search_dir_list, split(/\:/o, $1);
			last;
		}
	}
	close($fh);
}

sub search_dirs
{
	my $self = shift;
	$self->fillup_search_dirs;
	return @search_dir_list;
}

sub is_search_dir
{
	my ($self, $dir) = @_;
	$self->fillup_search_dirs;
	return $search_dir_obj->exists($dir);
}


# let's add the libsearchdirs and -R options there
package LT::Options;

sub add_libsearchdir
{
	my $self = shift;
	push(@{$self->{libsearchdir}}, @_);
}

sub libsearchdirs
{
	my $self = shift;
	return @{$self->{libsearchdir}};
}

# -R options originating from .la resolution
sub add_R
{
	my $self = shift;
	push(@{$self->{Rresolved}}, @_);
}

sub Rresolved
{
	my $self = shift;
	$self->{Rresolved} //= [];
	return @{$self->{Rresolved}};
}

package LT::Mode::Link;
our @ISA = qw(LT::Mode);

use LT::Util;
use LT::Trace;
use LT::Library;
use File::Basename;

use constant {
	OBJECT	=> 0, # unused ?
	LIBRARY	=> 1,
	PROGRAM	=> 2,
};

sub help
{
	print <<"EOH";

Usage: $0 --mode=link LINK-COMMAND ...
Link object files and libraries into a library or a program
EOH
}

my $shared = 0;
my $static = 1;

sub run
{
	my ($class, $ltprog, $gp, $ltconfig) = @_;

	my $noshared  = $ltconfig->noshared;
	my $cmd;
	my $libdirs = [];			# list of libdirs
	tie (@$libdirs, 'LT::UList');
	my $libs = LT::Library::Stash->new;	# libraries
	my $dirs = [];				# paths to find libraries
	tie (@$dirs, 'LT::UList', '/usr/lib');	# always look here

	$gp->handle_permuted_options(
	    'all-static',
	    'allow-undefined', # we don't care about THAT one
	    'avoid-version',
	    'dlopen:',
	    'dlpreopen:',
	    'export-dynamic',
	    'export-symbols:',
	    '-export-symbols:', sub { shortdie "the option is -export-symbols.\n--export-symbols wil be ignored by gnu libtool"; },
	    'export-symbols-regex:',
	    'module',
	    'no-fast-install',
	    'no-install',
	    'no-undefined',
	    'o:!@',
	    'objectlist:',
	    'precious-files-regex:',
	    'prefer-pic',
	    'prefer-non-pic',
	    'release:',
	    'rpath:@',
	    'L:!', sub { shortdie "libtool does not allow spaces in -L dir\n"},
	    'R:@',
	    'shrext:',
	    'static',
	    'thread-safe', # XXX and --thread-safe ?
	    'version-info:',
	    'version-number:');

	# XXX options ignored: dlopen, dlpreopen, no-fast-install,
	# 	no-install, no-undefined, precious-files-regex,
	# 	shrext, thread-safe, prefer-pic, prefer-non-pic

	my @RPopts = $gp->rpath;	 # -rpath options
	my @Ropts = $gp->R;		 # -R options on the command line

	# add the .libs dir as well in case people try to link directly
	# with the real library instead of the .la library
	$gp->add_libsearchdir(LT::OSConfig->search_dirs, './.libs');

	if (!$gp->o) {
		shortdie "No output file given.\n";
	}
	if ($gp->o > 1) {
		shortdie "Multiple output files given.\n";
	}

	my $outfile = ($gp->o)[0];
	tsay {"outfile = $outfile"};
	my $odir = dirname($outfile);
	my $ofile = basename($outfile);

	# what are we linking?
	my $linkmode = PROGRAM;
	if ($ofile =~ m/\.l?a$/) {
		$linkmode = LIBRARY;
		$gp->handle_permuted_options('x:!');
	}
	tsay {"linkmode: $linkmode"};

	my @objs;
	my @sobjs;
	if ($gp->objectlist) {
		my $objectlist = $gp->objectlist;
		open(my $ol, '<', $objectlist) or die "Cannot open $objectlist: $!\n";
		my @objlist = <$ol>;
		for (@objlist) { chomp; }
		generate_objlist(\@objs, \@sobjs, \@objlist);
	} else {
		generate_objlist(\@objs, \@sobjs, \@ARGV);
	}
	tsay {"objs = @objs"};
	tsay {"sobjs = @sobjs"};

	my $deplibs = [];	# list of dependent libraries (both -L and -l flags)
	tie (@$deplibs, 'LT::UList');
	my $parser = LT::Parser->new(\@ARGV);

	if ($linkmode == PROGRAM) {
		require LT::Mode::Link::Program;
		my $program = LT::Program->new;
		$program->{outfilepath} = $outfile;
		# XXX give higher priority to dirs of not installed libs
		if ($gp->export_dynamic) {
			push(@{$parser->{args}}, "-Wl,-E");
		}

		$parser->parse_linkargs1($deplibs, $gp, $dirs, $libs);
		tsay {"end parse_linkargs1"};
		tsay {"deplibs = @$deplibs"};

		$program->{objlist} = \@objs;
		if (@objs == 0) {
			if (@sobjs > 0) {
				tsay {"no non-pic libtool objects found, trying pic objects..."};
				$program->{objlist} = \@sobjs;
			} elsif (@sobjs == 0) {
				tsay {"no libtool objects of any kind found"};
				tsay {"hoping for real objects in ARGV..."};
			}
		}
		tie(my @temp, 'LT::UList', @Ropts, @RPopts, $gp->Rresolved);
		my $RPdirs = \@temp;
		$program->{RPdirs} = $RPdirs;

		$program->link($ltprog, $ltconfig, $dirs, $libs, $deplibs, $libdirs, $parser, $gp);
	} elsif ($linkmode == LIBRARY) {
		my $convenience = 0;
		require LT::Mode::Link::Library;
		my $lainfo = LT::LaFile->new;

		$shared = 1 if ($gp->version_info ||
				$gp->avoid_version ||
				$gp->module);
		if (!@RPopts) {
			$convenience = 1;
			$noshared = 1;
			$static = 1;
			$shared = 0;
		} else {
			$shared = 1;
		}
		if ($ofile =~ m/\.a$/ && !$convenience) {
			$ofile =~ s/\.a$/.la/;
			$outfile =~ s/\.a$/.la/;
		}
		(my $libname = $ofile) =~ s/\.l?a$//;	# remove extension
		my $staticlib = $libname.'.a';
		my $sharedlib = $libname.'.so';
		my $sharedlib_symlink;

		if ($gp->static || $gp->all_static) {
			$shared = 0;
			$static = 1;
		}
		$shared = 0 if $noshared;

		$parser->parse_linkargs1($deplibs, $gp, $dirs, $libs);
		tsay {"end parse_linkargs1"};
		tsay {"deplibs = @$deplibs"};

		my $sover = '0.0';
		my $origver = 'unknown';
		# environment overrides -version-info
		(my $envlibname = $libname) =~ s/[.+-]/_/g;
		my ($current, $revision, $age) = (0, 0, 0);
		if ($gp->version_info) {
			($current, $revision, $age) = parse_version_info($gp->version_info);
			$origver = "$current.$revision";
			$sover = $origver;
		}
		if ($ENV{"${envlibname}_ltversion"}) {
			# this takes priority over the previous
			$sover = $ENV{"${envlibname}_ltversion"};
			($current, $revision) = split /\./, $sover;
			$age = 0;
		}
		if (defined $gp->release) {
			$sharedlib_symlink = $sharedlib;
			$sharedlib = $libname.'-'.$gp->release.'.so';
		}
		if ($gp->avoid_version ||
			(defined $gp->release && !$gp->version_info)) {
			# don't add a version in these cases
		} else {
			$sharedlib .= ".$sover";
			if (defined $gp->release) {
				$sharedlib_symlink .= ".$sover";
			}
		}

		# XXX add error condition somewhere...
		$static = 0 if $shared && $gp->has_tag('disable-static');
		$shared = 0 if $static && $gp->has_tag('disable-shared');

		tsay {"SHARED: $shared\nSTATIC: $static"};

		$lainfo->{libname} = $libname;
		if ($shared) {
			$lainfo->{dlname} = $sharedlib;
			$lainfo->{library_names} = $sharedlib;
			$lainfo->{library_names} .= " $sharedlib_symlink"
				if defined $gp->release;
			$lainfo->link($ltprog, $ltconfig, $ofile, $sharedlib, $odir, 1, \@sobjs, $dirs, $libs, $deplibs, $libdirs, $parser, $gp);
			tsay {"sharedlib: $sharedlib"};
			$lainfo->{current} = $current;
			$lainfo->{revision} = $revision;
			$lainfo->{age} = $age;
		}
		if ($static) {
			$lainfo->{old_library} = $staticlib;
			$lainfo->link($ltprog, $ltconfig, $ofile, $staticlib, $odir, 0, ($convenience && @sobjs > 0) ? \@sobjs : \@objs, $dirs, $libs, $deplibs, $libdirs, $parser, $gp);
			tsay {($convenience ? "convenience" : "static"),
			    " lib: $staticlib"};
		}
		$lainfo->{installed} = 'no';
		$lainfo->{shouldnotlink} = $gp->module ? 'yes' : 'no';
		map { $_ = "-R$_" } @Ropts;
		unshift @$deplibs, @Ropts if @Ropts;
		tsay {"deplibs = @$deplibs"};
		$lainfo->set('dependency_libs', "@$deplibs");
		if (@RPopts) {
			if (@RPopts > 1) {
				tsay {"more than 1 -rpath option given, ",
				    "taking the first: ", $RPopts[0]};
			}
			$lainfo->{libdir} = $RPopts[0];
		}
		if (!($convenience && $ofile =~ m/\.a$/)) {
			$lainfo->write($outfile, $ofile);
			unlink("$odir/$ltdir/$ofile");
			symlink("../$ofile", "$odir/$ltdir/$ofile");
		}
		my $lai = "$odir/$ltdir/$ofile".'i';
		if ($shared) {
			my $pdeplibs = process_deplibs($deplibs);
			if (defined $pdeplibs) {
				$lainfo->set('dependency_libs', "@$pdeplibs");
			}
			if (! $gp->module) {
				$lainfo->write_shared_libs_log($origver);
			}
		}
		$lainfo->{'installed'} = 'yes';
		# write .lai file (.la file that will be installed)
		$lainfo->write($lai, $ofile);
	}
}

# populate arrays of non-pic and pic objects and remove these from @ARGV
sub generate_objlist
{
	my $objs = shift;
	my $sobjs = shift;
	my $objsource = shift;

	my $result = [];
	foreach my $a (@$objsource) {
		if ($a =~ m/\S+\.lo$/) {
			require LT::LoFile;
			my $ofile = basename($a);
			my $odir = dirname($a);
			my $loinfo = LT::LoFile->parse($a);
			if ($loinfo->{'non_pic_object'}) {
				my $o;
				$o .= "$odir/" if ($odir ne '.');
				$o .= $loinfo->{'non_pic_object'};
				push @$objs, $o;
			}
			if ($loinfo->{'pic_object'}) {
				my $o;
				$o .= "$odir/" if ($odir ne '.');
				$o .= $loinfo->{'pic_object'};
				push @$sobjs, $o;
			}
		} elsif ($a =~ m/\S+\.o$/) {
			push @$objs, $a;
		} else {
			push @$result, $a;
		}
	}
	@$objsource = @$result;
}

# convert 4:5:8 into a list of numbers
sub parse_version_info
{
	my $vinfo = shift;

	if ($vinfo =~ m/^(\d+):(\d+):(\d+)$/) {
		return ($1, $2, $3);
	} elsif ($vinfo =~ m/^(\d+):(\d+)$/) {
		return ($1, $2, 0);
	} elsif ($vinfo =~ m/^(\d+)$/) {
		return ($1, 0, 0);
	} else {
		die "Error parsing -version-info $vinfo\n";
	}
}

# prepare dependency_libs information for the .la file which is installed
# i.e. remove any .libs directories and use the final libdir for all the
# .la files
sub process_deplibs
{
	my $linkflags = shift;

	my $result;

	foreach my $lf (@$linkflags) {
		if ($lf =~ m/-L\S+\Q$ltdir\E$/) {
		} elsif ($lf =~ m/-L\./) {
		} elsif ($lf =~ m/\/\S+\/(\S+\.la)/) {
			my $lafile = $1;
			require LT::LaFile;
			my $libdir = LT::LaFile->parse($lf)->{'libdir'};
			if ($libdir eq '') {
				# this drops libraries which will not be
				# installed
				# XXX improve checks when adding to deplibs
				say "warning: $lf dropped from deplibs";
			} else {
				push @$result, $libdir.'/'.$lafile;
			}
		} else {
			push @$result, $lf;
		}
	}
	return $result;
}

package LT::Parser;
use File::Basename;
use Cwd qw(abs_path);
use LT::UList;
use LT::Util;
use LT::Trace;

my $calls = 0;

sub build_cache
{
	my ($self, $lainfo, $level) = @_;
	my $o = $lainfo->{cached} = {
	    deplibs => [], libdirs => [], result => [] };
	tie @{$o->{deplibs}}, 'LT::UList';
	tie @{$o->{libdirs}}, 'LT::UList';
	tie @{$o->{result}},  'LT::UList';
	$self->internal_resolve_la($o, $lainfo->deplib_list,
	    $level+1);
	push(@{$o->{deplibs}}, @{$lainfo->deplib_list});
	if ($lainfo->{libdir} ne '') {
		push(@{$o->{libdirs}}, $lainfo->{libdir});
	}
}

sub internal_resolve_la
{
	my ($self, $o, $args, $level) = @_;
	$level //= 0;
	tsay {"resolve level: $level"};
	$o->{pthread} = 0;
	foreach my $arg (@$args) {
# XXX still needed?
		if ($arg eq '-pthread') {
			$o->{pthread}++;
			next;
		}
		push(@{$o->{result}}, $arg);
		next unless $arg =~ m/\.la$/;
		require LT::LaFile;
		my $lainfo = LT::LaFile->parse($arg);
		if  (!exists $lainfo->{cached}) {
			$self->build_cache($lainfo, $level+1);
		}
		$o->{pthread} += $lainfo->{cached}{pthread};
		for my $e (qw(deplibs libdirs result)) {
LT::Trace::print { "Calls to resolve_la: $calls\n" } if $calls;
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

# XXX still needed?
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
	foreach my $arg (@$args) {
		if ($arg =~ m/^-L(.*)/) {
			push(@$dirs, $1);
			# XXX could be not adding actually, this is UList
			tsay {"    adding $_ to deplibs"}
			    if $level == 0;
			push(@$deplibs, $arg);
		}
	}
	foreach my $arg (@$args) {
		tsay {"  processing $arg"};
		if (!$arg || $arg eq '' || $arg =~ m/^\s+$/) {
			# skip empty arguments
		} elsif ($arg =~ m/^-Wc,(.*)/) {
			push(@$result, $1);
		} elsif ($arg eq '-Xcompiler') {
			next;
		} elsif ($arg eq '-pthread') {
			$self->{pthread} = 1;
		} elsif ($arg =~ m/^-L(.*)/) {
			# already read earlier, do nothing
		} elsif ($arg =~ m/^-R(.*)/) {
			# -R options originating from .la resolution
			# those from @ARGV are in @Ropts
			$gp->add_R($1);
		} elsif ($arg =~ m/^-l(\S+)/) {
			my @largs = ();
			my $key = $1;
			if (!exists $libs->{$key}) {
				$libs->create($key);
				require LT::LaFile;
				my $lafile = LT::LaFile->find($key, $dirs);
				if ($lafile) {
					$libs->{$key}->{lafile} = $lafile;
					my $absla = abs_path($lafile);
					tsay {"    adding $absla to deplibs"}
					    if $level == 0;
					push(@$deplibs, $absla);
					push(@$result, $lafile);
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
			tsay {"    adding $arg to deplibs"} if $level == 0;
			push(@$deplibs, $arg);
			push(@$result, $arg);
			my $dummy = []; # no need to add deplibs recursively
			$self->internal_parse_linkargs1($dummy, $gp, $dirs,
			    $libs, \@largs, $level+1) if @largs;
		} elsif ($arg =~ m/(\S+\/)*(\S+)\.a$/) {
			(my $key = $2) =~ s/^lib//;
			push(@$dirs, abs_dir($arg));
			$libs->create($key)->{fullpath} = $arg;
			push(@$result, $arg);
		} elsif ($arg =~ m/(\S+\/)*(\S+)\.la$/) {
			(my $key = $2) =~ s/^lib//;
			push(@$dirs, abs_dir($arg));
			my $fulla = abs_path($arg);
			require LT::LaFile;
			my $lainfo = LT::LaFile->parse($fulla);
			my $dlname = $lainfo->{dlname};
			my $oldlib = $lainfo->{old_library};
			my $libdir = $lainfo->{libdir};
			if ($dlname ne '') {
				if (!exists $libs->{$key}) {
					$libs->create($key)->{lafile} = $fulla;
				}
			}
			push(@$result, $arg);
			push(@$deplibs, $fulla) if $libdir ne '';
		} elsif ($arg =~ m/(\S+\/)*(\S+)\.so(\.\d+){2}/) {
			(my $key = $2) =~ s/^lib//;
			push(@$dirs, abs_dir($arg));
			$libs->create($key);
			# not really normal argument
			# -lfoo should be used instead, so convert it
			push(@$result, "-l$key");
		} else {
			push(@$result, $arg);
		}
	}
}

sub parse_linkargs1
{
	my ($self, $deplibs, $gp, $dirs, $libs, $args) = @_;
	$self->{result} = [];
	$self->internal_parse_linkargs1($deplibs, $gp, $dirs, $libs,
	    $self->{args});
	push(@$deplibs, '-pthread') if $self->{pthread};
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
	my $result = [];

	foreach my $arg (@{$self->{args}}) {
		tsay {"  processing $arg"};
		if (!$arg || $arg eq '' || $arg =~ m/^\s+$/) {
			# skip empty arguments
		} elsif ($arg eq '-lc') {
			# don't link explicitly with libc (just remove -lc)
		} elsif ($arg eq '-pthread') {
			$self->{pthread} = 1;
		} elsif ($arg =~ m/^-L(.*)/) {
			push(@$dirs, $1);
		} elsif ($arg =~ m/^-R(.*)/) {
			# -R options originating from .la resolution
			# those from @ARGV are in @Ropts
			$gp->add_R($1);
		} elsif ($arg =~ m/^-l(.*)/) {
			my @largs = ();
			my $key = $1;
			$libs->create($key);
			push(@$orderedlibs, $key);
		} elsif ($arg =~ m/(\S+\/)*(\S+)\.a$/) {
			(my $key = $2) =~ s/^lib//;
			$libs->create($key)->{fullpath} = $arg;
			push(@$staticlibs, $arg);
		} elsif ($arg =~ m/(\S+\/)*(\S+)\.la$/) {
			(my $key = $2) =~ s/^lib//;
			my $d = abs_dir($arg);
			push(@$dirs, $d);
			my $fulla = abs_path($arg);
			require LT::LaFile;
			my $lainfo = LT::LaFile->parse($fulla);
			my $dlname = $lainfo->stringize('dlname');
			my $oldlib = $lainfo->stringize('old_library');
			my $installed = $lainfo->stringize('installed');
			if ($dlname ne '' && $installed eq 'no') {
				tsay {"seen uninstalled la shared in $arg"};
				$self->{seen_la_shared} = 1;
			}
			if ($dlname eq '' && -f "$d/$ltdir/$oldlib") {
				push(@$staticlibs, "$d/$ltdir/$oldlib");
			} else {
				if (!exists $libs->{$key}) {
					$libs->create($key)->{lafile} = $fulla;
				}
				push(@$orderedlibs, $key);
			}
		} elsif ($arg =~ m/^-Wl,(\S+)$/) {
			# libtool accepts a list of -Wl options separated
			# by commas, and possibly with a trailing comma
			# which is not accepted by the linker
			my @Wlflags = split(/,/, $1);
			foreach my $f (@Wlflags) {
				push(@$result, "-Wl,$f");
			}
		} else {
			push(@$result, $arg);
		}
	}
	tsay {"end parse_linkargs2"};
	return $result;
}

sub new
{
	my ($class, $args) = @_;
	bless { args => $args, pthread => 0 }, $class;
}

package LT::Linker;
use LT::Trace;
use LT::Util;
use File::Basename;
use Cwd qw(abs_path);

sub new
{
	my $class = shift;
	bless {}, $class;
}

sub create_symlinks
{
	my ($self, $dir, $libs) = @_;
	if (! -d $dir) {
		mkdir($dir) or die "Cannot mkdir($dir) : $!\n";
	}

	foreach my $l (values %$libs) {
		my $f = $l->{fullpath};
		next if !defined $f;
		next if $f =~ m/\.a$/;
		my $libnames = [];
		tie (@$libnames, 'LT::UList');
		if (defined $l->{lafile}) {
			require LT::LaFile;
			my $lainfo = LT::LaFile->parse($l->{lafile});
			my $librarynames = $lainfo->stringize('library_names');
			push @$libnames, split(/\s/, $librarynames);
		} else {
			push @$libnames, basename($f);
		}
		foreach my $libfile (@$libnames) {
			my $link = "$dir/$libfile";
			tsay {"ln -s $f $link"};
			next if -f $link;
			my $p = abs_path($f);
			if (!symlink($p, $link)) {
				die "Cannot create symlink($p, $link): $!\n"
				    unless  $!{EEXIST};
			}
		}
	}
	return $dir;
}

sub common1
{
	my ($self, $parser, $gp, $deplibs, $libdirs, $dirs, $libs) = @_;

	$parser->resolve_la($deplibs, $libdirs);
	my $orderedlibs = [];
	tie(@$orderedlibs, 'LT::UList');
	my $staticlibs = [];
	my $args = $parser->parse_linkargs2($gp, $orderedlibs, $staticlibs, $dirs,
	    $libs);
	tsay {"staticlibs = \n", join("\n", @$staticlibs)};
	tsay {"orderedlibs = @$orderedlibs"};
	return ($staticlibs, $orderedlibs, $args);
}

sub infer_libparameter
{
	my ($self, $a, $k) = @_;
	my $lib = basename($a);
	if ($lib =~ m/^lib(.*)\.so(\.\d+){2}$/) {
		$lib = $1;
	} elsif ($lib =~ m/^lib(.*)\.so$/) {
		say "warning: library filename $a has no version number";
		$lib = $1;
	} else {
		say "warning: cannot derive -l flag from library filename $a, assuming hash key -l$k";
		$lib = $k;
	}
	return "-l$lib";
}

sub export_symbols
{
	my ($self, $ltconfig, $base, $gp, @o) = @_;
	my $symbolsfile;
	my $comment;
	if ($gp->export_symbols) {
		$symbolsfile = $gp->export_symbols;
		$comment = "/* version script derived from $symbolsfile */\n\n";
	} elsif ($gp->export_symbols_regex) {
		($symbolsfile = $base) =~ s/\.la$/.exp/;
		LT::Archive->get_symbollist($symbolsfile, $gp->export_symbols_regex, \@o);
		$comment = "/* version script generated from\n * ".join(' ', @o)."\n * using regexp ".$gp->export_symbols_regex. " */\n\n";
	} else {
		return ();
	}
	my $scriptfile;
	($scriptfile = $base) =~ s/(\.la)?$/.ver/;
	if ($ltconfig->{elf}) {
		open my $fh, ">", $scriptfile or die;
		open my $fh2, '<', $symbolsfile or die;
		print $fh $comment;
		print $fh "{\n";
		my $first = 1;
		while (<$fh2>) {
			chomp;
			if ($first) {
				print $fh "\tglobal:\n";
				$first = 0;
			}
			print $fh "\t\t$_;\n";
		}
		print $fh "\tlocal:\n\t\t\*;\n};\n";
		close($fh);
		close($fh2);
		return ("--version-script", $scriptfile);
	} else {
		return ("-retain-symbols-file", $symbolsfile);
	}
}

1;

