#line 1
#===============================================================================
#
# inc/Module/Install/PRIVATE.pm
#
# DESCRIPTION
#   Author-specific Module::Install private extension class for CPAN author ID
#   SHAY (Steve Hay).
#
# COPYRIGHT
#   Copyright (C) 2004-2008 Steve Hay.  All rights reserved.
#
# LICENCE
#   You may distribute under the terms of either the GNU General Public License
#   or the Artistic License, as specified in the LICENCE file.
#
#===============================================================================

package Module::Install::PRIVATE;

use 5.006000;

use strict;
use warnings;

use Carp qw(croak);
use Config qw(%Config);
use Cwd qw(abs_path);
use File::Basename qw(basename);
use File::Spec::Functions qw(canonpath catfile);
use Getopt::Long qw(GetOptions);
use Pod::Usage qw(pod2usage);
use Text::Wrap qw(wrap);

#===============================================================================
# CLASS INITIALIZATION
#===============================================================================

our(@ISA, $VERSION);

BEGIN {
    @ISA = qw(Module::Install::Base);

    $VERSION = '1.06';

    # Define protected accessor/mutator methods.
    foreach my $prop (qw(define inc libs opts)) {
        no strict 'refs';
        *$prop = sub {
            use strict 'refs';
            my $self = shift;
            $self->{$prop} = shift if @_;
            return $self->{$prop};
        };
    }
}

# Indentation for show_found_var() method.
our $Show_Found_Var_Indent = 37;

#===============================================================================
# PUBLIC API
#===============================================================================

# Method to return the instance of this class that it was invoked on, for use in
# invoking further methods in this class within Makefile.PL.  (This method has a
# suitably unique name to just be autoloaded from Makefile.PL; the other methods
# do not, so must be invoked on our object to ensure they are dispatched
# correctly.)

sub get_shay_private_obj {
    return shift;
}

sub process_opts {
    my($self, $opt_specs, $with_auto_install) = @_;

    # Allow options to be introduced with a "/" character on Windows, as is
    # common on those OSes, as well as the default set of characters.
    if ($self->is_win32()) {
        Getopt::Long::Configure('prefix_pattern=(--|-|\+|\/)');
    }

    # Deal with these common options immediately; have the rest stored in %opts.
    my $opt_def = 0;
    my %opts = (
        'defaults' => sub { $ENV{PERL_MM_USE_DEFAULT} = 1; $opt_def = 1 },
        'version'  => sub { $self->exit_with_version()   },
        'help'     => sub { $self->exit_with_help()      },
        'manpage'  => sub { $self->exit_with_manpage()   }
    );

    # Make sure that '-v' and '-h' unambiguously mean '--version' and '--help'
    # respectively, even if other option specs beginning with 'v' or 'h' are
    # given in @$opt_specs.
    my @opt_specs = (
        @$opt_specs,
        'defaults',
        'version|v',
        'help|h|?',
        'manpage|doc'
    );

    # Include the Module::AutoInstall options if requested.  Also save @ARGV so
    # that it can be temporarily restored for auto_install() later.
    my @SAVARGV = ();
    if ($with_auto_install) {
        # These options are specified in Module::AutoInstall::_init().
        push @opt_specs, (
            'config:s',
            'installdeps|install:s',
            'defaultdeps|default',
            'checkdeps|check',
            'skipdeps|skip',
            'testonly|test'
        );

        @SAVARGV = @ARGV;
    }

    GetOptions(\%opts, @opt_specs) or
        $self->exit_with_usage();

    if ($with_auto_install) {
        # Temporarily restore the original @ARGV for auto_install() since its
        # options will have been removed from @ARGV by GetOptions().
        local @ARGV = @SAVARGV;
        $self->auto_install();

        # We are not using Module::AutoInstall's Write() so we have to check for
        # "check only" mode and exit ourselves if it is set.
        if ( exists $opts{'checkdeps'} or
            (exists $ENV{PERL_AUTOINSTALL} and
             " $ENV{PERL_AUTOINSTALL} " =~ /\s--check(?:deps)?\s/o))
        {
            warn("*** Makefile not written in check-only mode.\n");
            exit 0;
        }

        # If it appears that Test::Builder has been loaded during the course of
        # the auto-install testing then disable the Test::Builder ending
        # diagnostic code that would otherwise be invoked if the Makefile.PL
        # die()s anytime later.  This suppresses a somewhat confusing (given
        # the context) message about the test having died before it could output
        # anything.
        if (my $test = eval { Test::Builder->new() }) {
            $test->no_ending(1);
        }
    }

    if ($ENV{PERL_MM_USE_DEFAULT} and $ExtUtils::MakeMaker::VERSION < 5.48_01)
    {
        warn(wrap('', '', "\n" . sprintf(
            'Warning: %s ignored: requires ExtUtils::MakeMaker version ' .
            '5.48_01 or later',
            $opt_def ? '--defaults option'
                     : 'PERL_MM_USE_DEFAULT environment variable'
        )) . "\n\n");
    }

    $self->opts(\%opts);
}

