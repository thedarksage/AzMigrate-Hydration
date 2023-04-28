<?php
  /*
 *$Header: /src/admin/web/app/Dashboard.php,v 1.60.8.1 2017/08/16 15:57:41 srpatnan Exp $
 *=================================================================
 * FILENAME
 *    Dashboard.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    Dashboard
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
*/
class Dashboard
{
	var $conn_obj;


   /*
     * Function Name: Dashboard
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
	public function Dashboard()
	{
		global $conn_obj;

		$this->conn_obj = $conn_obj;
		$this->components = $this->_initComponenets();
		$this->commonfunctions_obj = new CommonFunctions();
	}

	private function _initComponenets()
	{
	   $comp[0]['title'] = 'Protection Health';
	   $comp[0]['url'] =   'db_protection_health.php';
	   $comp[1]['title'] = 'Alerts and Notifications';
	   $comp[1]['url'] =   'db_alerts.php';
	   $comp[2]['title'] = 'CS/PS Health';
	   $comp[2]['url'] =   'db_cs_ps_health.php';

	   return $comp;
	}	
};
?>
