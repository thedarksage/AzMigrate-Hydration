<?
include_once("om_functions.php");
include_once("om_conn.php");


$vol_obj = new VolumeProtection();

$fabric_flag = false;

function inmage_get_configurator() 
{
    return configurator_create();
}

function configurator_create() 
{
    return obj_new( "configurator" );
}

function configurator_get_vx_agent( $cfg, $host_id )
{
    return vxstub_create( $cfg, $host_id );
}

/*
 * Function Name: register_agent
 *
 * Description:
 * $Infs,$Patch_history arguments to register_agent function call
 * Added the default values to the function to support CX upgradtion.
 *       
 * Parameters:
 *     Param 1  [IN]:$cfg 
 *     Param 2  [IN]:$id
 *     Param 3  [IN]:$agent_version
 *     Param 4  [IN]:$driver_version
 *     Param 5  [IN]:$hostname
 *     Param 6  [IN]:$ipaddress
 *     Param 7  [IN]:$capacity
 *     Param 8  [IN]:$os
 *     Param 9  [IN]:$compatibilityNo
 *     Param 10 [IN]:$vxAgentPath
 *     Param 11 [IN]:$osFlag
 *     Param 12 [IN]:$Syvl
 *     Param 13 [IN]:$Syvc
 *     Param 14 [IN]:$Syfs
 *     Param 15 [IN]:$Invc
 *     Param 16 [IN]:$Infs
 *     Param 17 [IN]:$Patch_history
 *     Param 18 [IN]:$AgentMode
 *
 * Return Value:
 */
function register_agent($cfg, $id, $agent_version,$driver_version, $hostname, 
									 $ipaddress, $os,$compatibilityNo, $vxAgentPath, $osFlag,$Dttm="",$Tmzn="",$Invc="",$Infs="",$Syvl="",
									 $Syvc="",$Syfs="",$Patch_history="",$AgentMode="",$prodVerson="",$HypervisorState="",$HypervisorName="",
									 $wwnInfo="",$endianess="",$processorArchitecture="",$cx_info="",$os_array="",$memory="",$mbr_details="",$bios_id="")
{
	global $fabric_flag,$MDS_ACTIVITY_ID;
	global $conn_obj,$commonfunctions_obj, $logging_obj;
	global $multi_query_static;
	
	$prism_conf = new PrismConfigurator();
	$auditFlag = false;
	$first_registeration = false;
	$auditPatchUpdate = false;
	
    update_agent_version( $id, 'sentinel_version', $agent_version );
    update_agent_version( $id, 'involflt_version', $driver_version );
    update_agent_version( $id, 'outpost_version' , $agent_version );

    # to do: simplify this code
    # Register host in the database
    
	#4051 -- To do audit log entry	
	$sql =  "select sentinelEnabled, outpostAgentEnabled from  hosts where id = '" . $conn_obj->sql_escape_string( $id )."'";
	$resultSet= $conn_obj->sql_query($sql);
	if($resultSet == false)
	{
		throw new Exception(mysqli_error($conn_obj));
	}
	$numRows = $conn_obj->sql_num_row($resultSet);
	$rowData = $conn_obj->sql_fetch_row( $resultSet );
	if ($numRows > 0)
	{
		if ($rowData[0] == 0 && $rowData[1] == 0)
		{
			$auditFlag = true;
			$first_registeration = true;
		}
	} 
	else
	{
        $first_registeration = true;
		$auditFlag = true;
	}
	
	$existing_bios_id = $conn_obj->sql_get_value("hosts","biosId","id = ?", array($id));
	
	$bios_id_check = "";
	if($bios_id && $existing_bios_id && ($existing_bios_id == $bios_id))
	{
		$bios_id_check = " AND biosId = '".$conn_obj->sql_escape_string($bios_id)."'";
	}
	
    $my_current_time = time();
    $iter = $conn_obj->sql_query( "select count(*), ipaddress, biosId from hosts where id = '" . $conn_obj->sql_escape_string( $id ) . "'$bios_id_check group by ipaddress");
    $row = $conn_obj->sql_fetch_row( $iter );
	$logging_obj->my_error_handler("INFO",array("count"=>$row[ 0 ],"Existing_bios_id"=>$existing_bios_id, "Reported_bios_id"=>$bios_id),Log::BOTH);
    if( $row[ 0 ] > 0 ) 
	{
		$col = $bios_column = "";
		
		if((!$row[2] && $bios_id) || ($row[2] && ($row[2] != $bios_id)))
		{	
			$bios_column = ", biosId = '".$conn_obj->sql_escape_string( $bios_id )."'";
		}
		
		$is_given_for_migration_column = ", isGivenToMigration = 0";
		
		if(is_array($mbr_details)) {	
			// Update Agent Role (Role=Agent(Source Agent)/MasterTarget(Master target))
			if(array_key_exists('Role',$mbr_details)) {
				$agent_role = trim($mbr_details['Role']);
				$agent_role = $conn_obj->sql_escape_string($agent_role);
				$col = $col.", agentRole = '$agent_role'";
			}
			
			// Updating resourceId
			if(array_key_exists('resource_id', $mbr_details)) 
			{
				$resource_id = trim($mbr_details['resource_id']);
				$resource_id = $conn_obj->sql_escape_string($resource_id);
				$originalHost = 1;                 
				$host_info = $conn_obj->sql_query("SELECT id FROM hosts WHERE resourceId = ? and id != ? and originalHost = '1'", array($resource_id, $id));                    
				if(count($host_info) == 1)  $originalHost = 0;
				$col = $col.", resourceId = '$resource_id', originalHost = '$originalHost'";
			}
						
			if(array_key_exists('MT_SUPPORTED_DATAPLANES', $mbr_details)) 
			{
				$data_plane = trim($mbr_details['MT_SUPPORTED_DATAPLANES']);
				$data_plane = $conn_obj->sql_escape_string($data_plane);
				$col = $col.", TargetDataPlane = '$data_plane'";
			}
			
			if(array_key_exists('external_ip_address', $mbr_details)) 
			{
				$public_ip = trim($mbr_details['external_ip_address']);
				$public_ip = $conn_obj->sql_escape_string($public_ip);
				$col = $col.", publicIp = '$public_ip'";
			}
			
			if(array_key_exists('MarsAgentVersion', $mbr_details)) 
			{
				$mars_agent_version = trim($mbr_details['MarsAgentVersion']);
				$mars_agent_version = $conn_obj->sql_escape_string($mars_agent_version);
				$col = $col.", marsVersion = '$mars_agent_version'";
			}

			if(array_key_exists('FirmwareType', $mbr_details)) 
			{
				$firmware_type = trim($mbr_details['FirmwareType']);
				$firmware_type = $conn_obj->sql_escape_string($firmware_type);
				$col = $col.", FirmwareType = '$firmware_type'";
			}
			
			if(array_key_exists('SystemUUID', $mbr_details)) 
			{
				$agent_bios_id = trim($mbr_details['SystemUUID']);
				$agent_bios_id = $conn_obj->sql_escape_string($agent_bios_id);
				$col = $col.", AgentBiosId = '$agent_bios_id'";
			}
			
			if(array_key_exists('FQDN', $mbr_details)) 
			{
				$agent_fqdn = trim($mbr_details['FQDN']);
				$agent_fqdn = $conn_obj->sql_escape_string($agent_fqdn);
				$col = $col.", AgentFqdn = '$agent_fqdn'";
			}
		}
		
        $query =  "update hosts set sentinelEnabled = 1, outpostAgentEnabled = 1,lastHostUpdateTime = $my_current_time,compatibilityNo = $compatibilityNo ,hypervisorState = $HypervisorState ,hypervisorName = '".$conn_obj->sql_escape_string($HypervisorName)."' , vxAgentPath = '".$conn_obj->sql_escape_string($vxAgentPath)."', endianess = '".$conn_obj->sql_escape_string($endianess)."', processorArchitecture='".$conn_obj->sql_escape_string($processorArchitecture)."', osBuild = '$os_array[0]' , majorVersion = '$os_array[2]', minorVersion = '$os_array[3]', memory = $memory , osCaption = '$os_array[1]', systemType = '$os_array[5]', systemDrive = '$os_array[6]', systemDirectory = '$os_array[7]', systemDriveDiskExtents = '$os_array[8]' $bios_column $is_given_for_migration_column $col where id = '" . $conn_obj->sql_escape_string( $id ) . "' $bios_id_check";
		$multi_query_static["update"][] = $query;


        # Account for the host changing its IP address
        $query = "update hosts set ipaddress = '" . $conn_obj->sql_escape_string($ipaddress) . "' where id = '" . $conn_obj->sql_escape_string( $id ) . "' and ipaddress <> '" . $conn_obj->sql_escape_string( $ipaddress ) . "'";

		$multi_query_static["update"][] = $query;


		$ps_result = $conn_obj->sql_query("select processServerEnabled from hosts where id = '".$conn_obj->sql_escape_string( $id )."'");
		$ps_verify = $conn_obj->sql_fetch_row($ps_result);
		/* Added the below condition for vCon multiple IP Address conflicts
		--Keerthy */
		if(!$ps_verify[0])
		{
		
        # Account for the host changing its IP address
		        $result = $conn_obj->sql_query( "UPDATE hosts set ipaddress = '" . $conn_obj->sql_escape_string($ipaddress) . "' where id = '" . $conn_obj->sql_escape_string( $id ) . "' and ipaddress <> '" . $conn_obj->sql_escape_string( $ipaddress ) . "'");
		}
		else
		{
			$logging_obj->my_error_handler("DEBUG","register_agent\n Added the above if condition as a vCon fix\n");
		}

		#Start of patch history merges
		$patchUpdate="";
		
		#5207 fix
		$Patch_history = addslashes(preg_replace('/\\\\+/', '\\', $Patch_history));
		$patchUpdate .= ", PatchHistoryVX = '".$conn_obj->sql_escape_string($Patch_history)."'";
	
		if(!empty($Dttm))
		{
			$patchUpdate .= ", agentTimeStamp = \"".$Dttm."\"";
		}
		if(!empty($Tmzn))
		{
			$patchUpdate .= ", agentTimeZone = \"".$conn_obj->sql_escape_string($Tmzn)."\"";
		}
		if(!empty($Invc))
		{
			$patchUpdate .= ", InVolCapacity = $Invc";
		}
		if(!empty($Infs))
		{
			$patchUpdate .= ", InVolFreeSpace = $Infs";
		}
		if(!empty($Syvl))
		{
			$patchUpdate .= ", SysVolPath = \"".$conn_obj->sql_escape_string($Syvl)."\"";
		}
		if(!empty($Syvc))
		{
			$patchUpdate .= ", SysVolCap = $Syvc";
		}
		if(!empty($Syfs))
		{
			$patchUpdate .= ", SysVolFreeSpace = $Syfs";
		}
	
		#End of patch history merges, added $patchUpdate to update query.
		
        # Account for the host changing its hostname
		#Added strtoupper to $hostname for  bug 4758
        $query = "update hosts set name = '" . $conn_obj->sql_escape_string($hostname) . "' $patchUpdate where id = '" . $conn_obj->sql_escape_string( $id ) . "' $bios_id_check";
		$multi_query_static["update"][] = $query;
		

		$query = "UPDATE hosts SET type = '".$conn_obj->sql_escape_string($AgentMode)."' WHERE id = '" . $conn_obj->sql_escape_string( $id ) . "'";
		
		$multi_query_static["update"][] = $query;
		

		if($prodVerson != "") 
		{
			$query ="UPDATE hosts SET `prod_version` = '".$conn_obj->sql_escape_string($prodVerson)."', `osType` = '".$conn_obj->sql_escape_string($os)."' WHERE id = '" . $conn_obj->sql_escape_string( $id ) . "'";
			
			$multi_query_static["update"][] = $query;
			

		}
		#4051 -- To do audit log entry
		$auditPatchUpdate = true;
			

    }
    else 
	{
	    #Start of patch history merges
	    $patchColumn="";
	    $patchValue="";
	    if(!empty($Patch_history))
		{
			$patchColumn .= ",`PatchHistoryVX`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($Patch_history)."\"";
	    }
        
		if(!empty($Dttm))
		{
			$patchColumn .= ",`agentTimeStamp`";
			$patchValue .= ",\"".$Dttm."\"";
	    }

	    if(!empty($Tmzn))
		{
			$patchColumn .= ",`agentTimeZone`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($Tmzn)."\"";
	    }
        
		if(!empty($Invc))
		{
			$patchColumn .= ",`InVolCapacity`";
			$patchValue .= ",$Invc";
	    }	
		
		if(!empty($Infs))
		{
			$patchColumn .= ",`InVolFreeSpace`";
			$patchValue .= ",$Infs";
	    }

	    if(!empty($Syvl))
		{
			$patchColumn .= ",`SysVolPath`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($Syvl)."\"";
	    }
	    
		if(!empty($Syvc))
		{
			$patchColumn .= ",`SysVolCap`";
			$patchValue .= ",$Syvc";
	    }
	    
		if(!empty($Syfs))
		{
			$patchColumn .= ",`SysVolFreeSpace`";
			$patchValue .= ",$Syfs";
	    }	
		if(!empty($agent_version))
		{
			$patchColumn .= ",`sentinel_version`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($agent_version)."\"";
	    }
		if(!empty($driver_version))
		{
			$patchColumn .= ",`involflt_version`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($driver_version)."\"";
	    }
		if(!empty($agent_version))
		{
			$patchColumn .= ",`outpost_version`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($agent_version)."\"";
	    }
		if(!empty($prodVerson))
		{
			$patchColumn .= ",`prod_version`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($prodVerson)."\"";
	    }
		if(!empty($bios_id))
		{
			$patchColumn .= ",`biosId`";
			$patchValue .= ",\"".$conn_obj->sql_escape_string($bios_id)."\"";
	    }

		if($compatibilityNo >= 650000)
		{
			$insert_column = ", `osBuild` , `majorVersion` , `minorVersion` , `memory` , `osCaption` , `systemType`, `systemDrive`, `systemDirectory`, `systemDriveDiskExtents`";
			$insert_value = ", '$os_array[0]', '$os_array[2]', '$os_array[3]', $memory , '$os_array[1]' , '$os_array[5]', '$os_array[6]', '$os_array[7]', '$os_array[8]'";
			
			if(is_array($mbr_details)) {
				if(array_key_exists('disks_layout_file_transfer_status',$mbr_details)) {
					$mbr_info = explode(':',$mbr_details['disks_layout_file_transfer_status']);
					$mbr_file = $conn_obj->sql_escape_string($mbr_info[0]);
					$status = trim($mbr_info[1]);
					if($status == "success") {
						$insert_column = $insert_column." , `latestMBRFileName`";
						$insert_value = $insert_value." , '$mbr_file'"; 
					}
				}
				
				// Insert Agent Role (Role=Agent(Source Agent)/MasterTarget(Master target))
				if(array_key_exists('Role',$mbr_details)) {
					$agent_role = trim($mbr_details['Role']);
					$insert_column = $insert_column." , `agentRole`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($agent_role)."'";
				}             
                // Inserting resourceId and originalHost flag(1- First host registration with same resourceId, 0 - Remainaning hosts registration with same resourceId)
				if(array_key_exists('resource_id', $mbr_details)) {
					$resource_id = trim($mbr_details['resource_id']);
                    $originalHost = 0;                 
                    $host_info = $conn_obj->sql_query("SELECT id FROM hosts WHERE resourceId = ?", array($resource_id));                    
                    if(count($host_info) == 0)  $originalHost = 1;
                    $insert_column = $insert_column." , `resourceId`, `originalHost`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($resource_id)."', '$originalHost'";
				}
				if(array_key_exists('MT_SUPPORTED_DATAPLANES', $mbr_details)) 
				{
					$data_plane = trim($mbr_details['MT_SUPPORTED_DATAPLANES']);
                    $insert_column = $insert_column." , `TargetDataPlane`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($data_plane)."'";
				}
				if(array_key_exists('external_ip_address', $mbr_details)) 
				{
					$public_ip = trim($mbr_details['external_ip_address']);
                    $insert_column = $insert_column." , `publicIp`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($public_ip)."'";
				}
				if(array_key_exists('MarsAgentVersion', $mbr_details)) 
				{
					$mars_agent_version = trim($mbr_details['MarsAgentVersion']);
                    $insert_column = $insert_column." , `marsVersion`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($mars_agent_version)."'";
				}
				if(array_key_exists('FirmwareType', $mbr_details)) 
				{
					$firmware_type = trim($mbr_details['FirmwareType']);
                    $insert_column = $insert_column." , `FirmwareType`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($firmware_type)."'";
				}
				if(array_key_exists('SystemUUID', $mbr_details)) 
				{
					$agent_bios_id = trim($mbr_details['SystemUUID']);
                    $insert_column = $insert_column." , `AgentBiosId`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($agent_bios_id)."'";
				}
				if(array_key_exists('FQDN', $mbr_details)) 
				{
					$agent_fqdn = trim($mbr_details['FQDN']);
                    $insert_column = $insert_column." , `AgentFqdn`";
					$insert_value = $insert_value." , '".$conn_obj->sql_escape_string($agent_fqdn)."'";
				}
			}
		}
		else
		{
			$insert_column = "";
			$insert_value = "";
		}
		$query = "insert into hosts " .
		"(`id`, `name`,`ipaddress`, `sentinelEnabled`, `outpostAgentEnabled`, `osType`,`lastHostUpdateTime`,`compatibilityNo`, `vxAgentPath`,`osFlag` $patchColumn, type , hypervisorState , hypervisorName, endianess, processorArchitecture  $insert_column) " .
		"values( '" . $id . "', \""
            . $conn_obj->sql_escape_string($hostname). "\",'" . $conn_obj->sql_escape_string($ipaddress) . "',1,1,'".$conn_obj->sql_escape_string($os)."',$my_current_time,$compatibilityNo,'".$conn_obj->sql_escape_string($vxAgentPath)."',$osFlag $patchValue, '".$conn_obj->sql_escape_string($AgentMode)."','$HypervisorState' , '".$conn_obj->sql_escape_string($HypervisorName)."', '".$conn_obj->sql_escape_string($endianess)."', '".$conn_obj->sql_escape_string($processorArchitecture)."' $insert_value)";
		
		$multi_query_static["insert"][] = $query;



    }
	
	$trg_agent_type = $commonfunctions_obj->get_host_type($id);
		
	#To do audit log entry
	if ($auditFlag)
	{	
		include_once("writeFile.php");
		if ($os == 1)
		{
			$osName = "WINDOWS";
		}
		if ($os == 2)
		{
			$osName = "LINUX";
		}
		if ($auditPatchUpdate)
		{
			$auditPatch = $patchUpdate;
		}
		else
		{
			$auditPatch = "$patchColumn::$patchValue";
		}
		
		$msg = $commonfunctions_obj->customer_branding_names("VX agent registered to CX with details (hostName::$hostname, ipaddress:$ipaddress, operatingSystem::$os, hostid::$id, sentinelEnabled::1, outpostAgentEnabled::1, HostUpdateTime::$my_current_time, Version::$compatibilityNo, Vx Agent Path::$vxAgentPath, patchDetails($auditPatch))");
		writeLog("admin",$msg);	

		$mds_obj = new MDSErrorLogging();
		$details_arr = array(
								'activityId' 	=> $MDS_ACTIVITY_ID,
								'jobType' 		=> 'Registration',
								'errorString'	=> $msg,
								'eventName'		=> 'CS'
								);
		$mds_obj->saveMDSLogDetails($details_arr);		
	}
	return $first_registeration;
}