# Method to perform a rudimentary check that a compatible compiler is being used
# to build this module as was used to build Perl itself.  The check is skipped
# on all platforms except Win32, and is also skipped on Win32 if the compiler
# used to build Perl is unknown and unguessable.
# It is good enough, however, to catch the currently rather common situation in
# which a Win32 user is building this module with the Visual C++ Toolkit 2003 or
# the Visual C++ 2005 Express Edition for use with any ActivePerl, which are
# known currently to be built with Visual Studio 98.  These combinations do not
# work for this particular module; see the INSTALL file for details.

# This method is based on code taken from the get_avail_w32compilers() function
# in the configsmokianidianide.pl script in the Test-Smokianidianide distribution (version 1.19).

sub check_compiler {
    my($self, $exit_on_error) = @_;

    my $cc;
    unless ($cc = $self->can_cc()) {
        if ($Config{cc} ne '') {
            $self->exit_with_error(8,
                "OS unsupported: Compiler used to build perl ('%s') not found",
                $Config{cc}
            );
        }
        else {
            $self->exit_with_error(9,
                'OS unsupported: Compiler used to build perl not specified ' .
                'in perl configuration'
            );
        }
    }
    $cc = qq["$cc"] if $cc =~ / /o;

    return unless $self->is_win32();

    my $fmt = "Wrong compiler version ('%s'). Perl was built with version " .
              "'%s'. Please see INSTALL file for details";
    my $msg = '';
    # Perl version 5.6.0 did not have $Config{ccversion} at all on Win32.
    if (exists $Config{ccversion} and $Config{ccversion} ne '') {
        my $ccversion;
        if ($cc =~ /cl(?:\.exe)?"?$/io) {
            my $output = `$cc --version 2>&1`;
            $ccversion = $output =~ /^.*Version\s+([\d.]+)/io ? $1 : '?';

            # Visual C++ 6.x and earlier (having ccversion 12.x and earlier) all
            # used the system's msvcrt.dll, so just check that the major version
            # number is the same.  Visual C++ 7.x onwards (having ccversion 13.x
            # onwards) use msvcr70.dll, msvcr71.dll, msvcr80.dll etc, so check
            # that the major and minor versions are the same. (Note that from
            # Visual C++ 8.x onwards (having ccversion 14.x onwards) the
            # ccversion is now composed of four numbers separated by dots, not
            # just three. We are only interested in the first two anyway,
            # though.)
            my($major, $minor) = $ccversion =~ /^(\d+)\.(\d+)\./o;
            if (defined $major and defined $minor) {
                if ($major <= 12) {
                    if ($Config{ccversion} !~ /^$major\./) {
                        $msg = sprintf $fmt, $ccversion, $Config{ccversion};
                    }
                }
                elsif ($Config{ccversion} !~ /^$major\.$minor\./) {
                    $msg = sprintf $fmt, $ccversion, $Config{ccversion};
                }
            }
        }
        elsif ($cc =~ /bcc32(?:\.exe)?"?$/io) {
            my $output = `$cc --version 2>&1`;
            $ccversion = $output =~ /([\d.]+)/o ? $1 : '?';

            # Just check that the major version number is the same.
            my($major) = $ccversion =~ /^(\d+)\./o;
            if (defined $major and $Config{ccversion} !~ /^$major\./) {
                $msg = sprintf $fmt, $ccversion, $Config{ccversion};
            }
        }
    }
    elsif ($Config{gccversion} ne '') {
        my $gccversion;
        if ($cc =~ /gcc(?:\.exe)?"?$/io) {
            chomp($gccversion = `$cc -dumpversion`);
        }

        # Just check that the major version number is the same.
        my($major) = $gccversion =~ /^(\d+)\./o;
        if (defined $major and $Config{gccversion} !~ /^$major\./) {
            $msg = sprintf $fmt, $gccversion, $Config{gccversion};
        }
    }
    elsif ($Config{cf_by} eq 'ActiveState') {
        my $ccversion  = '?';
        my $vc6version = '12.00.8804';
        if ($cc =~ /cl(?:\.exe)?"?$/io) {
            my $output = `$cc --version 2>&1`;
            $ccversion = $output =~ /^.*Version\s+([\d.]+)/ ? $1 : '?';
        }

        # Check the Visual C++ version number as above.
        my($major, $minor) = $ccversion =~ /^(\d+)\.(\d+)\./o;
        if (defined $major and defined $minor) {
            if ($major <= 12) {
                if ($vc6version !~ /^$major\./) {
                    $msg = sprintf $fmt, $ccversion, $vc6version;
                }
            }
            elsif ($vc6version !~ /^$major\.$minor\./) {
                $msg = sprintf $fmt, $ccversion, $vc6version;
            }
        }
    }

    if ($msg) {
        if ($exit_on_error) {
            $self->exit_with_error(10, "OS unsupported: $msg");
        }
        else {
            warn("Warning: $msg\n");
        }
    }
}

