<?php
class SqlProtection
{
	private $conn;
	private $app_obj;
	function __construct($app)
	{
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->app_obj = $app;
	}
}
?>