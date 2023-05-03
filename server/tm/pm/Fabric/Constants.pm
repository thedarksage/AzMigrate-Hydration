package Fabric::Constants;

use DynaLoader ();
use Exporter ();
use strict ;
no strict 'vars';

BEGIN
{
    @ISA = qw(Exporter DynaLoader);

    %EXPORT_TAGS = (
		  replication_pair_schema => [ qw (
			REPLICATION_PAIR_TABLE
                        PAIR_SOURCE_HOSTID
                        PAIR_SOURCE_DEVICENAME
                        PAIR_DESTINATION_HOSTID
                        PAIR_DESTINATION_DEVICENAME
                        PAIR_REPLICATION_STATUS
                        PAIR_FULLSYNC_START_TIME
                        PAIR_FULLSYNC_END_TIME
                        PAIR_FULLSYNC_BYTES_SEND
                        PAIR_DIFF_START_TIME
                        PAIR_DIFF_END_TIME
                        PAIR_PHYSICAL_LUN_ID
                        PAIR_LUN_STATE
                        PAIR_OFFSET
		) ],
		  cx_initiator_schema => [ qw (
			CXINITIATOR_TBL
                        CXINITIATOR_host_id
                        CXINITIATOR_cx_nodewwn
                        CXINITIATOR_cx_portwwn
                        CXINITIATOR_setFlag
                ) ],

		  virtual_create_states => [ qw (
			VIRTUAL_CREATE_PENDING
                        VIRTUAL_CREATE_PROGRESS
                        VIRTUAL_CREATE_SUCESS
                        VIRTUAL_CREATE_FAIL
                        VIRTUAL_DELETE_PENDING
                        VIRTUAL_DELETE_PROGRESS
                        VIRTUAL_DELETE_SUCESS
                        VIRTUAL_DELETE_FAIL
                ) ],
		  target_mode_states => [ qw (
                        TARGET_PORT_ENABLE_PENDING
                        TARGET_PORT_ENABLE_REQUIRED
                        TARGET_PORT_ENABLE_SUCCESS
                ) ],
		  mirror_create_states => [ qw (
                        MIRROR_CREATE_PENDING
                        MIRROR_CREATE_PROGRESS
                        MIRROR_CREATE_SUCESS
                        MIRROR_CREATE_FAIL
                        MIRROR_DELETE_PENDING
                        MIRROR_DELETE_PROGRESS
                        MIRROR_DELETE_SUCESS
                        MIRROR_DELETE_FAIL
                ) ],
		  volsync_progress_state => [ qw (
                        VOLUME_SYNCHRONIZATION_PROGRESS
                ) ],

		  san_physical_schema => [ qw (
                        SANPHYSICAL_TBL
                        SANPHYSICAL_appliance_lun_name
                        SANPHYSICAL_appliance_lun_no
                        SANPHYSICAL_capacity
                        SANPHYSICAL_lun_state
                        SANPHYSICAL_Rpairdeleted
                        SANPHYSICAL_uid
                ) ],

		  san_appliance_schema => [ qw (
                        SANAPPLIANCE_TBL
                        SANAPPLIANCE_uid
                ) ],

		  pgr_revoking_schema => [ qw (
                        PGR_REVOKE_PENDING
                        PGR_REVOKE_DONE
                ) ],

		  itl_nexus_schema => [ qw (
                        ITL_NEXUS_TBL
                        ITL_NEXUS_initnodewwn
                        ITL_NEXUS_initportwwn
                        ITL_NEXUS_tgtnodewwn
                        ITL_NEXUS_tgtportwwn
                        ITL_NEXUS_id
                        ITL_NEXUS_lun_no
                        ITL_NEXUS_uid
                        ITL_NEXUS_vendor_name
                ) ]
		  ) ;

	 Exporter::export_ok_tags( keys %EXPORT_TAGS) ;

} ;

