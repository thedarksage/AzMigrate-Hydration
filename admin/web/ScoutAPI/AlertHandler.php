<?php
class AlertHandler extends ResourceHandler 
{

	private $volume_obj;
	
	public function AlertHandler() 
	{
		$this->volume_obj = new VolumeProtection();
		$this->conn = new Connection();
	}
}