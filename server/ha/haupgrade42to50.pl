#!/usr/bin/perl
#$Header: /src/server/ha/haupgrade42to50.pl,v 1.5 2009/01/20 00:36:18 sawannavar Exp $
#================================================================= 
# FILENAME
#   haupgrade42to50.pl 
#
# DESCRIPTION
#    This perl script will upgrade HA from 4.2 to 5.0. It updates 
#    the FX jobs with new pre and post scripts, registers HA nodes 
#    forcefully in cxPairs table.
#
# HISTORY
#     15 JAN 2009     bknathan     Created
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
use lib("/home/svsystems/pm");
use Utilities;
use ConfigServer::HighAvailability::Register;
use tmanager;
use TimeShotManager;

my $amethyst_vars  = Utilities::get_ametheyst_conf_parameters();

my $host_guid = $amethyst_vars->{'HOST_GUID'};
my $cluster_ip = $amethyst_vars->{'CLUSTER_IP'};
my $installation_dir = $amethyst_vars->{'INSTALLATION_DIR'};

if ($host_guid eq "") 
{
    print "Host guid is empty. Cannot proceed further\n";
    exit 1;
}
if ($cluster_ip eq "")
{
    print "Cluster IP is empty. Cannot proceed further\n";
    exit 2;
}

#
# Connect to the Database
#
my $dbh = &connect_db();
if ($dbh eq undef)
{
	print "Error:Unable to connect to DB: $!\n";
	exit 3;
}

eval
{
	#
	# Register the HA node. In 5.0, this is done by 
	# the monitor_ha thread. We should do this such
	# that the existing jobs and UI flow work seamlessly
	# during upgrade and do not have to wait for the 
	# monitor_ha to register explicitly.
	#
	&register_ha_node ($host_guid, $cluster_ip);

	#
	# Update the FX job configuration in DB with the new
	# pre/post script values.
	#
	&update_fx_jobs($dbh);
};
if ($@)
{
	print "Error occurred while upgrading HA: $@\n";
}


