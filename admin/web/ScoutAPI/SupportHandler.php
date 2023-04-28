<?php
class SupportHandler extends ResourceHandler
{
	private $taskObj;
	
	public function SupportHandler() 
	{
		$this->taskObj = new Task();		
	}
}
?>