<?php
class LicensingHandler extends ResourceHandler
{
	private $conn;
	private $volObj;
	private $protectionObj;
	
	public function LicensingHandler() 
	{
		$this->volObj = new VolumeProtection();
		$this->conn = new Connection();
		$this->protectionObj = new ProtectionPlan();
	}
}
?>