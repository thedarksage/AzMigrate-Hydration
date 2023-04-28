<?php
class Db2Discovery
{
	private $conn;
	private $app_obj;
	function __construct($app)
	{
		global $conn_obj;
		
		$this->conn = $conn_obj;
		$this->app_obj = $app;
		$this->db2_obj = new Db2Protection();
	}	
}	
?>
