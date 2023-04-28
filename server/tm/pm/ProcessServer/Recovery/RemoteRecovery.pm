#$Header: 
#================================================================= 
# FILENAME
#   RemoteRecovery.pm 
#
# DESCRIPTION
#    This perl module is responsible for to check the role of cx  
#    means it is Active or Passive, and defined the different -2 
#    role of tmanagerd based on the cx role
#
# HISTORY
#     19 Dec 2008  varun    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ProcessServer::Recovery::RemoteRecovery;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use DBI();
use Utilities;
use Sys::Hostname;
use Common::Constants;
use TimeShotManager;
use Common::Log;
my $logging_obj = new Common::Log();
use Common::DB::Connection;
#
# This is like constractor
#
sub new
{
    my($class) = @_;
    my $self = {};
    bless($self,$class);
}


1;