function validate_mt_supported_data_planes_value($mbr_details)
{
	$return_value = 0;
	//MT is doing case insensitive comparison and hence doing in same way
	if (is_array($mbr_details))
	{
		if(array_key_exists('MT_SUPPORTED_DATAPLANES', $mbr_details)) 
		{
            $data_plane = $mbr_details['MT_SUPPORTED_DATAPLANES'];
            switch($data_plane)
            {
                case 'INMAGE_DATA_PLANE':
                    $return_value = 1;
                break;
                case 'AZURE_DATA_PLANE':
                    $return_value = 1;
                break;
                case 'INMAGE_DATA_PLANE,AZURE_DATA_PLANE':
                    $return_value = 1;
                break;
                case 'AZURE_DATA_PLANE,INMAGE_DATA_PLANE':
                    $return_value = 1;
                break;
                default;
                    $return_value = 0;
                break;
            }
		}
	}
	return $return_value;
}

function configurator_unregister_host( $cfg, $id )
{
	global $logging_obj;
	$volume_obj = new VolumeProtection();
	$logging_obj->my_error_handler("INFO",array("UnregisterHostId" => $id),Log::BOTH);
	$id = trim($id);
	if($id) 
	{
		$volume_obj->unregister_vx_agent($id);
	}
}

/*
 * Function Name: configurator_register_push_proxy
 *
 * Description: This function will register a push 
 *		proxy host switch agent
 *
 * Parameters:
 *     [IN]: host id, ipaddress, hostname, os flag
 *
 * Exceptions:
 *
 */
function configurator_register_push_proxy (
					    $cfg,
					    $host_id, 
					    $ip_address, 
					    $host_name, 
					    $os_flag="1"
				          )
{
	global $logging_obj;
	$push_install_obj = new PushInstall();
	
	$logging_obj->my_error_handler("DEBUG","configurator_register_push_proxy \n Host id = $host_id \n\n IP address of the host = $ip_address \n\n Hostname = $host_name \n\n osFlag = $os_flag \n");
	$ret_val = $push_install_obj->register_push_proxy( 
							   $host_id, 
							   $ip_address, 
							   $host_name, 
							   $os_flag
						   	 );
	if (!$ret_val)
	{
		$logging_obj->my_error_handler("DEBUG","Error in registering push server\n");
	}
	else
	{
	    $logging_obj->my_error_handler("DEBUG","Push server got registered successfully\n");
	}
}

/*
 * Function Name: configurator_unregister_push_proxy
 *
 * Description: This function will unregister 
 *		push proxy.
 *
 * Parameters:
 *     [IN]: host id
 *
 * Exceptions:
 *
 */
function configurator_unregister_push_proxy($cfg,$host_id)
{
	global $logging_obj;
	
	$logging_obj->my_error_handler("INFO",array("PushProxyHostId"=> $host_id),Log::BOTH);
	$push_install_obj = new PushInstall();
	$ret_val = $push_install_obj->unregister_push_proxy($host_id);
	if (!$ret_val)
	{
		$logging_obj->my_error_handler("DEBUG","Error in un registering push server\n");
	}
	else
	{
		$logging_obj->my_error_handler("DEBUG","Push server unregisterd successfully\n");
	}
}


/*
 * Function Name: get_volume_hash
 *
 * Description:
 *	   Used to get all disks,partition,logicalVolumes etc in an hash.
 *       
 * Parameters:
 *     Param 1  [IN]:$volumes array
 *
 * Return Value: volume hash
 */
function get_volume_hash($volumes)
{
	$outer_volume_hash;
	$key;
	for($i = 0 ; $i < count($volumes) ; $i++)
	{
		$inner_volume_hash = array() ;
		for($j = 0 ; $j < count($volumes[$i]) ; $j++)
		{
			if($j == 0 )
			{
				$inner_volume_hash["ver"] = $volumes[$i][$j];
			}
			if($j == 1 )
			{
				$inner_volume_hash['device_name'] = $volumes[$i][$j];
			}
			if($j == 2 )
			{
				$inner_volume_hash['mount_point'] = $volumes[$i][$j];
			}
			if($j == 3 )
			{
				$inner_volume_hash['file_system'] = $volumes[$i][$j];
			}
			if($j == 4 )
			{
				$inner_volume_hash['is_mounted'] = $volumes[$i][$j];
			}
			if($j == 5 )
			{
				$inner_volume_hash['is_system_volume'] = $volumes[$i][$j];
			}
			if($j == 6 )
			{
				$inner_volume_hash['is_cache_volume'] = $volumes[$i][$j];
			}
			if($j == 7 )
			{
				$inner_volume_hash['capacity'] = $volumes[$i][$j];
			}
			if($j == 8 )
			{
				$inner_volume_hash['freespace'] = $volumes[$i][$j];
			}
			if($j == 9 )
			{
				$inner_volume_hash['physical_offset'] = $volumes[$i][$j];
			}
			if($j == 10 )
			{
				$inner_volume_hash['sector_size'] = $volumes[$i][$j];
			}
			if($j == 11 )
			{
				$inner_volume_hash['deviceLocked'] = $volumes[$i][$j];
			}
			if($j == 12 )
			{
				$key = $volumes[$i][$j];
			}
			if($j == 13 )
			{
				$inner_volume_hash['volumeLabel'] = $volumes[$i][$j];
			}	
			if($j == 14 )
			{
				$inner_volume_hash['deviceId'] = $volumes[$i][$j];
			}
			if($j == 15 )
			{
				$inner_volume_hash['parentId'] = $volumes[$i][$j];
			}
			if($j == 16 )
			{
				$inner_volume_hash['isPartitionAtBlockZero'] = $volumes[$i][$j];
			}
			if($j == 17 )
			{
				$inner_volume_hash['isUsedForPaging'] = $volumes[$i][$j];
			}
			if($j == 18 )
			{
				$inner_volume_hash['rawSize'] = $volumes[$i][$j];
			}
			if($j == 19 )
			{
				$inner_volume_hash['writecacheEnabled'] = $volumes[$i][$j];
			}

			if($inner_volume_hash['isUsedForPaging'] != 1)
			{
				$inner_volume_hash['isUsedForPaging'] = 0;
            }
		}

		$outer_volume_hash[$key][] = $inner_volume_hash;
	}
	
	return $outer_volume_hash;
}


/*
 * Function Name: register_devices
 *
 * Description:
 * This function registers the new devices in the database 
 * or update the records of the existing devices.
 * Devices mean : disks,partition,logicalVolumes,vsnap,volPack,customDevices.
 *       
 * Parameters:
 *     Param 1  [IN]:$cfg 
 *     Param 2  [IN]:$id
 *     Param 3  [IN]:$volumes array
 *     Param 4  [IN]:$check_cluster
 *
 * Return Value:
 */
function register_devices($cfg,$id,$volumes,$check_cluster,$device_names,$compatibilityNo  , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array , $volume_resize_arary, $disk_groups_array , $application_scenario_array , $host_ids, $osFlag='',$cluster_volumes_array='',$pair_type_array='',$host_array='') 
{
    global $conn_obj;
    global $logging_obj;	
	
	$logging_obj->my_error_handler("DEBUG","register_devices \n  Compatibility number =  $compatibilityNo \n\n  Check cluster = $check_cluster \n\n Logical volumes data = ".print_r($logical_volumes_arary,TRUE)."\n Source volumes data = ".print_r($src_volumes_arary,TRUE)."\n Snapshot main data = ".print_r($snap_shot_main_array,TRUE)."\n Volume resize data = ".print_r($volume_resize_arary,TRUE)."\n Disk groups data = ".print_r($disk_groups_array,TRUE)."\n Host id = ".print_r($host_ids,true)." \n\n osFlag = $osFlag \n cluster volumes array = ".print_r($cluster_volumes_array,true)."Pair type array = ".print_r($pair_type_array,true)."\n Host names = ".print_r($host_array,true)."\n");
	
	foreach( $volumes as $case_volume_type => $volume ) 
	{

		switch ($case_volume_type) 
		{
			case 0:
					#DISK
					$logging_obj->my_error_handler("DEBUG","register_devices \n Disks = ".print_r($volume,TRUE)."\n");
					
					register_disks($id,$volume,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids, $osFlag,$cluster_volumes_array,$pair_type_array,$host_array);
				break;
			case 1:
					#VSNAP_MOUNTED
					register_logicalvolumes($id,$volume,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_arrays, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids, $osFlag,
					$cluster_volumes_array,$pair_type_array,$host_array);
				break;
			case 2:
					#VSNAP_UNMOUNTED
					register_logicalvolumes($id,$volume,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids, $osFlag,$cluster_volumes_array,$pair_type_array,$host_array);
				break;
			case 3:
					#UNKNOWN
				break;
			case 4:
					#PARTITION
					$logging_obj->my_error_handler("DEBUG","register_devices\n Disk Partitons = ".print_r($volume,TRUE)."\n");
					
					register_partitions($id,$volume,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids,$cluster_volumes_array,$pair_type_array,$host_array);
				break;
			case 5:
					#LOGICAL_VOLUME
					$logging_obj->my_error_handler("DEBUG","register_devices \n Disk Logicalvolumes = ".print_r($volume,TRUE)."\n");
					
					register_logicalvolumes($id,$volume,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids, $osFlag, $cluster_volumes_array,$pair_type_array,$host_array);
				break;
			case 6:
					#VOLPACK
					register_logicalvolumes($id,$volume,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids, $osFlag,$cluster_volumes_array,$pair_type_array,$host_array);
				break;
			case 7:
					#CUSTOM_DEVICE
				break;
			case 8:
					#EXTENDED PARTITION
				break;	
			case 9:
					#DISK PARTITION
					
					register_disks_partitions($id,$volume,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids);
				break;		
		}
    }
}


/*
 * Function Name: register_disks
 *
 * Description:
 * This function registers the new disks in the database 
 * or update the records of the existing disks
 *       
 * Parameters:
 *     Param 1  [IN]:$cfg 
 *     Param 2  [IN]:$id
 *     Param 3  [IN]:$disks array
 *     Param 4  [IN]:$check_cluster
 *
 * Return Value:
 */
