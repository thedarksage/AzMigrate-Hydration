<?php
class APIProcessData {
	
	// mode: Production (or) Simulation (or) ForceSimulation
	private $mode = "Production";
	
	// Controll Information for multiple response handle
	private $haltOnError = false;
	
	// Resource Constants
	const CONST_RESOURCE_ARRAY_KEY = "{ARY}";
	const CONST_RESOURCE_REPOSITORY_KEY = "{REP}";
	const CONST_RESOURCE_FABRICPROTECTION_KEY = "{FBP}";
	const CONST_RESOURCE_HOST_KEY = "{HST}";
	const CONST_RESOURCE_VOLUME_KEY = "{VLM}";
	const CONST_RESOURCE_PROTECTION_KEY = "{PRT}";
	const CONST_RESOURCE_MIGRATION_KEY = "{MGT}";
	const CONST_RESOURCE_MONITORING_KEY = "{MNTR}";
	const CONST_RESOURCE_METADATA_KEY = "{META}";
	const CONST_RESOURCE_RECOVERY_KEY = "{RCRY}";
	const CONST_RESOURCE_ERROR_KEY = "{ER}";
	const CONST_RESOURCE_LICENSING_KEY = "{LIC}";
	const CONST_RESOURCE_FX_KEY = "{FX}";
	const CONST_RESOURCE_SYSTEMSETTINGS_KEY = "{SYSSET}";
	const CONST_RESOURCE_ADMINISTRATION_KEY = "{ADMN}";
	const CONST_RESOURCE_LOG_KEY = "{LOG}";
	const CONST_RESOURCE_SYSTEMMONITORING_KEY = "{SYSM}";
	const CONST_RESOURCE_APPLICATIONS_KEY = "{APP}";
	const CONST_RESOURCE_EXCHANGE_KEY = "{EXCH}";
	const CONST_RESOURCE_SQLSERVER_KEY = "{MSSQL}";
	const CONST_RESOURCE_FILESERVER_KEY = "{MSFS}";
	const CONST_RESOURCE_ORACLE_KEY = "{ORCL}";
	const CONST_RESOURCE_ALERT_KEY = "{ALT}";
	const CONST_RESOURCE_PUSH_INSTALL_KEY = "{PSH}";
	const CONST_RESOURCE_POLICY_KEY = "{PLCY}";
	const CONST_RESOURCE_PS_KEY = "{PS}";
	const CONST_RESOURCE_STRG_KEY = "{STRG}";
	const CONST_RESOURCE_SPRT_KEY = "{SPRT}";
	const CONST_RESOURCE_ESX_KEY = "{CLOUD}";
	const CONST_RESOURCE_SCHEDULER_KEY = "{SCHD}";
	const CONST_RESOURCE_ACCOUNT_KEY = "{ACT}";
	const CONST_RESOURCE_MDS_KEY = "{MDS}";

	/*
  *Auth Data
  */
	
	private $idtyary = array ("DDC9525FF275C104EFA1DFFD528BD0145F903CB1" => "Y:4DCC775C9CFBB9F291AE2DBA64AF7CE54D10720A",	// For InMage
                              "A23FF8784CFE3F858A07B2CDEB25CBD27AA99808" => "Y:EA8917B5DCBEAB58D0EAA8A39A8130AF12759C80",	// For CISCO
                              "11F3242118FF2ADD5D117CBF216F29AC578F6BA6" => "Y:11F3242118FF2ADD5D117CBF216F29AC578F6BA6"	// For Microsoft
							  );

	/*
 * ACL Data
 */
	private $acl_enabled = true;
	
