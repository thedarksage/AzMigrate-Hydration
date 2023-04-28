<?php
/*
 *$Header: /src/admin/web/app/Export.php,v 1.35.72.1 2017/08/16 15:57:41 srpatnan Exp $ 
 *================================================================= 
 * FILENAME
 *    Export.php
 *
 * DESCRIPTION
 *    This script contains functions related to export device over
 *    FC, ISCSI and Mount Point
 *
 * HISTORY
 *     20-Feb-2008  Reena    
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
    
class Export
{
    /*
    * $conn: database connection object
    */
    private $conn;
    private $create_export_lun_pending;
    private $delete_export_lun_pending;
    private $DELETE_ACG_PENDING;
    private $CREATE_ACG_PENDING;
	private $DELETE_ACG_PORTS;
    private $snapshot_creation_pending;
	private $snapshot_creation_done;
    /*
    * Function Name: Export
    *
    * Description:   
    *    This is the constructor of the class
    *
    */
    public function Export()
    {
        global $CREATE_EXPORT_LUN_PENDING;
        global $DELETE_EXPORT_LUN_PENDING;
        global $CREATE_ACG_PENDING;
        global $DELETE_ACG_PENDING;
		global $DELETE_ACG_PORTS;
        global $SNAPSHOT_CREATION_PENDING,$SNAPSHOT_CREATION_DONE;
        
        global $CREATE_EXPORT_LUN_DONE;
        global $CREATE_ACG_DONE;
		global $conn_obj;
		
        $this->CREATE_EXPORT_LUN_DONE = $CREATE_EXPORT_LUN_DONE;
        $this->DELETE_ACG_PENDING = $DELETE_ACG_PENDING;
        $this->CREATE_ACG_PENDING = $CREATE_ACG_PENDING;
        $this->CREATE_ACG_DONE = $CREATE_ACG_DONE;
        $this->DELETE_ACG_PORTS = $DELETE_ACG_PORTS;
        $this->snapshot_creation_pending = $SNAPSHOT_CREATION_PENDING;
		$this->snapshot_creation_done = $SNAPSHOT_CREATION_DONE;
        
        $this->create_export_lun_pending = $CREATE_EXPORT_LUN_PENDING;
        $this->delete_export_lun_pending = $DELETE_EXPORT_LUN_PENDING;
        $this->delete_acg_pending = $DELETE_ACG_PENDING;
        $this->conn = $conn_obj;
    }
}
?>