function register_disks($id,$volumes,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids, $osFlag = '',$cluster_volumes_array='',$pair_type_array='',$host_array='')
{
	global $conn_obj;
    global $logging_obj;
	global $vol_obj;
	global $multi_query_static;
	
	$diskId_array = array();
	$disk_array = array();
	$disk_query =  "SELECT
						diskId
					FROM 
						physicalDisks 
					WHERE 
						hostId = '$id'";

	$disk_deviceData = $conn_obj->sql_query($disk_query);
	
	while ($row = $conn_obj->sql_fetch_object($disk_deviceData))
	{
		$disk_array[$row->diskId] = $row->diskId;
	}
	$time_flag = 1;
	$start_time = $end_time ="";
	foreach( $volumes as $disk )
	{
		if($time_flag)
		{
			$start_time = time();
			$time_flag=0;
		}
		if( $compatibilityNo >= 560000)
		{
			$freespace = 0;
		}
		else
		{
			$freespace = $disk['freespace'];
		}

		$scsi_id    				= $disk['scsi_id'];
		$disk_name					= $disk['device_name'];
		$device_type				= $disk['device_type'];
		$vendorOrigin				= $disk['vendorOrigin'];
		$file_system				= $disk['file_system'];
		$mount_point				= $disk['mount_point'];
		$is_mounted					= $disk['is_mounted'];
		$is_system_disk				= $disk['is_system_volume'];
		$is_cache_disk				= $disk['is_cache_volume'];
		$capacity					= $disk['capacity'];
		$diskLocked					= $disk['deviceLocked'];
		$physical_offset			= $disk['physical_offset'];
		$sector_size				= $disk['sector_size'];
		$diskLabel					= $disk['volumeLabel'];
		$isUsedForPaging   			= $disk['isUsedForPaging'];
		$isDiskAtBlockZero		 	= $disk['isPartitionAtBlockZero'];
		$diskGroup    				= $disk['diskGroup'];
		$diskGroupVendor    		= $disk['diskGroupVendor'];
		$isMultipath    			= $disk['isMultipath'];
		$writeCacheState   	 		= $disk['writeCacheState'];
		$diskId						= $disk['device_name'];
		$parentId					= $disk['parentId']; 
		$rawSize            		= $disk['rawSize'];
		$writecacheEnabled 			= $disk['writeCacheState'];
		$formatLabel 				= $disk['formatLabel'];
		$column = "";
		$values = "";
		$update_device = "";
		if($compatibilityNo >= 650000)
		{
			$device_string = '';
			$deviceProperties      = $disk['deviceProperties'];
			if(count($deviceProperties))
			{
				$device_string = serialize($deviceProperties);
			}
			$device_string = $conn_obj->sql_escape_string($device_string);
			$column = ", `deviceProperties`";
			$values = ", '$device_string'";
			$update_device = ", `deviceProperties`= '$device_string'";
		}
		$update_protect_flag = $protect_column  = $protect_flag = "";
		
		if($osFlag == 1)
		{
			$update_protect_flag = ", `isProtectable` = '0'";
			$protect_column = ", `isProtectable`";
			$protect_flag = ", '0'";
		}
		// $formatLabel LABEL_UNKNOWN = 0,SMI = 1,EFI = 2
		
		$my_current_time = time();
		$is_system_disk  = $is_system_disk ?1:0;
		$is_cache_disk   = $is_cache_disk ?1:0;
		$diskLocked      = $diskLocked ?1:0;

		$disk_name   = ($disk_name{strlen($disk_name)-1}=='\\') ? substr($disk_name,0,-1) : $disk_name;
		$device_name = $disk_name;
		$mount_point = ($mount_point{strlen($mount_point)-1}=='\\') ? substr($mount_point,0,-1) : $mount_point;
		$disk_id = $diskId;
		$disk_name = $conn_obj->sql_escape_string ($disk_name);
		$mount_point = $conn_obj->sql_escape_string ($mount_point);
		$diskId = $conn_obj->sql_escape_string ($diskId);

		$diskLabel = $conn_obj->sql_escape_string($diskLabel);


		$disk_numRows = 0;
		
		if(is_array($disk_array))
		{
			$disk_numRows = array_key_exists($disk_id , $disk_array);
		}
		if ($disk_numRows == 0)
		{
			$query = "INSERT 
					  INTO 
							physicalDisks 
							(`diskId`,
							 `hostId`,
							 `diskName`,
							 `diskVendor`,
							 `fileSystem`) 
					  VALUES
							( '$diskId', 
							  '$id',
							  '$disk_name',
							  '$diskLabel',
							  '$file_system')";
			
			$multi_query_static["insert"][] = $query;
		}
		else
		{
			$query="UPDATE 
						physicalDisks 
					SET 
						`diskId` = '$diskId' , 
						`hostId` = '$id' , 
						`diskName` = '$disk_name', 
						`diskVendor` = '$diskLabel' , 
						`fileSystem` = '$file_system' 
					WHERE
						`diskId` = '$diskId' AND 
						`hostId` = '$id'";	

			$multi_query_static["update"][] = $query;
		}
		
		$diskId_array[] = ($diskId{strlen($diskId)-1}=='\\') ? substr($diskId,0,-1) : $diskId;

		
		$lv_numRows = 0;
		if($device_names[$device_name] == "")
		{
			
			$lv_numRows = array_key_exists($device_name , $logical_volumes_arary);
			$where_cond = "hostId = '$id' AND 
						   deviceName = '$disk_name'";
		}
		else
		{
			if((isset($scsi_id)) && ($scsi_id != ""))
			{
				if(($scsi_id == $logical_volumes_arary[$device_name]['Phy_Lunid']) && ( $logical_volumes_arary[$device_name]['volumeType'] == 0))
				{
					$lv_numRows = 1;
				}
			}
			else
			{
				if(array_key_exists($device_name , $logical_volumes_arary) && $logical_volumes_arary[$device_name]['deviceId'] == $disk_id && $logical_volumes_arary[$device_name]['volumeType'] == 0)
				{
					$lv_numRows = 1;
				}
			}
			$where_cond = "hostId = '$id' AND 
							deviceName = '$disk_name' AND 
							deviceId = '$diskId' AND 
							volumeType = 0";

		}

			#12497 Fix
			/* Added for source volume resize notification*/
			$device_properties_obj = array();
			if($lv_numRows) volumeResizeNotification($device_properties_obj,$capacity , TRUE , $logical_volumes_arary[$device_name], $volume_resize_arary , $application_scenario_array, $host_ids ,$disk_name,$src_volumes_arary,$pair_type_array,$host_array,$cluster_volumes_array, $rawSize);
			$end_time = time();
			if(($end_time-$start_time)>=50)
			{
				$sql = "SELECT now()";
				$rs = $conn_obj->sql_query($sql);
				$time_flag=1;
			}
			#12130 Fix
			$farline_protected = $logical_volumes_arary[$device_name]["farLineProtected"];
			$device_flag_inuse = $logical_volumes_arary[$device_name]["deviceFlagInUse"];
			if ( $lv_numRows == 0) 
			{			
				$query="INSERT 
						INTO 
							logicalVolumes 
							(`hostId`,
							`deviceName`, 
							`mountPoint`, 
							`capacity`,
							`lastSentinelChange`,
							`lastOutpostAgentChange`, 
							`dpsId`, 
							`farLineProtected`,
							`startingPhysicalOffset`, 
							`fileSystemType`,
							`systemVolume`,									
							`cacheVolume`,
							`lastDeviceUpdateTime`,
							`deviceLocked`,
							`volumeType`,
							`volumeLabel`,
							`deviceId`,
							`isUsedForPaging`,
							`rawSize`,
							`writecacheEnabled`, 
							`Phy_Lunid`,
							`isMultipath`,
							`vendorOrigin`,
							`formatLabel` $column $protect_column
							)	
						VALUES
							('$id',
							 '$disk_name',
							 '$mount_point',
							 $capacity,
							 0,
							 0,
							 '',
							 0,
							 $physical_offset,
							 '$file_system',
							 $is_system_disk,
							 $is_cache_disk, 
							 $my_current_time,
							 $diskLocked,
							 $case_volume_type,
							 '$diskLabel',
							 '$diskId',
							 '$isUsedForPaging',
							 $rawSize,
							 '$writecacheEnabled',
							 '$scsi_id',
							 '$isMultipath',
							 '$vendorOrigin',
							 '$formatLabel' $values $protect_flag
							 )";

			$multi_query_static["insert"][] = $query;
			$db_volumes = $logical_volumes_arary[$device_name]; 
			$logging_obj->my_error_handler("DEBUG","register_disks \n Device name = $device_name \n\n Disk name = $disk_name \n\n lv numRows = $lv_numRows \n\n Agent sent disk info = ".print_r($disk,TRUE)."\n Database volumes = ".print_r($db_volumes,TRUE)."\n");
			$logging_obj->my_error_handler("DEBUG","register_disks \n Insert query into logicalVolumes table = $query \n");
			}
			else
			{
				$mount_data = get_volume_mount_point($id,$device_name,$mount_point,$logical_volumes_arary, $src_volumes_arary,$snap_shot_main_array);
				
				#12130 Fix
				$query_set = "UPDATE 
									logicalVolumes 
							  SET ";
							  
				$values = "`startingPhysicalOffset` = $physical_offset, 
							`fileSystemType` = '$file_system',
							`systemVolume` = $is_system_disk, 
							`cacheVolume` = $is_cache_disk, 
							`lastDeviceUpdateTime` = $my_current_time,
							`deviceLocked` = $diskLocked,									
							`volumeLabel` = '$diskLabel' 
							 $mount_data ,
							`deviceId` = '$diskId',
							`isUsedForPaging`= '$isUsedForPaging',
							`writecacheEnabled` = '$writecacheEnabled',
							`isMultipath`= '$isMultipath',
							`vendorOrigin`= '$vendorOrigin',
							`formatLabel`= '$formatLabel' $update_device $update_protect_flag";
				#$logging_obj->my_error_handler("DEBUG","register_disks:: farline_protected: $farline_protected, capacity: $capacity \n");	
				if(
						($farline_protected == 1) 
					&&  
						(
								($capacity == 0) 
							||  
								(
									($compatibilityNo >= 550000) 
								  &&
									($rawSize == 0)
								)
						)
				 )
				{												
					$query = $query_set." ".$values." WHERE $where_cond";									
				}
				else
				{
					if($farline_protected != 1)
					{
						$values = $values.", `Phy_Lunid`= '$scsi_id'";
					}
					#12729 Fix
					if($device_flag_inuse == 1 && $compatibilityNo >= 550000 && $rawSize == 0)
					{
						$values = $values.", `capacity` = $capacity, `volumeType` =$case_volume_type";
					}
					else
					{
						$values = $values.", `capacity` = $capacity, `rawSize` = $rawSize, `volumeType` =$case_volume_type";
					}
					$query = $query_set." ".$values." WHERE $where_cond";
				}
				$logging_obj->my_error_handler("DEBUG","register_disks \n Upadte logicalVolumes query = $query \n");
				$multi_query_static["update"][] = $query;
			}

			$dg_numRows = 0;
			
			$disk_key = $diskGroup."_".$device_name;
			if(is_array($disk_groups_array))
			{
				$dg_numRows = array_key_exists ($disk_key , $disk_groups_array);
			}
			$diskGroup = $conn_obj->sql_escape_string($diskGroup);
			if ($dg_numRows == 0) 
			{			
				if(!(empty($diskGroup)))
				{
					$disk_group = " INSERT
									INTO 
										diskGroups 
										(diskGroupName, 
										hostId, 
										deviceName) 
									VALUES 
										('$diskGroup',
										'$id',
										'$disk_name')";

					$multi_query_static["insert"][] = $disk_group;
				}					
			}
			else
			{
				if(!(empty($diskGroup)))
				{
					$disk_group = " UPDATE
										diskGroups 
									SET 
										`diskGroupName` = '$diskGroup',
										`deviceName` ='$disk_name'
									WHERE 
										`diskGroupName` = '$diskGroup' AND 
										`hostId` ='$id' AND 
										`deviceName` ='$disk_name'";
					$multi_query_static["update"][] = $disk_group;
				}
				else
				{
					$disk_group = "DELETE 
									FROM 
										diskGroups 
									WHERE 
										`diskGroupName` = '$diskGroup' AND 
										`hostId` ='$id' AND 
										`deviceName` ='$disk_name'";	
					
					$multi_query_static["delete"][] = $disk_group;

				}
			}
			
		}
 
	$diff_disks = array_diff($disk_array, $diskId_array);

	$logging_obj->my_error_handler("DEBUG","register_disks \n diskId array = ".print_r($diskId_array,TRUE)." \n diskId_db = ".print_r($diskId_db,TRUE)." \n diff_disks = ".print_r($diff_disks,TRUE)." \n");
	
	foreach( $diff_disks as $diff_disk )
	{

		/*
		 Bug fix 3669
		 Added  condition to check if the device flag is set for the volume.
		*/

		$search_key = $id."_".$diff_disk;
		if(is_array($src_volumes_arary))
		{
			if(array_key_exists ($search_key , $src_volumes_arary["source"]) || array_key_exists ($search_key , $src_volumes_arary["target"]) || $logical_volumes_arary[$diff_disk]["deviceFlagInUse"] != 0)
			{
				continue;
			}
		}
		else
		{
			$del_vol =  "DELETE FROM physicalDisks WHERE hostId = '" . $conn_obj->sql_escape_string( $id )."' AND diskId = '" .$conn_obj->sql_escape_string($diff_disk)."'";

			$multi_query_static["delete"][] = $del_vol;


		}
	}
	
}


/*
 * Function Name: register_partitions
 *
 * Description:
 * This function registers the new partition in the database 
 * or update the records of the existing partition
 *       
 * Parameters:
 *     Param 1  [IN]:$cfg 
 *     Param 2  [IN]:$id
 *     Param 3  [IN]:$disks array
 *     Param 4  [IN]:$check_cluster
 *
 * Return Value:
 */
function register_partitions($id,$volumes,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids,$cluster_volumes_array='',$pair_type_array='',$host_array='')
{
	global $conn_obj;
    global $logging_obj;
	global $vol_obj;
	global $multi_query_static;
	
	$partitionId_array = array();
	$partition_array = array();
	$partitionId_db = array();
	$disk_query =  "SELECT
						diskId,
						partitionId
					FROM 
						partitions 
					WHERE 
						hostId = '$id'";

	$disk_deviceData = $conn_obj->sql_query($disk_query);
	
	while ($row = $conn_obj->sql_fetch_object($disk_deviceData))
	{
		$partition_array[$row->diskId."_".$row->partitionId] = $row->partitionId;
		$partitionId_db [] = $row->partitionId;
	}
	
	foreach( $volumes as $partition )
	{
		
		if( $compatibilityNo >= 560000)
		{
			$freespace = 0;
		}
		else
		{
			$freespace = $partition['freespace'];
		}
		$scsi_id    				= $partition['scsi_id'];
		$partition_name				= $partition['device_name'];
		$device_type				= $partition['device_type'];
		$vendorOrigin				= $partition['vendorOrigin'];
		$file_system				= $partition['file_system'];
		$mount_point				= $partition['mount_point'];
		$is_mounted					= $partition['is_mounted'];
		$is_system_partition		= $partition['is_system_volume'];
		$is_cache_partition			= $partition['is_cache_volume'];
		$capacity					= $partition['capacity'];
		$partitionLocked			= $partition['deviceLocked'];
		$physical_offset			= $partition['physical_offset'];
		$sector_size				= $partition['sector_size'];
		$partitionLabel				= $partition['volumeLabel'];
		$isUsedForPaging    		= $partition['isUsedForPaging'];
		$isPartitionAtBlockZero   	= $partition['isPartitionAtBlockZero'];
		$diskGroup    				= $partition['diskGroup'];
		$diskGroupVendor    		= $partition['diskGroupVendor'];
		$isMultipath    			= $partition['isMultipath'];
		$writeCacheState    		= $partition['writeCacheState'];
		$partitionId				= $partition['device_name'];
		#$parentId					= $partition['parentId']; 
		$diskId			   			= $partition['parentId'];
		$rawSize					= $partition['rawSize'];
		$writecacheEnabled			= $partition['writeCacheState'];
		$formatLabel 				= $partition['formatLabel'];
		// $formatLabel LABEL_UNKNOWN = 0,SMI = 1,EFI = 2
		
		$my_current_time = time();
		$is_system_partition  = $is_system_partition ?1:0;
		$is_cache_partition   = $is_cache_partition ?1:0;
		$partitionLocked      = $partitionLocked ?1:0;
		$isPartitionAtBlockZero = $isPartitionAtBlockZero ?1:0;

		$partition_name   = ($partition_name{strlen($partition_name)-1}=='\\') ? substr($partition_name,0,-1) : $partition_name;
		$device_name = $partition_name;
		$mount_point = ($mount_point{strlen($mount_point)-1}=='\\') ? substr($mount_point,0,-1) : $mount_point;
		$disk_id = $diskId;
		$partition_id = $partitionId;
		$partition_name = $conn_obj->sql_escape_string ($partition_name);
		$mount_point = $conn_obj->sql_escape_string ($mount_point);
		$partitionId = $conn_obj->sql_escape_string ($partitionId);
		$diskId = $conn_obj->sql_escape_string ($diskId);

		$partitionLabel = $conn_obj->sql_escape_string($partitionLabel);
	   
		$partition_numRows = 0;
		$partition_key = $disk_id."_".$partition_id;
		if(is_array($partition_array))
		{
			$partition_numRows = array_key_exists($partition_key , $partition_array);
		}

		if ($partition_numRows == 0) 
		{
			$query="INSERT 
					INTO 
						partitions 
						(`partitionId`,
						`hostId`,
						`diskId`, 
						`partitionName`, 
						`freeSpace`,
						`capacity`, 
						`fileSystem`,
						`isPartitionAtBlockZero`) 
					VALUES
						('$partitionId', 
						 '$id',
						 '$diskId',
						 '$partition_name',
						 $freespace,
						 $capacity,
						 '$file_system',
						 $isPartitionAtBlockZero)";

			$multi_query_static["insert"][] = $query;
		}
		else
		{
			$query="UPDATE 
						partitions 
					SET 
						`partitionId` = '$partitionId' , 
						`hostId` = '$id' ,
						`diskId` = '$diskId', 
						`partitionName` = '$partition_name', 
						`freeSpace` = $freespace ,
						`capacity` = $capacity, 
						`fileSystem` = '$file_system' ,
						`isPartitionAtBlockZero` = $isPartitionAtBlockZero 
					WHERE 
						`partitionId` = '$partitionId' AND 
						`diskId` = '$diskId' AND 
						`hostId` = '$id'";

			$multi_query_static["update"][] = $query;
		}
		
		$partitionId_array[] = ($partitionId{strlen($partitionId)-1}=='\\') ? substr($partitionId,0,-1) : $partitionId;
		$pair_disk_numRows = 0;
		$search_key = $id."_".$disk_id;
		if(is_array($src_volumes_arary))
		{
			$pair_disk_numRows = array_key_exists ($search_key , $src_volumes_arary["source"]) || array_key_exists ($search_key , $src_volumes_arary["target"]);
		}

		$disk_is_part_of_snapshot_status = 0;
		if($logical_volumes_arary[$disk_id]['deviceFlagInUse'] == 2)
		{
			$disk_is_part_of_snapshot_status = 1;
		}
		
		$lv_numRows = 0;
		
		if($device_names[$device_name] == "")
		{
			
			$lv_numRows = array_key_exists($device_name , $logical_volumes_arary);
			$where_cond = "hostId = '$id' AND
						   deviceName = '$partition_name'";
		}
		else
		{
			if((isset($scsi_id)) && ($scsi_id != ""))
			{
				if(($scsi_id == $logical_volumes_arary[$device_name]['Phy_Lunid']) && ( $logical_volumes_arary[$device_name]['volumeType'] == 4))
				{
					$lv_numRows = 1;
				}
			}
			else
			{
				if(array_key_exists($device_name , $logical_volumes_arary) && $logical_volumes_arary[$device_name]['deviceId'] == $partition_id && $logical_volumes_arary[$device_name]['volumeType'] == 4)
				{
					$lv_numRows = 1;
				}
			}
			$where_cond = "hostId = '$id' AND 
							deviceId = '$partitionId' AND 
							deviceName = '$partition_name' AND 
							volumeType = 4";
		
		}

		if ((!$pair_disk_numRows) || (!$disk_is_part_of_snapshot_status))
		{

			#12497 Fix
			/* Added for source volume resize notification*/
			$device_properties_obj = array();
			if($lv_numRows) volumeResizeNotification($device_properties_obj,$capacity, TRUE , $logical_volumes_arary[$device_name], $volume_resize_arary , $application_scenario_array, $host_ids , $partition_name,$src_volumes_arary,$pair_type_array,$host_array,$cluster_volumes_array, $rawSize);
			#12130 Fix
							
			$farline_protected = $logical_volumes_arary[$device_name]["farLineProtected"];	
			$device_flag_inuse = $logical_volumes_arary[$device_name]["deviceFlagInUse"];
			
			if ($lv_numRows == 0) 
			{			
				$query="INSERT 
						INTO 
							logicalVolumes 
							(`hostId`,
							`deviceName`, 
							`mountPoint`,
							`capacity`,
							`lastSentinelChange`,
							`lastOutpostAgentChange`, 
							`dpsId`, 
							`farLineProtected`, 
							`startingPhysicalOffset`, 
							`fileSystemType`,
							`systemVolume`, 
							`cacheVolume`, 
							`lastDeviceUpdateTime`,
							`deviceLocked`,
							`volumeType`,
							`volumeLabel` ,
							`deviceId`,
							`isUsedForPaging`,
							`rawSize`,
							`writecacheEnabled`,
							`Phy_Lunid`,
							`isMultipath`,
							`vendorOrigin`,
							`formatLabel`) 
						VALUES
							('$id', 
							 '$partition_name',
							 '$mount_point',
							 $capacity,
							 0,
							 0,
							 '',
							 0,
							 $physical_offset,
							 '$file_system',
							 $is_system_partition,
							 $is_cache_partition,
							 $my_current_time,
							 $partitionLocked,
							 $case_volume_type,
							 '$partitionLabel',
							 '$partitionId',
							 '$isUsedForPaging',
							 $rawSize,
							 '$writecacheEnabled',
							 '$scsi_id',
							 '$isMultipath',
							 '$vendorOrigin',
							 '$formatLabel')";
				$multi_query_static["insert"][] = $query;
				$flag = 1;
				$db_volumes = $logical_volumes_arary[$device_name];
				$logging_obj->my_error_handler("DEBUG","register_partitions \n Device name = $device_name \n\n Partition name = $partition_name \n\n lv numRows = $lv_numRows \n\n Agent sent volume info = ".print_r($partition,TRUE)."\n Database volumes = ".print_r($db_volumes,TRUE)."\n");
				$logging_obj->my_error_handler("DEBUG","register_partitions \n insert query = $query \n");
			}
			else
			{
				$mount_data = get_volume_mount_point($id,$device_name,$mount_point,$logical_volumes_arary, $src_volumes_arary,$snap_shot_main_array);
				#12130 Fix
				$query_set = "UPDATE 
									logicalVolumes 
							  SET ";
							  
				$values = "`startingPhysicalOffset` = $physical_offset, 
							`fileSystemType` = '$file_system',
							`systemVolume` = $is_system_partition, 
							`cacheVolume` = $is_cache_partition, 
							`lastDeviceUpdateTime` = $my_current_time,
							`deviceLocked` = $partitionLocked,									
							`volumeLabel` = '$partitionLabel' 
							 $mount_data ,
							`deviceId` = '$partitionId',
							`isUsedForPaging` = '$isUsedForPaging',
							`writecacheEnabled` = '$writecacheEnabled',
							`isMultipath`= '$isMultipath',
							`vendorOrigin`= '$vendorOrigin',
							`formatLabel`='$formatLabel'";
				#$logging_obj->my_error_handler("DEBUG","register_partitions:: farline_protected: $farline_protected, capacity: $capacity \n");	
				if(
						($farline_protected == 1) 
					&&  
						(
								($capacity == 0) 
							||  
								(
									($compatibilityNo >= 550000) 
								  &&
									($rawSize == 0)
								)
						)
				   )
				{													
					$query = $query_set." ".$values." WHERE $where_cond";							
				}
				else
				{
					if($farline_protected != 1)
					{
						$values = $values.", `Phy_Lunid`= '$scsi_id'";
					}
					#12729 Fix
					if($device_flag_inuse == 1 && $compatibilityNo >= 550000 && $rawSize == 0)
					{
						$values = $values.", `capacity` = $capacity, `volumeType` =$case_volume_type";
					}
					else
					{
						$values = $values.", `capacity` = $capacity, `rawSize` = $rawSize, `volumeType` =$case_volume_type";
					}
					$query = $query_set." ".$values." WHERE $where_cond";
				}
			$logging_obj->my_error_handler("DEBUG","register_partitions \n Update query = $query \n");
				$multi_query_static["update"][] = $query;
			}

		}
		else
		{
			$delete_partition_query =  "DELETE 
										FROM 
											logicalVolumes
										WHERE 
											hostId = '$id' AND 
											deviceId = '$diskId' AND 
											deviceName = '$partitionId' AND 
											volumeType = 4";
			
			$multi_query_static["delete"][] = $delete_partition_query;
			$logging_obj->my_error_handler("DEBUG","register_partitions \n delete partition query = $delete_partition_query \n");
			$flag = 1;
		}
		
		$search_key = $id."_".$partition_id;
		$pair_partition_numRows = 0;
		
		if(is_array($src_volumes_arary))
		{
			$pair_partition_numRows = array_key_exists ($search_key , $src_volumes_arary["source"]) || array_key_exists ($search_key , $src_volumes_arary["target"]);
		}
		
		if (!$pair_partition_numRows)
		{

			$lv_query ="SELECT 
							*
						FROM 
							logicalVolumes 
						WHERE 
							$where_cond";
			
			$lv_deviceData = $conn_obj->sql_query($lv_query);
			if($lv_deviceData == false)
			{
				throw new Exception(mysqli_error($conn_obj));
			}
			$lv_numRows = $conn_obj->sql_num_row($lv_deviceData);
			
			if($flag != 1)
			{
				
				if ($lv_numRows == 0) 
				{	
					$query="INSERT
							INTO 
								logicalVolumes 
								(`hostId`,
								`deviceName`, 
								`mountPoint`, 
								`capacity`, 
								`lastSentinelChange`,
								`lastOutpostAgentChange`, 
								`dpsId`, 
								`farLineProtected`, 
								`startingPhysicalOffset`, 
								`fileSystemType`, 
								`systemVolume`,
								`cacheVolume`, 
								`lastDeviceUpdateTime`,
								`deviceLocked`,
								`volumeType`,
								`volumeLabel` ,
								`deviceId`,
								`isUsedForPaging`,
								`rawSize`,
								`writecacheEnabled`,
								`Phy_Lunid`,
								`isMultipath`,
								`vendorOrigin`,
								`formatLabel`) 
							VALUES
								('$id', 
								'$partition_name',
								'$mount_point',
								$capacity,
								0,
								0,
								'',
								0,
								$physical_offset,
								'$file_system',
								$is_system_partition,
								$is_cache_partition,
								$my_current_time,
								$partitionLocked,
								$case_volume_type,
								'$partitionLabel',
								'$diskId',
								'$isUsedForPaging',
								$rawSize,
								'$writecacheEnabled',
								'$scsi_id',
								'$isMultipath',
								'$vendorOrigin',
								'$formatLabel')";
					$multi_query_static["insert"][] = $query;
				}
				else
				{
					$mount_data = get_volume_mount_point($id,$partition_name,$mount_point,$logical_volumes_arary, $src_volumes_arary,$snap_shot_main_array);
					$query="UPDATE 
								logicalVolumes
							SET 
								`capacity` =$capacity, 
								`startingPhysicalOffset` = $physical_offset, 
								`fileSystemType` = '$file_system', 
								`systemVolume` = $is_system_partition, 
								`cacheVolume` = $is_cache_partition, 
								`lastDeviceUpdateTime` = $my_current_time,
								`deviceLocked` = $partitionLocked,
								`volumeType` =$case_volume_type,
								`volumeLabel` = '$partitionLabel' 
								 $mount_data ,
								`deviceId` = '$diskId',
								`isUsedForPaging` = '$isUsedForPaging',
								`rawSize`  = $rawSize,
								`writecacheEnabled` = '$writecacheEnabled',
								`Phy_Lunid`= '$scsi_id',
								`isMultipath`= '$isMultipath',
								`vendorOrigin`= '$vendorOrigin',
								`formatLabel`= '$formatLabel' 
							WHERE 
								`hostId` = '$id' AND 
								`deviceName` = '$partition_name' AND 
								`deviceId` = '$diskId'";

					
					$multi_query_static["update"][] = $query;
				}

			}					
		}
		
		$disk_key = $diskGroup."_".$device_name;
		#testLog("configurator_register_host_static_info \n disk_key = ".$disk_key);
		$dg_numRows = 0;
		if(is_array($disk_groups_array))
		{
			$dg_numRows = array_key_exists($disk_key , $disk_groups_array);
		}
		$diskGroup = $conn_obj->sql_escape_string($diskGroup);
		if ($dg_numRows == 0) 
		{			
			if(!(empty($diskGroup)))
			{
				$disk_group = " INSERT
								INTO 
									diskGroups 
									(diskGroupName, 
									hostId, 
									deviceName) 
								VALUES 
									('$diskGroup',
									'$id',
									'$partition_name')";

				$multi_query_static["insert"][] = $disk_group;

			}					
		}
		else
		{
			if(!(empty($diskGroup)))
			{
				$disk_group = " UPDATE
									diskGroups 
								SET 
									`diskGroupName` = '$diskGroup',
									`deviceName` ='$partition_name'
								WHERE 
									`diskGroupName` = '$diskGroup' AND
									`hostId` ='$id' AND 
									`deviceName` ='$partition_name'";

				$multi_query_static["update"][] = $disk_group;
			}
			else
			{
				$disk_group = "DELETE 
								FROM 
									diskGroups 
								WHERE 
									`diskGroupName` = '$diskGroup' AND
									`hostId` ='$id' AND 
									`deviceName` ='$partition_name'";
									
				$multi_query_static["delete"][] = $disk_group;

			}
		}
	}

	$diff_partitions = array_diff($partitionId_db, $partitionId_array);	
	$logging_obj->my_error_handler("DEBUG","register_partitions \n partitionId array = ".print_r($partitionId_array,TRUE)." \n partitionId_db = ".print_r($partitionId_db,TRUE)." \n diff_partitions = ".print_r($diff_partitions,TRUE)."\n");
	
	foreach( $diff_partitions as $diff_partition )
	{
		$search_key = $id."_".$diff_partition;
		if(is_array($src_volumes_arary))
		{
			if(array_key_exists ($search_key , $src_volumes_arary["source"]) || array_key_exists ($search_key , $src_volumes_arary["target"]) || $logical_volumes_arary[$diff_partition]["deviceFlagInUse"] != 0)
			{
				continue;
			}
		}
		else
		{
			$del_vol = "DELETE FROM partitions WHERE hostId = '" . $conn_obj->sql_escape_string( $id )."' AND partitionId = '" .$conn_obj->sql_escape_string($diff_partition)."'";

			$multi_query_static["delete"][] = $del_vol;

		}
	}	
}


