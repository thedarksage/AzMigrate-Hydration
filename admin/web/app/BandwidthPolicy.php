<?php
/*    
 *$Header: /src/admin/web/app/BandwidthPolicy.php,v 1.39.40.1 2017/08/16 15:57:41 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *    BandwidthPolicy.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    Bandwidth management
 *
 * HISTORY
 *     20-Feb-2008  Author    Created - Chandra Sekhar  & Varun
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
class BandwidthPolicy
{    
    private $commonfunctions_obj;
    private $in_profiler;
    private $in_applabel;
    private $conn;
    /*
     * Function Name: BandwidthPolicy
     *
     * Description:
     *    This is the constructor of the class 
     *    performs the following actions.
     *
     *    <Give more details here>    
     *
     * Parameters:
     *     Param 1 [IN/OUT]:
     *     Param 2 [IN/OUT]:
     *
     * Return Value:
     *     Ret Value: <Return value, specify whether Array or Scalar>
     *
     * Exceptions:
     *     <Specify the type of exception caught>
     */
    public function BandwidthPolicy()
    {
        global $IN_PROFILER;
        global $IN_APPLABEL;
		global $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
    
        $this->conn = $conn_obj;
        
        $this->in_profiler = $IN_PROFILER;
        $this->in_applabel = $IN_APPLABEL;
    }  
};
?>