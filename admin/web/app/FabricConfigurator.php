<?php
/*
 *================================================================= 
 * FILENAME
 *  FabricConfigurator.php
 *
 * DESCRIPTION
 *  This script contains all the Fabric background API
 *  calls which are invoked by Fabric Agent.	
 *
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/

class FabricConfigurator
{
	public $conn;
	
	/* Constructor comes here   */
	public function __construct()
	{
		global $conn_obj;
		
		$this->conn = $conn_obj;
		$this->fabric_obj = new Fabric();
		$this->commonfunctions_obj = new CommonFunctions();
		$this->proc_obj = new ProcessServer();
	}	
}
?>