/*
 * Function Name: register_logicalvolumes
 *
 * Description:
 * This function registers the new partition in the database 
 * or update the records of the existing partition
 *       
 * Parameters:
 *     Param 1  [IN]:$cfg 
 *     Param 2  [IN]:$id
 *     Param 3  [IN]:$disks array
 *     Param 4  [IN]:$check_cluster
 *
 * Return Value:
 */
function register_logicalvolumes($id,$volumes,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids, $osFlag='', $cluster_volumes_array='',$pair_type_array='',$host_array='')
{
	global $conn_obj;
    global $logging_obj;
	global $commonfunctions_obj;
	global $SNAPSHOT_CREATION_PENDING,$SNAPSHOT_CREATION_DONE;
	global $multi_query_static,$capacity_array;
	$exported_array = array();
	$exported_lun_array = array();

	$sql = "SELECT
				exportedFCLunId,
				exportedDeviceName
			FROM
				exportedFCLuns";		
	$rs = $conn_obj->sql_query($sql);
	
	while ($row = $conn_obj->sql_fetch_object($rs))
	{
		$exported_array[$row->exportedDeviceName] = $row->exportedFCLunId;
	}

	$sql_tl = "SELECT DISTINCT
					state,
					exportedFCLunId
				FROM
					exportedTLNexus";
					
	$rs_tl = $conn_obj->sql_query($sql_tl);
	
	while ($row = $conn_obj->sql_fetch_object($rs_tl))
	{
		$exported_lun_array[$row->exportedFCLunId] = $row->state;
	}
 
    $machine_name = $host_array[$id]['name'];
    $os_flag = $host_array[$id]['osFlag'];
    $arr_missing_volumes = array();
	$cluster_hostids = array();
	$cluster_device_array = array();
	/*
	*Below code collects all the peer hostid's from the same cluster other than calling hostid.
	*Also collects all the device info which are registered with CX from all the peer hostid's
	*/
	$cluster_hostids = get_cluster_peer_hostid($id);
	$peer_host_id = implode("','",$cluster_hostids);
	$select_device = "SELECT
						hostId,
						deviceName,
						capacity
					FROM
						logicalVolumes
					WHERE
						hostId IN('$peer_host_id')";
	$rs_device = $conn_obj->sql_query($select_device);
	while($rs_obj = $conn_obj->sql_fetch_object($rs_device))
	{
		$cluster_device_array[$rs_obj->hostId][$rs_obj->deviceName]['capacity'] = $rs_obj->capacity;
	}
	$logging_obj->my_error_handler("INFO","register_logicalvolumes \n select_device :: $select_device , peer_host_id :: $peer_host_id  \n cluster_device_array = ".print_r($cluster_device_array,TRUE));
	foreach( $volumes as $logicalVolume )
	{

		if( $compatibilityNo >= 560000)
		{
			$freespace = 0;
		}
		else
		{
			$freespace = $logicalVolume['freespace'];
		}

		$scsi_id    				= $logicalVolume['scsi_id'];
		$logicalVolume_name			= $logicalVolume['device_name'];
		$device_type				= $logicalVolume['device_type'];
		$vendorOrigin				= $logicalVolume['vendorOrigin'];
		$file_system				= $logicalVolume['file_system'];
		if(
			($file_system=='' || $file_system == NULL)
			&& ($osFlag == 1)
		   )
		{
			$file_system = "NTFS";
		}
			
		$mount_point				= $logicalVolume['mount_point'];
		$is_mounted					= $logicalVolume['is_mounted'];
		$is_system_logicalVolume	= $logicalVolume['is_system_volume'];
		$is_cache_logicalVolume		= $logicalVolume['is_cache_volume'];
		$capacity					= $logicalVolume['capacity'];
		$logicalVolumeLocked		= $logicalVolume['deviceLocked'];
		$physical_offset			= $logicalVolume['physical_offset'];
		$sector_size				= $logicalVolume['sector_size'];
		$logicalVolumeLabel			= $logicalVolume['volumeLabel'];
		$isUsedForPaging   			= $logicalVolume['isUsedForPaging'];
		$islogicalVolumeAtBlockZero = $logicalVolume['isPartitionAtBlockZero'];
		$diskGroup    				= $logicalVolume['diskGroup'];
		$diskGroupVendor    		= $logicalVolume['diskGroupVendor'];
		$isMultipath    			= $logicalVolume['isMultipath'];
		$writeCacheState    		= $logicalVolume['writeCacheState'];
		$logicalVolumeId			= $logicalVolume['device_name'];
		$parentId					= $logicalVolume['parentId']; 
		$rawSize				    = $logicalVolume['rawSize'];
		$writecacheEnabled			= $logicalVolume['writeCacheState'];
		$formatLabel 				= $logicalVolume['formatLabel'];
		
		// $formatLabel LABEL_UNKNOWN = 0,SMI = 1,EFI = 2
		$column = "";
		$values = "";
		$update_device = "";
		if($compatibilityNo >= 650000)
		{
			$device_string = '';
			$deviceProperties      = $logicalVolume['deviceProperties'];
			if(count($deviceProperties))
			{
				$device_string = serialize($deviceProperties);
			}
			$device_string = $conn_obj->sql_escape_string($device_string);
			$column = ", `deviceProperties`";
			$values = ", '$device_string'";
			$update_device = ", `deviceProperties`= '$device_string'";
		}	
		
		$my_current_time = time();
		$is_system_logicalVolume  = $is_system_logicalVolume ?1:0;
		$is_cache_logicalVolume   = $is_cache_logicalVolume ?1:0;
		$deviceLocked      = $logicalVolumeLocked ?1:0;
		$islogicalVolumeAtBlockZero = $islogicalVolumeAtBlockZero ?1:0;
		$logicalVolume_name = ($logicalVolume_name{strlen($logicalVolume_name)-1}=='\\') ? substr($logicalVolume_name,0,-1) : $logicalVolume_name;
		$device_name = $logicalVolume_name;
		$mount_point = ($mount_point{strlen($mount_point)-1}=='\\') ? substr($mount_point,0,-1) : $mount_point;
		$logicalVolumeId = ($logicalVolumeId{strlen($logicalVolumeId)-1}=='\\') ? substr($logicalVolumeId,0,-1) : $logicalVolumeId;
		$logicalVolume_name = $conn_obj->sql_escape_string ($logicalVolume_name);
		$mount_point = $conn_obj->sql_escape_string ($mount_point);
		$volume_id = $logicalVolumeId;
		$logicalVolumeId = $conn_obj->sql_escape_string ($logicalVolumeId);
		
		$logicalVolumeLabel = $conn_obj->sql_escape_string($logicalVolumeLabel);
		
		$numRows = 0;
		$logging_obj->my_error_handler("DEBUG","register_logicalvolumes \n Logical volumes data = ".print_r($logical_volumes_arary,TRUE));
	
		if($device_names[$device_name] == "")
		{
			$numRows = array_key_exists($device_name , $logical_volumes_arary);
			#testLog("IF");
			$where_cond = "hostId = '$id' AND
						   deviceName = '$logicalVolume_name'";
		}
		else
		{
			if((isset($scsi_id)) && ($scsi_id != ""))
			{
				if($scsi_id == $logical_volumes_arary[$device_name]['Phy_Lunid'])
				{
					$numRows = 1;
				}
			}
			else
			{
				if(array_key_exists($device_name , $logical_volumes_arary) && $logical_volumes_arary[$device_name]['deviceId'] == $volume_id)
				{
					$numRows = 1;
				}
			}			
			$where_cond = "hostId = '$id' AND 
						   deviceId = '$logicalVolumeId'";
		
		}

		$device_properties_obj = array();
		if($numRows) volumeResizeNotification($device_properties_obj,$capacity, TRUE , $logical_volumes_arary[$device_name], $volume_resize_arary , $application_scenario_array, $host_ids , $logicalVolume_name,$src_volumes_arary,$pair_type_array,$host_array,$cluster_volumes_array, $rawSize);
		#12130 Fix
		
		$farline_protected = $logical_volumes_arary[$device_name]["farLineProtected"];	
		$device_flag_inuse = $logical_volumes_arary[$device_name]["deviceFlagInUse"];
		
		if ($numRows == 0) 
		{
			$trg_agent_type = $commonfunctions_obj->get_host_type($id);
			$insert_logicalVolume= "INSERT 
									INTO
										logicalVolumes 
										(`hostId`,
										`deviceName`, 
										`mountPoint`, 
										`capacity`, 
										`lastSentinelChange`,
										`lastOutpostAgentChange`, 
										`dpsId`, 
										`farLineProtected`, 
										`startingPhysicalOffset`, 
										`fileSystemType`, 										
										`systemVolume`,
										`cacheVolume`, 
										`lastDeviceUpdateTime`,
										`deviceLocked`,
										`volumeType`,
										`volumeLabel` ,
										`deviceId`,
										`isUsedForPaging`,
										`rawSize`,
										`writecacheEnabled`,
										`Phy_Lunid`,
										`isMultipath`,
										`vendorOrigin`,
										`formatLabel` $column) 
									VALUES
										('$id', 
										'$logicalVolume_name',
										'$mount_point',
										$capacity,
										0,
										0,
										'',
										0,
										$physical_offset,
										'$file_system',										
										$is_system_logicalVolume,
										$is_cache_logicalVolume, 
										$my_current_time,
										$deviceLocked,
										$case_volume_type,
										'$logicalVolumeLabel',
										'$logicalVolumeId',
										'$isUsedForPaging',
										$rawSize,
										'$writecacheEnabled',
										'$scsi_id',
										'$isMultipath',
										'$vendorOrigin',
										'$formatLabel' $values)";
			
			$multi_query_static["insert"][] = $insert_logicalVolume;
			$db_volumes = $logical_volumes_arary[$device_name];
			$logging_obj->my_error_handler("DEBUG","register_logicalvolumes \n Device name = ".$device_name." \n\n Logical volume name = ".$logicalVolume_name." \n\n Number of rows = ".$numRows ." \n\n Agent sent volume info = ".print_r($logicalVolume,TRUE)."\n Database volumes = ".print_r($db_volumes,TRUE)." Insert into logicalVolumes query = $insert_logicalVolume\n");
			

			if($case_volume_type == 1 && strtolower($trg_agent_type) == 'fabric')
			{

				if(is_array($exported_array))
				{
					$export_num_rows = array_key_exists($device_name , $exported_array);
				}
				if($export_num_rows > 0)
				{

					$lun_id = $export_num_rows[$logicalVolume_name];

					if($exported_lun_array[$lun_id]== $SNAPSHOT_CREATION_PENDING)
					{
						$update_tl = "UPDATE
											exportedTLNexus
									  SET
											state = '$SNAPSHOT_CREATION_DONE'
									  WHERE
											exportedFCLunId = '$lun_id'";
						
						$multi_query_static["update"][] = $update_tl;
						
						$update_itl = "UPDATE
											exportedITLNexus
									  SET
											state = '$SNAPSHOT_CREATION_DONE'
									  WHERE
											exportedFCLunId = '$lun_id'";
						
						$multi_query_static["update"][] = $update_itl;

					}
				}
			}
			
			
		}
		else
		{
			$mount_data = get_volume_mount_point($id,$device_name,$mount_point,$logical_volumes_arary, $src_volumes_arary,$snap_shot_main_array);
			
			#12130 Fix
			$query_set = "UPDATE 
						  logicalVolumes 
						  SET ";
			$values = "`startingPhysicalOffset` = $physical_offset, 
						`fileSystemType` = '$file_system', 						
						`systemVolume` = $is_system_logicalVolume, 
						`cacheVolume` = $is_cache_logicalVolume, 
						`lastDeviceUpdateTime` = $my_current_time,
						`deviceLocked` = $deviceLocked,								
						`volumeLabel` = '$logicalVolumeLabel' 
						 $mount_data,
						`deviceId` = '$logicalVolumeId',
						`isUsedForPaging` = '$isUsedForPaging',
						`writecacheEnabled` = '$writecacheEnabled',
						`isMultipath`= '$isMultipath',
						`vendorOrigin`= '$vendorOrigin',
						`formatLabel`= '$formatLabel' $update_device";
								
			#$logging_obj->my_error_handler("DEBUG","register_logicalvolumes:: farline_protected: $farline_protected, capacity: $capacity, id: $id, deviceName: $deviceName \n");	
			if(
					($farline_protected == 1) 
				&&  
					(
							($capacity == 0) 
						||  
							(
								($compatibilityNo >= 550000) 
							  &&
								($rawSize == 0)
							)
					)
			 )
			{												
				$update_logicalVolume = $query_set." ".$values." WHERE $where_cond";
			}
			else
			{
				if($farline_protected != 1)
				{
					$values = $values.", `Phy_Lunid`= '$scsi_id'";
				}
				#12729 Fix
				if($device_flag_inuse == 1 && $compatibilityNo >= 550000 && $rawSize == 0)
				{
					$values = $values.", `capacity` = $capacity, `volumeType` =$case_volume_type";
				}
				else
				{
					$values = $values.", `capacity` = $capacity, `rawSize` = $rawSize, `volumeType` =$case_volume_type";
				}
				$update_logicalVolume = $query_set." ".$values." WHERE $where_cond";
			}	
			$logging_obj->my_error_handler("DEBUG","register_logicalvolumes \n Update logicalVolumes query = $update_logicalVolume \n");
			$multi_query_static["update"][] = $update_logicalVolume;
			
            //check if the protected device is unmounted ie volumeType is 2 and host's OS is Windows
            if($case_volume_type == 2 && $os_flag == 1 && $farline_protected == 1)
            {
                $arr_missing_volumes[] = $logicalVolume_name;
            }
		}
		$isClusterDevice = array_key_exists($device_name,$cluster_volumes_array);
		$logging_obj->my_error_handler("INFO","configurator_register_host_static_info \n isClusterDevice  = $isClusterDevice , osFlag :: $osFlag , count :: ".count($cluster_hostids)." capacity ::  $capacity"."\n");
		/*
		* As passive node is not capable of reporting mountpoints and capacities of normal volumes
		* we need to register mountpoint and update capacity for the passive node as part of active node VIC call.
		* To do that we need to verify whether each device is belongs to windows and capacity is non zero and also calling host has peer hostid's 
		* If yes below if block executes which inturn verifies whether each device is already register with passive node , if yes then updates the capacity else inserts a entry with passive node in logical volumes table.
		*/
		if($osFlag==1 && count($cluster_hostids)>0 && $isClusterDevice && $capacity!=0)
		{
			cluster_logical_volumes_sync($id,$scsi_id,$device_name,$mount_point,$capacity,$physical_offset,$file_system,$freespace,$is_system_logicalVolume,$is_cache_logicalVolume,$my_current_time,$deviceLocked,$case_volume_type,$logicalVolumeLabel,$logicalVolumeId,$isUsedForPaging,$rawSize,$writecacheEnabled,$isMultipath,$vendorOrigin,$formatLabel,$cluster_device_array,$farline_protected, $device_flag_inuse);
		}
		$disk_search = "";
		$dg_numRows = 0;
		$disk_search = $diskGroup."_".$device_name;
		$logging_obj->my_error_handler("DEBUG","register_logicalvolumes \n Disk key  = $disk_search \n");
		if(is_array($disk_groups_array))
		{
			$dg_numRows  = array_key_exists($disk_search , $disk_groups_array);
		}
		$diskGroup = $conn_obj->sql_escape_string($diskGroup);
		if ($dg_numRows == 0) 
		{			
			if(!(empty($diskGroup)))
			{
				$disk_group = " INSERT
								INTO 
									diskGroups 
									(diskGroupName, 
									hostId, 
									deviceName) 
								VALUES 
									('$diskGroup',
									'$id',
									'$logicalVolume_name')";
				
				$multi_query_static["insert"][] = $disk_group;

			}					
		}
		else
		{
			if(!(empty($diskGroup)))
			{
				$disk_group = " UPDATE
									diskGroups 
								SET 
									`diskGroupName` = '$diskGroup',
									`deviceName` ='$logicalVolume_name'
								WHERE 
									`diskGroupName` = '$diskGroup' AND
									`hostId` ='$id' AND 
									`deviceName` ='$logicalVolume_name'";
				
				$multi_query_static["update"][] = $disk_group;
			}
			else
			{
				$disk_group = "DELETE 
								FROM 
									diskGroups 
								WHERE 
									`diskGroupName` = '$diskGroup' AND
									`hostId` ='$id' AND 
									`deviceName` ='$logicalVolume_name'";	

				$multi_query_static["delete"][] = $disk_group;

			}
			}
		}
        
    if(count($arr_missing_volumes))
    {
        $error_id = "PROTECTION_ALERT"; //keeping the error_id same as error_template_id
        $error_template_id = "PROTECTION_ALERT";
        $summary = "Protected Drives are Missing";
        $message = "The following protected drive(s) in $machine_name cannot be detected by Scout:\n";
        $message .= implode(', ', $arr_missing_volumes)."\n";
        $message .= "Please ensure they are online and accessible."."\n";
        $message .= "No action is required if these are offline cluster volumes on the host.";
        $error_code = "EC0138";
        $arr_error_placeholders = array();
        $arr_error_placeholders['HostName'] = $machine_name;
        $arr_error_placeholders['VolumeNames'] = implode(', ', $arr_missing_volumes);
        $error_placeholders = serialize($arr_error_placeholders);
        $commonfunctions_obj->add_error_message($error_id, $error_template_id, $summary, $message, $id, '', $error_code, $error_placeholders);
    }
}

