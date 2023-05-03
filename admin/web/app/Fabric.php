<?PHP
/*
 *================================================================= 
 * FILENAME
 *  Fabric.php
 *
 * DESCRIPTION
 *  This script contains all the SAN functions.
 *
 * HISTORY
 *      07-March-2008  Created by Prakash Goyal 
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/

class Fabric
{

	private $commonfunctions_obj;
    private $conn;
    
    /* Constructor comes here   */
    public function Fabric()
    {
    	global $conn_obj;
		
		$this->commonfunctions_obj = new CommonFunctions();
        $this->conn = $conn_obj;
        $this->replication_root = $REPLICATION_ROOT;
    }
} 

?>
