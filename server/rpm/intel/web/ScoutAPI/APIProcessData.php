<?php
class APIProcessData {
	
	// mode: Production (or) Simulation (or) ForceSimulation
	private $mode = "Production";
	
	// Controll Information for multiple response handle
	private $haltOnError = false;
	
	// Resource Constants
	const CONST_RESOURCE_REPOSITORY_KEY = "{REP}";
	const CONST_RESOURCE_HOST_KEY = "{HST}";
	const CONST_RESOURCE_VOLUME_KEY = "{VLM}";
	const CONST_RESOURCE_PROTECTION_KEY = "{PRT}";
	const CONST_RESOURCE_MONITORING_KEY = "{MNTR}";
	const CONST_RESOURCE_METADATA_KEY = "{META}";
	const CONST_RESOURCE_RECOVERY_KEY = "{RCRY}";
	const CONST_RESOURCE_ERROR_KEY = "{ER}";
	const CONST_RESOURCE_LICENSING_KEY = "{LIC}";
	const CONST_RESOURCE_ALERT_KEY = "{ALT}";
	
	
	/*
  *Auth Data
  */
	
	private $idtyary = array ("39F830AD9C46DE48B87EBE56F4C919D7C7E96CD9" => "IntelGR:Intel:Y:6347D9177220913691CD5158460E1359524C1B97", "945A427A6EA65B27C549E47694436F96461519AC" => "IntelBR:Intel:Y:D40B236EED8E62E2056939CE59EDB3ADE9B2F474");
	
	/*
 * ACL Data
 */
	private $acl_enabled = true;
	
	/*
 * Resource Map
 */
	private $resource_map = array ("REPOSITORY" => self::CONST_RESOURCE_REPOSITORY_KEY, "REPOSITORIES" => self::CONST_RESOURCE_REPOSITORY_KEY, "HOST" => self::CONST_RESOURCE_HOST_KEY, "VOLUME" => self::CONST_RESOURCE_VOLUME_KEY, "PROTECTION" => self::CONST_RESOURCE_PROTECTION_KEY, "MONITORING" => self::CONST_RESOURCE_MONITORING_KEY, "METADATA" => self::CONST_RESOURCE_METADATA_KEY, "RECOVERY" => self::CONST_RESOURCE_RECOVERY_KEY, "ERROR" => self::CONST_RESOURCE_ERROR_KEY, "LICENSING" => self::CONST_RESOURCE_LICENSING_KEY,"ALERT" => self::CONST_RESOURCE_ALERT_KEY, "RESTORE" => self::CONST_RESOURCE_RECOVERY_KEY, "ASSOCIATE" => self::CONST_RESOURCE_METADATA_KEY, "PROTECTIONDETAILS" => self::CONST_RESOURCE_MONITORING_KEY  , "PAUSE" => self::CONST_RESOURCE_PROTECTION_KEY , "RESUME" => self::CONST_RESOURCE_PROTECTION_KEY , "ADDVOLUMES" => self::CONST_RESOURCE_PROTECTION_KEY,"CONSISTENCY" => self::CONST_RESOURCE_PROTECTION_KEY,"PROTECTIONPOLICIES"=>self::CONST_RESOURCE_LICENSING_KEY,"MBR" => self::CONST_RESOURCE_METADATA_KEY);
	
	//ACL TYPE Possible Values - A: ALL,R-Resource
	private $acl_type = array ("39F830AD9C46DE48B87EBE56F4C919D7C7E96CD9" => "P", "945A427A6EA65B27C549E47694436F96461519AC" => "P");
	
	private $acl_resource_permissions = array ("Intel" => "{REP}{HST}{VLM}{PRT}{MNTR}{RCRY}{ER}{LIC}{META}{ALT}");
	
	//ACL Function Permissions
	private $acl_function_permissions = array("Intel" => "ADDVOLUMESBYSYSTEMGUID,ASSOCIATESYSTEMWITHBACKUPSERVER,DELETEBACKUPPROTECTIONBYSYSTEMGUID,DOWNLOADALERTSFORSYSTEMGUID,GETERRORLOGPATHFORSYSTEMGUID,GETHOSTBYIP,GETHOSTBYNAME,GETHOSTBYSYSTEMGUID,GETPROTECTIONDETAILSBYSYSTEMGUID,ISSUECONSISTENCYBYSYSTEMGUID,LISTCONFIGUREDREPOSITORIES,LISTHOSTS,LISTPROTECTABLEVOLUMESBYSYSTEMGUID,LISTREPOSITORYDEVICES,LISTRESTOREPOINTSBYSYSTEMGUID,LISTVOLUMESBYSYSTEMGUID,PAUSEPROTECTIONBYSYSTEMGUID,PAUSEPROTECTIONFORVOLUMEBYSYSTEMGUID,RESUMEPROTECTIONBYSYSTEMGUID,RESUMEPROTECTIONFORVOLUMEBYSYSTEMGUID,RESYNCPROTECTIONBYSYSTEMGUID,RETRIEVESYSTEMMETADATA,SETUPBACKUPPROTECTIONBYSYSTEMGUID,SETUPREPOSITORY,SETUPREPOSITORYBYSYSTEMGUID,UPDATEPROTECTIONPOLICIESBYSYSTEMGUID,UPLOADSYSTEMMETADATA");
	
	/*
  *  Handler Factories
  */
	
	// Handler Declarations
	//	private $functionHandlers = array ("ListVolumes" => "ListVolumesHandler");
	private $functionHandlers = array ();
	
	private $resourceHandlers = array (self::CONST_RESOURCE_REPOSITORY_KEY => "RepositoryHandler", self::CONST_RESOURCE_HOST_KEY => "HostHandler", self::CONST_RESOURCE_VOLUME_KEY => "VolumeHandler", self::CONST_RESOURCE_PROTECTION_KEY => "ProtectionHandler", self::CONST_RESOURCE_MONITORING_KEY => "MonitoringHandler", self::CONST_RESOURCE_METADATA_KEY => "MetadataHandler", self::CONST_RESOURCE_RECOVERY_KEY => "RecoveryHandler", self::CONST_RESOURCE_ERROR_KEY => "ErrorHandler", self::CONST_RESOURCE_LICENSING_KEY => "LicensingHandler" , self::CONST_RESOURCE_ALERT_KEY => "AlertHandler");
	
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
			$pp = $recAry [3];
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
		
		foreach ( $this->resource_map as $k => $v ) {
			$pos = strpos($functionname, strtoupper($k));
			if ($pos===false) {
			}
			else 
			{
				if ($this->isFunctionSupported ( $functionName, $v )) {
					return $v;
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
			$authFlag = $recAry [2];
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
		$group = $this->getGroup ( $accesskey );
		$permissionList = "";
		if (array_key_exists ( $group, $this->acl_resource_permissions )) {
			$permissionList .= $this->acl_resource_permissions [$group];
		}
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
		$group = $this->getGroup ( $accesskey );
		$permissionList = "";
		if (array_key_exists ( $group, $this->acl_function_permissions )) {
			$permissionList .= $this->acl_function_permissions [$group];
			$permissionList .= ",";
		}
		if (array_key_exists ( $accesskey, $this->acl_function_permissions )) {
			$permissionList .= $this->acl_function_permissions [$accesskey];
		}
		return $permissionList;
	
	}

}

?>