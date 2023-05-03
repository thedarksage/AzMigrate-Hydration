#!/usr/bin/perl
#$Header: /src/host/appframeworklib/app/unix/Array/perl/clearwritesplits.pl,v 1.3 2014/03/16 06:04:42 nameti Exp $
#================================================================= 
# FILENAME
#   unregisterps.pl 
#
# DESCRIPTION
#    This perl script will unregister process server from 
#    configuration server. It checks if there are any replication 
#    pairs attached to PS and alerts the user about replication pairs 
#    being removed upon process server unregistration.
#
#    If the user proceeds to continue:
#
#    i.     Deletes the ps entries from 
#           srcLogicalVolumeDestLogicalVolume tables
#    ii.    Deletes the ps entries from processServer table
#    iii.   Deletes the ps entries from hosts table
#    iv.    Deletes the ps entries from networkAddress table
#    
# HISTORY
#     11 DEC 2008     bknathan     Created
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================

use strict;
use DBI;
use lib("/home/svsystems/pm");
use Utilities;
use Data::Dumper;
use tmanager;
use Common::Constants;
use Common::DB::Connection;
my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();


#
# Connect to the Database
#

my $db_name = $amethyst_vars->{'DB_NAME'};
my $db_passwd = $amethyst_vars->{'DB_PASSWD'};
my $db_host = $amethyst_vars->{'PS_CS_IP'};
my %args = ( 'DB_NAME' => $db_name,
		 'DB_PASSWD'=> $db_passwd,
		 'DB_HOST' => $db_host
		);
my $args_ref = \%args;
my $conn = new Common::DB::Connection($args_ref);

if ($conn->sql_ping_test())
{
	print "Error:Unable to connect to DB: $!\n";
	exit 1;
}
my $ps_id = $amethyst_vars->{"HOST_GUID"};
my $return_code = 0;
eval
{
    
	my $appliance_query = $conn->sql_query("select sharedApplianceGuid from appliance where applianceGuid = '$ps_id'");
	my $appliance_row = $conn->sql_fetchrow_hash_ref($appliance_query);
	my $applianceGuid = $appliance_row->{'sharedApplianceGuid'};
			
	my $get_pairs_query = $conn->sql_query("select sharedDeviceId from arrayLunMapInfo where protectedProcessServerId ='$ps_id' and lunType='SOURCE'");
    my $num_rows = $conn->sql_num_rows($get_pairs_query);
	my $message;
	if($num_rows > 0)
	{
		print "Deleting the WriteSplit for all the Luns mapped over this processServer\n";
		if($amethyst_vars->{'CUSTOM_BUILD_TYPE'} eq 2)
		{
			my %delDetails;

			while(my $ref = $conn->sql_fetchrow_hash_ref($get_pairs_query))
			{
				my $scsi_id = $ref->{'sharedDeviceId'};
				my $pair_type = TimeShotManager::get_pair_type($conn,$scsi_id); # pair_type 0=host,1=fabric,2=prism,3=array
				if($pair_type == 3)
				{
					my $get_array = $conn->sql_query("select arrayGuid,volumeGroup from arrayLuns where sanLunId = '$scsi_id'");
					my $array_row = $conn->sql_fetchrow_hash_ref($get_array);
					my $arrayGuid = $array_row->{'arrayGuid'};
					my $lun_fqn = $array_row->{'volumeGroup'};
					
					my $get_array_ip = $conn->sql_query("select ipAddress from arrayInfo where arrayGuid = '$arrayGuid'");
					$array_row   = $conn->sql_fetchrow_hash_ref($get_array_ip);
					my $array_ip = $array_row->{'ipAddress'};
					my $clear_split_pcli_cmd = '/usr/local/InMage/Vx/bin/'.$array_ip.'/AXIOMPCLI/pcli submit -H '.$array_ip.' -u replication -p TwinPeaks ClearWriteSplit SourceLunIdentity.Fqn='.$lun_fqn.' ApplianceGuid='.$applianceGuid.' 2>/tmp/'.$$;
					my $clear_split_pcli_op = `$clear_split_pcli_cmd`;
					my $clear_split_pcli_ret = `echo $?`;
					if($clear_split_pcli_ret != 0 || $clear_split_pcli_op =~ /error/i)
					{
						if(!$clear_split_pcli_op)
						{
							$clear_split_pcli_op = `cat /tmp/$$`;
							unlink("/tmp/$$");
						}
						$message .= "ClearWriteSplit for Lun with scsiId = $scsi_id and lunName = $lun_fqn failed with error $clear_split_pcli_op\nCmd - $clear_split_pcli_cmd\n";
						$return_code = $clear_split_pcli_ret;
					}
					else
					{
						$message .= "ClearWriteSplit for Lun with scsiId = $scsi_id and lunName = $lun_fqn succeeded\n";
					}
					my $del_hatlm_sql = "DELETE FROM hostApplianceTargetLunMapping where sharedDeviceId = '$scsi_id'";
					$conn->sql_query($del_hatlm_sql);
					#$logging_obj->log("DEBUG"," deleteStaleEntries :: Cleaned hatlm :$del_hatlm_sql\n");
					my $del_ab_sql = "DELETE FROM arrayLunMapInfo where sourceSharedDeviceId = '$scsi_id'";
					$conn->sql_query($del_ab_sql);
					#$logging_obj->log("DEBUG"," deleteStaleEntries :: Cleaned alm : $del_ab_sql\n");
					my $del_sd_sql = "DELETE FROM sharedDevices where sharedDeviceId = '$scsi_id'";
					$conn->sql_query($del_sd_sql);
					
				}		
			}
		} 
		print $message;
	}
	#Unregistereing the Appliance from Oracle Storage
	
	my $array_info_sql = $conn->sql_query("select ai.ipAddress from arrayInfo ai, arrayAgentMap am where ai.arrayGuid = am.arrayGuid and am.agentGuid='$ps_id'");
	while(my $array_info_ref = $conn->sql_fetchrow_hash_ref($array_info_sql))
	{
		my $array_ip = $array_info_ref->{'ipAddress'};
		print "Unregistering the Appliance $applianceGuid from Oracle Storage $array_ip\n";
		my $unreg_pcli_cmd = '/usr/local/InMage/Vx/bin/'.$array_ip.'/AXIOMPCLI/pcli submit -H '.$array_ip.' -u replication -p TwinPeaks DeregisterReplicationAppliance ApplianceGuid='.$applianceGuid.' 2>/tmp/'.$$;
		my $unreg_pcli_op = `$unreg_pcli_cmd`;
		my $unreg_pcli_ret = `echo $?`;
		$message .= "\n";
		if($unreg_pcli_ret != 0 || $unreg_pcli_op =~ /error/i)
		{
			if(!$unreg_pcli_op)
			{
				$unreg_pcli_op = `cat /tmp/$$`;
				unlink("/tmp/$$");
			}
			$message .= "Deregistration failed with error $unreg_pcli_op\nCmd - $unreg_pcli_cmd\n";
			$return_code = $unreg_pcli_ret;
		}
		else
		{
			$message .= "Deregister for $applianceGuid succeeded\n";
		}
	}
};
if ($@)
{
    print "Error executing query: ". $@."\n";
    exit 3;
}

