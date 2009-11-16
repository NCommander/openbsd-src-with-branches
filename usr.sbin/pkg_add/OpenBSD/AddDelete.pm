# ex:ts=8 sw=4:
# $OpenBSD$
#
# Copyright (c) 2007-2009 Marc Espie <espie@openbsd.org>
#

use strict;
use warnings;

# common framework, let's place most everything in there
package main;
our $not;

package OpenBSD::AddDelete;
use OpenBSD::Getopt;
use OpenBSD::Error;
use OpenBSD::Paths;

our $bad = 0;
our %defines = ();
our $state;

our ($opt_n, $opt_x, $opt_v, $opt_B, $opt_L, $opt_i, $opt_q, $opt_c, $opt_I);
$opt_v = 0;

sub setup_state
{
	lock_db($opt_n) unless $state->{defines}->{nolock};
	$state->setup_progressmeter($opt_x);
	$state->check_root;
}

sub handle_options
{
	my ($opt_string, $hash) = @_;
	$hash->{h} = sub { Usage(); };
	$hash->{f} = $hash->{F} = sub { 
		for my $o (split /\,/o, shift) { 
			$defines{$o} = 1;
		}
	};
	try {
		getopts('hciInqvxB:f:F:L:'.$opt_string, $hash);
	} catchall {
		Usage($_);
	};

	$opt_L = OpenBSD::Paths->localbase unless defined $opt_L;

	$state->{recorder} = OpenBSD::SharedItemsRecorder->new;
	$state->{not} = $opt_n;
	# XXX RequiredBy
	$main::not = $opt_n;
	$state->{defines} = \%defines;
	$state->{very_verbose} = $opt_v >= 2;
	$state->{verbose} = $opt_v;
	$state->{interactive} = $opt_i;
	$state->{beverbose} = $opt_n || ($opt_v >= 2);
	$state->{localbase} = $opt_L;
	$state->{quick} = $opt_q;
	$state->{extra} = $opt_c;
	$state->{dont_run_scripts} = $opt_I;
}

sub do_the_main_work
{
	my $code = shift;

	if ($bad) {
		exit(1);
	}

	my $handler = sub { my $sig = shift; die "Caught SIG$sig"; };
	local $SIG{'INT'} = $handler;
	local $SIG{'QUIT'} = $handler;
	local $SIG{'HUP'} = $handler;
	local $SIG{'KILL'} = $handler;
	local $SIG{'TERM'} = $handler;

	if ($state->{defines}->{debug}) {
		&$code;
	} else { 
		eval { &$code; };
	}
	my $dielater = $@;
	# cleanup various things
	$state->{recorder}->cleanup($state);
	OpenBSD::PackingElement::Lib::ensure_ldconfig($state);
	OpenBSD::PackingElement::Fontdir::finish_fontdirs($state);
	if ($state->{beverbose}) {
		OpenBSD::Vstat::tally();
	}
	$state->progress->clear;
	$state->log->dump;
	return $dielater;
}

package OpenBSD::SharedItemsRecorder;
sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub is_empty
{
	my $self = shift;
	return !(defined $self->{dirs} or defined $self->{users} or
		defined $self->{groups});
}

sub cleanup
{
	my ($self, $state) = @_;
	return if $self->is_empty or $state->{not};

	require OpenBSD::SharedItems;
	OpenBSD::SharedItems::cleanup($self, $state);
}

package OpenBSD::Log;
use OpenBSD::Error;
our @ISA = qw(OpenBSD::Error);

sub set_context
{
	&OpenBSD::Error::set_pkgname;
}

sub dump
{
	&OpenBSD::Error::delayed_output;
}


package OpenBSD::UI;
use OpenBSD::Error;

sub new
{
	my $class = shift;
	my $o = bless {}, $class;
	$o->init(@_);
	return $o;
}

sub init
{
	my $self = shift;
	$self->{l} = OpenBSD::Log->new;
	$self->{progressmeter} = bless {}, "OpenBSD::StubProgress";
}

sub log
{
	my $self = shift;
	if (@_ == 0) {
		return $self->{l};
	} else {
		$self->{l}->print(@_);
	}
}

sub print
{
	my $self = shift;
	$self->progress->print(@_);
}

sub say
{
	my $self = shift;
	$self->progress->print(@_, "\n");
}

sub errprint
{
	my $self = shift;
	$self->progress->errprint(@_);
}

sub errsay
{
	my $self = shift;
	$self->progress->errprint(@_, "\n");
}

sub progress
{
	my $self = shift;
	return $self->{progressmeter};
}

sub vsystem
{
	my $self = shift;
	$self->progress->clear;
	OpenBSD::Error::VSystem($self->{very_verbose}, @_);
}

sub system
{
	my $self = shift;
	$self->progress->clear;
	OpenBSD::Error::System(@_);
}

sub unlink
{
	my $self = shift;
	$self->progress->clear;
	OpenBSD::Error::Unlink(@_);
}

# we always have a progressmeter we can print to...
sub setup_progressmeter
{
	my ($self, $opt_x) = @_;
	if (!$opt_x && !$self->{beverbose}) {
		require OpenBSD::ProgressMeter;
		$self->{progressmeter} = OpenBSD::ProgressMeter->new;
	}
}

sub check_root
{
	my $state = shift;
	if ($< && !$state->{defines}->{nonroot}) {
		if ($state->{not}) {
			$state->errsay("$0 should be run as root");
		} else {
			Fatal "$0 must be run as root";
		}
	}
}

sub choose_location
{
	my ($state, $name, $list) = @_;
	if (@$list == 0) {
		$state->say("Can't find $name");
		return undef;
	} elsif (@$list == 1) {
		return $list->[0];
	}

	my %h = map {($_->name, $_)} @$list;
	if ($state->{interactive}) {
		require OpenBSD::Interactive;

		$h{'<None>'} = undef;
		$state->progress->clear;
		my $result = OpenBSD::Interactive::ask_list("Ambiguous: choose package for $name", 1, sort keys %h);
		return $h{$result};
	} else {
		$state->say("Ambiguous: $name could be ", join(' ', keys %h));
		return undef;
	}
}

# stub class when no actual progressmeter that still prints out.
package OpenBSD::StubProgress;
sub clear {}

sub show {}

sub message {}

sub next {}

sub set_header {}

sub print
{
	shift;
	print @_;
}

sub errprint
{
	shift;
	print STDERR @_;
}

package OpenBSD::PackingList;
sub compute_size
{
	my $plist = shift;
	my $totsize = 0;
	$plist->visit('compute_size', \$totsize);
	$totsize = 1 if $totsize == 0;
	$plist->{totsize} = $totsize;
}

package OpenBSD::PackingElement;
sub mark_progress
{
}

sub compute_size
{
}

package OpenBSD::PackingElement::FileBase;
sub mark_progress
{
	my ($self, $progress, $donesize, $totsize) = @_;
	return unless defined $self->{size};
	$$donesize += $self->{size};
	$progress->show($$donesize, $totsize);
}

sub compute_size
{
	my ($self, $totsize) = @_;

	$$totsize += $self->{size} if defined $self->{size};
}

package OpenBSD::PackingElement::Sample;
sub compute_size
{
	&OpenBSD::PackingElement::FileBase::compute_size;
}

1;