use constant
{
	REPLICATION_PAIR_TABLE          =>      'srcLogicalVolumeDestLogicalVolume',
	PAIR_SOURCE_HOSTID              =>      'sourceHostId',
	PAIR_SOURCE_DEVICENAME          =>      'sourceDeviceName',
	PAIR_DESTINATION_HOSTID         =>      'destinationHostId',
	PAIR_DESTINATION_DEVICENAME     =>      'destinationDeviceName',
	PAIR_REPLICATION_STATUS         =>      'replicationStatus',
	PAIR_FULLSYNC_START_TIME        =>      'fullSyncStartTime',
	PAIR_FULLSYNC_END_TIME          =>      'fullSyncEndTime',
	PAIR_FULLSYNC_BYTES_SEND        =>      'fullSyncBytesSend',
	PAIR_DIFF_START_TIME            =>      'differentialStartTime',
	PAIR_DIFF_END_TIME              =>      'differentialEndTime',
	PAIR_PHYSICAL_LUN_ID            =>      'Phy_Lunid',
	PAIR_LUN_STATE                  =>      'lun_state',
	PAIR_OFFSET                     =>      'offset'
};

use constant
{
	CXINITIATOR_host_id             =>      'host_id',
	CXINITIATOR_cx_nodewwn          =>      'cx_nodewwn',
	CXINITIATOR_cx_portwwn          =>      'cx_portwwn',
	CXINITIATOR_setFlag             =>      'setFlag'
};

use constant
{
	VIRTUAL_CREATE_PENDING          =>      '2000',
	VIRTUAL_CREATE_PROGRESS	        =>      '2001',
	VIRTUAL_CREATE_SUCESS	        =>      '2002',
	VIRTUAL_CREATE_FAIL             =>      '2003',
	VIRTUAL_DELETE_PENDING          =>      '2004',
	VIRTUAL_DELETE_PROGRESS         =>      '2005',
	VIRTUAL_DELETE_SUCESS           =>      '2006',
	VIRTUAL_DELETE_FAIL             =>      '2007'
};

use constant
{
	 TARGET_PORT_ENABLE_PENDING      =>      '2500',
	 TARGET_PORT_ENABLE_REQUIRED     =>      '2501',
	 TARGET_PORT_ENABLE_SUCCESS      =>      '2502'
};

use constant
{
	 MIRROR_CREATE_PENDING           =>      '3000',
	 MIRROR_CREATE_PROGRESS          =>      '3001',
	 MIRROR_CREATE_SUCESS            =>      '3002',
	 MIRROR_CREATE_FAIL              =>      '3003',
	 MIRROR_DELETE_PENDING           =>      '3004',
	 MIRROR_DELETE_PROGRESS          =>      '3005',
	 MIRROR_DELETE_SUCESS            =>      '3006',
	 MIRROR_DELETE_FAIL              =>      '3007'
};

use constant
{
	 VOLUME_SYNCHRONIZATION_PROGRESS =>      '5000'
};

use constant
{
	 SANPHYSICAL_appliance_lun_name	=>      'appliance_lun_name',
	 SANPHYSICAL_appliance_lun_no    =>      'appliance_lun_no',
	 SANPHYSICAL_capacity            =>      'capacity',
	 SANPHYSICAL_lun_state           =>      'lun_state',
	 SANPHYSICAL_Rpairdeleted        =>      'Rpairdeleted',
	 SANPHYSICAL_uid                 =>      'uid'
};

use constant
{
	 SANAPPLIANCE_uid                =>      'uid'
};

use constant
{
	 ITL_NEXUS_initnodewwn           =>      'initnodewwn',
	 ITL_NEXUS_initportwwn           =>      'initportwwn',
	 ITL_NEXUS_tgtnodewwn            =>      'tgtnodewwn',
	 ITL_NEXUS_tgtportwwn            =>      'tgtportwwn',
	 ITL_NEXUS_id                    =>      'id',
	 ITL_NEXUS_lun_no                =>      'lun_no',
	 ITL_NEXUS_uid                   =>      'uid',
	 ITL_NEXUS_vendor_name           =>      'vendor_name'
} ;

use constant
{
	PGR_REVOKE_PENDING               =>		'400',
    PGR_REVOKE_DONE					 =>		'401',
	DELETE_MIRROR_POLICY             =>     '19',
	DELETE_MAP_POLICY             	 =>     '20',
	DELETE_AT_LUN_BINDING_DEVCE_PENDING =>  '141',
	DELETE_AT_LUN_BINDING_DEVCE_DONE =>		'142',
	DELETE_AT_LUN_BINDING_DEVCE_FAILURE =>  '-2005',
	RELEASE_DRIVE_ON_TARGET_DELETE_PENDING => '24',
	RELEASE_DRIVE_ON_TARGET_DELETE_DONE	=>	'25',
	RELEASE_DRIVE_ON_TARGET_DELETE_FAILURE => '-25'

};

