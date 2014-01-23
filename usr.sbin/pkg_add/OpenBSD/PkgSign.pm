#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: PkgSign.pm,v 1.2 2014/01/23 12:28:48 espie Exp $
#
# Copyright (c) 2003-2014 Marc Espie <espie@openbsd.org>
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

use OpenBSD::AddCreateDelete;
use OpenBSD::Signer;

package OpenBSD::PkgSign::State;
our @ISA = qw(OpenBSD::CreateSign::State);

sub handle_options
{
	my $state = shift;

	$state->{opt} = {
	    'o' =>
		    sub {
			    $state->{output_dir} = shift;
		    },
	    'S' =>
		    sub {
			    $state->{source} = shift;
		    },
	};
	$state->SUPER::handle_options('j:o:S:',
	    '[-v] [-D name[=value]] -s x509|signify [-s cert] -s priv',
	    '[-o dir] [-S source] [pkg-name...]');
    	if (!defined $state->{signer}) {
		$state->usage("Can't invoke command without valid signing parameters");
	}
	if (!-d $state->{output_dir}) {
		require File::Path;
		File::Path::make_path($state->{output_dir})
		    or $state->usage("can't create dir");
	}
}

package OpenBSD::PackingElement;
sub copy_over
{
}

package OpenBSD::PackingElement::SpecialFile;
sub copy_over
{
	my ($self, $state, $wrarc, $rdarc) = @_;
	$wrarc->destdir($rdarc->info);
	my $e = $wrarc->prepare($self->{name});
	$e->write;
}

package OpenBSD::PackingElement::FileBase;
sub copy_over
{
	my ($self, $state, $wrarc, $rdarc) = @_;
	my $e = $rdarc->next;
	$e->copy($wrarc);
}

package OpenBSD::PkgSign;
use OpenBSD::Temp;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;

sub sign_existing_package
{
	my ($self, $state, $pkg) = @_;
	my $output = $state->{output_dir} // ".";
	my $dir = $pkg->info;
	my $plist = OpenBSD::PackingList->fromfile($dir.CONTENTS);
	$plist->set_infodir($dir);
	$state->add_signature($plist);
	$plist->save;
	my $tmp = OpenBSD::Temp::permanent_file($output, "pkg");
	my $wrarc = $state->create_archive($tmp, ".");
	$plist->copy_over($state, $wrarc, $pkg);
	$wrarc->close;
	$pkg->wipe_info;
	chmod((0666 & ~umask), $tmp);
	rename($tmp, $output.'/'.$plist->pkgname.".tgz") or
	    $state->fatal("Can't create final signed package: #1", $!);
	$state->system(sub {
	    chdir($output);
	    open(STDOUT, '>>', 'SHA256');
	    },
	    OpenBSD::Paths->sha256, '-b', $plist->pkgname.".tgz");
}

sub sign_list
{
	my ($self, $l, $repo, $maxjobs, $state) = @_;
	$state->{total} = scalar @$l;
	$maxjobs //= 1;
	my $code = sub {
		my $pkg = $repo->find(shift);
		$self->sign_existing_package($state, $pkg);
	    };
	my $display = $state->verbose ?
	    sub {
		$state->progress->set_header("Signed ".shift);
		$state->{done}++;
		$state->progress->next($state->ntogo);
	    } :
	    sub {
	    };
	if ($maxjobs > 1) {
		my $jobs = {};
		my $n = 0;
		my $reap_job = sub {
			my $pid = wait;
			if (!defined $jobs->{$pid}) {
				$state->fatal("Wait returned #1: unknown process", $pid);
			}
			if ($? != 0) {
				$state->fatal("Signature of #1 failed\n", 
				    $jobs->{$pid});
			}
			$n--;
			&$display($jobs->{$pid});
			delete $state->{signer}{pubkey};
			delete $jobs->{$pid};
		};
			
		while (@$l > 0) {
			my $name = shift @$l;
			my $pid = fork();
			if ($pid == 0) {
				$repo->reinitialize;
				&$code($name);
				exit(0);
			} else {
				$jobs->{$pid} = $name;
				$n++;
			}
			if ($n >= $maxjobs) {
				&$reap_job;
			}
		}
		while ($n != 0) {
			&$reap_job;
		}
	} else {
		for my $name (@$l) {
			&$code($name);
			&$display($name);
			delete $state->{signer}{pubkey};
		}
	}
	$state->system(sub {
	    chdir($state->{output_dir}) if $state->{output_dir};
	    open(STDOUT, '>', 'SHA256.new');
	    }, 'sort', 'SHA256');
	rename($state->{output_dir}.'/SHA256.new', 
	    $state->{output_dir}.'/SHA256');
}

sub sign_existing_repository
{
	my ($self, $state, $source) = @_;
	require OpenBSD::PackageRepository;
	my $repo = OpenBSD::PackageRepository->new($source, $state);
	my @list = sort @{$repo->list};
	if (@list == 0) {
		$state->errsay('Source repository "#1" is empty', $source);
    	}
	$self->sign_list(\@list, $repo, $state->opt('j'), $state);
}


sub parse_and_run
{
	my ($self, $cmd) = @_;
	my $state = OpenBSD::PkgSign::State->new($cmd);
	$state->handle_options;
	$state->{wantntogo} = $state->config->istrue("ntogo");
	if (!defined $state->{source} && @ARGV == 0) {
		$state->usage("Nothing to sign");
	}
	if (defined $state->{source}) {
		$self->sign_existing_repository($state, 
		    $state->{source});
	}
	$self->sign_list(\@ARGV, $state->repo, $state->opt('j'), 
	    $state);
	return 0;
}

