<?php
class ArrayProtection extends ArrayHandler
{
	public $conn;
	public $scenarioId;
	public $planProperties;
	public $editMode;
	public function ArrayProtection() 
	{
		global $conn_obj;
		$this->conn = $conn_obj;
	}
}
?>
