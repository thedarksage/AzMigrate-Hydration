<?php

class OracleProtection
{
	private $conn;
	
	function __construct()
	{
		global $conn_obj;

		$this->conn = $conn_obj;
	}
}
?>