# work states for ITL discovery and ITL protection
use constant
{
    # the following are the various work states for ITL discovery and protection
    # errors are reported as a value < 0 and all the tables could have an error
    # value. the php equivalents are in admin/web/config.php
    # if you add a new state here add it there as well even if the php code doesn't
    # actually use it. That way when new php ones are added it won't use a value
    # that is in here. the table.column_name that can have the state is listed in
    # the comment above the state variable name

    # sanITLNexus.state
    NO_WORK => 0,

    # srcLogicalVolumeDestLogicalVolume.lun_state, logicalVolumes.lun_state
    START_PROTECTION_PENDING => 1,

    # srcLogicalVolumeDestLogicalVolume.lun_state, logicalVolumes.lun_state
    # frBindingNexus.state
    PROTECTED => 2,

    # srcLogicalVolumeDestLogicalVolume.lun_state, logicalVolumes.lun_state
    STOP_PROTECTION_PENDING => 3,

    # sanITLNexus.state
    DELETE_PENDING => 4,

    # frBindingNexus.state
    CREATE_FR_BINDING_PENDING => 5,

    # frBindingNexus.state
    CREATE_FR_BINDING_DONE => 6,

    # frBindingNexus.state
    CREATE_AT_LUN_PENDING => 7,

    # frBindingNexus.state
    CREATE_AT_LUN_DONE => 8,

    # frBindingNexus.state
    DISCOVER_BINDING_DEVICE_PENDING => 9,

    # frBindingNexus.state
    DISCOVER_BINDING_DEVICE_DONE => 10,

    # frBindingNexus.state
    DELETE_FR_BINDING_PENDING => 11,

    # frBindingNexus.state
    DELETE_FR_BINDING_DONE => 12,

    # frBindingNexus.state
    DELETE_AT_LUN_PENDING => 13,

    # frBindingNexus.state
    DELETE_AT_LUN_DONE => 14,

    # frBindingNexus.state
    DELETE_AT_LUN_BINDING_DEVICE_PENDING => 141,

    # frBindingNexus.state
    DELETE_AT_LUN_BINDING_DEVICE_DONE => 142,

    # sanITNExus.state, discoveryNexus.state
    DISCOVER_IT_LUNS_PENDING => 15,

    # sanITNExus.state, discoveryNexus.state
    DISCOVER_IT_LUNS_DONE => 16,

    # sanITLNexus.state
    NEW_ENTRY_PENDING => 17,

    # frBindingNexus.state
    REBOOT_PENDING => 18,
	
	# host.state
    UNINSTALL_PENDING => 19,

    # discoveryNexus.state
    DISCOVER_IT_LUNS_SCAN_PROGRESS => 8888,

	# resynnc notification frBindingNexus.state
	RESYNC_PENDING => 21,

	# resynnc acknowledge frBindingNexus.state
	RESYNC_ACKED => 22,

    # resynnc acknowledge frBindingNexus.state
	RESYNC_ACKED_FAILED => -1005, 
	
	# source stop protection pending for srcLogicalVolumeDestVolume as per new stop protection logic
	SOURCE_DELETE_PENDING => 29,
	
	# source stop protection done for srcLogicalVolumeDestVolume as per new stop protection logic
	SOURCE_DELETE_DONE => 30,
	
	# full stop protection done for srcLogicalVolumeDestVolume as per new stop protection logic
	PS_DELETE_DONE => 31,
    
	PROCESS_DISCOVERY_NS_ZONE_PENDING => 25,
	
	PROCESS_DISCOVERY_NS_ZONE_DONE => 26,
	
    PROCESS_DISCOVERED_IT_PENDING => 32,

    PROCESS_DISCOVERED_IT_DONE => 33,

    PROCESS_DISCOVERED_IT_FAILED => -33,

    PROCESS_DISCOVERED_ZONE_PENDING => 34,

    PROCESS_DISCOVERED_ZONE_DONE => 35,

    PROCESS_DISCOVERED_ZONE_FAILED => -35,
	
	PROCESS_NAMESERVER_ZONE_PARSING_PENDING => 36,
	
	PROCESS_NAMESERVER_ZONE_PARSING_DONE => 37,
	
	PROCESS_NAMESERVER_PARSING_PENDING => 38,
	
	PROCESS_NAMESERVER_PARSING_DONE => 39,
	
	PROCESS_ZONE_PARSING_PENDING => 40,
	
	PROCESS_ZONE_PARSING_DONE => 41,
	
	PROCESS_NAMESERVER_ZONE_REPFRESH_PENDING => 42,
	
	PROCESS_NAMESERVER_ZONE_REPFRESH_DONE => 43,
	
	PROCESS_SAN_IT_PENDING  => 44 ,
	
	PROCESS_SAN_IT_DONE => 45 ,
	
	PROCESS_DELETE_PENDING => 48,
	
	UNABLE_TO_CONNECT_TO_IP => 50,
	
	USER_NAME_OR_PASSWORD_WRONG => 51,
	
	BP_IP=> 52,
	
	CREATE_SPLIT_MIRROR_PENDING => 53,
	
	CREATE_SPLIT_MIRROR_DONE => 54,
	
	DELETE_SPLIT_MIRROR_PENDING => 55,
	
	DELETE_SPLIT_MIRROR_DONE => 56,
	
	MIRROR_SETUP_PENDING_RESYNC_CLEARED => 57,
	
	DELETE_ENTRY_PENDING => 60,
	
	NODE_REBOOT_OR_PATH_ADDITION_PENDING => 4
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
	
	FAILOVER_TARGET_RECONFIGURATION_PENDING => 212
};