/*
 * Function Name: register_disks_partitions
 *
 * Description:
 * This function registers the new disks in the database 
 * or update the records of the existing disks
 *       
 * Parameters:
 *     Param 1  [IN]:$cfg 
 *     Param 2  [IN]:$id
 *     Param 3  [IN]:$disks array
 *     Param 4  [IN]:$check_cluster
 *
 * Return Value:
 */
function register_disks_partitions($id,$volumes,$case_volume_type,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array, $volume_resize_arary, $disk_groups_array, $application_scenario_array, $host_ids)
{
	global $conn_obj;
    global $logging_obj;
	global $vol_obj;
	global $multi_query_static;

	$diskId_array = array();
	$disk_array = array();

	$disk_query =  "SELECT
						diskPartitionId
					FROM 
						diskPartitions 
					WHERE 
						hostId = '$id'";

	$disk_deviceData = $conn_obj->sql_query($disk_query);
	
	while ($row = $conn_obj->sql_fetch_object($disk_deviceData))
	{
		$disk_array[$row->diskPartitionId] = $row->diskPartitionId;
		$diskPartitionId_db [] = $row->diskPartitionId;
	}

	foreach( $volumes as $diskPartition )
	{

		if( $compatibilityNo >= 560000)
		{
			$freespace = 0;
		}
		else
		{
			$freespace = $diskPartition['freespace'];
		}

		$scsi_id    				= $diskPartition['scsi_id'];
		$disk_partition_name		= $diskPartition['device_name'];
		$device_type				= $diskPartition['device_type'];
		$vendorOrigin				= $diskPartition['vendorOrigin'];
		$file_system				= $diskPartition['file_system'];
		$mount_point				= $diskPartition['mount_point'];
		$is_mounted					= $diskPartition['is_mounted'];
		$is_system_disk				= $diskPartition['is_system_volume'];
		$is_cache_disk				= $diskPartition['is_cache_volume'];
		$capacity					= $diskPartition['capacity'];
		$diskLocked					= $diskPartition['deviceLocked'];
		$physical_offset			= $diskPartition['physical_offset'];
		$sector_size				= $diskPartition['sector_size'];
		$disk_partition_label		= $diskPartition['volumeLabel'];
		$isUsedForPaging   			= $diskPartition['isUsedForPaging'];
		$isDiskPartitionAtBlockZero	= $diskPartition['isPartitionAtBlockZero'];
		$diskGroup    				= $diskPartition['diskGroup'];
		$diskGroupVendor    		= $diskPartition['diskGroupVendor'];
		$isMultipath    			= $diskPartition['isMultipath'];
		$writeCacheState   	 		= $diskPartition['writeCacheState'];
		$disk_partition_id			= $diskPartition['device_name'];
		$parentId					= $diskPartition['parentId']; 
		$rawSize            		= $diskPartition['rawSize'];
		$writecacheEnabled 			= $diskPartition['writeCacheState'];
		$formatLabel 				= $diskPartition['formatLabel'];
		
		// $formatLabel LABEL_UNKNOWN = 0,SMI = 1,EFI = 2
		
		$my_current_time = time();
		$is_system_disk  = $is_system_disk ?1:0;
		$is_cache_disk   = $is_cache_disk ?1:0;
		$diskLocked      = $diskLocked ?1:0;

		$disk_partition_name   = ($disk_partition_name{strlen($disk_partition_name)-1}=='\\') ? substr($disk_partition_name,0,-1) : $disk_partition_name;
		$mount_point = ($mount_point{strlen($mount_point)-1}=='\\') ? substr($mount_point,0,-1) : $mount_point;
		
		$mount_point = $conn_obj->sql_escape_string ($mount_point);
		$disk_partition_id_escaped = $conn_obj->sql_escape_string ($disk_partition_id);

		$disk_partition_label = $conn_obj->sql_escape_string($disk_partition_label);

		$disk_numRows = 0;
		if(is_array($disk_array))
		{
			$disk_numRows = array_key_exists($disk_partition_id , $disk_array);
		}
		if ($disk_numRows == 0)
		{
			$query = "INSERT 
					  INTO 
							diskPartitions 
							(`diskPartitionId`,
							 `hostId`,
							 `diskId`,
							 `isPartitionAtBlockZero`
							 ) 
					  VALUES
							( '$disk_partition_id_escaped', 
							  '$id',
							  '$parentId',
							  '$isDiskPartitionAtBlockZero'
							  )";

			$multi_query_static["insert"][] = $query;
		}
		else
		{
			$query="UPDATE 
						diskPartitions 
					SET 
						`diskPartitionId` = '$disk_partition_id_escaped' , 
						`hostId` = '$id' , 
						`diskId` = '$parentId', 
						`isPartitionAtBlockZero` = '$isDiskPartitionAtBlockZero' 
					WHERE
						`diskPartitionId` = '$disk_partition_id_escaped' AND 
						`hostId` = '$id'";	
			
			$multi_query_static["update"][] = $query;
		}
	
		$diskPartitionId_array[] = ($disk_partition_id_escaped{strlen($disk_partition_id_escaped)-1}=='\\') ? substr($disk_partition_id_escaped,0,-1) : $disk_partition_id_escaped;

		if($device_names[$disk_partition_id] == "")
		{
			if(is_array($logical_volumes_arary))
			{
				$numRows = array_key_exists($disk_partition_id , $logical_volumes_arary);
			}
			$where_cond = "hostId = '$id' AND 
						   deviceName = '$disk_partition_id_escaped'";
		}
		else
		{
			if(is_array($logical_volumes_arary))
			{
				if(array_key_exists($disk_partition_id , $logical_volumes_arary) && $logical_volumes_arary[$disk_partition_id]['deviceId'] == $disk_partition_id && $logical_volumes_arary[$disk_partition_id]['volumeType'] == 9)
				{
					$numRows = 1;

				}
			}

			$where_cond = "hostId = '$id' AND 
							deviceName = '$disk_partition_id_escaped' AND 
							deviceId = '$disk_partition_id_escaped' AND 
							volumeType = 9";

		}

			$farline_protected = $logical_volumes_arary[$disk_partition_name]["farLineProtected"];
			$device_flag_inuse = $logical_volumes_arary[$disk_partition_name]["deviceFlagInUse"];
			$disk_partition_name = $conn_obj->sql_escape_string ($disk_partition_name);				
			if ( $numRows == 0) 
			{			
				$query="INSERT 
						INTO 
							logicalVolumes 
							(`hostId`,
							`deviceName`, 
							`mountPoint`, 
							`capacity`,
							`lastSentinelChange`,
							`lastOutpostAgentChange`, 
							`dpsId`, 
							`farLineProtected`,
							`startingPhysicalOffset`, 
							`fileSystemType`, 							
							`systemVolume`,									
							`cacheVolume`,
							`lastDeviceUpdateTime`,
							`deviceLocked`,
							`volumeType`,
							`volumeLabel`,
							`deviceId`,
							`isUsedForPaging`,
							`rawSize`,
							`writecacheEnabled`, 
							`Phy_Lunid`,
							`isMultipath`,
							`vendorOrigin`,
							`formatLabel`
							)	
						VALUES
							('$id',
							 '$disk_partition_name',
							 '$mount_point',
							 $capacity,
							 0,
							 0,
							 '',
							 0,
							 $physical_offset,
							 '$file_system',							
							 $is_system_disk,
							 $is_cache_disk, 
							 $my_current_time,
							 $diskLocked,
							 $case_volume_type,
							 '$disk_partition_label',
							 '$disk_partition_id_escaped',
							 '$isUsedForPaging',
							 $rawSize,
							 '$writecacheEnabled',
							 '$scsi_id',
							 '$isMultipath',
							 '$vendorOrigin',
							 '$formatLabel'
							 )";

				$multi_query_static["insert"][] = $query;
				$db_volumes = $logical_volumes_arary[$disk_partition_id];
			$logging_obj->my_error_handler("DEBUG","register_disks_partitions \n Disk partition name = $disk_partition_name \n\n Disk partition id = ".$disk_partition_id." \n\n Number of rows = ".$numRows ." \n\n Agent sent volume info = ".print_r($diskPartition,TRUE)."\n Database volumes = ".print_r($db_volumes,TRUE)."\n inert query = $query \n");
			}
			else
			{
				$mount_data = get_volume_mount_point($id,$disk_name,$mount_point,$logical_volumes_arary, $src_volumes_arary,$snap_shot_main_array);
				
				#12130 Fix
				$query_set = "UPDATE 
									logicalVolumes 
							  SET ";
							  
				$values = "`startingPhysicalOffset` = $physical_offset, 
							`fileSystemType` = '$file_system', 							
							`systemVolume` = $is_system_disk, 
							`cacheVolume` = $is_cache_disk, 
							`lastDeviceUpdateTime` = $my_current_time,
							`deviceLocked` = $diskLocked,									
							`volumeLabel` = '$diskLabel' 
							 $mount_data ,
							`deviceId` = '$disk_partition_id_escaped',
							`isUsedForPaging`= '$isUsedForPaging',
							`writecacheEnabled` = '$writecacheEnabled',
							`isMultipath`= '$isMultipath',
							`vendorOrigin`= '$vendorOrigin',
							`formatLabel`= '$formatLabel'";
				#$logging_obj->my_error_handler("DEBUG","configurator_register_disks:: farline_protected: $farline_protected, capacity: $capacity \n");	
				if(
						($farline_protected == 1) 
					&&  
						(
								($capacity == 0) 
							||  
								(
									($compatibilityNo >= 550000) 
								  &&
									($rawSize == 0)
								)
						)
				  )				
				{													
					$query = $query_set." ".$values." WHERE $where_cond";									
				}
				else
				{
					if($farline_protected != 1)
					{
						$values = $values.", `Phy_Lunid`= '$scsi_id'";
					}
					#12729 Fix
					if($device_flag_inuse == 1 && $compatibilityNo >= 550000 && $rawSize == 0)
					{
						$values = $values.", `capacity` = $capacity, `volumeType` =$case_volume_type";
					}
					else
					{
						$values = $values.", `capacity` = $capacity, `rawSize` = $rawSize, `volumeType` =$case_volume_type";
					}
					$query = $query_set." ".$values." WHERE $where_cond";
				}
				$logging_obj->my_error_handler("DEBUG","register_disks_partitions \n Update query = $query \n");				
				$multi_query_static["update"][] = $query;
			}

			$dg_numRows = 0;
			$disk_search = $diskGroup."_".$disk_partition_name;
			
			if(is_array($disk_groups_array))
			{
				$dg_numRows  = array_key_exists($disk_search , $disk_groups_array);
			}
			$diskGroup = $conn_obj->sql_escape_string($diskGroup);
			if ($dg_numRows == 0) 
			{			
				if(!(empty($diskGroup)))
				{
					$disk_group = " INSERT
									INTO 
										diskGroups 
										(diskGroupName, 
										hostId, 
										deviceName) 
									VALUES 
										('$diskGroup',
										'$id',
										'$disk_partition_name')";
					
					$multi_query_static["insert"][] = $disk_group;
											
				}					
			}
			else
			{
				if(!(empty($diskGroup)))
				{
					$disk_group = " UPDATE
										diskGroups 
									SET 
										`diskGroupName` = '$diskGroup',
										`deviceName` ='$disk_partition_name'
									WHERE 
										`diskGroupName` = '$diskGroup' AND 
										`hostId` ='$id' AND 
										`deviceName` ='$disk_partition_name'";

					$multi_query_static["update"][] = $disk_group;
				}
				else
				{
					$disk_group = "DELETE 
									FROM 
										diskGroups 
									WHERE 
										`diskGroupName` = '$diskGroup' AND 
										`hostId` ='$id' AND 
										`deviceName` ='$disk_partition_name'";	
					
					$multi_query_static["delete"][] = $disk_group;
						
				}
			}
			
		}

	$diff_disk_partitions = array_diff($diskPartitionId_db, $diskPartitionId_array);

$logging_obj->my_error_handler("DEBUG","register_disks_partitions \n Disk partitionId array = ".print_r($diskPartitionId_array,TRUE)." \n Disk partitionId db = ".print_r($diskPartitionId_db,TRUE)." \n diff_disk_partitions = ".print_r($diff_disk_partitions,TRUE)."\n");
	
	foreach( $diff_disk_partitions as $diff_disk_partition )
	{
		/*
		 Bug fix 3669
		 Added  condition to check if the device flag is set for the volume.
		*/

		$search_key = $id."_".$diff_disk_partition;
		if(is_array($src_volumes_arary))
		{
			if(array_key_exists ($search_key , $src_volumes_arary["source"]) || array_key_exists ($search_key , $src_volumes_arary["target"]) || $logical_volumes_arary[$diff_disk_partition]["deviceFlagInUse"] != 0)
			{
				continue;
			}
		}
		else
		{
			$del_vol =  "DELETE FROM diskPartitions WHERE hostId = '" . $conn_obj->sql_escape_string( $id )."' AND diskPartitionId = '" .$conn_obj->sql_escape_string($diff_disk_partition)."'";

			$multi_query_static["delete"][] = $del_vol;
		}
	}

}


