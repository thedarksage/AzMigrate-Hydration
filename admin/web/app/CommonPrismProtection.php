<?php
class CommonPrismProtection
{
	private $conn;
	
	public function __construct()
	{
		global $conn_obj;
		$this->conn = $conn_obj;
	}
}
?>