package Common::Constants;

use lib qw(/home/svsystems/pm);
use DynaLoader ();
use Exporter ();
use strict ;
no strict 'vars';

BEGIN
{
    @ISA = qw(Exporter DynaLoader);
	Exporter::export_ok_tags( keys %EXPORT_TAGS) ;
} ;

# shouldResync flag srcLogicalVolumeDestLogicalVolume.state
use constant
{
    NO_RESYNC => 8
};


# host.state
use constant
{
    UNINSTALL_PENDING => 19
};

# replication states
use constant
{
    RELEASE_DRIVE_ON_TARGET_DELETE_PENDING => 24,
    RELEASE_DRIVE_ON_TARGET_DELETE_DONE => 25,
    RELEASE_DRIVE_ON_TARGET_DELETE_FAIL => -25,
    SOURCE_DELETE_PENDING => 29,
    SOURCE_DELETE_DONE => 30,
    PS_DELETE_DONE => 31
};

# replication states
use constant
{
    INVOLFLT_DRIVER_CLEANUP  => '1010100000'
};


# cxPair.role values for HA
use constant
{
    ACTIVE_ROLE => 'ACTIVE',
    PASSIVE_ROLE => 'PASSIVE',
    DB_SYNC_ENABLED  => 1,
    DB_SYNC_DISABLED =>0
};

# The following global variables are specific to HA fail-over
# srcLogicalVolumeDestLogicalVolume.lun_state
use constant
{
	# failover pre processing is pending
	FAILOVER_PRE_PROCESSING_PENDING => 200,

	# failover pre processing is done
	FAILOVER_PRE_PROCESSING_DONE => 201,

    FAILOVER_DRIVER_DATA_CLEANUP_PENDING => 202,

    FAILOVER_DRIVER_DATA_CLEANUP_DONE => 203,

    FAILOVER_CX_FILE_CLEANUP_PENDING => 204,

    FAILOVER_CX_FILE_CLEANUP_DONE => 205,

	# failover processing is done - lun
	FAILOVER_RECONFIGURATION_PENDING => 206,

	# failover processing is done - lun
	FAILOVER_RECONFIGURATION_DONE => 207,
	
	SOURCE_STALE_REMOVAL_FAIL   => -101,
	
    TARGET_STALE_REMOVAL_FAIL   => -102,
	
    FILE_IO_FAILURE             => -1,
	
    COMMAND_EXEC_FAILURE        => -2,
	
    COMMAND_EXEC_SUCCESS        =>  0,
	
    FILE_TAL                    =>  2,
	
    FTP_TAL                     =>  0,
	
    DIRECT_SYNC                 =>  3,
	
    FAST_SYNC                   =>  2
	
	
};

use constant
{
	CONFIGURATION_SERVER  => 1,
	PROCESS_SERVER => 2,
	CONFIG_PROCESS_SERVER => 3
};

use constant 
{
	PS_WINDOWS => 1,
	PS_LINUX => 2
};


use constant
{      
	PROFILER_HOST_ID        => '5C1DAEF0-9386-44a5-B92A-31FE2C79236A',
	PROFILER_DEVICE_NAME    => 'P'
};

#
# FX DB sync job name
#
use constant
{
    RR_DB_SYNC_JOB => 'RR_DB_SYNC',
    HA_DB_SYNC_JOB => 'HA_DB_SYNC'
};

#
# License file name
#
use constant
{
    LICENSE_FILE => 'license.dat'
};

#
# Replication pause states
#
use constant
{
    PAUSED_PENDING => 41,
    PAUSE_COMPLETED => 26
};

#
# Replication pair lun state
#
use constant
{
    PROTECTED => 2
};

#
# Replication pair lun state
#
use constant
{
    SCENARIO_DELETE_PENDING => 101
};

use constant
{      
	INMAGE_ACCOUNT_ID        => '5C1DAEF0-9386-44a5',
	INMAGE_ACCOUNT_TYPE    => 'PARTNER'
};

use constant
{
	 MONITOR_THREAD_TIME_INTERVAL    =>     60,
	 CS_MIN_MEM			 =>	8053063680,
     VOLSYNC_MAX			 =>	24,
	 SERVICES                        =>     ["monitor","monitor_ps","monitor_disks","volsync","scheduler","httpd","pushinstalld","heartbeat","gentrends","mysqld","bpm","proftpd","wintc","array","prism"],
     WIN_SERVICES                    	 =>     ["monitor","monitor_ps","monitor_disks","volsync","scheduler","w3wp","pushinstall","heartbeat","gentrends","mysqld","bpm","cxps","rbp"],
     MON_SERVICES                    	 =>     ["cxpsctl","tgtd","vxagent"],
	 SEVICE_MONITOR_BUFFER_TIME		=> 60
};

sub new
{
    my $class = shift;
    my $self = {};
    bless $self, $class;
}
1;