/*
 * Function Name: get_volume_mount_point
 *
 * Description:
 * This function used to get the mount point. 
 *       
 * Parameters:
 *     Param 1  [IN]:$id 
 *     Param 2  [IN]:$device_name
 *
 * Return Value: mountPoint
 */

function get_volume_mount_point($id,$device_name,$mount_point,$logical_volumes_arary, $src_volumes_arary ,$snap_shot_main_array)
{
	global $conn_obj;
    global $logging_obj;
	
	$isVisible  = $logical_volumes_arary[$device_name]['isVisible'];
	$mountPoint = $logical_volumes_arary[$device_name]['mountPoint'];

	/*
	* #5741:Retention log path issue:: Checked the destdevice name is part of replication
	* if so, check if isVisible is 1 or 3 then do not update the mount point else update the mount point
	*/
	
	$search_key = "";
	$num_src_rows = 0;
	$search_key = $id."_".$device_name;
	if(is_array($src_volumes_arary))
	{
		$num_src_rows = array_key_exists($search_key , $src_volumes_arary["target"]);
	}

	if( $num_src_rows > 0 )
	{
		/*
		* #5741: if device is in processing state then do not update the mount point with the value sent by agent
		*/
		$mount_point =(intval($isVisible) == 1 || intval($isVisible) == 3) ? $conn_obj->sql_escape_string($mountPoint) : $mount_point;	
	}

	if (($logical_volumes_arary['os_flag'] == 2) ||  ($logical_volumes_arary['os_flag'] == 3))
	{ 
		$count = 0;
		if(is_array($snap_shot_main_array["snap_main"]))
		{
			foreach($snap_shot_main_array["snap_main"] as $key=>$snap_details)
			{
				if( ($logical_volumes_arary[$device_name]['deviceName'] == $snap_details[0] || $logical_volumes_arary[$device_name]['deviceName'] == $snap_details[1]) && $logical_volumes_arary[$device_name]['deviceName'] == $device_name)
				{
					$count++;
				}
			}
		}

		if($count >= 1)
		{
			$mount_data = "";
		}
		else
		{
			$mount_data = ",mountPoint='$mount_point'";
		}
		$count = 0;
		if(is_array($snap_shot_main_array["snap"]))
		{
			foreach($snap_shot_main_array["snap"] as $key=>$snap_details)
			{
				if( ($logical_volumes_arary[$device_name]['deviceName'] == $snap_details[0] || $logical_volumes_arary[$device_name]['deviceName'] == $snap_details[1]) && $logical_volumes_arary[$device_name]['deviceName'] == $device_name)
				{
					$count++;
				}
			}
		}
		if($count > 0)
		{
			$mount_data = "";
		}
		else
		{
			$mount_data = ",mountPoint='$mount_point'";
		}
			
		
	}
	else
	{
		$mount_data = ",mountPoint='$mount_point'";
	}
	return $mount_data;
}

