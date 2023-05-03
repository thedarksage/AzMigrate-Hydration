<?php
class VolumeHandler extends ResourceHandler 
{

	private $volume_obj;
	
	public function VolumeHandler() 
	{
		$this->volume_obj = new VolumeProtection();
		$this->conn = new Connection();
	}
}

?>