package Fabric::VsnapProcessor;
#$Header: /src/server/tm/pm/Fabric/VsnapProcessor.pm,v 1.3 2015/11/25 14:04:02 prgoyal Exp $
#================================================================= 
# FILENAME
#    VsnapProcessor.pm
#
# DESCRIPTION
#    This module is used by fabricservice.pl responsible for
#    multiple actions and performs the following actions.
#
#    1. To process  exportedTLNexus, exportedITLNexus table.
#    2. update both the table with state and move state to export lun creation state.
#
# HISTORY
#     <24/06/2009>  Prakash Goyal    Created.
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
use Fabric::FabricUtilities;
use Fabric::Constants;
use Data::Dumper;
use Utilities;
use Common::DB::Connection;
use Common::Constants;
use Common::Log;

our $conn;
our $msg;
my $logging_obj = new Common::Log();


sub set_database_handle($)
{
  ($conn) = @_;
}

sub processVsnapInfo
{
	$logging_obj->log("DEBUG","Entered processVsnapInfo");
	my @par_array = (Fabric::Constants::CREATE_EXPORT_LUN_PENDING, Fabric::Constants::SNAPSHOT_CREATION_DONE);
	my $par_array_ref = \@par_array;
	my $updateTLStmnt = "UPDATE
						  exportedTLNexus
					SET 
						  state	 = ? 
					WHERE
						  state = ?	";
	my $sth = $conn->sql_query($updateTLStmnt,$par_array_ref);				  
	$logging_obj->log("DEBUG","processVsnapInfo ".$updateTLStmnt.Fabric::Constants::CREATE_EXPORT_LUN_PENDING);
	@par_array = (Fabric::Constants::CREATE_ACG_PENDING, Fabric::Constants::SNAPSHOT_CREATION_DONE);	
	my $par_array_ref = \@par_array;
	my $updateITLStmnt = "UPDATE
							  exportedITLNexus
						SET 
							  state	 = ? 
						WHERE
							  state = ?	";
	
	$logging_obj->log("DEBUG","processVsnapInfo : updateITLStmnt : $updateITLStmnt");	
	my $sthITL = $conn->sql_query($updateITLStmnt,$par_array_ref);			  
}


sub processVsnapComponent
{
	
	my %args;
	my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
	my $db_name = $AMETHYST_VARS->{'DB_NAME'};
	my $db_passwd = $AMETHYST_VARS->{'DB_PASSWD'};
	my $db_host = "localhost";

	%args = ( 'DB_NAME' => $db_name,
	          'DB_PASSWD'=> $db_passwd,
		      'DB_HOST' => $db_host
	        );
	my $args_ref = \%args;
    $logging_obj->log("DEBUG","processVsnapComponent : vsnap process_requests");

    my $origAutoCommit;
    my $origRaiseError;
    eval
    {
        $conn = new Common::DB::Connection($args_ref);
				   
        $origAutoCommit = $conn->get_auto_commit();
        $origRaiseError = $conn->get_raise_error();
        $conn->set_auto_commit(0,0);

        set_database_handle($conn);						
        
		$Fabric::FabricUtilities::conn = $conn;
		if (!$conn->sql_ping_test())
		{
			&processVsnapInfo();
			$conn->sql_commit();
		}
    };
	
    if ($@)
    {
        $logging_obj->log("EXCEPTION","ERROR - " . $@);
		eval
		{
           $conn->sql_rollback();
        };
		$conn->set_auto_commit($origAutoCommit,$origRaiseError);
        return;
    }
	
    $conn->set_auto_commit($origAutoCommit,$origRaiseError);
    Fabric::FabricUtilities::checkForQuitEvent();
}

1;