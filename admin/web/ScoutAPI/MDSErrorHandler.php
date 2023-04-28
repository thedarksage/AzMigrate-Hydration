<?php
class MDSErrorHandler extends ResourceHandler 
{
	private $conn;
		
	public function MDSErrorHandler() 
	{
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->mds_obj = new MDSErrorLogging();
	}
	
	public function GetErrorLogs_validate($identity, $version, $functionName, $parameters)
	{		
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			
			$log_id = '';
			
			if(isset($parameters["Token"]))
			{
				$log_id = trim($parameters["Token"]->getValue());
                if(!$log_id) 
                {
                    ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Token"));
                }
                elseif(!ctype_digit("$log_id"))
                {
                    ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Token"));
                }
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	
	public function GetErrorLogs($identity, $version, $functionName, $parameters) 
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			$response = new ParameterGroup ( "Response" );
			
			$log_id = (isset($parameters["Token"])) ? trim($parameters["Token"]->getValue()) : '';
			
			if($log_id) $this->mds_obj->removeErrorLogs($log_id);
			
			$log_details = $this->mds_obj->getErrorLogs($log_id);
			
			if(!$log_details)
			{
				$log_grp = new ParameterGroup("Logs");
				$response->addParameterGroup($log_grp);
			}
			else
			{
				$token_id = $this->mds_obj->getToken($log_id);
				$next_id = $this->conn->sql_get_value('MDSLogging', 'logId', "logId > ?", array($token_id));
				$recall_flag = $next_id ? "Yes": "No";
				
				$response->setParameterNameValuePair("Token",$token_id);
				$response->setParameterNameValuePair("ReCallFlag",$recall_flag);
			
				if(count($log_details))
				{
					foreach($log_details as $key=>$val)
					{
						$logs = new ParameterGroup ("Log".++$key);
						foreach($val as $k=>$v)
						{
							$logs->setParameterNameValuePair($k,$v);
						}
						$response->addParameterGroup ($logs);
					}
				}	
			}
			
			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
		
	}
}