<?php
class ReplicationOptions extends Template
{
	private $options;
	
	public function getOptions($groupName, $params='')
	{
		switch($groupName)
		{
			case 'Intel':
				return $this->getIntelOptions();
			case 'Array':
				return $this->getArrayOptions($params);
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
		$this->options['batchResync'] = 3;
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
	
	private function getArrayOptions($params)
	{
		$this->options['syncOptions'] = 2;
		$this->options['secureCXtotrg'] = 0;
		$this->options['secureSrctoCX'] = 0;
		$this->options['resynThreshold'] = 2048;
		$this->options['rpo'] = 30;
		$this->options['batchResync'] = 3;			
		$this->options['diffThreshold'] = 8192;
		$this->options['autoResync'] = array('isEnabled'=>1,'start_hr'=>18,'start_min'=>00,'stop_hr'=>06,'stop_min'=>00,'wait_min'=>30);
		$this->options['compression'] = 0;
		
		list($primary_app_id,$primary_app_name) = explode("=",$params['primary_appliance']);
		list($secondary_app_id,$secondary_app_name) = explode("=",$params['secondary_appliance']);
		//echo "$secondary_app_id == $primary_app_id && $primary_app_name == $secondary_app_name";
		if($secondary_app_id == $primary_app_id)
		{			
			$this->options['secureSrctoCXOption'] = 'DISABLED';	
			$this->options['secureCXtoTrgOption'] = 'DISABLED';				
			$this->options['compressionOption']	  = 'DISABLED';
			$this->options['syncOption']	  = 'DIRECT_COPY';
		}
		else
		{
			$this->options['secureSrctoCXOption'] = '';
			$this->options['secureCXtoTrgOption'] = '';			
			$this->options['compressionOption']	  = '';	
		}

		return $this->options;
	}
	
}
?>