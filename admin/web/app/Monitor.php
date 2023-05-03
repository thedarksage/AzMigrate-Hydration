<?php
  /*
 *$Header: /src/admin/web/app/Monitor.php,v 1.33.8.2 2018/07/16 13:31:08 prgoyal Exp $
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
class Monitor
{
    private $conn_obj;
	private $commonfunctions_obj;
	private $report_obj;
	private $components;

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
	function __construct()
	{
		global $conn_obj;
		
		$this->conn_obj = $conn_obj;
		$this->commonfunctions_obj = new CommonFunctions();
		$this->components = $this->_initComponenets();
	}
	
	private function _initComponenets()
	{
	  global $_;
       $comp = array(); 
	   $comp[0]['title'] = $this->commonfunctions_obj->NLS('Protection Health');
	   $comp[0]['url'] =   'monitor_plans.php?ajaxlink=1';
       $comp[1]['title'] = $this->commonfunctions_obj->NLS('Alerts and Notifications');
	   $comp[1]['url'] =   'db_alerts.php';
	   $comp[2]['title'] = $this->commonfunctions_obj->NLS('CS/PS Health');
	   $comp[2]['url'] =   'monitor_refresh.php';
   
	   return $comp;
	}
	
	public function getDashboardAlerts($err_category ='', $err_level = '', $alert_id ='')
	{
		global $VM_HEALTH, $THREE, $ONE, $ZERO;
		$args = array();
		
		$err_category_col = $err_level_col = $alert_id_col = '';
		if(strtolower($err_category) != 'all' && $err_category != '')  {
			$err_category_col = "AND dba.errCategory = ?";
			array_push($args,$err_category);
		}
		if(strtolower($err_level) != 'all' && $err_level != '')  {
			$err_level_col = "AND dba.errLevel = ?";
			array_push($args, strtoupper($err_level));
		}
		if($alert_id)  {
			$alert_id_col = "AND dba.alertId > ?";
			array_push($args,$alert_id);
			array_push($args,$alert_id);
		}
		$sql = "SELECT                        
						dba.errCode as EventCode,
						ec.errType as EventType,
						dba.errCategory as Category,
						ec.errComponent as Component,
						dba.hostId as HostId,
						h.name as HostName,
						dba.errLevel as Severity,        
						UNIX_TIMESTAMP(dba.errStartTime) as EventTime,
                        ec.errManagementFlag as ManagementFlag,
						dba.errSummary as Summary,    
						dba.errMessage as Details, 
						ec.errCorrectiveAction as CorrectiveAction,         
						dba.errPlaceholders
					FROM
						dashboardAlerts dba,
                        eventCodes ec,
                        hosts h                
					WHERE
                        dba.hostId = h.id AND
						dba.eventCodesDependency = '$ONE' AND 
                        dba.errCode = ec.errCode AND
						ec.category = 'Alerts/Events' AND
						ec.errInUse = 'Yes' AND
						ec.errType != '' $err_category_col $err_level_col $alert_id_col
					UNION
					SELECT
						dba.errCode as EventCode,
						'$VM_HEALTH' as EventType,
						dba.errCategory as Category,
						dba.errComponent as Component,
						dba.hostId as HostId,
						h.name as HostName,
						dba.errLevel as Severity,        
						UNIX_TIMESTAMP(dba.errStartTime) as EventTime,
                        '$THREE' as ManagementFlag,
						dba.errSummary as Summary,    
						dba.errMessage as Details, 
						dba.errCode as CorrectiveAction,         
						dba.errPlaceholders
					FROM
						dashboardAlerts dba,
                        hosts h                
					WHERE
                        dba.hostId = h.id AND
						dba.eventCodesDependency = '$ZERO' 
                        $err_category_col $err_level_col $alert_id_col";

		$alert_details = $this->conn_obj->sql_query($sql, $args);
        $event_details = array();
        
        // Get all host ids
        $host_ids = array();
        foreach($alert_details as $alert)
        {
            $host_ids[] = $alert['HostId'];
        }
        
        // Get all display names for the above hostIds
        $host_ids_str = implode(",", $host_ids);
        $vm_display_array = $this->conn_obj->sql_get_array("SELECT hostId, displayName FROM infrastructureVMs WHERE FIND_IN_SET(hostId, ?)", "hostId", "displayName", array($host_ids_str));
        
        foreach($alert_details as $alert)
        {
            if($vm_display_array[$alert['HostId']]) $alert['HostName'] = $vm_display_array[$alert['HostId']];
            if(($alert['EventCode'] == "EA0436") || ($alert['EventCode'] == "EA0437") || ($alert['EventCode'] == "EA0438")) $alert['Details'] = "";
            $event_details[] = $alert;
        }
        
		return $event_details;
	}
};
?>