	/*
 * Resource Map
 */
	private $resource_map = array (self::CONST_RESOURCE_ARRAY_KEY              => array(
                                                                                        "GETKNOWNARRAYS",
                                                                                        "GETARRAY", 
                                                                                        "GETPROTECTEDLUNS",
                                                                                        "FLUSHANDHOLDWRITES",
                                                                                        "RESUMEREPLICATION",
                                                                                        "FAILOVER",
                                                                                        "GETPAUSESTATUS",
                                                                                        "GETRESUMESTATUS",  
                                                                                        "GETFAILOVERSTATUS", 
                                                                                        "GETMASTERNODE"
                                                                                        ),
									self::CONST_RESOURCE_REPOSITORY_KEY         => array(
                                                                                        "LISTREPOSITORYDEVICES",
                                                                                        "LISTCONFIGUREDREPOSITORIES",
                                                                                        "SETUPREPOSITORYBYSYSTEMGUID",
                                                                                        "SETUPREPOSITORY"
                                                                                        ),
                                    self::CONST_RESOURCE_HOST_KEY               => array(
                                                                                        "HOST",
                                                                                        "LISTSCOUTCOMPONENTS",
                                                                                        "LISTHOSTS",
                                                                                        "GETHOSTBYSYSTEMGUID",
                                                                                        "GETHOSTBYNAME",
                                                                                        "GETHOSTBYIP",
                                                                                        "GETHOSTBYAGENTGUID",
                                                                                        "GETHOSTINFO",
                                                                                        "REFRESHHOSTINFO",
                                                                                        "GETHOSTAGENTDETAILS",
																						"INSERTHOSTCREDENTIALS",
																						"GETHOSTCREDENTIALS",
																						"GETCLUSTERINFO",
                                                                                        "SETVPNDETAILS",
																						"RENEWCERTIFICATE",
																						"MIGRATEACSTOAAD"
                                                                                        ),
									self::CONST_RESOURCE_VOLUME_KEY             => array(
                                                                                        "LISTVOLUMESBYAGENTGUID",
                                                                                        "LISTPROTECTABLEVOLUMESBYAGENTGUID",
                                                                                        "LISTVOLUMESBYSYSTEMGUID",
                                                                                        "LISTPROTECTABLEVOLUMESBYSYSTEMGUID",
                                                                                        "LISTPROTECTEDVOLUMESBYAGENTGUID",
                                                                                        "GETRESYNCPENDINGVOLUMERESIZEPAIRS"
                                                                                        ),
									self::CONST_RESOURCE_PROTECTION_KEY         => array(
                                                                                        "RESTARTRESYNCREPLICATIONPAIRS",
                                                                                        "GETCONFIGUREDPAIRS",
                                                                                        "DELETEPAIRS",
                                                                                        "GETRESIZEORPAUSESTATUS",
                                                                                        "CREATEDATADISKPROTECTION",
                                                                                        "PROTECTIONREADINESSCHECK",
                                                                                        "GETPROTECTIONDETAILS",
                                                                                        "REMOVEPROTECTION",
                                                                                        "STOPPROTECTION",
                                                                                        "RESTARTRESYNC",
                                                                                        "PAUSETRACKINGFORAGENTGUID",
                                                                                        "PAUSETRACKINGFORVOLUMEFORAGENTGUID",
                                                                                        "RESUMETRACKINGFORAGENTGUID",
                                                                                        "RESUMETRACKINGFORVOLUMEFORAGENTGUID",
                                                                                        "SETUPBACKUPPROTECTIONBYSYSTEMGUID",
                                                                                        "DELETEBACKUPPROTECTIONBYSYSTEMGUID",
                                                                                        "ADDVOLUMESBYSYSTEMGUID",
                                                                                        "ISSUECONSISTENCYBYSYSTEMGUID",
                                                                                        "SETUPBACKUPPROTECTIONBYAGENTGUID",
                                                                                        "ISSUECONSISTENCYBYAGENTGUID",
                                                                                        "DELETEBACKUPPROTECTIONBYAGENTGUID",
                                                                                        "ADDVOLUMESBYAGENTGUID",
                                                                                        "PAUSEPROTECTIONBYAGENTGUID",
                                                                                        "RESUMEPROTECTIONBYAGENTGUID",
                                                                                        "CREATEPROTECTION",
                                                                                        "GETPROTECTIONSTATUS",
																						"MODIFYPROTECTION",
																						"MODIFYPROTECTIONPROPERTIES",
																						"PAUSEREPLICATIONPAIRS",
																						"RESUMEREPLICATIONPAIRS",
																						"GETRESYNCREQUIREDHOSTS",
																						"CREATEPROTECTIONV2",
																						"MODIFYPROTECTIONV2",
																						"MODIFYPROTECTIONPROPERTIESV2"
                                                                                        ),
                                    self::CONST_RESOURCE_MONITORING_KEY         => array(
                                                                                        "REFRESHHOSTINFOSTATUS",
                                                                                        "GETPROTECTIONDETAILSBYSYSTEMGUID",
                                                                                        "LISTVOLUMESWITHPROTECTIONDETAILSBYAGENTGUID",
                                                                                        "GETPROTECTIONDETAILSBYAGENTGUID",
                                                                                        "GETREQUESTSTATUS",
                                                                                        "GETHOSTINFO",
                                                                                        "GETCONFIGUREDPLANS",
                                                                                        "GETPROTECTIONSTATE",
																						"LISTPLANS",
																						"CLEANROLLBACKPLANS",
																						"LISTEVENTS",
																						"GETCSPSHEALTH",
                                                                                        "SENDALERTS",
																						"UNREGISTERAGENT",
																						"GETROLLBACKSTATE"
                                                                                        ),                                    
                                    self::CONST_RESOURCE_METADATA_KEY           => array(
                                                                                        "ASSOCIATESYSTEMWITHBACKUPSERVER",
                                                                                        "UPLOADSYSTEMMETADATA",
                                                                                        "RETRIEVESYSTEMMETADATA",
                                                                                        "GETHOSTMBRPATH"     
                                                                                        ),
									
									self::CONST_RESOURCE_RECOVERY_KEY           => array(
                                                                                        "GETCOMMONRECOVERYPOINT",
                                                                                        "LISTRESTOREPOINTSBYAGENTGUID",
                                                                                        "LISTRESTOREPOINTSBYSYSTEMGUID",
                                                                                        "CREATEROLLBACK",
                                                                                        "CREATEHOSTROLLBACK",
																						"ROLLBACKREADINESSCHECK",
																						"VERIFYTAG",
																						"LISTCONSISTENCYPOINTS"
                                                                                        ),
                                    self::CONST_RESOURCE_LICENSING_KEY          => array(
                                                                                        "UPDATEPROTECTIONPOLICIESBYAGENTGUID",
                                                                                        "UPDATEPROTECTIONPOLICIESBYSYSTEMGUID"
                                                                                        ),
									self::CONST_RESOURCE_FX_KEY                 => array(
                                                                                        "PROTECTESX",
                                                                                        "UPDATEESXPROTECTIONDETAILS",
                                                                                        "MONITORESXPROTECTIONSTATUS",
                                                                                        "REMOVEEXECUTIONSTEP",
                                                                                        "STARTFXJOB"
                                                                                        ),  
									self::CONST_RESOURCE_ALERT_KEY              => array(
                                                                                        "DOWNLOADALERTSFORSYSTEMGUID",
                                                                                        "DOWNLOADALERTSFORAGENTGUID"
                                                                                        ),                                                                           
									self::CONST_RESOURCE_PUSH_INSTALL_KEY       => array(
                                                                                        "LISTPUSHSERVERS",
                                                                                        "GETUPGRADEBUILDS",
                                                                                        "UPGRADEAGENTS",
                                                                                        "GETAVAILABLEBUILDS",
                                                                                        "INSTALLAGENT",
                                                                                        "GETINSTALLATIONSTATUS",
                                                                                        "GETUNINSTALLATIONSTATUS",
                                                                                        "UNINSTALLAGENT",
                                                                                        "INSTALLMOBILITYSERVICE",
                                                                                        "UNINSTALLMOBILITYSERVICE",
																						"UPDATEMOBILITYSERVICE"
                                                                                        ),                                    
									self::CONST_RESOURCE_POLICY_KEY             => array(
                                                                                        "INSERTSCRIPTPOLICY"
                                                                                        ),									
									self::CONST_RESOURCE_PS_KEY                 => array(
                                                                                        "GETPSSETTINGS",
                                                                                        "UPDATEDB",
                                                                                        "ADDERRORMESSAGE",
                                                                                        "REGISTERHOST",
                                                                                        "UNREGISTERHOST",
                                                                                        "GETCXROLE",
                                                                                        "SETPSNATIP",
																						"PSFAILOVER",
																						"PSUPDATEDETAILS",
                                                                                        "GETDBDATA",
																						"ISROLLBACKINPROGRESS",
																						"MARKRESYNCREQUIREDTOPAIR",
																						"MARKRESYNCREQUIREDTOPAIRWITHREASONCODE",
																						"GETCSREGISTRY",
																						"GETCXPSCONFIGURATION"
                                                                                        ),									
									self::CONST_RESOURCE_SPRT_KEY               => array(
                                                                                        "GETJOBDETAILS",
                                                                                        "GETPENDINGJOBS",
                                                                                        "ISPRIMARYACTIVE",
                                                                                        "REGISTERSYSTEMHOST",
                                                                                        "UPDATEJOBSTATUS",
                                                                                        "MONITORSTUCKJOBS",
                                                                                        "PROCESSRETRYJOBS",
                                                                                        "GETDNSINFO"
                                                                                        ),									
									self::CONST_RESOURCE_ESX_KEY                => array(                                    
                                                                                        "GETCLOUDDISCOVERY",
                                                                                        "INFRASTRUCTUREDISCOVERY",
                                                                                        "REMOVEINFRASTRUCTURE",
                                                                                        "LISTINFRASTRUCTURE",
																						"UPDATEINFRASTRUCTUREDISCOVERY"
                                                                                        ),                                    
									self::CONST_RESOURCE_SCHEDULER_KEY          => array(
                                                                                        "SCHEDULERUPDATE"
                                                                                        ),
									self::CONST_RESOURCE_ACCOUNT_KEY         	=> array(
																						"CREATEACCOUNT",
																						"UPDATEACCOUNT",
																						"REMOVEACCOUNT",
																						"LISTACCOUNTS"
																						),
									self::CONST_RESOURCE_MDS_KEY    			=> array(
																						 "GETERRORLOGS"
																						 ),													
									self::CONST_RESOURCE_ERROR_KEY				=> array(
																						"NOTIFYERROR"
																						),
																						
									self::CONST_RESOURCE_MIGRATION_KEY			=> array(
																						"MACHINEREGISTRATIONTORCM",
																						"FINALREPLICATIONCYCLE",
																						"MOBILITYAGENTSWITCHTORCM",
																						"RESUMEREPLICATIONTOCS"
																						)
									);
	
