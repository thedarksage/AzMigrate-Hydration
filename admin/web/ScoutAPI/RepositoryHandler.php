<?php
class RepositoryHandler extends ResourceHandler
{
	private $conn;
	private $volObj;
	private $expotObj;
	
	public function RepositoryHandler() 
	{
		$this->volObj = new VolumeProtection();
		$this->conn = new Connection();
		$this->expotObj = new Export();
	}
}
?>