# The following global variables are specific to LUN resize
# srcLogicalVolumeDestLogicalVolume.lun_state
use constant
{
    LUN_RESIZE_PRE_PROCESSING_PENDING => 208,
    LUN_RESIZE_PRE_PROCESSING_DONE => 209,
    LUN_RESIZE_RECONFIGURATION_PENDING => 210,
    LUN_RESIZE_RECONFIGURATION_DONE => 211
};


# sanITNexus deleteFlag values
use constant
{
	 SAN_ITNEXUS_CREATE_REQUESTED => 0,
	 SAN_ITNEXUS_DELETE_REQUESTED => 1,
	 SAN_ITNEXUS_RESCAN_REQUESTED	=> 2
};


# shouldResync flag srcLogicalVolumeDestLogicalVolume.state
use constant
{
    RESYNC_NEEDED => 3,
    RESYNC_MUST_REQUIRED => 4,
    RESYNC_IS_REQUIRED => 5,
    RESYNC_MAY_REQUIRED => 6,
    RESYNC_RECOMMENDED => 7,
    NO_RESYNC => 8,
	RESYNC_NEEDED_UI => 9,
	EXCEPTION_SET => 1,
	EXCEPTION_RESET => 0,
	NO_COMMUNICATION_FROM_SWITCH => 10

};


#  SA Exceptions
use constant
{
    SA_EXCEPTION_APPLIANCE_DISK_DOWN => 1,
    SA_EXCEPTION_APPLIANCE_DISK_UP => 0,
    SA_EXCEPTION_PHYSICAL_DISK_DOWN => 1,
    SA_EXCEPTION_PHYSICAL_DISK_UP => 0,
	SA_EXCEPTION_APPLIANCE_LUN_IO_ERROR => 0,
    SA_EXCEPTION_PHYSICAL_LUN_IO_ERROR => 1
};

#  No of retry for resync during SA exception
use constant
{
    MAX_RESYNC_COUNTER => 3
};

# sanITNexus.rescanFlag values
use constant
{
    RESCAN_RESET => 0,
    RESCAN_SET => 1

};

# sanLuns.capacityChanged values
use constant
{
    CAPACITY_CHANGE_RESET => 0,
    CAPACITY_CHANGE_REPORTED => 1,
    CAPACITY_CHANGE_RESCANNED => 2,
    CAPACITY_CHANGE_UPDATED => 3,
	CAPACITY_CHANGE_PROCESSING => 4
};

# cxPair.role values for HA
use constant
{
    ACTIVE_ROLE => 'ACTIVE',
    PASSIVE_ROLE => 'PASSIVE'
};

