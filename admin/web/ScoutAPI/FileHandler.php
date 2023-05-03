<?php
class FileHandler extends ResourceHandler 
{
	private $fxObj;
	private $protectionObj;
	private $commonfunctionObj;
	private $conn;
	
	public function FileHandler() 
	{
		$this->fxObj = new FileProtection();
		$this->protectionObj = new ProtectionPlan();
		$this->commonfunctionObj = new CommonFunctions();
		$this->conn = new Connection();
	}
}
?>