<?php
class MetadataHandler extends ResourceHandler 
{
	private $system_obj;
	
	public function MetadataHandler() 
	{
		$this->system_obj = new System();
		$this->conn = new Connection();
	}
}