	//ACL TYPE Possible Values - A: ALL,R-Resource
	private $acl_type = array ("DDC9525FF275C104EFA1DFFD528BD0145F903CB1" => "P", "A23FF8784CFE3F858A07B2CDEB25CBD27AA99808" => "P", "11F3242118FF2ADD5D117CBF216F29AC578F6BA6" => "P");
	
	private $acl_resource_permissions = array (	"DDC9525FF275C104EFA1DFFD528BD0145F903CB1" => "{SYSSET}{ADMN}{REP}{HST}{META}{VLM}{PRT}{MNTR}{RCRY}{ER}{LIC}{ALT}{PSH}{FX}{PLCY}{PS}{SPRT}{CLOUD}{SCHD}{ACT}{MDS}{MGT}",
												"A23FF8784CFE3F858A07B2CDEB25CBD27AA99808" => "{PSH}{PRT}{HST}{RCRY}", 
												"11F3242118FF2ADD5D117CBF216F29AC578F6BA6" => "{HST}{CLOUD}{MNTR}{PS}{RCRY}{PRT}{PSH}{ACT}{MDS}{ER}{MGT}"
												);
	
	//ACL Function Permissions
	private $acl_function_permissions = array(
												"DDC9525FF275C104EFA1DFFD528BD0145F903CB1" =>"ADDVOLUMESBYAGENTGUID,DELETEBACKUPPROTECTIONBYAGENTGUID,DISCONNECTREPOSITORY,DOWNLOADALERTSFORAGENTGUID,EXPORTDEVICEFROMAGENTONISCSI,EXPORTREPOSITORYONCIFS,EXPORTREPOSITORYONNFS,GETERRORLOGPATHFORAGENTGUID,GETEXPORTEDREPOSITORYDETAILSBYAGENTGUID,GETHOSTBYAGENTGUID,GETHOSTBYIP,GETHOSTBYNAME,GETPROTECTIONDETAILSBYAGENTGUID,ISSUECONSISTENCYBYAGENTGUID,LISTCONFIGUREDREPOSITORIES,LISTHOSTS,LISTPROTECTABLEVOLUMESBYAGENTGUID,LISTPROTECTEDVOLUMESBYAGENTGUID,LISTREPOSITORYDEVICES,LISTRESTOREPOINTSBYAGENTGUID,LISTVOLUMESBYAGENTGUID,LISTVOLUMESWITHPROTECTIONDETAILSBYAGENTGUID,PAUSEPROTECTIONBYAGENTGUID,PAUSEPROTECTIONFORVOLUMEBYAGENTGUID,PAUSETRACKINGFORAGENTGUID,PAUSETRACKINGFORVOLUMEFORAGENTGUID,RESUMEPROTECTIONBYAGENTGUID,RESUMEPROTECTIONFORVOLUMEBYAGENTGUID,RESUMEPROTECTIONWITHRESYNCBYAGENTGUID,RESUMETRACKINGFORAGENTGUID,RESUMETRACKINGFORVOLUMEFORAGENTGUID,RESYNCPROTECTIONBYAGENTGUID,SETUPBACKUPPROTECTIONBYAGENTGUID,SETUPREPOSITORY,UPDATEPROTECTIONPOLICIESBYAGENTGUID,GETHOSTINFO,GETHOSTMBRPATH,REFRESHHOSTINFO,GETREQUESTSTATUS,LISTPUSHSERVERS,PROTECTESX,GETCOMMONRECOVERYPOINT,GETUPGRADEBUILDS,UPGRADEAGENTS,INSERTSCRIPTPOLICY,GETAVAILABLEBUILDS,UPDATEESXPROTECTIONDETAILS,MONITORESXPROTECTIONSTATUS,GETCONFIGUREDPLANS,REMOVEPROTECTION,STOPPROTECTION,REMOVEEXECUTIONSTEP,STARTFXJOB,REFRESHHOSTINFOSTATUS,GETHOSTAGENTDETAILS,GETPSSETTINGS,UPDATEDB,PAUSEREPLICATIONPAIRS,RESUMEREPLICATIONPAIRS,RESTARTRESYNCREPLICATIONPAIRS,GETRESYNCPENDINGVOLUMERESIZEPAIRS,DELETEPAIRS,GETCONFIGUREDPAIRS,GETJOBDETAILS,GETPENDINGJOBS,ISPRIMARYACTIVE,REGISTERSYSTEMHOST,UPDATEJOBSTATUS,MONITORSTUCKJOBS,PROCESSRETRYJOBS,GETDNSINFO,GETRESIZEORPAUSESTATUS,ADDERRORMESSAGE,REGISTERHOST,UNREGISTERHOST,GETCLOUDDISCOVERY,SCHEDULERUPDATE,INSTALLAGENT,GETINSTALLATIONSTATUS,CREATEPROTECTION,GETPROTECTIONSTATUS,CREATEROLLBACK,UNINSTALLAGENT,GETUNINSTALLATIONSTATUS,GETCXROLE,LISTSCOUTCOMPONENTS,INFRASTRUCTUREDISCOVERY,LISTINFRASTRUCTURE,REMOVEINFRASTRUCTURE,SETPSNATIP,CREATEDATADISKPROTECTION,CREATEHOSTROLLBACK,INSTALLMOBILITYSERVICE,UNINSTALLMOBILITYSERVICE,PROTECTIONREADINESSCHECK,GETPROTECTIONSTATE,GETPROTECTIONDETAILS,REMOVEPROTECTION,RESTARTRESYNC,MODIFYPROTECTION,LISTPLANS,CLEANROLLBACKPLANS,LISTEVENTS,GETCSPSHEALTH,VERIFYTAG,GETCLUSTERINFO,INSERTHOSTCREDENTIALS,GETHOSTCREDENTIALS,LISTCONSISTENCYPOINTS,GETROLLBACKSTATE,UPDATEINFRASTRUCTUREDISCOVERY,SETVPNDETAILS,SENDALERTS,UNREGISTERAGENT,GETRESYNCREQUIREDHOSTS,CREATEACCOUNT,UPDATEACCOUNT,REMOVEACCOUNT,LISTACCOUNTS,GETERRORLOGS,PSFAILOVER,NOTIFYERROR,PSUPDATEDETAILS,UPDATEMOBILITYSERVICE,CREATEPROTECTIONV2,MODIFYPROTECTIONV2,MODIFYPROTECTIONPROPERTIESV2,GETDBDATA,ISROLLBACKINPROGRESS,MARKRESYNCREQUIREDTOPAIR,GETCSREGISTRY,RENEWCERTIFICATE,MIGRATEACSTOAAD,GETCXPSCONFIGURATION,MARKRESYNCREQUIREDTOPAIRWITHREASONCODE,MACHINEREGISTRATIONTORCM,FINALREPLICATIONCYCLE,MOBILITYAGENTSWITCHTORCM,RESUMEREPLICATIONTOCS",
												
												"A23FF8784CFE3F858A07B2CDEB25CBD27AA99808" => "INSTALLAGENT,GETINSTALLATIONSTATUS,CREATEDATADISKPROTECTION,GETPROTECTIONSTATUS,LISTHOSTS,CREATEHOSTROLLBACK,UNINSTALLAGENT,GETUNINSTALLATIONSTATUS,LISTPUSHSERVERS", 
												
												"11F3242118FF2ADD5D117CBF216F29AC578F6BA6" => "LISTSCOUTCOMPONENTS,INFRASTRUCTUREDISCOVERY,GETREQUESTSTATUS,LISTINFRASTRUCTURE,REMOVEINFRASTRUCTURE,SETPSNATIP,CREATEPROTECTION,CREATEROLLBACK,INSTALLMOBILITYSERVICE,UNINSTALLMOBILITYSERVICE,PROTECTIONREADINESSCHECK,GETPROTECTIONSTATE,GETPROTECTIONDETAILS,REMOVEPROTECTION,STOPPROTECTION,RESTARTRESYNC,MODIFYPROTECTIONPROPERTIES,MODIFYPROTECTION,LISTPLANS,CLEANROLLBACKPLANS,ROLLBACKREADINESSCHECK,LISTEVENTS,GETCSPSHEALTH,LISTCONSISTENCYPOINTS,GETROLLBACKSTATE,UPDATEINFRASTRUCTUREDISCOVERY,SETVPNDETAILS,SENDALERTS,UNREGISTERAGENT,CREATEACCOUNT,UPDATEACCOUNT,REMOVEACCOUNT,LISTACCOUNTS,GETERRORLOGS,PSFAILOVER,NOTIFYERROR,PSUPDATEDETAILS,UPDATEMOBILITYSERVICE,CREATEPROTECTIONV2,MODIFYPROTECTIONV2,MODIFYPROTECTIONPROPERTIESV2,RENEWCERTIFICATE,MIGRATEACSTOAAD,GETCSREGISTRY,GETCXPSCONFIGURATION,MACHINEREGISTRATIONTORCM,FINALREPLICATIONCYCLE,MOBILITYAGENTSWITCHTORCM,RESUMEREPLICATIONTOCS");
	
