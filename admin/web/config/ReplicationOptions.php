<?php
class ReplicationOptions extends Template
{
	private $options;
	
	public function getOptions($groupName)
	{
		switch($groupName)
		{
			case 'Intel':
				return $this->getIntelOptions();
			default:
				return $this->getdefaultOptions();
		}
	}
	
	/*
   * Get default Replication options for Intel
   */
	private function getIntelOptions()
	{
		$this->options['syncOptions'] = 2;
		$this->options['secureCXtotrg'] = 0;
		$this->options['resynThreshold'] = 2048;
		$this->options['rpo'] = 30;
		$this->options['batchResync'] = 0;
		$this->options['secureSrctoCX'] = 0;
		$this->options['diffThreshold'] = 8192;
		$this->options['autoResync'] = array('isEnabled' => 0);
		$this->options['compression'] = 0;
		
		return $this->options;
	}
	
	/*
   * Get default Replication options for Intel
   */
	private function getdefaultOptions()
	{
		$this->options['syncOptions'] = 2;
		$this->options['secureCXtotrg'] = 0;
		$this->options['resynThreshold'] = 2048;
		$this->options['rpo'] = 30;
		$this->options['batchResync'] = 3;
		$this->options['secureSrctoCX'] = 0;
		$this->options['diffThreshold'] = 8192;
		$this->options['autoResync'] = array('isEnabled'=>1,'start_hr'=>18,'start_min'=>00,'stop_hr'=>06,'stop_min'=>00,'wait_min'=>30);
		$this->options['compression'] = 1;
		
		return $this->options;
	}
	
}
?>