#
# Update FX jobs with pre and post scripts 
#
sub update_fx_jobs
{
    my ($dbh) = @_;

    my $active_passive_ref;
    my $passive_active_ref;

    #
    # New parameters for 5.0 HA DB sync jobs
    # 
    my $backup_dir =  "$installation_dir/ha/backup";
    my $restore_dir =  "$installation_dir/ha/restore";
    my $pre_script_path  = "$installation_dir/bin/db_sync_src_pre_script.pl -mode HA";
    my $post_script_path = "$installation_dir/bin/db_sync_tgt_post_script.pl -mode HA"; 
    my $ha_appname = "HA_DB_SYNC";

    #
    # Get the existing FX job configuration for 
    # DB SYNC (ACTIVE to PASSIVE)
    #
    my $active_passive_job_sql = "SELECT 
    					appname, id 
				  FROM 
				  	frbJobConfig 
				  WHERE 
				  	sourcePath='/home/svsystems/bin/db1' and 
					destPath='/home/svsystems/bin/db1' and 
					preCommandSource ='/home/svsystems/bin/db1/db_backup.sh' and 
					postCommandTarget ='/home/svsystems/bin/db1/db_restore.sh'";

    #
    # Get the existing FX job configuration for 
    # DB SYNC (ACTIVE to PASSIVE)
    #
    my $passive_active_job_sql = "SELECT 
    					appname, id 
				  FROM 
				  	frbJobConfig 
				  WHERE 
				  	sourcePath='/home/svsystems/bin/db2' and 
					destPath='/home/svsystems/bin/db2' and 
					preCommandSource ='/home/svsystems/bin/db2/db_backup.sh' and 
					postCommandTarget ='/home/svsystems/bin/db2/db_restore.sh'";

   if ($active_passive_ref = $dbh->selectrow_hashref($active_passive_job_sql))
   {
	my $id = $active_passive_ref->{'id'};
	my $appname = $active_passive_ref->{'appname'};
	
	print "Changing FX job for $appname\n\n";
	print "-" x 10 ." DETAILS " . "-" x 10 ."\n";
	print "Job id : $id ==> $id\n";
	print "Appname: $appname ==> $ha_appname\n";
	print "Source directory: /home/svsystems/bin/db1 ==> $backup_dir \n";
	print "Dest directory : /home/svsystems/bin/db1 ==> $restore_dir \n";
	print "Pre command source: /home/svsystems/bin/db1/db_backup.sh ==> $pre_script_path\n";
	print "Post command target: /home/svsystems/bin/db1/db_restore.sh ==> $post_script_path \n";

	#
	# Update the FX job configuration with the modified
	# parameters for 5.0 (ACTIVE to PASSIVE)
	#	
	my $update_active_passive = "UPDATE
					frbJobConfig
				     SET
				     	appname = \'$ha_appname\',
					sourcePath = \'$backup_dir\',
					destPath = \'$restore_dir\',
					preCommandSource = \'$pre_script_path\',
					postCommandTarget = \'$post_script_path\'
				     WHERE
				     	id ='$id' and
					appname = \'$appname\'";
	$dbh->do($update_active_passive) or die $dbh->errstr;
    }

    if ($passive_active_ref = $dbh->selectrow_hashref($passive_active_job_sql))
    {

	my $id = $passive_active_ref->{'id'};
	my $appname = $passive_active_ref->{'appname'};

	print "Changing FX job for $appname\n";
        print "-" x 10 ." DETAILS " . "-" x 10 ."\n";
        print "Job id : $id ==> $id\n";
        print "Appname: $appname ==> $ha_appname\n";
        print "Source directory: /home/svsystems/bin/db2 ==> $backup_dir \n";
        print "Dest directory : /home/svsystems/bin/db2 ==> $restore_dir \n";
        print "Pre command source: /home/svsystems/bin/db2/db_backup.sh ==> $pre_script_path\n";
        print "Post command target: /home/svsystems/bin/db2/db_restore.sh ==> $post_script_path \n";

	#
	# Update the FX job configuration with the modified
	# parameters for 5.0 (PASSIVE to ACTIVE)
	#	
	my $update_passive_active = "UPDATE
					frbJobConfig
				     SET
				     	appname = \'$ha_appname\',
					sourcePath = \'$backup_dir\',
					destPath = \'$restore_dir\',
					preCommandSource = \'$pre_script_path\',
					postCommandTarget = \'$post_script_path\'
				     WHERE
				     	id ='$id' and
					appname = \'$appname\'";
	$dbh->do($update_passive_active) or die $dbh->errstr;
    }
}

#
# Register a HA node to CX pairs 
#
sub register_ha_node
{
    my ($host_guid, $cluster_ip) = @_;

    my %args = (
		 'dbh'=> $dbh,
		 'msg' => '',
		 'host_guid' => $host_guid,
		 'cluster_ip' => $cluster_ip
	       );
	
    #
    # Register HA node
    #
    my $ha_obj = new ConfigServer::HighAvailability::Register(%args);
    $ha_obj->register_node();
}


#
# Connects to DB and returns a valid DB resource.
#
sub connect_db
{
    my $db_type        = $amethyst_vars->{"DB_TYPE"};
    my $db_name        = $amethyst_vars->{"DB_NAME"};
    my $db_host        = "localhost";
    my $db_user        = $amethyst_vars->{"DB_USER"};
    my $db_passwd      = $amethyst_vars->{"DB_PASSWD"};
    my $dbh = undef;
    
    eval
    {
   	 $dbh = DBI->connect("DBI:$db_type:database=$db_name;host=$db_host", 
	    		 $db_user, 
			 $db_passwd, 
			 {
				'RaiseError' => 0, 
				'PrintError' => 1, 
				'AutoCommit' => 1
			 }) || die "Error: $!\n";
    };
    if ($@)
    {
	print "Error connecting to DB: $@\n";
	exit 3;
    }
    else 
    {
        return $dbh; 
    }
}