	// ACL Function exceptions only on ComponentAuth method
	private $acl_function_exceptions = array('RegisterHost');
	
	/*
  *  Handler Factories
  */
	
	// Handler Declarations
	//	private $functionHandlers = array ("ListVolumes" => "ListVolumesHandler");
	private $functionHandlers = array ();
	
	private $resourceHandlers = array (self::CONST_RESOURCE_REPOSITORY_KEY => "RepositoryHandler", self::CONST_RESOURCE_HOST_KEY => "HostHandler", self::CONST_RESOURCE_VOLUME_KEY => "VolumeHandler", self::CONST_RESOURCE_PROTECTION_KEY => "ProtectionHandler", self::CONST_RESOURCE_MIGRATION_KEY => "MigrationHandler",self::CONST_RESOURCE_MONITORING_KEY => "MonitoringHandler", self::CONST_RESOURCE_METADATA_KEY => "MetadataHandler", self::CONST_RESOURCE_RECOVERY_KEY => "RecoveryHandler", self::CONST_RESOURCE_ERROR_KEY => "ErrorHandler", self::CONST_RESOURCE_LICENSING_KEY => "LicensingHandler" , self::CONST_RESOURCE_ALERT_KEY => "AlertHandler", self::CONST_RESOURCE_PUSH_INSTALL_KEY => "PushInstallHandler", self::CONST_RESOURCE_FX_KEY => "FileHandler", self::CONST_RESOURCE_POLICY_KEY => "PolicyHandler",self::CONST_RESOURCE_PS_KEY => "PSAPIHandler",self::CONST_RESOURCE_SPRT_KEY => "SupportHandler", self::CONST_RESOURCE_ESX_KEY => "ESXHandler", self::CONST_RESOURCE_SCHEDULER_KEY => "SchedulerHandler", self::CONST_RESOURCE_ACCOUNT_KEY => "AccountHandler", self::CONST_RESOURCE_MDS_KEY => "MDSErrorHandler");
	
