#!/usr/bin/perl

#FILENAME
#   fabricmanager.pl
#
#DESCRIPION
#   This file implements the tasks related to fabric
#
#HISTORY
#   06 November 2012 Raghu rmadanala@inmage.com Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
#
##################################################################

use lib ("/home/svsystems/pm");
use lib qw(/usr/local/InMage/Vx/scripts/Array/ /home/svsystems/pm);
use strict;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use Log;
use fabriccontroller;

my $logging_obj = new Log();
#my $LOG_DIR = $CONF_PARAM{"LogPathname"}."fabriccontroller.log";
my $ARGC = @ARGV;
my $exit_code=1;

#add the functions to this hash for each command.
my %cmd_hash = (
                "ReportHBAs" => \&fabriccontroller::reportHBAs
);

if($ARGC == 2)
{
    my $cmd      = $ARGV[0];
    my $cmd_args = $ARGV[1];
    my %fabric_info;
    if($cmd_args)
    {
        $logging_obj->log("DEBUG", "fabricmanager.pl : Data input is $cmd_args\n");
        my $fabric_info_str = PHP::Serialization::unserialize($cmd_args);
        %fabric_info = %$fabric_info_str;
        if (exists $cmd_hash{$cmd})
        {
            my %retVal = $cmd_hash{$cmd}->(%fabric_info);
            my $retVal = PHP::Serialization::serialize(\%retVal);
            $logging_obj->log("DEBUG", "fabricmanager.pl : Return output is $retVal\n");
            print $retVal."\n";
            $exit_code = 0;
        }
        else
        {
            $exit_code = 1;
            $logging_obj->log("ERROR", "fabricmanager.pl : undefined function $cmd\n");
        }
    }
}
if ($exit_code != 0)
{
    exit (1);
}