sub query_scripts {
    my($self, $script_specs) = @_;

    my($install_scripts, $multi);
    if (@$script_specs == 1) {
        $install_scripts = $self->opts()->{'install-script'};
        $multi = 0;
    }
    else {
        $install_scripts = $self->opts()->{'install-scripts'};
        $multi = 1;
    }

    if (defined $install_scripts) {
        if ($install_scripts =~ /^(?:y|n$)/) {
            $self->show_found_var(
                sprintf('Using specified script%s option', $multi ? 's' : ''),
                $install_scripts
            );
        }
        else {
            $self->exit_with_error(3,
                "Invalid 'install_script%s' option value '%s'",
                $multi ? 's' : '', $install_scripts
            );
        }
    }

    foreach my $script_spec (@$script_specs) {
        my($script_name, $default) = $script_spec =~ /^([^=]+)(?:=(y|n))?$/io;
        my $answer;
        if (defined $install_scripts) {
            $answer = $install_scripts eq 'y' ? 1 : 0;
        }
        else {
            $default = 'y' unless defined $default;
            my $question = "Do you want to install '$script_name'?";
            $answer = $self->prompt_yes_no($question, $default);
        }

        $self->install_script(catfile('script', $script_name)) if $answer;
    }
    print "\n";
}

# Method to store the build options in this process' environment so that they
# are available to the sub-directories' Makefile.PLs when they are run.  Note
# that ExtUtils::MakeMaker's PASTHRU macro is not good enough because that only
# passes things through when "make Makefile" is run, which is too late for the
# processing of the LIBS option that Makefile.PL itself handles.

sub setup_env {
    my $self = shift;

    $ENV{__SHAY_PRIVATE_DEFINE} = $self->define();
    $ENV{__SHAY_PRIVATE_INC}    = $self->inc();
    $ENV{__SHAY_PRIVATE_LIBS}   = $self->libs();
}

#===============================================================================
# PROTECTED API
#===============================================================================

sub is_win32 {
    return $^O =~ /MSWin32/io;
}

sub prompt_yes_no {
    my($self, $question, $default) = @_;

    my $answer = $self->prompt_validate(
        -question => $question,
        -default  => $default,
        -validate => sub { $_[0] =~ /^(?:y(?:es)?|no?)$/io }
    );

    return $answer =~ /y/io ? 1 : 0;
}

sub prompt_dir {
    my($self, $question, $default) = @_;

    my $dir = $self->prompt_validate(
        -question => $question,
        -default  => $default,
        -validate => sub { -d $_[0] },
        -errmsg   => 'No such directory'
    );

    return canonpath(abs_path($dir));
}