	/**
	 * @return the $haltOnError
	 */
	public function getHaltOnError() {
		return $this->haltOnError;
	}
	
	/**
	 * @param field_type $haltOnError
	 */
	public function setHaltOnError($haltOnError) {
		$this->haltOnError = $haltOnError;
	}
	
	/*
 * Get the mode of operation
 * Production (or) Simulation
 */
	public function getMode() {
		return $this->mode;
	}
	
	/*
 *  Returns FunctionHandlers array
 */
	public function getFunctionHandlers() {
		return $this->functionHandlers;
	}
	
	/*
 *  Returns ResourceHandlers array
 */
	public function getResourceHandlers() {
		return $this->resourceHandlers;
	}
	
	/*
 *  CheckIf access key is present
 */
	public function isAccessKeyPresent($accesskey) {
		return array_key_exists ( $accesskey, $this->idtyary );
	}
	
	/*
 * Gets PP for give identity
 */
	public function getPP($accesskey) {
		// Convert the identity supplied by use as guid
		$accesskey = strtoupper ( $accesskey );
		// return pp if found
		if (array_key_exists ( $accesskey, $this->idtyary )) {
			$record = $this->idtyary [$accesskey];
			$recAry = explode ( ":", $record );
			$pp = $recAry [1];
			return $pp;
		}
	}
	
