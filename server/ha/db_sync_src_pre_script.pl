#!/usr/bin/perl
#$Header: /src/server/ha/db_sync_src_pre_script.pl,v 1.29 2015/11/25 12:59:40 prgoyal Exp $
#================================================================= 
# FILENAME
#   pre_script.pl 
#
# DESCRIPTION
#    This perl script will act as pre-script for FX to do
#    DB sync job in case of HA environment.
#    
#    This will update its role to the databes (Active/Passive)
#    It will also take the DB dump if it's executing at active 
#    node
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

use constant FILE_IO_FAILURE => -1;
use constant COMMAND_EXEC_FAILURE => -2;
use constant COMMAND_EXEC_SUCCESS => 0;


my $AMETHYST_VARS        = Utilities::get_ametheyst_conf_parameters();
my $DB_TYPE              = $AMETHYST_VARS->{"DB_TYPE"};
my $DB_NAME              = $AMETHYST_VARS->{"DB_NAME"};
my $DB_HOST              = $AMETHYST_VARS->{"DB_HOST"};
my $DB_USER              = $AMETHYST_VARS->{"DB_USER"};
my $DB_PASSWD            = $AMETHYST_VARS->{"DB_PASSWD"};
my $GUID_FILE            = $AMETHYST_VARS->{"GUID_FILE"};
my $INSTALLATION_DIR     = $AMETHYST_VARS->{"INSTALLATION_DIR"};
my $SVSYSTEM_BIN         = $AMETHYST_VARS->{"SVSYSTEM_BIN"};
my $CLUSTER_IP           = $AMETHYST_VARS->{"CLUSTER_IP"};
my $PS_CS_IP             = $AMETHYST_VARS->{"PS_CS_IP"};
my $MYSQL_DUMP_CMD       = "mysqldump";

my ($mode, $ipAddress);
GetOptions( "mode:s", \$mode,"ip:s", \$ipAddress);

my $guidFile = "$INSTALLATION_DIR/$GUID_FILE";
my $cxHostId = &get_my_host_id();
my $cxDbBackupPath;
my ($cxSourceMBRSrcPath,$cxSourceMBRTgtPath);
my ($vxInstallPathFailoverSrcPath,$vxInstallPathFailoverTgtPath);
my $DbDumpCmd;
my ($auditLogFileCopyCmd,$sourceMBRFilesCopyCmd,$vxInstallPathFailoverFilesCopyCmd);

