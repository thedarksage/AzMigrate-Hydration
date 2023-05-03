#!/usr/bin/perl
#$Header: /src/server/ha/db_sync_tgt_post_script.pl,v 1.28 2015/11/25 12:59:40 prgoyal Exp $
#================================================================= 
# FILENAME
#   post_script.pl 
#
# DESCRIPTION
#    This perl script will act as post-script for FX to do
#    DB sync job in case of HA environment.
#    It checks its role from the databes, and if found as running
#    at passive node, it does the DB restore 
#    
# HISTORY
#     24 Nov 2008     kbhattacharya      Created
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;
use Common::Constants;
use DBI();
use Getopt::Long;
use Common::DB::Connection;
use Data::Dumper;
use File::Path;

use constant FILE_IO_FAILURE => -1;
use constant COMMAND_EXEC_FAILURE => -2;
use constant COMMAND_EXEC_SUCCESS => 0;

my $AMETHYST_VARS        = Utilities::get_ametheyst_conf_parameters();
my $DB_TYPE              = $AMETHYST_VARS->{"DB_TYPE"};
my $DB_NAME              = $AMETHYST_VARS->{"DB_NAME"};
my $DB_HOST              = $AMETHYST_VARS->{"DB_HOST"};
my $PS_CS_IP              = $AMETHYST_VARS->{"PS_CS_IP"};
my $DB_USER              = $AMETHYST_VARS->{"DB_USER"};
my $DB_PASSWD            = $AMETHYST_VARS->{"DB_PASSWD"};
my $GUID_FILE            = $AMETHYST_VARS->{"GUID_FILE"};
my $INSTALLATION_DIR     = $AMETHYST_VARS->{"INSTALLATION_DIR"};
my $SVSYSTEM_BIN         = $AMETHYST_VARS->{"SVSYSTEM_BIN"};
my $CLUSTER_IP           = $AMETHYST_VARS->{"CLUSTER_IP"};
my $MYSQL_DUMP_CMD       = "mysql";


my ($mode, $ipAddress);

GetOptions( "mode:s", \$mode, "ip:s", \$ipAddress);

my $guidFile = $INSTALLATION_DIR."/".$GUID_FILE;

my $cxHostId = &get_my_host_id();

my $cxDbRestorePath;
my $DbRestoreCmd;

my ($cxSourceMBRSrcPath,$cxSourceMBRTgtPath);
my ($vxInstallPathFailoverSrcPath,$vxInstallPathFailoverTgtPath);
my ($auditLogFileCopyCmd,$sourceMBRFilesCopyCmd,$vxInstallPathFailoverFilesCopyCmd);

print "CX GUID = $cxHostId\n"; 

