<?php
class SRAHandler extends ResourceHandler
{
	private $conn;

	public function SRAHandler() 
	{
        $this->conn = new Connection();
        $this->commonfunc = new CommonFunctions();
        $this->SRMObj = new SRM();
	}
}
?>