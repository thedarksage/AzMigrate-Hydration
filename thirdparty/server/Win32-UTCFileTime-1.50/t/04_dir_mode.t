#!perl
#===============================================================================
#
# t/04_dir_mode.t
#
# DESCRIPTION
#   Test script to check getting directory mode.
#
# COPYRIGHT
#   Copyright (C) 2003-2006 Steve Hay.  All rights reserved.
#
# LICENCE
#   You may distribute under the terms of either the GNU General Public License
#   or the Artistic License, as specified in the LICENCE file.
#
#===============================================================================

use 5.006000;

use strict;
use warnings;

use Config qw(%Config);
use Test::More tests => 7;

#===============================================================================
# INITIALIZATION
#===============================================================================

BEGIN {
    use_ok('Win32::UTCFileTime');
}

#===============================================================================
# MAIN PROGRAM
#===============================================================================

MAIN: {
    my $dir = 'test';
    my $bcc = $Config{cc} =~ /bcc32/io;

    my(@cstats, @rstats, @astats);

    mkdir $dir or die "Can't create directory '$dir': $!\n";

    chmod 0777, $dir;
    @cstats = CORE::stat $dir;
    @rstats = Win32::UTCFileTime::stat $dir;
    @astats = Win32::UTCFileTime::alt_stat($dir);
    is($rstats[2], $cstats[2],
       "stat() works for executable directory");
    SKIP: {
        skip 'Borland\'s stat(2) sets directory mode differently', 1 if $bcc;
        is($astats[2], $cstats[2],
           "alt_stat() works for executable directory");
    }

    @cstats = CORE::lstat $dir;
    @rstats = Win32::UTCFileTime::lstat $dir;
    is($rstats[2], $cstats[2],
       "lstat() works for executable directory");

    chmod 0444, $dir;
    @cstats = CORE::stat $dir;
    @rstats = Win32::UTCFileTime::stat $dir;
    @astats = Win32::UTCFileTime::alt_stat($dir);
    is($rstats[2], $cstats[2],
       "stat() works for read-only directory");
    SKIP: {
        skip 'Borland\'s stat(2) sets directory mode differently', 1 if $bcc;
        is($astats[2], $cstats[2],
           "alt_stat() works for read-only directory");
    }

    @cstats = CORE::lstat $dir;
    @rstats = Win32::UTCFileTime::lstat $dir;
    is($rstats[2], $cstats[2],
       "lstat() works for read-only directory");

    chmod 0777, $dir;
    rmdir $dir;
}

#===============================================================================