#
# The following block is for taking DB dump
# in case of high-availability 
#
if(uc($mode) eq "HA")
{
     $cxDbBackupPath = $INSTALLATION_DIR."/ha/backup/".$DB_NAME.".".$mode.".sql.bak";
     $DbDumpCmd = "$MYSQL_DUMP_CMD --host=$PS_CS_IP -u $DB_USER -p$DB_PASSWD -C $DB_NAME > $cxDbBackupPath 2>&1";

	my $cliVmDataSrcPath = $INSTALLATION_DIR."/var/cli/vcon";
    my $cliVmDataTgtPath = $INSTALLATION_DIR."/ha/backup/vcon1";

    my $cliVmDataCopyCmd = "cp -Rf $cliVmDataSrcPath $cliVmDataTgtPath";
    
    $cxSourceMBRSrcPath = $INSTALLATION_DIR."/admin/web/SourceMBR";
    $cxSourceMBRTgtPath = $INSTALLATION_DIR."/ha/backup";
    
    $sourceMBRFilesCopyCmd = "cp -Rf $cxSourceMBRSrcPath $cxSourceMBRTgtPath";
    
    $vxInstallPathFailoverSrcPath = $INSTALLATION_DIR."/vcon";
    $vxInstallPathFailoverTgtPath = $INSTALLATION_DIR."/ha/backup/vcon2";
    
    $vxInstallPathFailoverFilesCopyCmd = "cp -Rf $vxInstallPathFailoverSrcPath $vxInstallPathFailoverTgtPath";    
    
    print "It's the request for High-Avaialability\n";
    
    #
    # DB should be backed up from cluster DB
    #
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

    $DB_HOST = $ipAddress;

    print "MODE:==>". $mode."  DB Details:: DB Type: ".
        $DB_TYPE." DB Name: ".$DB_NAME." DB Host: ".$CLUSTER_IP.
        " DB User: ".$DB_USER ." DB Password:" .$DB_PASSWD."\n";

  
	my %args = ( 'DB_NAME' => $DB_NAME,
					   'DB_PASSWD'=> $DB_PASSWD,
					   'DB_HOST' => $CLUSTER_IP
					  );
	my $args_ref = \%args;  
	my $conn = new Common::DB::Connection($args_ref);
    if($activeIp)
    {
        #my $seqFile = "/sys/involflt/common/SequenceNumber";

		my $procDir = "/etc/vxagent/bin/inm_dmit";
		my $seqNo = `$procDir --get_attr SequenceNumber`;

        #if(-e $seqFile)
		if ($seqNo != undef) 
		{     
            chomp $seqNo;
            print "Sequence No: ".$seqNo."\n";
            
            print "It's the active NODE - Take the DB Dump\n";
            
            #
            # Update cxIP table with current ACTIVE role
            #
         
            my @param_array = ($seqNo,$cxHostId);
            my $param_array_ref = \@param_array;
            #my $sqlstmnt = $conn->sql_query("UPDATE cxPair SET sequenceNumber = ? WHERE hostId = ?",$param_array_ref);
			my $sqlstmnt = $conn->sql_query("UPDATE applianceNodeMap SET sequenceNumber = ? WHERE nodeId = ?",$param_array_ref);
            
        }

        my $retVal = &execute_command($DbDumpCmd);
        if(COMMAND_EXEC_SUCCESS != $retVal)
        {
            print "Failed to take DB dump\n";
        }

        if(-e "$cliVmDataSrcPath")
        {
            print "Backing up vmdata xml file: $cliVmDataSrcPath \n";
            if(-e "$cliVmDataTgtPath")
            {
                my $retVal = &execute_command("rm -rf $cliVmDataTgtPath");
            }
            my $retVal = &execute_command($cliVmDataCopyCmd);
            if(COMMAND_EXEC_SUCCESS != $retVal)
            {
                print "Failed to back up $cliVmDataSrcPath file to the back-up directory (($cliVmDataCopyCmd failed))\n";
            }
        }
        else
        {
            print "$cliVmDataSrcPath does not exist\n";
        }

		if(-e "$cxSourceMBRSrcPath")
        {
            print "Backing up the contents of SourceMBR directory: $cxSourceMBRSrcPath \n";
            my $retVal = &execute_command($sourceMBRFilesCopyCmd);
            if(COMMAND_EXEC_SUCCESS != $retVal)
            {
                print "Failed to back up the contents of SourceMBR directory to the back-up directory (($sourceMBRFilesCopyCmd failed))\n";
            }
        }
        else
        {
            print "$cxSourceMBRSrcPath does not exist\n";
        }

		if(-e "$vxInstallPathFailoverSrcPath")
        {
            print "Backing up the contents of VxAgent failover_data directory: $vxInstallPathFailoverSrcPath \n";
            if(-e "$vxInstallPathFailoverTgtPath")
            {
                my $retVal = &execute_command("rm -rf $vxInstallPathFailoverTgtPath");
            }
            my $retVal = &execute_command($vxInstallPathFailoverFilesCopyCmd);
            if(COMMAND_EXEC_SUCCESS != $retVal)
            {
                print "Failed to back up the contents of /home/svsystems/pm/vcon directory to the back-up directory (($vxInstallPathFailoverFilesCopyCmd failed))\n";
            }
        }
        else
        {
            print "$vxInstallPathFailoverSrcPath does not exist\n";
        }
        
        exit $?;  
    }
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
