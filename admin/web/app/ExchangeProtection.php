<?php
class ExchangeProtection
{
	private $conn;
	private $app_obj;
	
	public function __construct($app)
	{
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->app_obj = $app;
	}
}

?>