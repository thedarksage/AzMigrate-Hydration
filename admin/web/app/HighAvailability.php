<?php        

/*
 *================================================================= 
 * FILENAME
 *  HA.php
 *
 * DESCRIPTION
 *  This script contains all the HA functions.
 *
 * HISTORY
 *      2-Dec-2008  Created by Shrinivas M A
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
class HighAvailability
{
    /*
    * class constructor
    */
    private $conn;
    
	public function HighAvailability()
    {
        global $conn_obj;
		$this->conn = $conn_obj;
        $this->volume_obj = new VolumeProtection();
        $this->file_obj = new FileProtection();
 	    $this->commonfunctions_obj = new CommonFunctions();
    }
};
?>
