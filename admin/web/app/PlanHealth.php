<?php
class planHealth
{
	private $planId;
	private $conn_obj;
	private $commonfunctions_obj;
	private $data = array();
	public $health_status_arr = array();
	
	public function __construct($planId="",$from_plan_health="")
	{
		global $conn_obj;
		$this->conn_obj = $conn_obj;
		$this->planId = $planId;
		$this->commonfunctions_obj = new CommonFunctions();
		if($from_plan_health) $this->from_plan_health = 1;
		#$this->loadPlanHealthSettings();
	}
	
	public function __set($dt, $vl) 
	{
		$this->data[$dt] = $vl;
	}
	
	public function __get($dt) 
	{
		return $this->data[$dt];
	}
}
?>