#
# The following block is for restoring DB dump
# taken by pre script in case of high-availability 
#
if(uc($mode) eq "HA")
{
    $cxDbRestorePath = $INSTALLATION_DIR."/ha/restore/".$DB_NAME.".".$mode.".sql.bak";
    $DbRestoreCmd = "$MYSQL_DUMP_CMD -u $DB_USER $DB_NAME -p$DB_PASSWD < $cxDbRestorePath 2>&1";
    
    my $cliVmDataSrcPath = $INSTALLATION_DIR."/ha/restore/vcon1";
    my $cliVmDataTgtPath = $INSTALLATION_DIR."/var/cli/vcon";

	my $cliVmDataFileCopyCmd = "cp -Rf $cliVmDataSrcPath/* $cliVmDataTgtPath";

    $cxSourceMBRSrcPath = $INSTALLATION_DIR."/ha/restore/SourceMBR";
    $cxSourceMBRTgtPath = $INSTALLATION_DIR."/admin/web";
    
    $sourceMBRFilesCopyCmd = "cp -Rf $cxSourceMBRSrcPath $cxSourceMBRTgtPath";
    
    $vxInstallPathFailoverSrcPath = $INSTALLATION_DIR."/ha/restore/vcon2";
    $vxInstallPathFailoverTgtPath = $INSTALLATION_DIR."/vcon";
    
    $vxInstallPathFailoverFilesCopyCmd = "cp -Rf $vxInstallPathFailoverSrcPath/* $vxInstallPathFailoverTgtPath";

    print "Its the request for High-Avaialability\n";
    if($CLUSTER_IP)
    {
        $ipAddress =  $CLUSTER_IP;
    }

    if($ipAddress eq "")
    {
        #
        # Neither cluster Ip is supplied
        # nor it is present in amethyst.conf
        #
        exit;
    }
  
    my $activeIp = `ifconfig | grep $ipAddress`;
    
    #my $dbh = DBI->connect_cached("DBI:$DB_TYPE:database=$DB_NAME;host=$ipAddress", $DB_USER, $DB_PASSWD, {
    #                                                                'RaiseError' => 0,
    #                                                                'PrintError' => 1,
    #                                                                 'AutoCommit' => 1});
	my %args = ( 'DB_NAME' => $DB_NAME,
	           'DB_PASSWD'=> $DB_PASSWD,
		       'DB_HOST' => $CLUSTER_IP
	          );
	my $args_ref = \%args;
	
    my $conn = new Common::DB::Connection($args_ref);

    my @param_array = (Common::Constants::PASSIVE_ROLE, $cxHostId);
    my $param_array_ref = \@param_array;
    #my $sqlstmnt = $conn->sql_query("SELECT role FROM cxPair WHERE role = ? AND hostId = ?",$param_array_ref);
	my $sqlstmnt = $conn->sql_query("SELECT nodeRole FROM applianceNodeMap WHERE nodeRole = ? AND nodeId = ?",$param_array_ref);
     

 
   
     if ($conn->sql_num_rows($sqlstmnt) > 0) {
        print "Its the passive NODE - Take the DB restore\n";
        my $retVal = &execute_command($DbRestoreCmd);
        if(COMMAND_EXEC_SUCCESS != $retVal)
        {
            print "Failed to restore DB\n";
        }
        else
        {
            #
            # Update last Db sync timeStamp as DB restore is successfull
            #
            
            my @param_array = ($cxHostId);
            my $param_array_ref = \@param_array;
            my $sqlUpdate = $conn->sql_query("UPDATE applianceNodeMap SET dbTimeStamp = now() WHERE nodeId = ?",$param_array_ref);

            if(-e "$cxDbRestorePath")
            {
                if(!unlink($cxDbRestorePath))
                {
                    print "Failed to delete ($cxDbRestorePath) after restoring\n";
                }
            }
        }

		if(-e "$cliVmDataSrcPath")
        {
            print "Restoring audit log file: $cliVmDataSrcPath \n";
            unless(-d $cliVmDataTgtPath){
                eval(mkpath($cliVmDataTgtPath));
                if($@) #mkpath failed
                {
                    print "Failed to create the directory $cliVmDataTgtPath \n";
                }
			}
			my $retVal = &execute_command($cliVmDataFileCopyCmd);
            if(COMMAND_EXEC_SUCCESS != $retVal)
            {
                print "Failed to restore $cliVmDataSrcPath log file ($cliVmDataFileCopyCmd failed)\n";
            }
        }
        else
        {
            print "$cliVmDataSrcPath does not exist\n";
        }

		if(-e "$cxSourceMBRSrcPath")
        {
            print "Restoring the contents of SourceMBR directory: $cxSourceMBRSrcPath \n";
			my $retVal = &execute_command($sourceMBRFilesCopyCmd);
            if(COMMAND_EXEC_SUCCESS != $retVal)
            {
                print "Failed to restore the contents of $cxSourceMBRSrcPath ($sourceMBRFilesCopyCmd failed)\n";
            }
        }
        else
        {
            print "$cxSourceMBRSrcPath does not exist\n";
        }
        
		if(-e "$vxInstallPathFailoverSrcPath")
        {
            print "Restoring the contents of VxAgent failover_data directory: $vxInstallPathFailoverSrcPath \n";
            unless(-d $vxInstallPathFailoverTgtPath){
				my $mkdir_output = mkdir $vxInstallPathFailoverTgtPath;
                if($mkdir_output ne 1)
                {
                    print "Failed to create the directory $vxInstallPathFailoverTgtPath \n";
                }
			}
			my $retVal = &execute_command($vxInstallPathFailoverFilesCopyCmd);
            if(COMMAND_EXEC_SUCCESS != $retVal)
            {
                print "Failed to restore the contents of $vxInstallPathFailoverSrcPath ($vxInstallPathFailoverFilesCopyCmd failed)\n";
            }
        }
        else
        {
            print "Either $vxInstallPathFailoverSrcPath does not exist\n";
        }        
    }
    else
    {
        #
        # Update last Db sync timeStamp (for passive node to active node)
        # Even though the DB is actually NOT synced
        #

         my @param_array = ($cxHostId);
         my $param_array_ref = \@param_array;
         my $sqlUpdate = $conn->sql_query("UPDATE applianceNodeMap SET dbTimeStamp = now() WHERE nodeId = ?",$param_array_ref);
       
	}
    exit $?; 
}

#
# This functions returns the host Id from GUID file
#
sub get_my_host_id()
{
    if (!open (GUID_FH, "< $guidFile")) 
    {    
        print GUID_FH "Could not open $guidFile for reading:$!\n";
        return FILE_IO_FAILURE;
    }

    my $cxHostId = <GUID_FH>;
    close(GUID_FH);
    chomp $cxHostId; 
    return $cxHostId;
}


#
# This will execute the system command.
# Also returns the command execution status
#
sub execute_command
{
    my ($command) = @_;
        my $retVal;
    eval 
    {
        if (Utilities::isLinux()) 
        {
            $retVal = system($command);
        }
        else
        {
            chdir("C:\\Program Files\\MySQL\\MySQL Server 4.1\\bin");
            $retVal = system($command);
        }
                
    };
    if ($@)
    {
        print "Error executing $command: $@\n";
        return COMMAND_EXEC_FAILURE;
    }
    else
    {
        return COMMAND_EXEC_SUCCESS;
    }
}

1;