	/*
 * Is Acl enabled ?
 */
	public function isACLEnabled() {
		return $this->acl_enabled;
	}
	
	/*
  *  Get ACL Type
  *  A - All, R - Resource , F - Function
  */
	public function getACLType($accesskey) {
		if (array_key_exists ( $accesskey, $this->acl_type )) {
			return $this->acl_type [$accesskey];
		}
	}
	
	/*
   * Get Resource Name 
   */
	 public function getResource($functionName) {
        $functionname = strtoupper ( $functionName );
        foreach ($this->resource_map as $handler => $map_functions)
        {
            if(is_array($map_functions) && in_array($functionname, $map_functions))
            {
                if ($this->isFunctionSupported ( $functionName, $handler )) {
                    return $handler;
                }
            }
        }
    }

	
	/*
 *  Base Function Implementation (ResourceHandler)
 *  Returns true if the function is implemented 
 */
	public function isFunctionSupported($functionName, $resourceCode) {
		$found = false;
		
		if (array_key_exists ( $resourceCode, $this->resourceHandlers )) 

		{
			$className = $this->resourceHandlers [$resourceCode];
			if (file_exists($className . '.php')) {
				$class_methods = get_class_methods ( $className );
				if (array_search ( $functionName, $class_methods ) === false) {
					$found = false;
				} else {
					$found = true;
				}
			}
		}
		
		return $found;
	}
	
