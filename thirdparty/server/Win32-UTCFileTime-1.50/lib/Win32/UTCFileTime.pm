#$Header: /thirdparty/thirdparty/server/Win32-UTCFileTime-1.50/lib/Win32/UTCFileTime.pm
#                                            2010/11/19 15:13:39 Rajani Exp $
#===========================================================================
# FILENAME
#   UTCFileTime.pm 
#
# DESCRIPTION
#    This perl module is responsible for getting stat of a file in windows OS 
#    
# HISTORY
#     19 NOV 2010  Rajani    Created.
#
#===========================================================================
#                 Copyright (c) InMage Systems                    
#===========================================================================
package Win32::UTCFileTime;

use 5.006000;

use strict;
use warnings;

use Carp qw(croak);
use Exporter qw();
use XSLoader qw();

sub stat(;$);
sub lstat(;$);
sub file_alt_stat(;$);
sub utime(@);

our(@ISA, @EXPORT, @EXPORT_OK, $VERSION);

BEGIN {
    @ISA = qw(Exporter);
    
    @EXPORT = qw(
        stat
        lstat
        utime
    );
    
    @EXPORT_OK = qw(
        $ErrorStr
        file_alt_stat
    );
    
    $VERSION = '1.50';

    XSLoader::load(__PACKAGE__, $VERSION);
}

our $ErrorStr = '';

our $Try_Alt_Stat_Count = 0;

sub AUTOLOAD {
    our $AUTOLOAD;

    (my $constant = $AUTOLOAD) =~ s/^.*:://;

    croak('Unexpected error in AUTOLOAD(): constant() is not defined')
        if $constant eq 'constant';

    my($err, $value) = constant($constant);
   
    croak($err) if $err;

    {
        no strict 'refs';
        *$AUTOLOAD = sub { return $value };
    }

    goto &$AUTOLOAD;
}

sub import {
    my $i = 1;
    while ($i < @_) {
        if ($_[$i] =~ /^:globally$/io) {
            splice @_, $i, 1;
            {
                no warnings 'once';
                *CORE::GLOBAL::stat  = \&Win32::UTCFileTime::stat;
                *CORE::GLOBAL::lstat = \&Win32::UTCFileTime::lstat;
                *CORE::GLOBAL::utime = \&Win32::UTCFileTime::utime;
            }
            next;
        }
        $i++;
    }

    goto &Exporter::import;
}

sub stat(;$) {
    my $input_file = @_ ? shift : $_;

    $ErrorStr = '';

    my $error_mode = _set_error_mode(SEM_FAILCRITICALERRORS());

    if (wantarray) {
        my @stats_file = CORE::stat $input_file;

        unless (@stats_file) {
            _set_error_mode($error_mode);
            if ($Try_Alt_Stat_Count) {
                goto &file_alt_stat;
            }
            else {
                $ErrorStr = "Can't stat file '$input_file': $!";
                return;
            }
        }

        unless (@stats_file[8 .. 10] = _get_utc_file_times($input_file)) {
            _set_error_mode($error_mode);
            return;
        }

        _set_error_mode($error_mode);
        return @stats_file;
    }
    else {
        my $ret_stat = CORE::stat $input_file;

        unless ($ret_stat) {
            _set_error_mode($error_mode);
            if ($Try_Alt_Stat_Count) {
                goto &file_alt_stat;
            }
            else {
                $ErrorStr = "Can't stat file '$input_file': $!";
                return $ret_stat;
            }
        }

        _set_error_mode($error_mode);
        return $ret_stat;
    }
}

sub lstat(;$) {
    my $input_link = @_ ? shift : $_;

    $ErrorStr = '';

    my $err_mode = _set_error_mode(SEM_FAILCRITICALERRORS());

    if (wantarray) {
        my @link_stats = CORE::lstat $input_link;

        unless (@link_stats) {
            _set_error_mode($err_mode);
            if ($Try_Alt_Stat_Count) {
                goto &file_alt_stat;
            }
            else {
                $ErrorStr = "Can't stat link '$input_link': $!";
                return;
            }
        }

        unless (@link_stats[8 .. 10] = _get_utc_file_times($input_link)) {
            _set_error_mode($err_mode);
            return;
        }

        _set_error_mode($err_mode);
        return @link_stats;
    }
    else {
        my $ret_stats = CORE::lstat $input_link;

        unless ($ret_stats) {
            _set_error_mode($err_mode);
            if ($Try_Alt_Stat_Count) {
                goto &file_alt_stat;
            }
            else {
                $ErrorStr = "Can't stat link '$input_link': $!";
                return $ret_stats;
            }
        }

        _set_error_mode($err_mode);
        return $ret_stats;
    }
}

sub file_alt_stat(;$) {
    my $input_file = @_ ? shift : $_;

    $ErrorStr = '';

    my $err_mode = _set_error_mode(SEM_FAILCRITICALERRORS());

    if (wantarray) {
        my @alt_stats = _alt_stat($input_file);

        _set_error_mode($err_mode);
        return @alt_stats;
    }
    else {
        my $ret_stats = _alt_stat($input_file);

        _set_error_mode($err_mode);
        return $ret_stats;
    }
}

sub utime(@) {
    my($atime, $mtime, @input_files) = @_;

    $ErrorStr = '';

    my $time = time;

    $atime = $time unless defined $atime;
    $mtime = $time unless defined $mtime;

    my $count = 0;
    foreach my $file (@input_files) {
        _set_utc_file_times($file, $atime, $mtime) and $count++;
    }

    return $count;
}

1;