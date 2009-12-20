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

our ($opt_n, $opt_x, $opt_v, $opt_B, $opt_L, $opt_i, $opt_q, $opt_c, $opt_I, $opt_s);
$opt_v = 0;

sub handle_options
{
	my ($opt_string, $hash, @usage) = @_;

	set_usage(@usage);
	$state = OpenBSD::State->new;
	$hash->{h} = sub { Usage(); };
	$hash->{f} = $hash->{F} = sub { 
		for my $o (split /\,/o, shift) { 
			$defines{$o} = 1;
		}
	};
	try {
		getopts('hciInqvsxB:f:F:L:'.$opt_string, $hash);
	} catchall {
		Usage($_);
	};

	$opt_L = OpenBSD::Paths->localbase unless defined $opt_L;

	$state->{recorder} = OpenBSD::SharedItemsRecorder->new;
	if ($opt_s) {
		$opt_n = 1;
	}
	$state->{not} = $opt_n;
	# XXX RequiredBy
	$main::not = $opt_n;
	$state->{defines} = \%defines;
	$state->{interactive} = $opt_i;
	$state->{v} = $opt_v;
	$state->{localbase} = $opt_L;
	$state->{size_only} = $opt_s;
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
	return $dielater;
}

sub framework
{
	my $code = shift;
	try {
		lock_db($opt_n) unless $state->{defines}->{nolock};
		$state->setup_progressmeter($opt_x);
		$state->check_root;
		process_parameters();
		my $dielater = do_the_main_work($code);
		# cleanup various things
		$state->{recorder}->cleanup($state);
		OpenBSD::PackingElement::Lib::ensure_ldconfig($state);
		OpenBSD::PackingElement::Fontdir::finish_fontdirs($state);
		$state->progress->clear;
		$state->log->dump;
		finish_display();
		if ($state->verbose >= 2 || $opt_s) {
			$state->vstat->tally;
		}
		# show any error, and show why we died...
		rethrow $dielater;
	} catch {
		print STDERR "$0: $_\n";
		if ($_ =~ m/^Caught SIG(\w+)/o) {
			kill $1, $$;
		}
		exit(1);
	};

	if ($bad) {
		exit(1);
	}
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

package OpenBSD::MyStat;
use OpenBSD::Vstat;
sub new
{
	my $class = shift;
	bless {}, $class
}

sub add
{
	shift;
	&OpenBSD::Vstat::add;
}

sub remove
{
	shift;
	&OpenBSD::Vstat::remove;
}

sub exists
{
	shift;
	&OpenBSD::Vstat::vexists;
}

sub stat
{
	shift;
	&OpenBSD::Vstat::filestat;
}

sub tally
{
	shift;
	&OpenBSD::Vstat::tally;
}

sub synchronize
{
	shift;
	&OpenBSD::Vstat::synchronize;
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
	$self->{vstat} = OpenBSD::MyStat->new;
	$self->{progressmeter} = bless {}, "OpenBSD::StubProgress";
	$self->{v} = 0;
}

sub ntogo
{
	my ($self, $offset) = @_;

	return $self->progress->ntogo($self->todo, $offset);
}

sub verbose
{
	return shift->{v};
}

sub vstat
{
	return shift->{vstat};
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
	OpenBSD::Error::VSystem($self->verbose >= 2, @_);
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
	if (!$opt_x && $self->verbose) {
		require OpenBSD::ProgressMeter;
		$self->{progressmeter} = OpenBSD::ProgressMeter->new;
	}
}

sub check_root
{
	my $state = shift;
	if ($< && !$state->{defines}->{nonroot}) {
		if ($state->{not}) {
			$state->errsay("$0 should be run as root") if $state->verbose;
		} else {
			Fatal "$0 must be run as root";
		}
	}
}

sub choose_location
{
	my ($state, $name, $list, $is_quirks) = @_;
	if (@$list == 0) {
		$state->errsay("Can't find $name") unless $is_quirks;
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
		$state->errsay("Ambiguous: $name could be ", join(' ', keys %h));
		return undef;
	}
}

sub confirm
{
	my ($state, $prompt, $default) = @_;

	return 0 if !$state->{interactive};
	require OpenBSD::Interactive;
	return OpenBSD::Interactive::confirm($prompt, $default);
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

sub ntogo
{
	return "";
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