	/*
   * Get Group for given id
   */
	public function getGroup($accesskey) {
		$accesskey = strtoupper ( $accesskey );
		$result = "";
		if (array_key_exists ( $accesskey, $this->idtyary )) {
			$record = $this->idtyary [$accesskey];
			$recAry = explode ( ":", $record );
			$group = $recAry [1];
			$result = $group;
		}
		return $result;
	}
	
	/*
   * Get Group for given id
   */
	public function needAuthentication($accesskey) {
		$accesskey = strtoupper ( $accesskey );
		$result = false;
		if (array_key_exists ( $accesskey, $this->idtyary )) {
			$record = $this->idtyary [$accesskey];
			$recAry = explode ( ":", $record );
			$authFlag = $recAry [0];
			if (strtoupper ( $authFlag ) == "Y") {
				$result = true;
			}
		}
		
		return $result;
	}
	
	/*
   * Get Identity Record
   */
	
	public function getIdentityRecord($accesskey) {
		$accesskey = strtoupper ( $accesskey );
		$result = null;
		
		if (array_key_exists ( $accesskey, $this->idtyary )) {
			$record = $this->idtyary [$accesskey];
			$recAry = explode ( ":", $record );
			$accesskeyRec = $recAry [0] . ":" . $recAry [1];
			$result = $accesskeyRec;
		}
		return $result;
	}
	
	/*
   *  Get Resource Permissions
   */
	public function getResourcePermissions($accesskey) {
		
		// Get Group for given $accesskey
		$accesskey = strtoupper ( $accesskey );
		$permissionList = "";
		if (array_key_exists ( $accesskey, $this->acl_resource_permissions )) {
			$permissionList .= $this->acl_resource_permissions [$accesskey];
		}
		return $permissionList;
	}
	
	/*
   *  Get Function Permissions
   */
	public function getFunctionPermissions($accesskey) {
		
		// Get Group for given $accesskey
		$accesskey = strtoupper ( $accesskey );
		$permissionList = "";
		if (array_key_exists ( $accesskey, $this->acl_function_permissions )) {
			$permissionList .= $this->acl_function_permissions [$accesskey];
		}
		return $permissionList;
	}

	/*
   *  Check Function Exceptions   
   */
    public function checkFunctionExceptions($function){
		
		return in_array( $function, $this->acl_function_exceptions);
   }

}

?>
