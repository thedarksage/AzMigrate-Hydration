<?php
/*
 *$Header: /src/admin/web/app/CxReport.php,v 1.141.8.2 2017/08/16 15:57:41 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *    CxReport.php
 *
 * DESCRIPTION
 *    This script contains methods related to
 *    xml rpc call
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
class CxReport
{
    var $conn;
    var $commonfunctions_obj;
    var $volume_obj;
    var $fx_obj;
    var $trending_obj;
	var $app_obj;
	var $protection_plan_obj;
	var $recovery_obj;
	var $recovery_plan_obj;
    var $rrdcommand;
    var $installation_dir;
	var $sparseretention_obj;
	var $system_obj;
	var $ps_obj;
	var $esx_master_conf_obj;
	var $esx_recovery;
	var $error_codes;
	
    /*
     * Function Name: CxReport
     *
     * Description:
     *    This is the constructor of the class 
     *    performs the following actions.
     *
     *
     * Parameters: None 
     *
     * Return Value:None
     *
     * Exceptions:None
     *    
     */	
    public function CxReport()
    {
        global $INSTALLATION_DIR;
        global $conn_obj;
		
        $this->conn = $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
        $this->volume_obj = new VolumeProtection();
        $this->fx_obj = new FileProtection();
        $this->trending_obj = new Trending();
		$this->app_obj = new Application(); 
		$this->protection_plan_obj = new ProtectionPlan(); 		
		$this->recovery_plan_obj = new RecoveryPlan();
		$this->recovery_obj = new Recovery();
		$this->sparseretention_obj = new SparseRetention();	
		$this->system_obj = new System();
		$this->ps_obj = new ProcessServer();
		$this->esx_master_conf_obj = new ESXMasterConfigParser();
		$this->esx_recovery = new ESXRecovery();
        $this->installation_dir = $INSTALLATION_DIR;
		
        if(!$this->commonfunctions_obj->is_linux())
        {
			$this->amethyst_details = $this->commonfunctions_obj->amethyst_values();
            $this->rrd_install_path = $this->amethyst_details['RRDTOOL_PATH'];
			$this->rrd_install_path = addslashes($this->rrd_install_path);
			$this->rrdcommand = $this->rrd_install_path."\\rrdtool\\Release\\rrdtool.exe";
        }
        else
        {
            $this->rrdcommand = "rrdtool";
        }  

		$this->error_codes = $this->setErrorCodes();
    }	 
		
	/*
     * Function Name: setErrorCodes
     *
     * Description:
     *    This function sets the error codes 
     *     variable for the XML RPC errors
     *
     * Parameters:
	 *	   [IN] : NONE
     *
     * Return Value: Array containing error codes
     *
     * Exceptions:None
     *    
     */	
	
	private function setErrorCodes()
	{
		$arr['INVALID_CREDENTIALS'] = array ('1000', 'No users matching the credentials');
		
		return $arr;
	}
}
?>