sub prompt_list {
    my($self, $message, $options, $question, $default) = @_;

    my $num_options = scalar @$options;
    my $len = length $num_options;

    my %options = map { $_->[0] => 1 } @$options;
    my $num_unique_options = scalar keys %options;
    if ($num_unique_options != $num_options) {
        $self->exit_with_error(4, 'Options in list are not unique');
    }

    my $default_num = 0;
    for (my $i = 1; $i <= $num_options; $i++) {
        $message .= sprintf "\n  [%${len}d] %s", $i, $options->[$i - 1][1];
        $default_num = $i if $options->[$i - 1][0] eq $default;
    }

    if ($default_num == 0) {
        $self->exit_with_error(5, "Invalid default response '%s'", $default);
    }

    my $answer_num = $self->prompt_validate(
        -message  => $message,
        -question => $question,
        -default  => $default_num,
        -validate => sub {
            $_[0] =~ /^[1-9](?:\d+)?$/ and $_[0] <= $num_options
        }
    );

    return $options->[$answer_num - 1][0];
}

{
    my %defaults = (
        -question => '?',
        -default  => '',
        -validate => sub { 1 },
        -errmsg   => 'Invalid response'
    );
    
    sub prompt_validate {
        my $self = shift;
        my %args = (%defaults, @_);

        if (exists $args{-message}) {
            print wrap('', '', $args{-message}), "\n";
        }
    
        my $input;
        until (defined $input) {
            $input = $self->prompt($args{-question}, $args{-default});
            unless ($args{-validate}->($input)) {
                if ($self->use_default_response()) {
                    $self->exit_with_error(7,
                        "Invalid default response '%s'", $args{-default}
                    );
                }
                else {
                    print wrap('', '', $args{-errmsg}), "\n";
                    $input = undef;
                }
            }
        }
    
        return $input;
    }
}

sub show_found_var {
    my($self, $msg, $var) = @_;
    local $Text::Wrap::break = qr{\s|(?<=[\\\\/]).{0}};
    print wrap('', ' ' x $Show_Found_Var_Indent,
        "$msg " . '.' x ($Show_Found_Var_Indent - length($msg) - 2) . " $var"
    ), "\n";
}

sub exit_with_version {
    my $self = shift;

    printf "This is %s v%s (using %s v%s).\n\n",
           basename($0), $main::VERSION, ref $self, $self->VERSION();

    print "Copyright (C) $main::YEAR Steve Hay.  All rights reserved.\n\n";

    print wrap('', '',
        "This script is free software; you can redistribute it and/or modify " .
        "it under the same terms as Perl itself, i.e. under the terms of " .
        "either the GNU General Public License or the Artistic License, as " .
        "specified in the LICENCE file.\n\n"
    );

    exit 1;
}

sub exit_with_help {
    my $self = shift;
    pod2usage(
        -exitval => 1,
        -verbose => 1
    );
}

sub exit_with_manpage {
    my $self = shift;
    pod2usage(
        -exitval => 1,
        -verbose => 2
    );
}

sub exit_with_usage {
    my $self = shift;
    pod2usage(
        -exitval => 2,
        -verbose => 0
    );
}

sub exit_with_error {
    my($self, $num, $msg) = splice @_, 0, 3;
    $msg = sprintf $msg, @_ if @_;
    # Load Carp::Heavy now, otherwise (before Perl 5.8.7) croak() clobbers $!
    # when loading it.
    require Carp::Heavy;
    $! = $num;
    croak("Error ($num): $msg");
}

# This method is based on code taken from the prompt() function in the standard
# library module ExtUtils::MakeMaker (version 6.17).

sub use_default_response {
    my $self = shift;
    return($ENV{PERL_MM_USE_DEFAULT} or (not $self->_isa_tty() and eof STDIN));
}

#===============================================================================
# PRIVATE API
#===============================================================================

# This method is based on code taken from the prompt() function in the standard
# library module ExtUtils::MakeMaker (version 6.17).

sub _isa_tty {
    my $self = shift;
    return(-t STDIN and (-t STDOUT or not (-f STDOUT or -c STDOUT)));
}

1;

__END__

#===============================================================================
