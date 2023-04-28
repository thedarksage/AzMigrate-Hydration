<?php
/*
 *================================================================= 
 * FILENAME
 *    FileProtection.php
 *
 * DESCRIPTION
 *   This script consists a class,which contains
 *   all the functions related to FX.                 
 *=================================================================
*/
class FileProtection 
{
    
    private $commonfunctions_obj;
    private $conn;
        
    /*
    * Constructor Name: FileProtection
    *
    * Description:
    *    This Constructor is responsible for forking multiple processes that 
    *    performs the following actions.
    *
    * Parameters:
    *     Param 1 [IN/OUT]:
    *     Param 2 [IN/OUT]:
    *
    * Return Value:
    *     Ret Value: Returns Scalar value of ruleId.
    *
    * Exceptions:
    *     DataBase Connection fails.
    */
    public function FileProtection()
    {
		global $conn_obj;		
		$this->commonfunctions_obj = new CommonFunctions();
        $this->conn = $conn_obj;
    }

    /*		
    * Function Name: unregister_fr_agent
    *
    * Description:
    *    Releases the Fx agent. Sets filereplicationAgentEnabled flag
    *    in hosts table.				
    *
    * Parameters:
    *     Param 1 [IN]: id
    *
    * Return Value:
    *     Ret Value: N/A
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function unregister_fr_agent ($id, $flag='',$do_not_delete_job=0)
    {
		global $logging_obj;		
		$status_flag = FALSE;
		
		try
		{
			$logging_obj->my_error_handler("INFO","Entered into unregister_fr_agent with id::$id");
			
			$this->conn->sql_query("BEGIN");
			
			$host_info = $this->commonfunctions_obj->get_host_info($id);
			$logging_obj->my_error_handler("INFO","unregister_fr_agent host_info".print_r($host_info,true));
			
			$hostid = $host_info['id'];
			$hostname = $host_info['name'];
			$ipaddress = $host_info['ipaddress'];
			$sentinelEnabled = $host_info['sentinelEnabled'];
			$outpostAgentEnabled = $host_info['outpostAgentEnabled'];
			$filereplicationAgentEnabled = $host_info['filereplicationAgentEnabled'];
		 
			if($flag != 1 )
			{
				#bug 6318
				$update_hosts = $this->conn->sql_query ("UPDATE  
											hosts 
									   SET 
											filereplicationAgentEnabled='0',
											fr_version='',
											PatchHistoryFX=''
											WHERE id='$id'");
				#End Bug 6318
				#BUG 7716 Added processServerEnabled check while deleting entry from hosts table
				#BUG 7794 Added processServerEnabled NULL check in query
				if(!$update_hosts)
				{
					throw new Exception("Unable to update the hosts table details");
				}
				
				$logging_obj->my_error_handler("INFO","unregister_fr_agent sentinelEnabled::$sentinelEnabled, outpostAgentEnabled::$outpostAgentEnabled");

				if(($flag && $sentinelEnabled == 0 && $outpostAgentEnabled==0) || (!$flag))
				{
					$query = "DELETE
							FROM
								hosts 
							WHERE 
								sentinelEnabled='0'
							AND
								outpostAgentEnabled='0' " .
							"AND
								filereplicationAgentEnabled='0' 
							AND 
								(processServerEnabled = '0'
								 OR 
								 processServerEnabled IS NULL)
							AND 
								id='$id'";
					$logging_obj->my_error_handler("INFO","unregister_fr_agent query::$query");	
					$delete_hosts = $this->conn->sql_query ($query);
					
					if(!$delete_hosts)
					{
						throw new Exception("Unable to delete the entries from hosts table");
					}
				}
			}	
		
			$this->conn->sql_query("COMMIT");
			//For audit log
			$this->commonfunctions_obj->writeLog($_SESSION['username'],$this->commonfunctions_obj->customer_branding_names("FX Agent unregistered from CX with details (hostName::$hostname, ipaddress:$ipaddress, hostid::$hostid, filereplicationAgentEnabled::$filereplicationAgentEnabled)"));
			$status_flag = TRUE;
		}		
		catch (Exception $e)
		{
			//If any query fails then rollbacking the changes
			$message = $e->getMessage();
			$logging_obj->my_error_handler("INFO","unregister_fr_agent message::$message");	
			$this->conn->sql_query("ROLLBACK");
		}
		return $status_flag;
	}
};
?>