# error states for ITL discovery and ITL protection
# the FA and SA have there own error states that
# they might set. all errors will be < 0
use constant
{
	 MATCHING_SA_NOT_FOUND_ERROR => -2,
	 DISCOVER_IT_LUNS_FAILED => -3,
	 DISCOVER_ITL_FAILED_DONE_CLEANUP => -4,
    APPLIANCE_INITIATORS_NOT_CONFIGURED => -5,
    APPLIANCE_TARGETS_NOT_CONFIGURED => -6,
};



use constant
{

	 BASE_WEIGHTAGE 					=> 10000,
	 WEIGHTAGE_FOR_USER_PREFERRED_SA			=> 300,
	 WEIGHTAGE_FOR_SWITCH_HIGH_AVAILABILITY			=> 200,
	 WEIGHTAGE_FOR_VIRTUAL_ENTITY_REUSE			=> 200,
	 WEIGHTAGE_FOR_SA_LOAD_BALANCE				=> 100,
	 WEIGHTAGE_FOR_DPC_LOAD_BALANCE				=> 100,
	 WEIGHTAGE_FOR_ALLOCATED_VIRTUAL_ENTITY			=> -1
};

use constant
{
	
	DEFAULT_MAX_VE_COUNT_PER_DPC				=> 250,
	SAS320_MAX_VE_COUNT_PER_DPC				=> 500
	
};

use constant
{
	
	VI_ENTITIES_NEEDED_FOR_PROTECTING_SINGLE_PIPT_NEXUS	=> 1,
	VT_ENTITIES_NEEDED_FOR_PROTECTING_SINGLE_PIPT_NEXUS	=> 1,
	VTB_ENTITIES_NEEDED_FOR_PROTECTING_SINGLE_PIPT_NEXUS	=> 1
};

use constant
{
	BASE_DIR 					=> '/home/svsystems',
	AMETHYST_CONF_FILE				=> 'etc/amethyst.conf',
	INMAGE_VERSION_FILE				=> 'etc/version',
	COMMON_VI_NODE_WWN				=> '21:00:00:00:00:00:00:00',
	COMMON_VT_NODE_WWN				=> '23:00:00:00:00:00:00:00',
	COMMON_VTB_NODE_WWN				=> '22:00:00:00:00:00:00:00',      
	PROFILER_HOST_ID				=> '5C1DAEF0-9386-44a5-B92A-31FE2C79236A',
	PROFILER_DEVICE_NAME				=> 'P'
};
use constant 
{
    #defining initial lun device name
    INITIAL_LUN_DEVICE_NAME => '/dev/mapper/'
};

# this state added for export vsnap process.
use constant
{
	
	CREATE_EXPORT_LUN_PENDING => 2100,
	CREATE_EXPORT_LUN_DONE => 2101,
	CREATE_EXPORT_LUN_ID_FAILURE => -2102,
	CREATE_EXPORT_LUN_NUMBER_FAILURE => -2103,
	DELETE_EXPORT_LUN_PENDING => 2110,
	DELETE_EXPORT_LUN_DONE => 2111,
	DELETE_EXPORT_LUN_NUMBER_FAILURE => -2112,
	DELETE_EXPORT_LUN_ID_FAILURE => -2113,
	SNAPSHOT_CREATION_PENDING => 2050,
	SNAPSHOT_CREATION_DONE => 2051,
	CREATE_ACG_PENDING => 3100,
	CREATE_ACG_DONE => 3101
};	

use constant
{
	PRISM_FABRIC => 1
};

use constant
{
	TARGET_LUN_MAP_PENDING => 300,
	TARGET_LUN_MAP_DONE => 301,
	TARGET_LUN_MAP_FAILED => -301,
	TARGET_DEVICE_DISCOVERY_PENDING => 303,
	TARGET_DEVICE_DISCOVERY_DONE => 304,
	TARGET_DEVICE_DISCOVERY_FAILED => -304,
	MAP_DISCOVERY_PENDING=>300,
	MAP_DISCOVERY_DONE => 301,
	MAP_DISCOVERY_FAILED => -301
};

use constant
{
    ARRAY_SOLUTION => 'ARRAY',
    PRISM_SOLUTION => 'PRISM'
};

sub new
{
    my $class = shift;
    my $self = {};
    bless $self, $class;
}
1;