/*
 * Function Name: configurator_register_host_dynamic_info
 *
 * Description:
 * This function used to update the dynamic information
 * for host related information.
 *
 * Return Value: 
 */
 function configurator_register_host_dynamic_info($cfg, $id,$local_time, $sysVolCapacity, $sysVolFreeSpace, $insVolCapacity, $insVolFreeSpace, $VolumeDynamicInfos)
 {
	global $conn_obj;
	global $commonfunctions_obj;
	global $logging_obj;
	global $multi_query;
	$batchsql_obj = new BatchSqls();
	
	$logging_obj->my_error_handler("DEBUG"," configurator_register_host_dynamic_info \n Host id = $id \n\n Local time = $local_time \n\n System volume capacity = $sysVolCapacity \n\n System volume free space = $sysVolFreeSpace \n\n Volume dynamic info = ".print_r($VolumeDynamicInfos,TRUE)."\n");
	
	//Always excepts this format 2022-06-26 09:53:28 from agent.
	if (isset($local_time) && $local_time != '')
	{
		if (!preg_match('/^\{?[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}[:][0-9]{2}[:][0-9]{2}\}?$/', $local_time))
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for localtime $local_time for host id $id in configurator_register_host_dynamic_info");
		}
	}
	
	$sysVolCapacity_str = (string)$sysVolCapacity;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($sysVolCapacity) && $sysVolCapacity != '' && (!preg_match('/^\d+$/',$sysVolCapacity_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for sys vol capacity $sysVolCapacity for host id $id in configurator_register_host_dynamic_info");
	}
	
	$sysVolFreeSpace_str = (string)$sysVolFreeSpace;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($sysVolFreeSpace) && $sysVolFreeSpace != '' && (!preg_match('/^\d+$/',$sysVolFreeSpace_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for sys vol freespace $sysVolFreeSpace for host id $id in configurator_register_host_dynamic_info");
	}
	
	$insVolCapacity_str = (string)$insVolCapacity;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($insVolCapacity) && $insVolCapacity != '' && (!preg_match('/^\d+$/',$insVolCapacity_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for in vol capacity $insVolCapacity for host id $id in configurator_register_host_dynamic_info");
	}
	
	$insVolFreeSpace_str = (string)$insVolFreeSpace;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($insVolFreeSpace) && $insVolFreeSpace != '' && (!preg_match('/^\d+$/',$insVolFreeSpace_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for vol freespace $insVolFreeSpace for host id $id in configurator_register_host_dynamic_info");
	}

    $action = "update";
	$sql = $conn_obj->sql_query( "select count(*),ipaddress from hosts where id = '" . $conn_obj->sql_escape_string( $id ) . "' group by ipaddress");
	$row = $conn_obj->sql_fetch_row( $sql );
    $sql_update = '';
	$multi_query = array();
	if( $row[ 0 ] > 0 ) 
	{		
		$multi_query["update"][] = "UPDATE 
		                     hosts
					   SET
					         agentTimeStamp = '$local_time',
					         SysVolCap='$sysVolCapacity',
							 SysVolFreeSpace='$sysVolFreeSpace',
							 InVolCapacity='$insVolCapacity',
							 InVolFreeSpace='$insVolFreeSpace'
					   WHERE
					         id='$id'";

		$new_volumes = $commonfunctions_obj->get_volume_hash_dynamic_data($VolumeDynamicInfos);
		ksort($new_volumes);
		foreach( $new_volumes as $case_volume_type => $volume ) 
		{		
			update_register_volumes($id,$new_volumes);
			
		}
		$result = (int)$batchsql_obj->filterAndUpdateBatches($multi_query, "om_configurator :: configurator_register_host_dynamic_info");	
		$logging_obj->my_error_handler("DEBUG"," configurator_register_host_dynamic_info \n Result to agent= $result \n");
		
		// Update the infrastructureVMs with the hostId
		$host_ip = $conn_obj->sql_get_value("hosts", "ipAddress", "id = ?", array($id));
		$logging_obj->my_error_handler("DEBUG","configurator_register_host_dynamic_info : updateInfraHostId : host_ip : $host_ip, id : $id\n");
		$system_obj = new System();
		$system_obj->updateInfraHostId($host_ip, $id);
		
		return $result;		
	}	
 }

/*
 * Function Name: update_register_volumes
 *
 * Description:
 * This function used to update the dynamic information
 * for volumes related information.
 *
 * Return Value: 
 */
function update_register_volumes($id,$volumes)
{
	global $conn_obj;
    global $logging_obj;
	global $commonfunctions_obj;
	global $multi_query;
	$my_current_time = time();
	
	foreach( $volumes as $hash_volume_type => $disks ) 
	{
		$query_set = '';
		foreach( $disks as $disk )
		{
			$scsi_id    = $disk['scsi_id'];
			$disk_name  = $disk['device_name'];
			$freespace  = $disk['freespace'];
			$freespace_str = (string)$freespace;
			//If the string contains all digits, then only allow, otherwise return.
			if (isset($freespace) && $freespace != '' && (!preg_match('/^\d+$/',$freespace_str))) 
			{
				$commonfunctions_obj->bad_request_response("Invalid post data for free space $freespace  of device $disk_name for host id $id in configurator_register_host_dynamic_info");
			}
		
			#$logging_obj->my_error_handler("INFO","configurator_register_host_dynamic_info : update_register_volumes $id, $disk_name, $freespace  \n");
			$disk_name  = ($disk_name{strlen($disk_name)-1}=='\\') ? substr($disk_name,0,-1) : $disk_name;
			$disk_name  = $conn_obj->sql_escape_string ($disk_name);
			$multi_query["update"][]= "UPDATE 
								logicalVolumes 
						  SET 
							    freeSpace = '$freespace',
								lastDeviceUpdateTime = $my_current_time,
								lastOutpostAgentChange=now(),
								lastSentinelChange=now() 
						  WHERE
							    deviceName= '$disk_name'
						  AND
							    hostId = '$id'";						  
		}

	}
	
}

/*
 * Function Name: configurator_register_host_static_info
 *
 * Description:
 * This function used to register the host and update the static
 * information of the hosts and volumes.
 * Return Value: 
 */

function configurator_register_host_static_info($cfg, $id, $agent_version,$driver_version,$hostname, $ipaddress,
									$os,$compatibilityNo,$vxAgentPath,$osFlag,$Tmzn,$Syvl="",
									$Patch_history="",$volumes="",
									$AgentMode="",$prodVerson="" ,$nicinfo="",$hypervisorstate = "",$wwnInfo="",
									$endianess="",$processorArchitecture="",$cx_info="",$memory="",$mbr_details="",$bios_id="")
{

	global $conn_obj,$commonfunctions_obj;
	global $logging_obj;
	global $cluster_mountpoint_query;
	global $vol_obj;
    global $capacity_array;
	$cluster_mountpoint_query = array();
	global $HOST_GUID,$UNDER_SCORE;
	$capacity_array = array();
	$Dttm="";
	$Syvc="";
	$Syfs="";
	$Invc="";
	$Infs="";
	$prism_obj = new PrismConfigurator();
	$app_obj = new Application();
	$commonfunctions_obj = new CommonFunctions();
	$diskGroupArray = array();
   	global $multi_query_static;
	$multi_query_static = array();
	$nic_multi_query_static = array();
	$batchsql_obj = new BatchSqls();
	try
	{
		/*
			1. Passphrase once updated on configuration server, it blocks the entire control path communication between clients and configuration server. Once customer corrected updated on all clients machine and Once the communication established between source/Master target (to) configuration server, as the first step deleting health issue of  passphrase change on configuration server. This ensures the cleanup of passphrase health factor.
			2. This delete query not giving to transaction and reason is, need to clean the health as first step because communication established between client and server. Next point is there are some checks below in code registration to continue or discard based on mobility agent reported data.If any reason IP address doesn't report or OS not reported registration is not allowing below. Registration is not allowing shouldn't impact clean-up of passphrase health. Passphrase health clearly depends on communication establishment between clients and server. Though below query fails because of some reason like database server down or any other database error, servers sends 500 response to mobility agent and mobility agent has capability of retrying the same request until it gets succeeds.
			3. Cleaning both source mobility agent and master target agent passphrase update health issue with delete query in registration call.
		*/
		//Source passphrase update health clean-up
		$pass_phrase_delete_health_sql = 	"DELETE 
											FROM 
												healthfactors
											WHERE 
												`sourceHostId` = ? AND 
												`healthFactorCode` = ?";
		$pass_phrase_delete_result = $conn_obj->sql_query($pass_phrase_delete_health_sql, array($id,'ECH00027')); 
		
		//Master target infrastructure passphrase update health clean-up
		$pass_phrase_delete_health_sql = 	"DELETE 
											FROM 
												infrastructurehealthfactors 
											WHERE 
												`hostId` = ? AND 
												`healthFactorCode` = ?";
		$pass_phrase_delete_result = $conn_obj->sql_query($pass_phrase_delete_health_sql, array($id, 'EMH0004')); 
							
		$GLOBALS['record_meta_data']['HostName'] = $hostname;
		$GLOBALS['record_meta_data']['IpAddress'] = $ipaddress;
		$os_array = array();
		if(is_array($os))
		{
			$os_array[] = $conn_obj->sql_escape_string($os[1]['build']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['caption']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['major_version']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['minor_version']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['name']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['systemtype']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['systemdrive']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['systemdirectory']);
			$os_array[] = $conn_obj->sql_escape_string($os[1]['systemdrivediskextents']);
		}
		$os = (isset($os[1]['name'])) ? $os[1]['name'] : '';
		
		if(is_array($nicinfo))
		{
			foreach($nicinfo AS $mac_address => $arr)
			{
				foreach($arr AS $key => $value)
				{
					$ip_address = array();
					if($value[1]['ip_addresses'] and $value[1]['ip_subnet_masks']) {
						$arr_ip_address = explode(',',$value[1]['ip_addresses']);
						$arr_subnet_masks = explode(',',$value[1]['ip_subnet_masks']);
						$ip_address = array_combine($arr_ip_address,$arr_subnet_masks);
					}
					
					$ip_types = array();
					if($value[1]['ip_addresses'] and $value[1]['ip_types']) {
						$arr_ip_address = explode(',',$value[1]['ip_addresses']);
						$type = explode(',',$value[1]['ip_types']);
						$type = array_map('strtolower', $type);
						$ip_types = array_combine($arr_ip_address,$type);
					}
					
					$nicSpeed = '';
					if($value[1]['max_speed'])
					{
						$nicSpeed = $value[1]['max_speed'];
					}
					$isDhcpEnabled = 0;					
					if($value[1][is_dhcp_enabled] == "true") {
						$isDhcpEnabled = 1;
					}
					$nicinfo1[] = array(0, $value[1]['name'],$ip_address,$value[1][dns_host_name],$mac_address,$value[1][default_ip_gateways],$value[1][dns_server_addresses],$value[1][index],$isDhcpEnabled,$value[1][manufacturer],$nicSpeed,$ip_types); 
				}
			}
			$nicinfo = $nicinfo1;
		}
		$cpu_array = array();
		if(is_array($processorArchitecture))
		{
			$cpu_array = $processorArchitecture;
			foreach($processorArchitecture AS $value){
				$processorArchitecture = (isset($value[1]['architecture'])) ? $value[1]['architecture'] : '';
				break;
			}
		}
        
		if($bios_id)
		{
			$logging_obj->my_error_handler("INFO","configurator_register_host_static_info Before changing bios id:: $bios_id\n");
			$telemetry_log_string["ReportedBiosId"] = $bios_id;
			$bios_id = generateUUIDFormat($bios_id);
			$telemetry_log_string["FormattedBiosId"] = $bios_id;
			$logging_obj->my_error_handler("INFO","configurator_register_host_static_info After changing bios id:: $bios_id\n BIOSID::$BIOSID \n");
		}
		
		$logging_obj->my_error_handler("INFO","configurator_register_host_static_info \n Host id = $id(Hostname = $hostname, IP address = $ipaddress) \n\n Volumes = ".print_r($volumes,TRUE)." Agent version = $agent_version \n\n Driver version = ".$driver_version." \n\n Hostname = ".$hostname ." \n\n IP address of the host = ".$ipaddress." \n\n Operating system = ".$os." \n\n Compatibility number = ".$compatibilityNo." \n\n vx agent path = ".$vxAgentPath." \n\n osFlag = ".$osFlag." \n\n Time zone = ".$Tmzn." \n\n System volume path = ".$Syvl." \n\n Applied patches info = ".$Patch_history." \n\n Agent mode = ".$AgentMode." \n\n Product version = ".$prodVerson." \n\n NIC info = ".print_r($nicinfo,TRUE)."\n Hypervisor state = ".print_r($hypervisorstate,TRUE)."\n wwn info = ".print_r($wwnInfo,TRUE)." \n Endianess = ".$endianess." \n\n Processor architecture = ".$processorArchitecture." \n\n cx info = ".print_r($cx_info,TRUE)."\n mbr_details = ".print_r($mbr_details,True)."\n os_array = ".print_r($os_array,True)."\n bios_id:: $bios_id \n");
		$telemetry_log_string["Id"] = $id;
		$telemetry_log_string["Name"] = $hostname;
		$telemetry_log_string["Ip"] = $ipaddress;
		$telemetry_log_string["Bios"]=$bios_id;
		$telemetry_log_string["AgentVersion"] = $agent_version;
		$telemetry_log_string["DriverVersion"] = $driver_version;
		$telemetry_log_string["Os"] = $os;
		$telemetry_log_string["Compatibility"] = $compatibilityNo;
		$telemetry_log_string["VxPath"]=$vxAgentPath;
		$telemetry_log_string["OsFlag"]=$osFlag;
		$telemetry_log_string["TimeZone"]=$Tmzn;
		$telemetry_log_string["SysVol"]=$Syvl;
		$telemetry_log_string["AgentMode"]=$AgentMode;
		$telemetry_log_string["ProdVersion"]=$prodVerson;
		$telemetry_log_string["Nics"]=$nicinfo;
		$telemetry_log_string["Hypervisor"]=$hypervisorstate;
		$telemetry_log_string["Endianess"]=$endianess;
		$telemetry_log_string["Architecture"]=$processorArchitecture;
		$telemetry_log_string["OsDetails"]=$os_array;
		$telemetry_log_string["MbrDetails"]=$mbr_details;
		$logging_obj->my_error_handler("INFO",$telemetry_log_string,Log::APPINSIGHTS);
		
		/*
			Code to handle Loop Back IpAddress.
				1) Check if the Primary IPaddress,sent by agent, is either IPV4 loopback address (127.0.0.0.1)  or IPV6 Addresss
				2) If Yes, get the first Ipaddress from the NICInfo details which is not IPV4 LoopBack or IPV6
				3) If no Valid address is found in step 2, return the call without registration as Success
		*/
		$logging_obj->my_error_handler("INFO","configurator_register_host_static_info ENTERED: IP Address = $ipaddress \n");
		$telemetry_log_ip_validation["ReportedIpAddress"] = $ipaddress;
		$ipaddress = validate_ip_address($nicinfo, $ipaddress, $osFlag);
		$telemetry_log_ip_validation["FilteredIpAddress"] = $ipaddress;
		$logging_obj->my_error_handler("INFO",array("FilteredIpAddress"=>$ipaddress),Log::BOTH);
		if(!$ipaddress) 
		{
			$telemetry_log_ip_validation["Reason"] = "No Valid IP Address found";
			$telemetry_log_ip_validation["Status"] = "Fail";
			$telemetry_log_ip_validation["Result"] = 1;
			$logging_obj->my_error_handler("INFO",$telemetry_log_ip_validation,Log::BOTH);
			return 1;
		}
		
		$status = validate_mt_supported_data_planes_value($mbr_details);
		if (!$status)
		{
			$logging_obj->my_error_handler("INFO",array("Reason"=>"Invalid MT_SUPPORTED_DATAPLANES configuration in MT drscout.conf file.","Status"=>"Fail","Result"=>1),Log::BOTH);
			return 1;
		}
		
		$compatibility_no_str = (string)$compatibilityNo;
		//If the string contains all digits, then only allow, otherwise return.
		if (isset($compatibilityNo) && $compatibilityNo != '' && (!preg_match('/^\d+$/',$compatibility_no_str))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for compatibilityNo $compatibilityNo in configurator_register_host_static_info."); 
		}
		
		$os_flag_str = (string)$osFlag;
		//If the string contains all digits, then only allow, otherwise return.
		if (isset($osFlag) && $osFlag != '' && (!preg_match('/^\d+$/',$os_flag_str))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for osFlag $osFlag in configurator_register_host_static_info."); 
		}
		
		$memory_str = (string) $memory;
		//If the string contains all digits, then only allow, otherwise return.
		if (isset($memory) && $memory != '' && (!preg_match('/^\d+$/',$memory_str))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for memory $memory in configurator_register_host_static_info."); 
		}
		
		$devicenames = array();
		$device_to_update = array();
		$devicenames_db = array();
		$deviceIds = array();
		$deviceIds_db = array();
		$deviceInfo = array();
		$logical_volumes_arary = array();
		$device_names = array();
		$host_ids = array();
		$snap_shot_main_array = array();
		$host_shared_device = array();
		$volume_resize_arary = array();
		$disk_groups_array = array();
		$application_scenario_array = array();
		$src_volumes_arary = array();
		$src_volumes_arary["source"] = array();
		$src_volumes_arary["target"] = array();
		$cluster_volumes_array = array();
		$host_array = array();
		$pair_type_array = array();
		
		$sql_host = "SELECT
						osFlag,
                        name,
						id,
						ipAddress,
						csEnabled
					 FROM
						hosts
					 WHERE
						(sentinelEnabled = 1 
							OR 
						csEnabled = 1)
					AND
						id != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
		$result_host = $conn_obj->sql_query($sql_host); 
		while($row_host = $conn_obj->sql_fetch_object($result_host))
		{
			$host_array[$row_host->id]['osFlag'] = $row_host->osFlag;
			$host_array[$row_host->id]['csEnabled'] = $row_host->csEnabled;
			$host_array[$row_host->id]['name'] = $row_host->name;
			$host_array[$row_host->id]['ipAddress'] = $row_host->ipAddress;
			
		}
		$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info sql_host :: $sql_host\n");
		$sql = "SELECT
					deviceName,
					volumeType,
					deviceId,
					Phy_Lunid,
					hostId,
					deviceFlagInUse,
					capacity,
					farLineProtected,
					mountPoint,
					isVisible,
					vendorOrigin
				FROM
					logicalVolumes
				WHERE
					hostId = '$id'";
		
		$result = $conn_obj->sql_query($sql); 
		
		while ($row = $conn_obj->sql_fetch_object($result))
		{
			$logical_volumes_arary[$row->deviceName]["deviceId"] = $row->deviceId;
			$logical_volumes_arary[$row->deviceName]["volumeType"] = $row->volumeType;
			$logical_volumes_arary[$row->deviceName]["deviceName"] = $row->deviceName;
			$logical_volumes_arary[$row->deviceName]["Phy_Lunid"] = $row->Phy_Lunid;
			$logical_volumes_arary[$row->deviceName]["hostId"] = $row->hostId;
			$logical_volumes_arary[$row->deviceName]["deviceFlagInUse"] = $row->deviceFlagInUse;
			$logical_volumes_arary[$row->deviceName]["capacity"] = $row->capacity;
			$logical_volumes_arary[$row->deviceName]["farLineProtected"] = $row->farLineProtected;
			$logical_volumes_arary[$row->deviceName]["mountPoint"] = $row->mountPoint;
			$logical_volumes_arary[$row->deviceName]["isVisible"] = $row->isVisible;
			$logical_volumes_arary[$row->deviceName]["vendorOrigin"] = $row->vendorOrigin;
			
			$devices_db = $row->deviceName;
			$row->deviceName = $conn_obj->sql_escape_string($row->deviceName);
			$device_names[$row->deviceName] = $row->deviceId;
			
			$scsi_id = $row->Phy_Lunid;
			if($scsi_id != "")
			{
				$devicenames_db[] = $devices_db."^".$scsi_id;
			}
			else
			{
				$devicenames_db[] = $devices_db;
			}
		}

	
		$logical_volumes_arary['os_flag'] = $host_array[$id]['osFlag'];
		
		$logging_obj->my_error_handler("INFO","configurator_register_host_static_info ENTERED logical_volumes_arary: ".print_r($logical_volumes_arary,TRUE)." \n");
		$sql = "SELECT
					sourceDeviceName,
					destinationDeviceName,
					sourceHostId,
					destinationHostId,
					Phy_Lunid,
					srcCapacity,
					jobId 
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					sourceHostId = '$id'
				OR
					destinationHostId = '$id'";
		$result = $conn_obj->sql_query($sql); 
		$count = 0;
		while ($row = $conn_obj->sql_fetch_object($result))
		{
			$src_volumes_arary["source"][$row->sourceHostId."_".$row->sourceDeviceName] = $row->sourceHostId."_".$row->sourceDeviceName;
			$src_volumes_arary["target"][$row->destinationHostId."_".$row->destinationDeviceName] = $row->destinationHostId."_".$row->destinationDeviceName;
			$src_volumes_arary["pair_details"][$count]['sourceDeviceName'] = $row->sourceDeviceName;
			$src_volumes_arary["pair_details"][$count]['sourceHostId'] = $row->sourceHostId;
			$src_volumes_arary["pair_details"][$count]['destinationDeviceName'] = $row->destinationDeviceName;
			$src_volumes_arary["pair_details"][$count]['destinationHostId'] = $row->destinationHostId;
			$src_volumes_arary["pair_details"][$count]['Phy_Lunid'] = $row->Phy_Lunid;
			$src_volumes_arary["pair_details"][$count]['srcCapacity'] = $row->srcCapacity ;
			$src_volumes_arary["pair_details"][$count]['jobId'] = $row->jobId ;
			$count++;
		}
		$logging_obj->my_error_handler("INFO",array("PairData"=>$src_volumes_arary),Log::BOTH);
		$host_ids[] = $id;
		$cluster_id = array();
		$cluster_id_sql = "SELECT 
							   clusterId,
							   deviceName
						  FROM 
								clusters 
						  WHERE 
								hostId = '$id'";
		$result_set = $conn_obj->sql_query($cluster_id_sql);		
		while ($row_object = $conn_obj->sql_fetch_object($result_set))
		{
			$cluster_id[] = $row_object->clusterId;
			$cluster_volumes_array[$row_object->deviceName] = $row_object->deviceName;
		}
		$cluster_id = array_unique($cluster_id);
		$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info cluster_id_sql :: $cluster_id_sql , cluster_id ".print_r($cluster_id,true)."\n");
		if($cluster_id)
		{
			$cluster_hostid_arr = array();
			$query = "SELECT 
							hostId
						FROM 
							clusters
						WHERE 
							clusterId = '".$cluster_id[0]."'";
			$rs = $conn_obj->sql_query($query);		
			while ($row = $conn_obj->sql_fetch_object($rs))
			{
				$cluster_hostid_arr[] = $row->hostId;
			}		
			$cluster_hostid_arr = array_unique($cluster_hostid_arr);
			$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info query:: $query , cluster_hostid_arr :: ".print_r($cluster_hostid_arr,true)."\n");
			foreach($cluster_hostid_arr as $key=>$clus_host_id)
			{
				if (!in_array($clus_host_id,$host_ids))
				{
					$host_ids[] = $clus_host_id;
				}
			}
		}
		$logging_obj->my_error_handler("INFO","configurator_register_host_static_info \n Host ids(including other cluster Host Ids incase of cluster)= ".print_r($host_ids,TRUE)." \n");
		$sql = "SELECT
					deviceName,
					isValid,
					newCapacity,
					oldCapacity
				FROM
					volumeResizeHistory
				WHERE
					hostId = '$id'";
		
		$result = $conn_obj->sql_query($sql); 
		while ($row = $conn_obj->sql_fetch_object($result))
		{
			
			$volume_resize_arary[$row->deviceName]["isValid"] = $row->isValid;
			$volume_resize_arary[$row->deviceName]["newCapacity"] = $row->newCapacity;
			$volume_resize_arary[$row->deviceName]["oldCapacity"] = $row->oldCapacity;
		}
		$logging_obj->my_error_handler("INFO","configurator_register_host_static_info \n Volume resize info = ".print_r($volume_resize_arary , TRUE)."\n");
		
		$diskGroupDb = array();
		$dg_query ="SELECT
						deviceName,
						diskGroupName
					FROM 
						diskGroups 
					WHERE 
						hostId = '$id'";
		
		$dg_deviceData = $conn_obj->sql_query($dg_query);
		
		while ($row = $conn_obj->sql_fetch_object($dg_deviceData))
		{
			$disk_groups_array[$row->diskGroupName."_".$row->deviceName] = $row->deviceName;
			$diskGroupDb [] = $id."^".$row->deviceName."^".$row->diskGroupName;
		}
		$logging_obj->my_error_handler("INFO",array("DiskGroups"=>$disk_groups_array),Log::BOTH);
		
		$dg_query ="SELECT
						scenarioId,
						protectionDirection,
						sourceId,
						sourceVolumes,
						targetId,
						targetVolumes,
						scenarioType,
						scenarioDetails
					FROM 
						applicationScenario";
		
		$dg_deviceData = $conn_obj->sql_query($dg_query);
		
		$count = 0;
		while ($row = $conn_obj->sql_fetch_object($dg_deviceData))
		{
			$application_scenario_array[$count]['scenarioId'] = $row->scenarioId;
			$application_scenario_array[$count]['protectionDirection'] = $row->protectionDirection;
			$application_scenario_array[$count]['sourceId'] = $row->sourceId;
			$application_scenario_array[$count]['sourceVolumes'] = $row->sourceVolumes;
			$application_scenario_array[$count]['targetId'] = $row->targetId;
			$application_scenario_array[$count]['targetVolumes'] = $row->targetVolumes;
			$application_scenario_array[$count]['scenarioType'] = $row->scenarioType;
			$application_scenario_array[$count]['scenarioDetails'] = $row->scenarioDetails;
			$count++;
		}
		$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info \n Application scenario data = ".print_r($application_scenario_array,TRUE)."\n");
		
		
		if($compatibilityNo == '')
		{
			$compatibilityNo = 0; 
		}
		
		#4144 & 4148 --  if - else block added.
		#4144 & 4148 --  if - else block added.
		$osFlag = trim($osFlag);
		$ipaddress = trim($ipaddress);
		$id = trim($id);
		$os = trim($os);
		if (is_array($hypervisorstate))
		{
			list( 
						$ver,
						$HypervisorState,
						$HypervisorName,
						$version
					) = $hypervisorstate;
		}
		
		$hypervisor_state_str = (string) $HypervisorState;
		//If the string contains all digits, then only allow, otherwise return.
		if (isset($HypervisorState) && $HypervisorState != '' && (!preg_match('/^\d+$/',$hypervisor_state_str))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for hypervisorstate $HypervisorState in configurator_register_host_static_info."); 
		}
		
		$logging_obj->my_error_handler("DEBUG"," Before register_agent \n osFlag =$osFlag \n\n IP address = $ipaddress \n\n Host id = $id \n\n Operating system = $os");
		
		//If firmware type has value NULL or Empty doesn't allow registration.
		if(array_key_exists('FirmwareType', $mbr_details)) 
		{
			$firmware_type = trim($mbr_details['FirmwareType']);
			if (!isset($firmware_type) || empty($firmware_type))
			{
				$logging_obj->my_error_handler("INFO",array("FirmwareType"=>$firmware_type, "Reason"=>"NULL", "Status"=>"Fail"),Log::BOTH);
				return;
			}
		}

		if ($osFlag!=NULL && $ipaddress!=NULL && $id!=NULL && $os!=NULL && $osFlag!='' && $ipaddress!='' && $id!='' && $os!='')
		{	
			if((trim($hostname) == '' ) || (trim($hostname) == NULL ) || empty($hostname))
            {
                $hostname = "host_".$ipaddress;
            }
			$logging_obj->my_error_handler("DEBUG","register_host_static_info\n Calling register_agent with cx info = ".print_r($cx_info,TRUE)." \n");
			$first_registeration = register_agent( $cfg, $id, $agent_version, $driver_version, $hostname, $ipaddress, $os,$compatibilityNo, $vxAgentPath,$osFlag,$Dttm,$Tmzn,$Invc,$Infs,$Syvl,$Syvc,$Syfs,$Patch_history,$AgentMode,$prodVerson,$HypervisorState,$HypervisorName,$wwnInfo,$endianess,$processorArchitecture,$cx_info,$os_array,$memory,$mbr_details,$bios_id);			
		}
		else
		{
			$logging_obj->my_error_handler("INFO",array("OsFlag"=>$osFlag, "IpAddress"=>$ipaddress, "Id"=>$id, "OsType"=>$os, "Reason"=>"NULL", "Status"=>"Fail"),Log::BOTH);
			return;
		}
		
		$pair_type_array = $vol_obj->get_pair_type_new($id,$logical_volumes_arary);
		
		if (is_array($volumes))
		{
			
			$new_volumes = $commonfunctions_obj->get_volume_hash_data($volumes,$compatibilityNo);
			ksort($new_volumes);
			foreach ($new_volumes as $key=>$disks_info)
			{
				//Capture disks/logical volumes/paratitions/extended partitions/Disk partitions 
				if (in_array($key, array(0,4,5,8,9)))
				{
					$logging_obj->my_error_handler("INFO",array("Disks"=>$disks_info),Log::APPINSIGHTS);		
				}
			}

			$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info \n New volumes = ".print_r($new_volumes,TRUE)." \n");
			foreach( $new_volumes as $volKey => $new_volume ) 
			{
				foreach( $new_volume as $volume ) 
				{
					
					$device_name = $volume['device_name'];
					$escaped_device_names = ($device_name{strlen($device_name)-1}=='\\') ? substr($device_name,0,-1) : $device_name;
					
					if ($volume['scsi_id'] != "")
					{
						// if volumeType is disk(0) or disk-partition(9) then only need to check.
						if($volKey == 0  || $volKey == 9) 
						{
							$protectedVolumeType = $prism_obj->isScsiIdProtected($id,$volume['scsi_id'], $host_shared_device , $logical_volumes_arary);
							if(isset($protectedVolumeType) && ($protectedVolumeType == $volKey))
							{
								$deviceInfo[$volume['scsi_id']][]=$volume['device_name']."^".$volume['vendorOrigin'];
							}
						}
						$devicenames[] = $escaped_device_names."^".$volume['scsi_id'];
						$device_to_update[$escaped_device_names] = $volume['scsi_id'];
						
					}
					else
					{
						$devicenames[] = $escaped_device_names;
					
					}
					
					if(!(empty($volume['diskGroup'])))
					{
						$diskGroupArray[] =  $id."^".$escaped_device_names."^".$volume['diskGroup'];
					}
					
				}
				
			}

			$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info \n Reported devices = ".print_r($deviceInfo,TRUE)." \n");	 
			register_devices($cfg, $id,$new_volumes, FALSE,$device_names,$compatibilityNo , $logical_volumes_arary , $src_volumes_arary ,$snap_shot_main_array , $volume_resize_arary , $disk_groups_array , $application_scenario_array , $host_ids, $osFlag, $cluster_volumes_array,$pair_type_array,$host_array);
				 
			// $prism_obj->compare_devices($id,$deviceInfo);
		}
		 
		
		$logging_obj->my_error_handler("DEBUG","Devicenames db = ".print_r($devicenames_db,TRUE)." \n Devicenames = ".print_r($devicenames,TRUE)."\n");
		$diff_devices = array_diff($devicenames_db, $devicenames);
		$logging_obj->my_error_handler("INFO",array("DiffDevices"=>$diff_devices),Log::BOTH);

		$diffDiskGroups = array_diff($diskGroupDb, $diskGroupArray);
	
		if(isset($diffDiskGroups))
		{	
			foreach( $diffDiskGroups as $diffDiskGroup )
			{
				$diffInfo = explode("^",$diffDiskGroup);
				$hostId = $diffInfo[0];
				$deviceName = $diffInfo[1];
				$diskGroupName = $diffInfo[2];

				$del = "DELETE FROM diskGroups WHERE hostId = '" . $conn_obj->sql_escape_string( $hostId )."' AND deviceName = '" .$conn_obj->sql_escape_string($deviceName)."' AND diskGroupName = '" .$conn_obj->sql_escape_string($diskGroupName)."'";
				
				$multi_query_static["delete"][] = $del;
				$logging_obj->my_error_handler("DEBUG"," Delete DiskGroup = $del\n");

			}		
		}
		$nic_multi_query_static = register_network_devices($nicinfo , $id, $osFlag, $cpu_array);	

		foreach( $diff_devices as $diff_device )
		{
			$logging_obj->my_error_handler("INFO","configurator_register_host_static_info \n diff_device = $diff_device \n");
			$deviceDataInfo = explode("^",$diff_device);
			$diff_deviceName = $deviceDataInfo[0];
			
			$logging_obj->my_error_handler("INFO","configurator_register_host_static_info \n Device data info = ".print_r($deviceDataInfo,TRUE)." \n diff_deviceName = $diff_deviceName\n");
			
			$search_key = $id."_".$diff_deviceName;
			$logging_obj->my_error_handler("INFO","configurator_register_host_static_info search_key :: $search_key \n");
			if((array_key_exists ($search_key , $src_volumes_arary["source"]) || array_key_exists ($search_key , $src_volumes_arary["target"]) || $logical_volumes_arary[$diff_deviceName]["deviceFlagInUse"] != 0) && is_array($src_volumes_arary))
			{
				$logging_obj->my_error_handler("INFO","configurator_register_host_static_info inside if \n");
				#Updating device with new ScsiId reported by agent
				if(array_key_exists ($diff_deviceName, $device_to_update))
				{
					$newScsiId = $device_to_update[$diff_deviceName];
					$del_query = "delete from logicalVolumes where hostId = '" . $conn_obj->sql_escape_string( $id )."' and deviceName = '".$conn_obj->sql_escape_string($diff_deviceName)."' and Phy_Lunid = '".$newScsiId."' and deviceFlagInUse = 0 and farLineProtected = 0";	
					$multi_query_static["delete"][] = $del_query;
					
					$logging_obj->my_error_handler("INFO","configurator_register_host_static_info delete query \n  $del_query");
					
					$update_query = "Update logicalVolumes set Phy_Lunid = '".$newScsiId."' where hostId = '" . $conn_obj->sql_escape_string( $id )."' and deviceName = '".$conn_obj->sql_escape_string($diff_deviceName)."' and Phy_Lunid = '".$deviceDataInfo[1]."'";	
					$multi_query_static["update"][] = $update_query;
					
					$logging_obj->my_error_handler("INFO","configurator_register_host_static_info update query \n  $update_query");
				}	
				continue;
			}
			else
			{
				$deviceDataInfo = explode("^",$diff_device);
				$diffDeviceName = $deviceDataInfo[0];
				$diffScsiId = '';			
				
				if(isset($deviceDataInfo[1]))
				{
					$diffScsiId = $deviceDataInfo[1];
				}
				$isClusterDevice = array_key_exists($diffDeviceName,$cluster_volumes_array);
				$logging_obj->my_error_handler("INFO","configurator_register_host_static_info isClusterDevice :: $isClusterDevice , length :: ".strlen($diffDeviceName) ."\n");
				/*
				*When paasive node registration call comes 
				*it will not report mount point devicenames as paasive node is not capable.
				*So , we will insert mountpoint entries for passive node also 
				*as part of active node registration call
				*That means mount ponit entries need to be present for passive node.
				*Eventhough passive will not report we should not remove those entries from database
				*Below condition checks whther calling host is windows
				*And current device is mountpoint
				*And current device is cluster device then we will go into if loop which will skip deletion
				*/
				if($osFlag == 1 && strlen($diffDeviceName)>1 && $isClusterDevice)
				{
					$logging_obj->my_error_handler("INFO","configurator_register_host_static_info diffDeviceName :: $diffDeviceName skipped \n");
					continue;
				}
				else
				{
					/*						 
					 * For both cases 
					 * 1)agent reported device with phy_lunid 
					 * 2)agent reported device without phy_lunid
					 * while deleting the entry from database we need to consider both
					 * devicename and phy_luid even if phy_lunid is blank
					*/
					$del_qry = "delete from logicalVolumes where hostId = '" . $conn_obj->sql_escape_string( $id )."' and deviceName = '".$conn_obj->sql_escape_string($diffDeviceName)."' and Phy_Lunid = '".$diffScsiId."'";		
								
				}

				$multi_query_static["delete"][] = $del_qry;
			}
		}
		
		$logging_obj->my_error_handler("DEBUG"," configurator_register_host_static_info \n Multi query static = ".print_r($multi_query_static,TRUE)." \n");
		
		$result = (int)$batchsql_obj->filterAndUpdateBatches($multi_query_static, "om_configurator :: configurator_register_host_static_info");
		if(!$result) 
		{
			$nic_status = (int)$batchsql_obj->sequentialBatchUpdates($nic_multi_query_static, "om_configurator :: configurator_register_host_static_info");
			if ($nic_status)
			{
				$result = 0;
				$status = "success";
				$logging_obj->my_error_handler("DEBUG"," configurator_register_host_static_info, MBR_DATA = ".print_r($mbr_details,TRUE)." \n");
				if(is_array($mbr_details)) 
				{
					if(array_key_exists('disks_layout_file_transfer_status',$mbr_details)) {
						$mbr_info = explode(':',$mbr_details['disks_layout_file_transfer_status']);
						$mbr_file = $conn_obj->sql_escape_string($mbr_info[0]);
						$status = trim($mbr_info[1]);
						if($status == "success") {
							$sql_mbr =  "update hosts set latestMBRFileName = '".$mbr_file."' where id = '" . $conn_obj->sql_escape_string( $id ) . "'";
							$rs_mbr = $conn_obj->sql_query($sql_mbr);
							if(!$rs_mbr)
							{
								$result = 1;	
							}
						}
						if(array_key_exists('on_demand_registration_request_id',$mbr_details)) 
						{
							$on_demand_registration_request_id_str = (string) $mbr_details['on_demand_registration_request_id'];
							//If the string contains all digits, then only allow, otherwise return.
							if (isset($mbr_details['on_demand_registration_request_id']) && $mbr_details['on_demand_registration_request_id'] != '' && (!preg_match('/^\d+$/',$on_demand_registration_request_id_str))) 
							{
								$commonfunctions_obj->bad_request_response("Invalid post data for on_demand_registration_request_id ".$mbr_details['on_demand_registration_request_id']." in configurator_register_host_static_info."); 
							}
		
							update_api_request($id,$mbr_details['on_demand_registration_request_id'],$status);
							$sql_refresh =  "update hosts set refreshStatus = '0' where id = '" . $conn_obj->sql_escape_string( $id ) . "' and refreshStatus = '".$mbr_details['on_demand_registration_request_id']."'";
							$rs_refresh = $conn_obj->sql_query($sql_refresh);
							if(!$rs_refresh)
							{
								$result = 1;	
							}
						}
					}
					else 
					{
						if(array_key_exists('on_demand_registration_request_id',$mbr_details)) 
						{
							$on_demand_registration_request_id_str = (string) $mbr_details['on_demand_registration_request_id'];
							//If the string contains all digits, then only allow, otherwise return.
							if (isset($mbr_details['on_demand_registration_request_id']) && $mbr_details['on_demand_registration_request_id'] != '' && (!preg_match('/^\d+$/',$on_demand_registration_request_id_str))) 
							{
								$commonfunctions_obj->bad_request_response("Invalid post data for on_demand_registration_request_id ".$mbr_details['on_demand_registration_request_id']." in configurator_register_host_static_info."); 
							}
							
							update_api_request($id,$mbr_details['on_demand_registration_request_id'],$status);
							$sql_refresh =  "update hosts set refreshStatus = '0' where id = '" . $conn_obj->sql_escape_string( $id ) . "' and refreshStatus = '".$mbr_details['on_demand_registration_request_id']."'";
							$rs_refresh = $conn_obj->sql_query($sql_refresh);
							if(!$rs_refresh)
							{
								$result = 1;	
							}
						}
					}
				}
			}
			else
			{
				$result = 1;
				$logging_obj->my_error_handler("INFO",array("Result"=>$result,"Status"=>"Failure","Reason"=>"BatchUpdateFailure"),Log::BOTH);
			}
			if ($sql_refresh)
				$logging_obj->my_error_handler("INFO",array("RefreshSql"=>$sql_refresh),Log::BOTH);			
		}
		
		//Processing for cluster mountpoints
		foreach($cluster_mountpoint_query as $hostId => $data)
		{
			foreach($data as $volume => $sql)
			{
				$query = "select hostId from logicalVolumes where hostId = '$hostId' and  deviceName = '". $conn_obj->sql_escape_string($volume). "'";
				$deviceData = $conn_obj->sql_query($query);
				if($conn_obj->sql_num_row($deviceData) == 0)
				{
					$rs = $conn_obj->sql_query($sql);
				}
       
			}
		}
		
		if(array_key_exists('Role',$mbr_details)) 
		{
			$agent_role = trim($mbr_details['Role']);
		}
		
		if($agent_role == "Agent")
		{
			// Update host id in discovery table and insert as physical if the record does not exist.
			$ip_address_list = array();
			$ip_address_list[] = $ipaddress;
			$system_obj = new System();
			
			if(array_key_exists('external_ip_address', $mbr_details))
			{				
				$ip_address_list[] = trim($mbr_details['external_ip_address']);
			}
			$logging_obj->my_error_handler("INFO","configurator_register_host_static_info : updateHostIdInDiscovery : bios_id : $bios_id, host_id : $id\n, ip_address_list".print_r($ip_address_list)." \n mbr_details".print_r($mbr_details, true));
			$system_obj->updateHostIdInDiscovery($bios_id, $id, $nicinfo, $ip_address_list, $hostname, $osFlag); 
		}

		if (!$GLOBALS['http_header_500'])
			$logging_obj->my_error_handler("INFO",array("Result"=>$result,"Status"=>"Success"),Log::BOTH);
		else
			$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
		return $result;
	}
	catch(Exception $e)
	{
		$exception_msg = $e->getMessage();
		$logging_obj->my_error_handler("INFO",array("Result"=>1,"Status"=>"Fail","Reason"=>$exception_msg),Log::BOTH);
		return 1;
	}
}

function generateUUIDFormat($bios_id)
{
    $bios_id = str_replace(array(' ', '-', 'VMware'), array('', '', ''), $bios_id);
	$biosId = array(
        substr($bios_id,0,8),
        substr($bios_id,8,4),
        substr($bios_id,12,4),
        substr($bios_id,16,4),
        substr($bios_id,20,strlen($bios_id))
    );
    return implode('-',$biosId);
}

function testLog ($debugString)
{
	global $REPLICATION_ROOT, $commonfunctions_obj;
	
	if(is_windows())
	{
		$PHPDEBUG_FILE = "$REPLICATION_ROOT\\var\\volume_register.log";
	}
	else
	{
	  $PHPDEBUG_FILE = "$REPLICATION_ROOT/var/volume_register.log";
	}
	  $time = date("d-m-Y H:i:s");
	  $debugString = $time." ".$debugString;
	  #global $PHPDEBUG_FILE;
	  
	  # Some debug
	  $fr = $commonfunctions_obj->file_open_handle($PHPDEBUG_FILE, 'a+');
	  if(!$fr) {
		  return;
		#echo "Error! Couldn't open the file.";
	  }
	  if (!fwrite($fr, $debugString . "\n")) {
		#print "Cannot write to file ($filename)";
	  }
	  if(!fclose($fr)) {
		#echo "Error! Couldn't close the file.";
	  }
}

function arrayLog ($debugString)
{
	global $REPLICATION_ROOT;
	if(is_windows())
	{
		$PHPDEBUG_FILE = "$REPLICATION_ROOT\\var\\array_register.log";
	}
	else
	{
	  $PHPDEBUG_FILE = "$REPLICATION_ROOT/var/array_register.log";
	}
	  $time = date("d-m-Y H:i:s");
	  $debugString = $time." ".$debugString;
	  #global $PHPDEBUG_FILE;
	  
	  # Some debug
	  $fr = fopen($PHPDEBUG_FILE, 'a+');
	  if(!$fr) {
		#echo "Error! Couldn't open the file.";
	  }
	  if (!fwrite($fr, $debugString . "\n")) {
		#print "Cannot write to file ($filename)";
	  }
	  if(!fclose($fr)) {
		#echo "Error! Couldn't close the file.";
	  }
}
function volumesLog ($debugString)
{
	$time = date("d-m-Y H:i:s");
	$debugString = $time." ".$debugString;
	global $REPLICATION_ROOT;
	#global $PHPDEBUG_FILE;
	$PHPDEBUG_FILE = "$REPLICATION_ROOT/volumes.txt";
	# Some debug
	$fr = fopen($PHPDEBUG_FILE, 'a+');
	if(!$fr) {
		return;
	#echo "Error! Couldn't open the file.";
	}
	if (!fwrite($fr, $debugString . "\n")) {
	#print "Cannot write to file ($filename)";
	}
	if(!fclose($fr)) {
	#echo "Error! Couldn't close the file.";
	}
}

/*
 * Function Name: validate_ip_address
 *
 * Description:
 *   Used to get the valid Ipaddress from the list of NICs. 
 *   Excludes all the loop back address,IPV6 address.
 * 
 * Return Value: Returns the first IPV4 IP from the sorted list
 */

function validate_ip_address($nicinfo, $ip_address = 0, $osFlag)
{
	global $logging_obj;
	$primary_ip_address = 0;
	$ip_list = array();
	if(is_array($nicinfo))
	{
		if(count($nicinfo)) 
		{
			foreach($nicinfo as $key => $value)
			{
				if($osFlag == 1)
				{
					if(is_array($value[11]))
					{
						foreach($value[11] as $key2 => $value2)
						{
							if(preg_match('/physical/i', $value2))
							{
								if((preg_match('/^127\./',$key2)) || (preg_match('/:/',$key2))) 
								{
									continue;
								}
								else
								{
									$ip_list[] = $key2;
								}
							}
						}
					}
				}
				else if(is_array($value[2]))
				{
					foreach($value[2] as $key2 => $value2)
					{
						if((preg_match('/^127\./',$key2)) || (preg_match('/:/',$key2))) 
						{
							continue;
						}
						else
						{
							$ip_list[] = $key2;
						}
					}
				}
			}
			$logging_obj->my_error_handler("INFO",array("ValidatedIpList"=>$ip_list),Log::BOTH);
			if(count($ip_list))
			{
				$primary_ip_address = ($ip_address) ? $ip_address : array_shift($ip_list); 
			}
		}
	}
	return $primary_ip_address;
}

/* 
Function Name:configurator_register_volumes 
 
Description:
This function registers the new volpack in the database 
or updates the capacity of the existing volpack.
       
Parameters:
    Param 1 [IN]:$cfg
    Param 2 [IN]:$host_id
    Param 3 [IN]:$array_volpack_info 
*/

function configurator_register_volumes($cfg, $host_id, $array_volpack_info)
{
    try
    {
	    global $logging_obj;
        global $conn_obj;
        return 0;
    }
    catch(Exception $e)
    {
        $logging_obj->my_error_handler("DEBUG"," configurator_register_volumes EXCEPTION ".$e->getMessage());
		return 1;
	}
}

function configurator_register_cluster_info($cfg,$hostid,$clustername,$groupname,$groupid,
											$action,$newgroupname="",$volumes=array(),$fieldName="",
											$fieldValue="")
{
	return TRUE;
}
?>	