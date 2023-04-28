<?php
/*
 *$Header: /src/admin/web/app/UI.php,v 1.161.8.1 2017/08/16 15:57:42 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *    UI.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    Menu as header.footer,top menu
 *
 * HISTORY
 *     20-Feb-2008  Author    Created - Varun
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
class UI
{
    private $commonfunctions_obj;
    private $conn;

    /*
     * Function Name: License
     *
     * Description:
     *    This is the constructor of the class 
     *    performs the following actions.
     *
     *    <Give more details here>    
     * Exceptions:
     *     <Specify the type of exception caught>
     */
    function __construct()
    {
		global $conn_obj;
		$this->commonfunctions_obj = new CommonFunctions();
		$this->conn = $conn_obj;
    }	
};
?>