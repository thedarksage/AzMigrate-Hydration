#!/usr/bin/perl
#$Header: /src/server/ha/cxfailover.pl,v 1.37 2012/08/30 21:23:02 rajella Exp $
#================================================================= 
# FILENAME
#   cxfailover.pl 
#
# DESCRIPTION
#    This perl script will forked during HA node level failover
#    During failover, it does the following:
#       A. Fabric Pair
#         i. Mark the pairs for processing 
#        ii. Change the host id/PS Id in logicalVolumes and
#            srcLogicalVolumeDestLogicalVolume tables
#       iii. Mark the pairs for fabric service to do the
#            next round of stale clean ups.
#       B. Host pair
#         i. Change process sever
#        ii. Delete the stale files
#       iii. Restart resync if the pair is resyncing
#        iv. Set Resync Required flag as YES for pairs are in diff sync
#    
# HISTORY
#     24 Nov 2008     kbhattacharya    Created
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================

use lib("/home/svsystems/pm");
use strict;
use DBI;
use ConfigServer::HighAvailability::Failover;
use Utilities;
my $failoverObj = new ConfigServer::HighAvailability::Failover();

#
# Main Call
#
sub main
{
    my $dbh = undef;
    my $conn;
	eval
    {
		my $cxHostId = $failoverObj->get_my_host_id();
		
		while(1)
		{
			print "$0 is forked my Host Id = $cxHostId \n";
			$conn = $failoverObj->process_active_db_handler($cxHostId);
		   #print "Conn string:: $conn \n";
			
			if(!$conn->sql_ping_test())
			{
				#
				# Connected to the DB, So check for role
				# 
				my $origAutoCommit = $conn->get_auto_commit();
				my $origRaiseError = $conn->get_raise_error();

				$conn->set_auto_commit(0,1);
				if($failoverObj->is_ha_enabled($conn))
				{
				   $failoverObj->failover($conn, $cxHostId); 
				}
				last;
			}
			
			$conn->sql_disconnect();
			$dbh = undef;
			sleep 10;
		}
		if (!$conn->sql_ping_test())
		{
			$conn->sql_disconnect();
		}	
	};
	if ($@) 
	{
		$failoverObj->trace_log("ERROR - Roll Back: ".$@."\n");
		#
		# ERROR - Roll Back
		# 
		eval 
		{
			$conn->sql_rollback();
		};
	} 
}

&main();   
