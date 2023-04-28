package Fabric::FabricUtilities;
#$Header: /src/server/tm/pm/Fabric/FabricUtilities.pm,v 1.3 2015/11/25 14:04:02 prgoyal Exp $
#=================================================================
# FILENAME
#    FabricUtilities.pm
#
# DESCRIPTION
#    This module is used by ITLDiscovery responsible for
#    multiple functionalities and performs the following actions.
#
#    1. To retrieve switchagent id, and dpc based on load balancing
#    algorithm.
#    2. appropriate algorithm to get vt, vi.
#    3. Other functions used by ITLDiscovery
#
# HISTORY
#     <23/04/2008>  Srinivas K    Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================


use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use DBI;
use Utilities;
use Common::Log;
use Common::DB::Connection;
our $SHUT_DIR = "/var/cx";
our $conn;
my $SHUTDOWN_FILE		= ".shutdown" ;
my $logging_obj = new Common::Log();
sub new
{
    my ($class, %args) = @_;
    my $self = {};

    bless ($self, $class);
    return $self;
}

#
# This function look for whether the shutdown file exists or not
#

sub shouldQuit
{
    my $shutdown_file = Utilities::makePath( $SHUT_DIR ."/".$SHUTDOWN_FILE ) ;
    if ( -e $shutdown_file ) 
    {
        return 1 ;
    }
    else
    {
        return 0 ;
    }
}

#
# This function is responsible for checking if the quit 
# event is requested by ITLProtector/ITLDiscovery modules.
# When called upon, it checks if the event is raised when there 
# is shutdown requested.
#

sub checkForQuitEvent
{
   
    if (shouldQuit())
    {
        if($conn->sql_ping_test())
		{
            exit 3;
        }
        else
        {
           
            $conn->sql_disconnect();
			exit 2;
        }
    }
}

1;

