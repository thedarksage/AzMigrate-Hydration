<?php
class ManageArray
{
	private $conn;
	public $commonfunctions_obj;
	function __construct()
    {
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->commonfunctions_obj = new CommonFunctions();
	}
	
	private function GUID()
	{
		return sprintf('%04X%04X-%04X-%04X-%04X-%04X%04X%04X', mt_rand(0, 65535), mt_rand(0, 65535), mt_rand(0, 65535), mt_rand(16384, 20479), mt_rand(32768, 49151), mt_rand(0, 65535), mt_rand(0, 65535), mt_rand(0, 65535));
	}
};
	
?>